// SPDX-License-Identifier: Apache-2.0
// Firewall host — installs a few example rules into the HashTable element
// then prints pass/drop counters every second.
#include "openclicknp/platform.hpp"
#include "openclicknp/element.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

int main(int argc, char** argv) {
    using namespace openclicknp;

    std::string xclbin = (argc > 1) ? argv[1] : "build/Firewall.xclbin";
    Platform p;
    if (!p.open(xclbin, "", Platform::TransportKind::XDMA)) {
        std::fprintf(stderr, "open failed (running emulator-only mode)\n");
    }
    p.launchAll();

    Element* ht  = p.element("ht");
    Element* act = p.element("act");

    auto install_rule = [&](uint32_t idx, uint64_t key0, uint64_t key1,
                            uint64_t action) {
        if (!ht) return;
        SignalRequest r{};
        r.cmd = 1;
        r.sparam = idx;
        r.lparam[0] = key0;
        r.lparam[1] = key1;
        r.lparam[2] = action;
        SignalResponse rsp{};
        ht->signal(r, rsp);
    };

    // Allow source-IP 10.0.0.1, dst-IP 10.0.0.2.
    uint64_t key0 = (static_cast<uint64_t>(0x0A000001u) << 32) | 0x0A000002u;
    install_rule(/*idx=*/123, key0, /*proto=*/6, /*action=*/1);

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        SignalRequest q{}; q.cmd = 0;
        SignalResponse rsp{};
        if (act) act->signal(q, rsp);
        std::printf("Firewall: %lu passed, %lu dropped\n",
                    rsp.lparam[0], rsp.lparam[1]);
    }
    return 0;
}
