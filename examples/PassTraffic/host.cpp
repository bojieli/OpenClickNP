// SPDX-License-Identifier: Apache-2.0
// PassTraffic host program.
//
// Opens the platform, polls per-second flit/packet counters, prints
// throughput. Same loop runs against the SW emulator (without an FPGA)
// or a programmed Alveo U50.
#include "openclicknp/platform.hpp"
#include "openclicknp/element.hpp"

#include <chrono>
#include <cstdio>
#include <thread>

int main(int argc, char** argv) {
    using namespace openclicknp;

    std::string xclbin = (argc > 1) ? argv[1] : "build/PassTraffic.xclbin";
    std::string bdf    = (argc > 2) ? argv[2] : "";
    std::string xprt   = (argc > 3) ? argv[3] : "xdma";

    Platform p;
    auto t = (xprt == "qdma") ? Platform::TransportKind::QDMA
                              : Platform::TransportKind::XDMA;
    if (!p.open(xclbin, bdf, t)) {
        std::fprintf(stderr, "open failed (running emulator-only mode)\n");
    }
    p.launchAll();

    Element* tor_rx = p.element("tor_rx_cnt");
    Element* nic_rx = p.element("nic_rx_cnt");

    uint64_t last_tor_packets = 0, last_nic_packets = 0;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        SignalRequest req{};
        req.cmd = 0;
        SignalResponse rsp_tor{};
        SignalResponse rsp_nic{};
        if (tor_rx) tor_rx->signal(req, rsp_tor);
        if (nic_rx) nic_rx->signal(req, rsp_nic);

        uint64_t tor_packets = rsp_tor.lparam[1];
        uint64_t nic_packets = rsp_nic.lparam[1];
        std::printf("ToR rx: %lu pkts (%lu pps)   NIC rx: %lu pkts (%lu pps)\n",
                    tor_packets, tor_packets - last_tor_packets,
                    nic_packets, nic_packets - last_nic_packets);
        last_tor_packets = tor_packets;
        last_nic_packets = nic_packets;
    }
    return 0;
}
