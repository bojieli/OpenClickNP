// SPDX-License-Identifier: Apache-2.0
//
// Signal RPC dispatcher utilities. The actual AXI-Lite peek/poke happens in
// platform.cpp; this file holds free helpers for marshaling typed parameter
// lists into a SignalRequest payload.

#include "openclicknp/platform.hpp"

#include <cstring>

namespace openclicknp {

void packSignalParam(SignalRequest& req, int slot, uint64_t value) {
    if (slot >= 0 && slot < 7) req.lparam[slot] = value;
}

uint64_t unpackSignalParam(const SignalResponse& rsp, int slot) {
    if (slot < 0 || slot >= 7) return 0;
    return rsp.lparam[slot];
}

}  // namespace openclicknp
