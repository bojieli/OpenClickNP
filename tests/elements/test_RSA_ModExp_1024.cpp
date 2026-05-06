// SPDX-License-Identifier: Apache-2.0
// End-to-end correctness test for RSA_ModExp_1024.
//
// Drives a known (m, e, n) test vector through the kernel via the
// SW-emu harness; verifies the output flits encode m^e mod n.
// Reference value computed offline via Python pow(m, 65537, n).
#include "element_test_harness.hpp"
#include "openclicknp/bigint.hpp"

#include <cstring>

using namespace openclicknp;
using namespace openclicknp::test;

extern "C" void kernel_u(
    SwStream& in_1, SwStream& in_2, SwStream& in_3, SwStream& out_1,
    std::atomic<bool>& _stop);

static constexpr int LIMBS = 16;
static constexpr int FLITS = 4;

template <int N>
static void from_hex(bigint::U<N>& out, const char* hex) {
    out.set_zero();
    int nh = (int)std::strlen(hex);
    int bit = 0;
    for (int i = nh - 1; i >= 0 && bit < N * bigint::LIMB_BITS; --i) {
        char c = hex[i]; uint64_t v;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else continue;
        out.limbs[bit / bigint::LIMB_BITS] |= (v & 0xF) << (bit % bigint::LIMB_BITS);
        bit += 4;
    }
}

static void push_operand(SwStream& s, const bigint::U<LIMBS>& x) {
    for (int i = 0; i < FLITS; ++i) {
        flit_t f{};
        for (int j = 0; j < 4; ++j) f.set(j, x.limbs[i * 4 + j]);
        f.set_sop(i == 0); f.set_eop(i == FLITS - 1);
        s.write(f);
    }
}

int main() {
    bigint::U<LIMBS> m, e, n, want;
    from_hex(m, "72ff5d2a386ecbe06b65a6a48b8148f6b38a088ca65ed389b74d0fb132e706298fadc1a606cb0fb39a1de644815ef6d13b8faa1837f8a88b17fc695a07a0ca6e0822e8f36c031199972a846916419f828b9d2434e465e150bd9c66b3ad3c2d6d1a3d1fa7bc8960a923b8c1e9392456de3eb13b9046685257bdd640fb06671ad3");
    e.set_zero(); e.limbs[0] = 65537;
    from_hex(n, "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff5c4e8663");
    from_hex(want, "6c4bb62248e10f7451f02e8c5897155f31452270068a0f00c4802b0212b74cb4e104a40152f2ec982074fa13605c32c06a444364fcbcd8a223e57ab2df73213f75ffbfe7acd03e49cceeeb824a49485db58f17572ddec90426caf712b9f2c8a01d333339799016efbde0feafc8ac5933f0818c6ba1c5f0dbae893768b8524c45");

    // Sanity: assert hex strings are the right length so a stray paste
    // error becomes an immediate failure rather than wrong-result.
    if ((int)std::strlen("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff5c4e8663") != 1024/4) {
        std::printf("FAIL: n hex length is %d, expected %d\n",
                    (int)std::strlen("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff5c4e8663"), 1024/4); return 1;
    }

    SwStream in_m(64), in_e(64), in_n(64), out(64);
    KernelHarness h;
    h.start(kernel_u, std::ref(in_m), std::ref(in_e), std::ref(in_n),
            std::ref(out), std::ref(h.stop));

    push_operand(in_m, m); push_operand(in_e, e); push_operand(in_n, n);

    std::vector<flit_t> outs;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while ((int)outs.size() < FLITS && std::chrono::steady_clock::now() < deadline) {
        flit_t f;
        if (out.read_nb(f)) outs.push_back(f);
        else std::this_thread::sleep_for(2ms);
    }
    h.run_for(20ms);

    ELEM_ASSERT_EQ(outs.size(), (size_t)FLITS, "RSA-1024 should produce 4 output flits");
    bigint::U<LIMBS> got{};
    for (int i = 0; i < FLITS; ++i)
        for (int j = 0; j < 4; ++j)
            got.limbs[i * 4 + j] = outs[i].get(j);
    if (bigint::cmp(got, want) != 0) {
        std::printf("FAIL: ciphertext mismatch\n"); return 1;
    }
    ELEM_PASS("RSA_ModExp_1024");
}
