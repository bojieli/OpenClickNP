// SPDX-License-Identifier: Apache-2.0
// Internal slot-bridge interface, shared by XDMA and QDMA backends and by
// the Platform implementation. Not installed as a public header.
#pragma once

#include "openclicknp/flit.hpp"

#include <memory>

namespace openclicknp::internal {

class SlotBridge {
public:
    virtual ~SlotBridge() = default;
    virtual bool send(uint16_t slot, const flit_t&) = 0;
    virtual bool recv(uint16_t slot, flit_t&, bool blocking) = 0;
    virtual void shutdown() = 0;
};

std::unique_ptr<SlotBridge> makeXdmaBridge();
std::unique_ptr<SlotBridge> makeQdmaBridge();
std::unique_ptr<SlotBridge> makeStubBridge();

}  // namespace openclicknp::internal
