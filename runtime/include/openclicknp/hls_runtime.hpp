// SPDX-License-Identifier: Apache-2.0
// Vitis HLS runtime header — included by every generated kernel .cpp.
//
// The signal RPC interface is implemented as a struct passed by reference
// with `s_axilite` interface. HLS synthesizes the struct's scalar fields
// as individual AXI-Lite registers, so each access is a clean register
// read/write — no pointer arithmetic, no array indexing on a scalar port.
//
// We do NOT include hls_stream.h here so this header can compile on a
// plain Linux toolchain (it gets included by L1 unit tests too).
#pragma once

#include "openclicknp/flit.hpp"

namespace openclicknp {

// Layout matches what `Platform::dispatchSignal` writes from the host.
// Field order is fixed; the host expects exactly this layout.
struct SignalIO {
    uint32_t status;          // bit 0 = pending, bit 1 = response valid
    uint16_t cmd;
    uint32_t sparam;
    uint64_t lparam[7];       // 7 × 8 = 56 bytes
    uint64_t rsp_lparam[7];
    uint16_t rsp_cmd;
    uint32_t rsp_sparam;
};

inline bool poll_signal(SignalIO& io, ClSignal* out) {
    if ((io.status & 0x1) == 0) return false;
    out->cmd     = io.cmd;
    out->sparam  = io.sparam;
    for (int i = 0; i < 7; ++i) out->lparam[i] = io.lparam[i];
    io.status = 0;
    return true;
}

inline void respond_signal(SignalIO& io, const ClSignal& s) {
    io.rsp_cmd    = s.cmd;
    io.rsp_sparam = s.sparam;
    for (int i = 0; i < 7; ++i) io.rsp_lparam[i] = s.lparam[i];
    io.status = 0x2;
}

}  // namespace openclicknp
