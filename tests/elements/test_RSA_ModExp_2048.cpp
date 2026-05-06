// SPDX-License-Identifier: Apache-2.0
// End-to-end correctness test for RSA_ModExp_2048.
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

static constexpr int LIMBS = 32;
static constexpr int FLITS = 8;

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
    from_hex(m, "29a3b2e95d65a441d58842dea2bc372f7412b29347294739614ff3d719db3ad0ddd1dfb23b982ef8daf61a26146d3f31fc377a4c4a15544dc5e7ce8a3a578a8ea9488d990bbb259911ce5dd2b45ed1f03139d32c93cd59bf5c941cf0dc98d2c1e2acf72f9e574f7aa0ee89aed453dd324b0dbb418d5288f1142c3fe860e7a113ec1b8ca1f91e1d4c1ff49b7889463e85759cde66bacfb3d00b1f9163ce9ff57f43b7a3a69a8dca03580d7b71d8f564135be6128e18c267976142ea7d17be31111a2a73ed562b0f79c37459eef50bea63371ecd7b27cd813047229389571aa8766c307511b2b9437a28df6ec4ce4a2bbdc241330b01a9e71fde8a774bcf36d58d");
    e.set_zero(); e.limbs[0] = 65537;
    from_hex(n, "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff6925e253");
    from_hex(want, "d26445f1a2509a149b5b565927a749bb80b447715908585f413423f80493f17884304eb17080f864bba0c294e1c4b366229c33138786e0a1bf6714b74b3bfaae1084bb7c6f50116fd1548df024ba4903744e04210b8fb1f8dba274db449433ce43695bee2aad4e2e16c7c45790ef60c5e4f49237b30e577f91aa9c9935f4dfc2b46395ea33901225e3f04ee61dd0064d102c1dad64d9ec8db7da5ba0bdda050c2894ae5143d07ff877caeb16eb5d2d9586ac422257b7940a7ce80b7d8cef5f00b1a1e441bf0cbc697a1702dae1e764d9e06641c353245ada44069be04a020de0015c984e08ac6417e75dc4c72fc6fbf127de20c2ba3a21f106e9c1090a524f4a");

    // Sanity: assert hex strings are the right length so a stray paste
    // error becomes an immediate failure rather than wrong-result.
    if ((int)std::strlen("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff6925e253") != 2048/4) {
        std::printf("FAIL: n hex length is %d, expected %d\n",
                    (int)std::strlen("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff6925e253"), 2048/4); return 1;
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

    ELEM_ASSERT_EQ(outs.size(), (size_t)FLITS, "RSA-2048 should produce 8 output flits");
    bigint::U<LIMBS> got{};
    for (int i = 0; i < FLITS; ++i)
        for (int j = 0; j < 4; ++j)
            got.limbs[i * 4 + j] = outs[i].get(j);
    if (bigint::cmp(got, want) != 0) {
        std::printf("FAIL: ciphertext mismatch\n"); return 1;
    }
    ELEM_PASS("RSA_ModExp_2048");
}
