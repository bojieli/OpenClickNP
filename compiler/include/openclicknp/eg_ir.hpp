// SPDX-License-Identifier: Apache-2.0
// Element-Graph IR: typed kernel graph after resolution and group inlining.
#pragma once

#include "openclicknp/ast.hpp"
#include "openclicknp/source.hpp"

#include <map>
#include <string>
#include <vector>

namespace openclicknp::eg {

enum class ChannelKind { Flit, Metadata };
enum class SpecialKind {
    None, TorIn, TorOut, NicIn, NicOut, HostIn, HostOut, Drop, Idle, Begin, End
};

[[nodiscard]] SpecialKind specialFromName(const std::string& name);
[[nodiscard]] const char* specialName(SpecialKind);

struct Kernel {
    std::string name;          // unique instance name
    std::string type;           // element type ("" for pseudo-elements)
    SpecialKind special = SpecialKind::None;

    int  n_in_ports  = 0;
    int  n_out_ports = 0;
    bool host_control = false;
    bool autorun      = false;

    // The four element-body opaque blocks (only present for non-special).
    std::string state_cpp;
    std::string init_cpp;
    std::string handler_cpp;
    std::string signal_cpp;

    std::vector<ast::SignalParam> signal_params;
    std::vector<ast::InstanceParam> params;

    int  channel_depth = 64;        // default depth for outgoing edges

    // Filled in by analyses
    int  axilite_base = -1;         // signal RPC AXI-Lite base (if @)
    int  gid          = -1;          // 16-bit signal-dispatch ID
    SourceRange src;
};

struct Edge {
    std::string src_kernel;
    int         src_port = 1;
    std::string dst_kernel;
    int         dst_port = 1;
    bool        lossy    = false;
    int         depth    = 64;
    ChannelKind kind     = ChannelKind::Flit;
    SourceRange src;
};

struct Graph {
    std::vector<Kernel> kernels;
    std::vector<Edge>   edges;

    // Convenience lookup by name (rebuilt on demand).
    [[nodiscard]] const Kernel* find(const std::string& name) const;
};

}  // namespace openclicknp::eg
