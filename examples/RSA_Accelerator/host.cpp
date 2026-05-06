// SPDX-License-Identifier: Apache-2.0
//
// RSA_Accelerator host program. Drives a known-answer (m, e, n) test
// vector into the RSA_ModExp_1024 kernel via host_in and verifies the
// returned ciphertext byte-exactly against the reference computed by
// Python pow(m, 65537, n).
//
// This is the host-side endpoint of the .xclbin path; the bitstream
// itself is gated on the U50 platform package (see FINAL_REPORT § 7).
// For local correctness testing without an FPGA, see
// tests/elements/test_RSA_ModExp_1024.cpp which exercises the same
// element via the SW emulator.
#include "openclicknp/platform.hpp"
#include "openclicknp/element.hpp"
#include "openclicknp/bigint.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

using namespace openclicknp;

static constexpr int LIMBS = 16;
static constexpr int FLITS = 4;

template <int N>
static void from_hex(bigint::U<N>& out, const char* hex) {
    out.set_zero();
    int nh = (int)std::strlen(hex); int bit = 0;
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

int main(int argc, char** argv) {
    Platform p;
    p.open(argc>1 ? argv[1] : "build/RSA_Accelerator/RSA_Accelerator.xclbin",
           "", Platform::TransportKind::XDMA);
    p.launchAll();

    bigint::U<LIMBS> m, e, n, want;
    from_hex(m, "72ff5d2a386ecbe06b65a6a48b8148f6b38a088ca65ed389b74d0fb132e706298fadc1a606cb0fb39a1de644815ef6d13b8faa1837f8a88b17fc695a07a0ca6e0822e8f36c031199972a846916419f828b9d2434e465e150bd9c66b3ad3c2d6d1a3d1fa7bc8960a923b8c1e9392456de3eb13b9046685257bdd640fb06671ad3");
    e.set_zero(); e.limbs[0] = 65537;
    from_hex(n, "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff5c4e8663");
    from_hex(want, "6c4bb62248e10f7451f02e8c5897155f31452270068a0f00c4802b0212b74cb4e104a40152f2ec982074fa13605c32c06a444364fcbcd8a223e57ab2df73213f75ffbfe7acd03e49cceeeb824a49485db58f17572ddec90426caf712b9f2c8a01d333339799016efbde0feafc8ac5933f0818c6ba1c5f0dbae893768b8524c45");

    // Slot IDs assigned by lower.cpp's host-stream pass: input ports 1/2/3
    // get slot 32/33/34, output port 1 gets slot 35. (See FINAL_REPORT
    // §3 for the boundary-conn lowering convention.)
    auto push = [&](uint16_t slot_id, const bigint::U<LIMBS>& x) {
        for (int i = 0; i < FLITS; ++i) {
            flit_t f{};
            for (int j = 0; j < 4; ++j) f.set(j, x.limbs[i * 4 + j]);
            f.set_sop(i == 0); f.set_eop(i == FLITS - 1);
            (void)p.sendSlot(slot_id, f);
        }
    };
    push(32, m);
    push(33, e);
    push(34, n);

    bigint::U<LIMBS> got{};
    for (int i = 0; i < FLITS; ++i) {
        flit_t f;
        if (!p.recvSlot(/*slot_id=*/35, f, /*blocking=*/true)) {
            std::puts("RSA-1024 round-trip: timed out waiting for ciphertext flit");
            return 1;
        }
        for (int j = 0; j < 4; ++j) got.limbs[i * 4 + j] = f.get(j);
    }

    if (bigint::cmp(got, want) == 0) { std::puts("RSA-1024 round-trip: OK"); return 0; }
    std::puts("RSA-1024 round-trip: MISMATCH");
    return 1;
}
