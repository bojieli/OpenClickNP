// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/platform.hpp"
#include "openclicknp/element.hpp"
#include <chrono>
#include <thread>

int main(int argc, char** argv) {
    using namespace openclicknp;
    Platform p;
    p.open(argc>1?argv[1]:"build/DPI_Engine/DPI_Engine.xclbin", "",
           Platform::TransportKind::XDMA);
    p.launchAll();
    while (true) std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
