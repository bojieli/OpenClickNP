// SPDX-License-Identifier: Apache-2.0
//
// ECMP_Router host program. Demonstrates two control paths:
//   1. Signal RPC      — install per-flow lookup entries via @-marked
//                        elements (lookup, lb, rd, wr).
//   2. host_in stream  — write nexthop-table updates as a stream of
//                        (index, gateway_ip) flits via the slot bridge.
#include "openclicknp/platform.hpp"
#include "openclicknp/element.hpp"
#include "openclicknp/flit.hpp"

#include <chrono>
#include <cstdio>
#include <thread>

int main(int argc, char** argv) {
    using namespace openclicknp;
    Platform p;
    p.open(argc>1?argv[1]:"build/ECMP_Router/ECMP_Router.xclbin", "",
           Platform::TransportKind::XDMA);
    p.launchAll();

    // Path 1: install flow rules via signal RPC.
    Element* lookup = p.element("lookup");
    if (lookup) {
        for (uint32_t i = 0; i < 4; ++i) {
            SignalRequest r{};
            r.cmd = 1;                          // insert
            r.sparam = i;                        // index
            r.lparam[0] = 0x0a000001ull + i;     // backend IP
            SignalResponse rsp{};
            lookup->signal(r, rsp);
        }
        std::printf("ECMP: 4 flow rules installed via signal RPC\n");
    }

    // Path 2: stream nexthop-table updates over host_in. The slot id is
    // assigned by the compiler in generated/host/kernel_table.cpp; in
    // our topology, the wr element receives host_in stream slot 32
    // (the conventional first user-side PCIe slot).
    Element* wr = p.element("wr");
    if (wr) {
        for (uint16_t i = 0; i < 8; ++i) {
            flit_t update{};
            update.set(0, i);                          // table index
            update.set(1, 0x0a010001ull + i);          // gateway IP
            update.set_sop(true); update.set_eop(true);
            HostMessage msg;
            msg.flit = update;
            msg.slot_id = 32;
            wr->send(msg);
        }
        std::printf("ECMP: 8 nexthop updates streamed via host_in\n");
    }

    // Periodically read pass counter from lb.
    Element* lb = p.element("lb");
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (lb) {
            SignalRequest r{}; r.cmd = 0;
            SignalResponse rsp{};
            lb->signal(r, rsp);
            std::printf("ECMP: balanced=%lu\n", rsp.lparam[0]);
        }
    }
    return 0;
}
