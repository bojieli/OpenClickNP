// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/element.hpp"

#include <utility>

namespace openclicknp {

// SignalDispatchTable lives inside platform.cpp; we provide an accessor.
namespace internal {
int findSignalGid(const std::string& kernel_name);   // defined in lookups.cpp
}

void Element::launch() {}

bool Element::signal(const SignalRequest& req, SignalResponse& rsp,
                     int timeout_ms) {
    int gid = internal::findSignalGid(name_);
    if (gid < 0) return false;
    return platform_.dispatchSignal(gid, req, rsp, timeout_ms);
}

void Element::send(const HostMessage& msg) {
    platform_.sendSlot(msg.slot_id, msg.flit);
}

bool Element::receive(HostMessage& msg, bool blocking) {
    return platform_.recvSlot(msg.slot_id, msg.flit, blocking);
}

void Element::setCallback(Callback cb) {
    callback_ = std::move(cb);
}

}  // namespace openclicknp
