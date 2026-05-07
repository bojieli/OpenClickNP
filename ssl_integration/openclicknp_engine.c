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
#include <openssl/evp.h>
#include <openssl/objects.h>
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
extern int openclicknp_modexp_consttime(uint64_t* out,
                              const uint64_t* m,
                              const uint64_t* e,
                              const uint64_t* n,
                              int n_limbs);
extern int openclicknp_rsa_crt(uint64_t* out,
                              const uint64_t* c,
                              const uint64_t* p,
                              const uint64_t* q,
                              const uint64_t* dp,
                              const uint64_t* dq,
                              const uint64_t* qInv,
                              int n_limbs);

// AES-128 forward declarations — implementation in
// openclicknp_aes_shim.cpp (so we can stay in C here).
extern int openclicknp_aes128_init(const unsigned char* key);
extern int openclicknp_aes128_encrypt_block(unsigned char* out, const unsigned char* in, void* opaque);
extern int openclicknp_aes128_decrypt_block(unsigned char* out, const unsigned char* in, void* opaque);
extern void* openclicknp_aes128_alloc_key(void);
extern void openclicknp_aes128_free_key(void* opaque);
extern int openclicknp_aes128_expand(void* opaque, const unsigned char* key);

// Counters are useful for `openssl engine -t` to confirm it actually
// got called.
static unsigned long g_modexp_calls = 0;
static unsigned long g_modexp_consttime_calls = 0;
static unsigned long g_crt_calls = 0;
static unsigned long g_modexp_unsupported = 0;
static unsigned long g_aes_blocks = 0;

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
        return BN_mod_exp_mont(r, a, p, m, ctx, NULL);
    }
    uint64_t a_limbs[64] = {0}, p_limbs[64] = {0}, m_limbs[64] = {0}, r_limbs[64] = {0};
    if (!bn_to_limbs(a, a_limbs, n_limbs) ||
        !bn_to_limbs(p, p_limbs, n_limbs) ||
        !bn_to_limbs(m, m_limbs, n_limbs)) {
        ++g_modexp_unsupported;
        return BN_mod_exp_mont(r, a, p, m, ctx, NULL);
    }

    // If any of the operands has BN_FLG_CONSTTIME set, use the
    // constant-time path. OpenSSL sets this flag on the secret
    // exponent for private-key operations.
    int needs_consttime = BN_get_flags(p, BN_FLG_CONSTTIME);
    int rc;
    if (needs_consttime) {
        rc = openclicknp_modexp_consttime(r_limbs, a_limbs, p_limbs, m_limbs, n_limbs);
        if (rc == 0) ++g_modexp_consttime_calls;
    } else {
        rc = openclicknp_modexp(r_limbs, a_limbs, p_limbs, m_limbs, n_limbs);
        if (rc == 0) ++g_modexp_calls;
    }
    if (rc != 0) {
        ++g_modexp_unsupported;
        return BN_mod_exp_mont(r, a, p, m, ctx, NULL);
    }
    return limbs_to_bn(r_limbs, n_limbs, r);
}

// CRT-based RSA private-key modexp. Hooked via RSA_meth_set_mod_exp.
// Signature: int (*)(BIGNUM *r0, const BIGNUM *I, RSA *rsa, BN_CTX *ctx);
//   r0 = I^d mod n.
static int ocnp_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa, BN_CTX *ctx) {
    const BIGNUM *bn_n = NULL, *bn_d = NULL, *bn_p = NULL, *bn_q = NULL,
                 *bn_dp = NULL, *bn_dq = NULL, *bn_qi = NULL;
    RSA_get0_key(rsa, &bn_n, NULL, &bn_d);
    RSA_get0_factors(rsa, &bn_p, &bn_q);
    RSA_get0_crt_params(rsa, &bn_dp, &bn_dq, &bn_qi);

    if (!bn_p || !bn_q || !bn_dp || !bn_dq || !bn_qi) {
        // No CRT params available — fall back to BN_mod_exp_mont_consttime.
        ++g_modexp_unsupported;
        return BN_mod_exp_mont_consttime(r0, I, bn_d, bn_n, ctx, NULL);
    }

    int bits = BN_num_bits(bn_n);
    int n_limbs = round_up_limbs_for_bits(bits);
    if (!n_limbs) {
        ++g_modexp_unsupported;
        return BN_mod_exp_mont_consttime(r0, I, bn_d, bn_n, ctx, NULL);
    }
    int half = n_limbs / 2;

    uint64_t c_limbs[64] = {0}, r_limbs[64] = {0};
    uint64_t p_limbs[32] = {0}, q_limbs[32] = {0}, dp_limbs[32] = {0},
             dq_limbs[32] = {0}, qi_limbs[32] = {0};

    if (!bn_to_limbs(I,  c_limbs,  n_limbs) ||
        !bn_to_limbs(bn_p,  p_limbs,  half) ||
        !bn_to_limbs(bn_q,  q_limbs,  half) ||
        !bn_to_limbs(bn_dp, dp_limbs, half) ||
        !bn_to_limbs(bn_dq, dq_limbs, half) ||
        !bn_to_limbs(bn_qi, qi_limbs, half)) {
        ++g_modexp_unsupported;
        return BN_mod_exp_mont_consttime(r0, I, bn_d, bn_n, ctx, NULL);
    }

    if (openclicknp_rsa_crt(r_limbs, c_limbs,
                            p_limbs, q_limbs,
                            dp_limbs, dq_limbs, qi_limbs,
                            n_limbs) != 0) {
        ++g_modexp_unsupported;
        return BN_mod_exp_mont_consttime(r0, I, bn_d, bn_n, ctx, NULL);
    }

    ++g_crt_calls;
    return limbs_to_bn(r_limbs, n_limbs, r0);
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
                "[openclicknp ENGINE] modexp=%lu modexp_ct=%lu crt=%lu fallback=%lu aes_blocks=%lu\n",
                g_modexp_calls, g_modexp_consttime_calls, g_crt_calls,
                g_modexp_unsupported, g_aes_blocks);
        return 1;
    }
    return 0;
}

// ---------- AES-128-ECB cipher implementation ----------
//
// Hooks NID_aes_128_ecb so any OpenSSL caller (EVP_aes_128_ecb,
// openssl speed, openssl enc -aes-128-ecb, etc.) routes through us
// when the engine is loaded.

static int ocnp_aes128_ecb_init(EVP_CIPHER_CTX* ctx, const unsigned char* key,
                                const unsigned char* iv, int enc) {
    (void)iv; (void)enc;
    void* k = EVP_CIPHER_CTX_get_cipher_data(ctx);
    return openclicknp_aes128_expand(k, key);
}

static int ocnp_aes128_ecb_do_cipher(EVP_CIPHER_CTX* ctx,
                                     unsigned char* out,
                                     const unsigned char* in,
                                     size_t inl) {
    void* k = EVP_CIPHER_CTX_get_cipher_data(ctx);
    int enc = EVP_CIPHER_CTX_encrypting(ctx);
    if (inl % 16) return 0;
    for (size_t off = 0; off < inl; off += 16) {
        if (enc) openclicknp_aes128_encrypt_block(out + off, in + off, k);
        else     openclicknp_aes128_decrypt_block(out + off, in + off, k);
        ++g_aes_blocks;
    }
    return 1;
}

static int ocnp_aes128_ecb_cleanup(EVP_CIPHER_CTX* ctx) {
    (void)ctx;
    return 1;
}

// Lazy-built EVP_CIPHER for AES-128-ECB.
static EVP_CIPHER* g_ocnp_aes128_ecb = NULL;

static const EVP_CIPHER* ocnp_get_aes128_ecb(void) {
    if (g_ocnp_aes128_ecb) return g_ocnp_aes128_ecb;
    EVP_CIPHER* c = EVP_CIPHER_meth_new(NID_aes_128_ecb, /*block_size=*/16, /*key_len=*/16);
    if (!c) return NULL;
    EVP_CIPHER_meth_set_iv_length(c, 0);
    EVP_CIPHER_meth_set_flags(c, EVP_CIPH_ECB_MODE);
    EVP_CIPHER_meth_set_init(c, ocnp_aes128_ecb_init);
    EVP_CIPHER_meth_set_do_cipher(c, ocnp_aes128_ecb_do_cipher);
    EVP_CIPHER_meth_set_cleanup(c, ocnp_aes128_ecb_cleanup);
    EVP_CIPHER_meth_set_impl_ctx_size(c, 256); // round-keys (176) + slack
    g_ocnp_aes128_ecb = c;
    return c;
}

static int ocnp_ciphers(ENGINE* e, const EVP_CIPHER** cipher,
                        const int** nids, int nid) {
    static int our_nids[] = { NID_aes_128_ecb };
    (void)e;
    if (!cipher) {
        *nids = our_nids;
        return 1;  // count
    }
    if (nid == NID_aes_128_ecb) {
        *cipher = ocnp_get_aes128_ecb();
        return 1;
    }
    *cipher = NULL;
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
    RSA_meth_set_mod_exp   (g_ocnp_rsa_method, ocnp_rsa_mod_exp);
    if (!ENGINE_set_RSA(e, g_ocnp_rsa_method)) return 0;

    // Cipher dispatch — AES-128-ECB.
    if (!ENGINE_set_ciphers(e, ocnp_ciphers)) return 0;

    return 1;
}

IMPLEMENT_DYNAMIC_BIND_FN(bind_helper)
IMPLEMENT_DYNAMIC_CHECK_FN()
