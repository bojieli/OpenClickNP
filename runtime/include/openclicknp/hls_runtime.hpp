// SPDX-License-Identifier: Apache-2.0
// Vitis HLS runtime header — included by every generated kernel .cpp.
//
// We do NOT include hls_stream.h here so this header can compile on a
// plain Linux toolchain (it gets included by L1 unit tests too). Vitis
// HLS users are expected to include hls_stream.h themselves before
// including this file (the generated kernels do).
#pragma once

#include "openclicknp/flit.hpp"

namespace openclicknp {

// Stub helpers: in HLS these are translated to AXI-Lite poke/peek; in
// software they are no-ops. The signature is kept simple to keep the
// generator simple.
inline bool poll_signal(volatile uint32_t* regs, ClSignal* out) {
    if (!regs) return false;
    if ((regs[0] & 0x1) == 0) return false;
    // Marshal 16x32-bit words → 64-byte ClSignal.
    uint32_t* p = reinterpret_cast<uint32_t*>(out);
    for (int i = 0; i < 16; ++i) p[i] = regs[1 + i];
    regs[0] = 0;   // clear pending
    return true;
}

inline void respond_signal(volatile uint32_t* regs, const ClSignal& s) {
    if (!regs) return;
    const uint32_t* p = reinterpret_cast<const uint32_t*>(&s);
    for (int i = 0; i < 16; ++i) regs[17 + i] = p[i];
    regs[0] = 0x2;  // response valid
}

}  // namespace openclicknp
