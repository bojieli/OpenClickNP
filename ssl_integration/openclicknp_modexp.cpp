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
