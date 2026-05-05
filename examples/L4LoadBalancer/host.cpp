// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/platform.hpp"
#include "openclicknp/element.hpp"
#include <chrono>
#include <cstdio>
#include <thread>

int main(int argc, char** argv) {
    using namespace openclicknp;
    Platform p;
    p.open(argc>1?argv[1]:"build/L4LoadBalancer/L4LoadBalancer.xclbin", "",
           Platform::TransportKind::XDMA);
    p.launchAll();
    Element* lb = p.element("lb");
    // Install 4 backends.
    for (int i = 0; i < 4; ++i) {
        SignalRequest r{}; r.cmd = 1; r.sparam = static_cast<uint32_t>(i);
        r.lparam[0] = 0x0a000001ull + i;
        SignalResponse rsp{};
        if (lb) lb->signal(r, rsp);
    }
    while (true) std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
