// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/be_ir.hpp"

namespace openclicknp::be {

const char* platformName(Platform p) {
    switch (p) {
        case Platform::U50_XDMA: return "u50_xdma";
        case Platform::U50_QDMA: return "u50_qdma";
    }
    return "?";
}
Platform platformFromString(const std::string& s) {
    if (s == "u50_qdma") return Platform::U50_QDMA;
    return Platform::U50_XDMA;
}

}  // namespace openclicknp::be
