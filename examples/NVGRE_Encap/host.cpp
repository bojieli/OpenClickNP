// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/platform.hpp"
#include "openclicknp/element.hpp"
#include <chrono>
#include <thread>

int main(int argc, char** argv) {
    using namespace openclicknp;
    Platform p;
    p.open(argc>1?argv[1]:"build/NVGRE_Encap/NVGRE_Encap.xclbin", "",
           Platform::TransportKind::XDMA);
    p.launchAll();
    Element* enc = p.element("encap");
    SignalRequest r{}; r.cmd = 1; r.lparam[0] = 0xABCD;
    SignalResponse rsp{};
    if (enc) enc->signal(r, rsp);
    while (true) std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
