// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/eg_ir.hpp"

namespace openclicknp::eg {

SpecialKind specialFromName(const std::string& n) {
    if (n == "tor_in")   return SpecialKind::TorIn;
    if (n == "tor_out")  return SpecialKind::TorOut;
    if (n == "nic_in")   return SpecialKind::NicIn;
    if (n == "nic_out")  return SpecialKind::NicOut;
    if (n == "host_in")  return SpecialKind::HostIn;
    if (n == "host_out") return SpecialKind::HostOut;
    if (n == "Drop")     return SpecialKind::Drop;
    if (n == "Idle")     return SpecialKind::Idle;
    if (n == "begin")    return SpecialKind::Begin;
    if (n == "end")      return SpecialKind::End;
    return SpecialKind::None;
}

const char* specialName(SpecialKind k) {
    switch (k) {
        case SpecialKind::None:    return "(user)";
        case SpecialKind::TorIn:   return "tor_in";
        case SpecialKind::TorOut:  return "tor_out";
        case SpecialKind::NicIn:   return "nic_in";
        case SpecialKind::NicOut:  return "nic_out";
        case SpecialKind::HostIn:  return "host_in";
        case SpecialKind::HostOut: return "host_out";
        case SpecialKind::Drop:    return "Drop";
        case SpecialKind::Idle:    return "Idle";
        case SpecialKind::Begin:   return "begin";
        case SpecialKind::End:     return "end";
    }
    return "?";
}

const Kernel* Graph::find(const std::string& name) const {
    for (const auto& k : kernels) if (k.name == name) return &k;
    return nullptr;
}

}  // namespace openclicknp::eg
