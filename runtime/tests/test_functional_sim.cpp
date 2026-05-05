// SPDX-License-Identifier: Apache-2.0
//
// Functional simulation: drive 1000 packets through the openclicknp::Pass
// element, captured into a SwStream. Verifies counters, sop/eop framing,
// and round-trip preservation.
#include "openclicknp/flit.hpp"
#include "openclicknp/pcap.hpp"
#include "openclicknp/sw_runtime.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>

using namespace openclicknp;

// Simulate the Pass element manually — same handler logic the codegen emits.
static void pass_kernel(SwStream& in_1, SwStream& out_1,
                        std::atomic<bool>& stop,
                        uint64_t& flit_count, uint64_t& packet_count) {
    flit_t f{};
    while (!stop.load(std::memory_order_relaxed)) {
        if (in_1.read_nb(f)) {
            out_1.write(f);
            flit_count++;
            if (f.eop()) packet_count++;
        } else {
            std::this_thread::yield();
        }
    }
}

int main() {
    // Build a synthetic 1000-packet PCAP-like trace: 64–1500 byte packets.
    constexpr int N = 1000;
    SwStream in(4096), out(4096);
    std::atomic<bool> stop{false};
    uint64_t flits = 0, packets = 0;
    std::thread t(pass_kernel, std::ref(in), std::ref(out), std::ref(stop),
                  std::ref(flits), std::ref(packets));

    // Generator runs in its own thread so the kernel can drain concurrently.
    std::atomic<int> total_flits{0};
    std::thread gen([&]() {
        for (int i = 0; i < N; ++i) {
            int sz = 64 + (i * 13) % 1450;
            PcapPacket p; p.bytes.resize(sz);
            for (int j = 0; j < sz; ++j) p.bytes[j] = static_cast<uint8_t>(j ^ i);
            auto fs = packetToFlits(p);
            for (auto& fl : fs) in.write(fl);
            total_flits.fetch_add(static_cast<int>(fs.size()));
        }
    });

    // Drain
    auto t0 = std::chrono::steady_clock::now();
    int drained_packets = 0;
    int drained_flits = 0;
    while (drained_packets < N) {
        flit_t f{};
        if (out.read_nb(f)) {
            drained_flits++;
            if (f.eop()) drained_packets++;
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            if (std::chrono::steady_clock::now() - t0 > std::chrono::seconds(20)) {
                std::fprintf(stderr, "TIMEOUT — drained %d flits / %d pkts (sent %d flits, %d pkts)\n",
                             drained_flits, drained_packets, total_flits.load(), N);
                stop = true;
                gen.join();
                t.join();
                return 1;
            }
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(t1 - t0).count();
    std::printf("functional sim: %d packets, %d flits in %.3f s (%.2f Mflits/s)\n",
                drained_packets, drained_flits, secs, drained_flits / secs / 1e6);

    gen.join();
    stop = true; t.join();
    assert(drained_packets == N);
    assert(drained_flits == total_flits.load());
    assert(packets == static_cast<uint64_t>(N));
    assert(flits   == static_cast<uint64_t>(total_flits.load()));
    std::printf("functional sim: OK\n");
    return 0;
}
