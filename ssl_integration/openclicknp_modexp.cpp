// SPDX-License-Identifier: Apache-2.0
//
// openclicknp_modexp.cpp — C-callable shim that routes modexp requests
// from the OpenSSL ENGINE to either:
//   • the OpenClickNP SW emulator path (today, when no .xclbin is
//     loaded) — directly calls bigint::modexp for an immediate reply;
//   • the OpenClickNP runtime / XRT path (future) — pushes operand
//     flits to the FPGA via host_send and reads the reply via host_recv.
//
// The SW-emu path here is what the v0.1 ssl-integration demo runs.
// Switching to the FPGA path is a one-function change once a real
// .xclbin is in hand (see comment in openclicknp_modexp_fpga()).
#include "openclicknp/bigint.hpp"

#include <cstdint>
#include <cstring>
#include <type_traits>

extern "C" int openclicknp_modexp(uint64_t* out,
                                  const uint64_t* m,
                                  const uint64_t* e,
                                  const uint64_t* n,
                                  int n_limbs) {
    using namespace openclicknp::bigint;

    auto run = [&](auto tag) -> int {
        constexpr int N = decltype(tag)::value;
        U<N> mu, eu, nu, ou;
        for (int i = 0; i < N; ++i) {
            mu.limbs[i] = m[i];
            eu.limbs[i] = e[i];
            nu.limbs[i] = n[i];
        }
        // bigint::modexp requires odd modulus (Montgomery precondition).
        // OpenSSL never asks for modexp with an even modulus on a real
        // RSA path, but BN_mod_exp_mont in libcrypto can be invoked
        // with any odd m. If the LSB is 0, bail and let libcrypto fall
        // back. (Could also implement even-modulus modexp, but it's
        // outside the RSA fast path.)
        if ((nu.limbs[0] & 1u) == 0u) return 1;
        modexp(ou, mu, eu, nu);
        for (int i = 0; i < N; ++i) out[i] = ou.limbs[i];
        return 0;
    };

    if (n_limbs == 16) return run(std::integral_constant<int, 16>{});
    if (n_limbs == 32) return run(std::integral_constant<int, 32>{});
    if (n_limbs == 64) return run(std::integral_constant<int, 64>{});
    return 1;  // unsupported width — caller falls back to libcrypto
}

// Constant-time modexp: same shape as openclicknp_modexp, used by the
// ENGINE for any caller that requires side-channel resistance.
extern "C" int openclicknp_modexp_consttime(uint64_t* out,
                                            const uint64_t* m,
                                            const uint64_t* e,
                                            const uint64_t* n,
                                            int n_limbs) {
    using namespace openclicknp::bigint;
    auto run = [&](auto tag) -> int {
        constexpr int N = decltype(tag)::value;
        U<N> mu, eu, nu, ou;
        for (int i = 0; i < N; ++i) { mu.limbs[i] = m[i]; eu.limbs[i] = e[i]; nu.limbs[i] = n[i]; }
        if ((nu.limbs[0] & 1u) == 0u) return 1;
        modexp_consttime(ou, mu, eu, nu);
        for (int i = 0; i < N; ++i) out[i] = ou.limbs[i];
        return 0;
    };
    if (n_limbs == 16) return run(std::integral_constant<int, 16>{});
    if (n_limbs == 32) return run(std::integral_constant<int, 32>{});
    if (n_limbs == 64) return run(std::integral_constant<int, 64>{});
    return 1;
}

// CRT-accelerated RSA private-key operation.
// All limb arrays are little-endian. n_limbs is the RSA modulus size in
// 64-bit limbs (16/32/64 = RSA-1024/2048/4096); p, q, dp, dq, qInv are
// each n_limbs/2 limbs (the half-bit-width primes and CRT params).
extern "C" int openclicknp_rsa_crt(uint64_t* out,
                                   const uint64_t* c,
                                   const uint64_t* p,
                                   const uint64_t* q,
                                   const uint64_t* dp,
                                   const uint64_t* dq,
                                   const uint64_t* qInv,
                                   int n_limbs) {
    using namespace openclicknp::bigint;
    auto run = [&](auto tag) -> int {
        constexpr int N = decltype(tag)::value;
        constexpr int M = N / 2;
        U<N> cu, ou;
        U<M> pu, qu, dpu, dqu, qiu;
        for (int i = 0; i < N; ++i) cu.limbs[i] = c[i];
        for (int i = 0; i < M; ++i) {
            pu.limbs[i] = p[i]; qu.limbs[i] = q[i];
            dpu.limbs[i] = dp[i]; dqu.limbs[i] = dq[i];
            qiu.limbs[i] = qInv[i];
        }
        if ((pu.limbs[0] & 1u) == 0u || (qu.limbs[0] & 1u) == 0u) return 1;
        rsa_crt_decrypt<N, M>(ou, cu, pu, qu, dpu, dqu, qiu);
        for (int i = 0; i < N; ++i) out[i] = ou.limbs[i];
        return 0;
    };
    if (n_limbs == 16) return run(std::integral_constant<int, 16>{});
    if (n_limbs == 32) return run(std::integral_constant<int, 32>{});
    if (n_limbs == 64) return run(std::integral_constant<int, 64>{});
    return 1;
}
