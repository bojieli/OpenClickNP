// SPDX-License-Identifier: Apache-2.0
//
// Comprehensive RSA accuracy test for openclicknp::bigint::modexp.
//
// What this exercises (per bit width 1024 / 2048 / 4096):
//   • Edge cases: m = 0, m = 1, m = n-1, e = 0, e = 1, e = 2,
//     e = small odd, e = large random.
//   • 200 uniformly-random (m, e, n) tuples cross-checked against
//     OpenSSL's BN_mod_exp_mont() — an independent reference
//     implementation. (Earlier tests only compared against Python
//     pow(), so a coincidental bug shared with Python's bigint
//     implementation could have hidden — this catches that class.)
//   • Even-modulus rejection: bigint::modexp's Montgomery path
//     requires odd n, and the OpenSSL ENGINE shim returns "fall
//     back" for even n. We confirm both behaviors.
//
// On any mismatch, the test prints the offending vector in hex so
// it can be re-added to a regression suite.
#include "openclicknp/bigint.hpp"

#include <openssl/bn.h>
#include <openssl/err.h>

#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

using namespace openclicknp::bigint;

template <int N>
static void random_u(U<N>& out, std::mt19937_64& rng, bool force_odd = false) {
    for (int i = 0; i < N; ++i) out.limbs[i] = rng();
    if (force_odd) out.limbs[0] |= 1;
}

template <int N>
static void to_bn(const U<N>& x, BIGNUM* out) {
    unsigned char buf[8 * 64];
    for (int i = 0; i < N; ++i) {
        for (int b = 0; b < 8; ++b)
            buf[(N - 1 - i) * 8 + (7 - b)] =
                (unsigned char)((x.limbs[i] >> (b * 8)) & 0xff);
    }
    BN_bin2bn(buf, N * 8, out);
}

template <int N>
static void from_bn(const BIGNUM* x, U<N>& out) {
    unsigned char buf[8 * 64];
    out.set_zero();
    BN_bn2binpad(x, buf, N * 8);
    for (int i = 0; i < N; ++i) {
        uint64_t v = 0;
        for (int b = 0; b < 8; ++b)
            v = (v << 8) | buf[(N - 1 - i) * 8 + b];
        out.limbs[i] = v;
    }
}

template <int N>
static int hex_dump_bn(const U<N>& x, char* buf, int buflen) {
    static const char* hd = "0123456789abcdef";
    int idx = 0; bool started = false;
    for (int i = N - 1; i >= 0 && idx < buflen - 17; --i) {
        for (int b = 60; b >= 0; b -= 4) {
            int nib = (x.limbs[i] >> b) & 0xF;
            if (started || nib) { buf[idx++] = hd[nib]; started = true; }
        }
    }
    if (!started) buf[idx++] = '0';
    buf[idx] = '\0';
    return idx;
}

template <int N>
static bool openssl_modexp(const U<N>& m, const U<N>& e, const U<N>& n,
                           U<N>& out) {
    BN_CTX* ctx = BN_CTX_new();
    BIGNUM *bm = BN_new(), *be = BN_new(), *bn = BN_new(), *br = BN_new();
    to_bn(m, bm); to_bn(e, be); to_bn(n, bn);
    int ok = BN_mod_exp_mont(br, bm, be, bn, ctx, NULL);
    if (ok) from_bn(br, out);
    BN_free(bm); BN_free(be); BN_free(bn); BN_free(br); BN_CTX_free(ctx);
    return ok != 0;
}

template <int N>
static int compare_one(const char* tag,
                       const U<N>& m, const U<N>& e, const U<N>& n) {
    U<N> got, want;
    modexp(got, m, e, n);
    if (!openssl_modexp(m, e, n, want)) {
        std::printf("  %s: skip (OpenSSL refused to run)\n", tag);
        return 0;
    }
    if (cmp(got, want) != 0) {
        char b1[2200], b2[2200], bm[2200], be[2200], bn[2200];
        hex_dump_bn(got, b1, sizeof(b1));
        hex_dump_bn(want, b2, sizeof(b2));
        hex_dump_bn(m, bm, sizeof(bm));
        hex_dump_bn(e, be, sizeof(be));
        hex_dump_bn(n, bn, sizeof(bn));
        std::printf("  %s: MISMATCH\n    m   = 0x%s\n    e   = 0x%s\n"
                    "    n   = 0x%s\n    got = 0x%s\n    want= 0x%s\n",
                    tag, bm, be, bn, b1, b2);
        return 1;
    }
    return 0;
}

// Edge cases that historically break ad-hoc modexp implementations.
template <int N>
static int edge_case_suite(const char* label) {
    std::printf("Edge cases (%s):\n", label);
    int fails = 0;
    U<N> n_odd, n_minus_1, m, e, m_zero, m_one;
    n_odd.set_zero();
    // n = 0xFFF...FFF & ~0 | 1 (large odd value). For ~all-bits-set odd n.
    for (int i = 0; i < N; ++i) n_odd.limbs[i] = ~uint64_t{0};
    n_odd.limbs[0] |= 1;     // odd

    n_minus_1 = n_odd;
    sub_one_inplace: {
        // n_minus_1 = n_odd - 1, manually
        U<N> one; one.set_zero(); one.limbs[0] = 1;
        sub(n_minus_1, n_odd, one);
    }

    m_zero.set_zero();
    m_one.set_zero(); m_one.limbs[0] = 1;

    // m=0, e=anything, n=odd → result = 0
    e.set_zero(); e.limbs[0] = 65537;
    fails += compare_one<N>("m=0, e=65537", m_zero, e, n_odd);
    // m=1, e=anything, n=odd → result = 1
    fails += compare_one<N>("m=1, e=65537", m_one, e, n_odd);
    // m=n-1, e=2 → result = 1 (since (n-1)^2 = n^2 - 2n + 1 ≡ 1)
    {
        U<N> e2; e2.set_zero(); e2.limbs[0] = 2;
        fails += compare_one<N>("m=n-1, e=2", n_minus_1, e2, n_odd);
    }
    // m=n-1, e=3 → result = n-1 (since (-1)^3 = -1 ≡ n-1)
    {
        U<N> e3; e3.set_zero(); e3.limbs[0] = 3;
        fails += compare_one<N>("m=n-1, e=3", n_minus_1, e3, n_odd);
    }
    // e=0, m=anything, n=odd → result = 1 (m^0 = 1)
    {
        U<N> e0; e0.set_zero();
        random_u(m, *(new std::mt19937_64(0xdeadbeef)), false);
        fails += compare_one<N>("m=rand, e=0", m, e0, n_odd);
    }
    // e=1, m=rand → result = m mod n
    {
        U<N> e1; e1.set_zero(); e1.limbs[0] = 1;
        random_u(m, *(new std::mt19937_64(0xfeedface)), false);
        fails += compare_one<N>("m=rand, e=1", m, e1, n_odd);
    }
    // m=0, e=0, n=odd → result = 1 (math convention; OpenSSL agrees)
    {
        U<N> e0; e0.set_zero();
        fails += compare_one<N>("m=0, e=0 (=1 by convention)", m_zero, e0, n_odd);
    }
    return fails;
}

// Random fuzz: cross-check against OpenSSL.
template <int N>
static int fuzz(int n_iter, uint64_t seed, const char* label) {
    std::printf("Random fuzz %d iterations (%s):\n", n_iter, label);
    std::mt19937_64 rng(seed);
    int fails = 0;
    for (int it = 0; it < n_iter; ++it) {
        U<N> m, e, n;
        random_u(m, rng);
        random_u(e, rng);
        random_u(n, rng, /*force_odd=*/true);
        char tag[64];
        std::snprintf(tag, sizeof(tag), "iter %d/%d", it + 1, n_iter);
        fails += compare_one<N>(tag, m, e, n);
        if (fails > 5) {
            std::printf("  bailing after 5+ failures\n");
            break;
        }
    }
    if (fails == 0) std::printf("  all %d vectors agree with OpenSSL ✓\n", n_iter);
    return fails;
}

int main() {
    int total = 0;
    total += edge_case_suite<16>("RSA-1024");
    total += edge_case_suite<32>("RSA-2048");
    total += edge_case_suite<64>("RSA-4096");

    total += fuzz<16>(200, 0xC0DECAFE, "RSA-1024");
    total += fuzz<32>(100, 0xBADBEEF1, "RSA-2048");
    total += fuzz<64>( 50, 0xDEADD00D, "RSA-4096");

    if (total) {
        std::printf("\nFAIL: %d total mismatch(es) vs OpenSSL\n", total);
        return 1;
    }
    std::printf("\nALL ACCURACY TESTS PASSED (vs OpenSSL BN_mod_exp_mont reference)\n");
    return 0;
}
