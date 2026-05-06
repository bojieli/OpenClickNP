// SPDX-License-Identifier: Apache-2.0
// End-to-end correctness test for RSA_ModExp_4096.
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

static constexpr int LIMBS = 64;
static constexpr int FLITS = 16;

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
    from_hex(m, "1b3dbd5ce9a1fa6f81f76d1c2dbc2134c30ff46e8026695ff8cda88b436d76e2b83cfe0be037e5edb8db0672f42d47cc00d4af5974273ca3287d06ca6f4cc69a4b22d3081c8eaee95715bd6fa4161293c4c2e2e3444ea7c8c03987108976e334e2817efdae8492171d53434bb88139b9ae270da702f06b90f143262fdc5c0eed8da0365bf89897b9405cacec877409a977d21e02ff01cf99988c24c961b1cd2262801c4510435a1098ae43346c12ace8ae340454cac5b68c28f49481a0a04dc427209bdf1c11f735dc713d960c0fd195c17af08a1745d6d87e570ddf827050a82369b584ff5e9ff0ff50bde4382567b85cabcc97663f1c97956269f0e5d7b8756dadd6c795a76d79bf3c4c06434308bc89fa6a688fb5d27bbeb799193f22faf823bed01d43cf2fde24933b83757750a9a491f0b2ea1fca65e27a984d654821d07fcd9eb1a7cad415366eb16f508ebad7b7c93acfe059a0ee9132b63ef16287e4e9c349e03602f8ac10f1bc81448aaa9e66b2bc5b50c187fcce177b4e0837b8a3d261a7ab3aa2e4f90e51f30dc6a7ee39c4b032ccd7c524a55304317faf42e12f3838b3268e944239b02b61c4a3d70628ece66fa2fd5166e6451b4cf36123fdf77656af7229d4beef3eabedcbbaa80dd488bd64072bcfbe01a28defe39bf0027312476f57a5e5a5abaefcfad8efc89849b3aa7efe4458a885ab9099a435a240b0");
    e.set_zero(); e.limbs[0] = 65537;
    from_hex(n, "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffa13bd1f7");
    from_hex(want, "037b4ff14108d16da82444ebaa66dfb16f9b2a277990ac0bcd6fa59a232e69bb1ae5c676e724241ae3ab60ead2e4447acba99432342928fd921247025f06dd7598bc0d62ebc0d4cfa0e23bcfd80a4a576742f26ec3d7b9ce6e17960bbf229adeec9bebf591e6a4df3c4711c7c16f82e8a8655d33442638c6ac5bfac77fc8a8f6450c3f3768ea13beebad8a36cc760d7d16170088f047bacfaec05cf243be00c6dc3af9414d535fe53aab6f4e954117add7a3a9a894d5d9372e2e757d4fb8f21fed95f1f3eff144d3d9c85d7067768ee55e478a107e0262abf19709b450f009f4d9a0f0722df1aad4bb55d549e3471961c222ba085ef8cd87c6283f68993a581a75a5bd516b21499b286e93a3fe75193ecf7ab98e6178c8bfc8e987362bc3e8f3a442fdf2e5b132fb8b5e5b444ebb9a99e7bdc28f28313927c54e33e9b9a7b6f786db6ac34c5fc2ddfe97ed18adeda734b23335a89c333715573d490dc8b56433ca37ee55567d5eb66fa7281aff63dd177e1b9d1c965cb5f247df5899e72ee67465b0c68d8c484ac06dbaa0f87548c57ae8304fe8b13d4f0a9eab6821afb007435ed09bc18107da585cf75893813cf77e58201cd1c15d961f8434ad1e3a73a0591c05be754323db7b6352edeb292384e4c3b05e94c4d136fcce906c8bcef983d849847999c0a2ba3a9c4b338ddaaef3fb8ad16535f3977749a328d373b33a57ec");

    // Sanity: assert hex strings are the right length so a stray paste
    // error becomes an immediate failure rather than wrong-result.
    if ((int)std::strlen("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffa13bd1f7") != 4096/4) {
        std::printf("FAIL: n hex length is %d, expected %d\n",
                    (int)std::strlen("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffa13bd1f7"), 4096/4); return 1;
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

    ELEM_ASSERT_EQ(outs.size(), (size_t)FLITS, "RSA-4096 should produce 16 output flits");
    bigint::U<LIMBS> got{};
    for (int i = 0; i < FLITS; ++i)
        for (int j = 0; j < 4; ++j)
            got.limbs[i * 4 + j] = outs[i].get(j);
    if (bigint::cmp(got, want) != 0) {
        std::printf("FAIL: ciphertext mismatch\n"); return 1;
    }
    ELEM_PASS("RSA_ModExp_4096");
}
