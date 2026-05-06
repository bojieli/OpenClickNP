// SPDX-License-Identifier: Apache-2.0
//
// openclicknp_engine — OpenSSL 3.x ENGINE that routes RSA modular
// exponentiation through the OpenClickNP RSA accelerator path.
//
// This is the integration layer that lets stock OpenSSL (and anything
// that links it — `openssl speed rsa`, `openssl s_server`, libcurl,
// nginx, etc.) drive RSA operations through our hardware path
// without any application change. With the engine loaded, every
// BN_mod_exp_mont call in libcrypto's RSA code path is dispatched to
// openclicknp_modexp() in openclicknp_modexp.cpp, which forwards to
// the SW emulator (today) or the FPGA (when an .xclbin is available).
//
// Build (see ssl_integration/CMakeLists.txt):
//   cc -shared -fPIC -o openclicknp.so openclicknp_engine.c \
//      openclicknp_modexp.cpp -lcrypto -lstdc++
//
// Use:
//   OPENSSL_CONF=ssl_integration/openssl.cnf \
//   openssl engine -t -c openclicknp
//   openssl speed -engine openclicknp rsa1024 rsa2048 rsa4096
//   openssl s_server -engine openclicknp -cert ... -key ...
//
// References (no code copied — written from API docs):
//   crypto/engine/eng_*.c in openssl,
//   man 3 ENGINE_set_id, BN_mod_exp_mont, RSA_meth_*.
#include <openssl/engine.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OCNP_ENGINE_ID   "openclicknp"
#define OCNP_ENGINE_NAME "OpenClickNP RSA modexp accelerator"

// Forward declaration — implementation in openclicknp_modexp.cpp.
// Both buffers are little-endian arrays of n_limbs uint64_t each.
// Returns 0 on success, non-zero on failure.
extern int openclicknp_modexp(uint64_t* out,
                              const uint64_t* m,
                              const uint64_t* e,
                              const uint64_t* n,
                              int n_limbs);

// Counters are useful for `openssl engine -t` to confirm it actually
// got called.
static unsigned long g_modexp_calls = 0;
static unsigned long g_modexp_unsupported = 0;

// ---------- BIGNUM <-> limb-array conversion ----------

static int bn_to_limbs(const BIGNUM* x, uint64_t* out, int n_limbs) {
    int blen = BN_num_bytes(x);
    if (blen > n_limbs * 8) return 0;
    unsigned char buf[8 * 64];  // up to 4096 bits
    if (BN_bn2binpad(x, buf, n_limbs * 8) < 0) return 0;
    // BN_bn2binpad emits big-endian; convert to limb-LSB-first.
    for (int i = 0; i < n_limbs; ++i) {
        uint64_t v = 0;
        for (int b = 0; b < 8; ++b)
            v = (v << 8) | buf[(n_limbs - 1 - i) * 8 + b];
        out[i] = v;
    }
    return 1;
}

static int limbs_to_bn(const uint64_t* x, int n_limbs, BIGNUM* out) {
    unsigned char buf[8 * 64];
    for (int i = 0; i < n_limbs; ++i) {
        uint64_t v = x[i];
        for (int b = 0; b < 8; ++b)
            buf[(n_limbs - 1 - i) * 8 + (7 - b)] = (unsigned char)((v >> (b * 8)) & 0xff);
    }
    return BN_bin2bn(buf, n_limbs * 8, out) != NULL;
}

static int round_up_limbs_for_bits(int bits) {
    if (bits <= 1024) return 16;
    if (bits <= 2048) return 32;
    if (bits <= 4096) return 64;
    return 0;  // unsupported
}

// ---------- The hooked BN_mod_exp ----------
//
// Signature comes from RSA_meth_set_bn_mod_exp:
//     int (*bn_mod_exp)(const RSA *rsa, BIGNUM *r, const BIGNUM *I,
//                       const RSA_METHOD *method);
// — but RSA_METHOD's bn_mod_exp slot is for raw (a, p, m). The actual
// dispatch within libcrypto's rsa_ossl.c uses BN_mod_exp_mont on
// (a, p, m). To keep things tractable for v0.1 we hook the public-key
// op (RSA_public_encrypt) which is the modexp the TLS handshake's
// signature-verify uses. Private-key ops fall back to libcrypto.

static RSA_METHOD* g_ocnp_rsa_method = NULL;
static const RSA_METHOD* g_default_rsa_method = NULL;

// RSA_meth_set_bn_mod_exp signature is
//   int (*)(BIGNUM*, const BIGNUM*, const BIGNUM*, const BIGNUM*,
//           BN_CTX*, BN_MONT_CTX*)
// — no RSA* argument. The earlier draft had RSA* prepended which
// produced an arg-misaligned call and segfaulted.
static int ocnp_bn_mod_exp(BIGNUM *r,
                           const BIGNUM *a, const BIGNUM *p,
                           const BIGNUM *m, BN_CTX *ctx,
                           BN_MONT_CTX *m_ctx) {
    (void)m_ctx;
    int bits = BN_num_bits(m);
    int n_limbs = round_up_limbs_for_bits(bits);
    if (!n_limbs) {
        ++g_modexp_unsupported;
        // Fall back to default impl for >4096-bit moduli.
        return BN_mod_exp_mont(r, a, p, m, ctx, NULL);
    }

    uint64_t a_limbs[64] = {0}, p_limbs[64] = {0}, m_limbs[64] = {0}, r_limbs[64] = {0};
    if (!bn_to_limbs(a, a_limbs, n_limbs) ||
        !bn_to_limbs(p, p_limbs, n_limbs) ||
        !bn_to_limbs(m, m_limbs, n_limbs)) {
        ++g_modexp_unsupported;
        return BN_mod_exp_mont(r, a, p, m, ctx, NULL);
    }

    if (openclicknp_modexp(r_limbs, a_limbs, p_limbs, m_limbs, n_limbs) != 0) {
        ++g_modexp_unsupported;
        return BN_mod_exp_mont(r, a, p, m, ctx, NULL);
    }

    ++g_modexp_calls;
    return limbs_to_bn(r_limbs, n_limbs, r);
}

// ---------- ENGINE callbacks ----------

static int ocnp_init(ENGINE *e) {
    (void)e;
    return 1;
}

static int ocnp_finish(ENGINE *e) {
    (void)e;
    return 1;
}

static int ocnp_destroy(ENGINE *e) {
    (void)e;
    if (g_ocnp_rsa_method) { RSA_meth_free(g_ocnp_rsa_method); g_ocnp_rsa_method = NULL; }
    return 1;
}

// ENGINE_CMD_FROM_NAME-style commands so `openssl engine -c` lists
// inspection commands.
#define OCNP_CMD_STATS 200
static const ENGINE_CMD_DEFN ocnp_cmds[] = {
    { OCNP_CMD_STATS, "STATS", "Print modexp call counters", ENGINE_CMD_FLAG_NO_INPUT },
    { 0, NULL, NULL, 0 }
};

static int ocnp_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f)(void)) {
    (void)e; (void)i; (void)p; (void)f;
    if (cmd == OCNP_CMD_STATS) {
        fprintf(stderr,
                "[openclicknp ENGINE] modexp_calls=%lu modexp_unsupported_fallback=%lu\n",
                g_modexp_calls, g_modexp_unsupported);
        return 1;
    }
    return 0;
}

// ---------- Engine binding ----------

static int bind_helper(ENGINE *e, const char *id) {
    (void)id;
    if (!ENGINE_set_id(e, OCNP_ENGINE_ID))            return 0;
    if (!ENGINE_set_name(e, OCNP_ENGINE_NAME))        return 0;
    if (!ENGINE_set_init_function(e, ocnp_init))      return 0;
    if (!ENGINE_set_finish_function(e, ocnp_finish))  return 0;
    if (!ENGINE_set_destroy_function(e, ocnp_destroy)) return 0;
    if (!ENGINE_set_ctrl_function(e, ocnp_ctrl))      return 0;
    if (!ENGINE_set_cmd_defns(e, ocnp_cmds))          return 0;

    // Build our RSA_METHOD by cloning the default and overriding bn_mod_exp.
    g_default_rsa_method = RSA_get_default_method();
    g_ocnp_rsa_method = RSA_meth_dup(g_default_rsa_method);
    if (!g_ocnp_rsa_method) return 0;
    RSA_meth_set1_name(g_ocnp_rsa_method, OCNP_ENGINE_NAME " RSA");
    RSA_meth_set_bn_mod_exp(g_ocnp_rsa_method, ocnp_bn_mod_exp);
    if (!ENGINE_set_RSA(e, g_ocnp_rsa_method)) return 0;

    return 1;
}

IMPLEMENT_DYNAMIC_BIND_FN(bind_helper)
IMPLEMENT_DYNAMIC_CHECK_FN()
