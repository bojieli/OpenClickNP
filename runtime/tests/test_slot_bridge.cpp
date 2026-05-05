// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/platform.hpp"

#include <cassert>
#include <cstdio>
#include <thread>

int main() {
    using namespace openclicknp;

    // Build a Platform in stub mode (no XRT). Bridge falls back to in-memory
    // queues — this exercises the host-side slot multiplexing logic without
    // an FPGA in the loop.
    Platform p;
    bool ok = p.open("", "", Platform::TransportKind::XDMA);
    assert(ok);
    assert(p.isOpen());

    flit_t a{}, b{};
    a.set(0, 0xAAAA'AAAAull);
    b.set(0, 0xBBBB'BBBBull);
    assert(p.sendSlot(7,  a));
    assert(p.sendSlot(13, b));

    flit_t out{};
    assert(p.recvSlot(7,  out, /*blocking=*/true));
    assert(out.get(0) == 0xAAAA'AAAAull);
    assert(p.recvSlot(13, out, /*blocking=*/true));
    assert(out.get(0) == 0xBBBB'BBBBull);
    assert(!p.recvSlot(7, out, /*blocking=*/false));
    p.close();
    std::printf("slot_bridge: OK\n");
    return 0;
}
