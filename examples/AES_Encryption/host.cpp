// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/platform.hpp"
#include "openclicknp/element.hpp"
#include <chrono>
#include <thread>

int main(int argc, char** argv) {
    using namespace openclicknp;
    Platform p;
    p.open(argc>1?argv[1]:"build/AES_Encryption/AES_Encryption.xclbin", "",
           Platform::TransportKind::XDMA);
    p.launchAll();
    Element* e_aes = p.element("aes");
    if (e_aes) {
        SignalRequest r{}; r.cmd = 1;
        r.lparam[0] = 0xdeadbeefdeadbeefull;
        r.lparam[1] = 0x123456789abcdefull;
        SignalResponse rsp{};
        e_aes->signal(r, rsp);
    }
    while (true) std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
