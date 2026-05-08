// SPDX-License-Identifier: Apache-2.0
// Backend IR: the build plan consumed by every codegen backend.
#pragma once

#include "openclicknp/eg_ir.hpp"

#include <map>
#include <string>
#include <vector>

namespace openclicknp::be {

enum class Platform { U50_XDMA, U50_QDMA };

[[nodiscard]] const char* platformName(Platform);
[[nodiscard]] Platform    platformFromString(const std::string&);

struct Port {
    int         index = 1;        // 1-based
    eg::ChannelKind kind = eg::ChannelKind::Flit;
    int         width_bits = 512;
};

struct KernelHls {
    std::string name;             // matches the eg::Kernel name
    std::string element_type;
    bool        autorun = false;
    bool        has_signal = false;
    int         axilite_base = -1;
    int         pipeline_ii = 0;  // 0 = backend default (1)
    std::vector<std::string> hls_pragmas;  // free-form HLS directives
    std::vector<Port> in_ports;
    std::vector<Port> out_ports;
    std::vector<std::string> params;

    // Element-body verbatim source (joined and prologue/epilogue inserted by
    // the HLS C++ emitter):
    std::string state_cpp;
    std::string init_cpp;
    std::string handler_cpp;
    std::string signal_cpp;

    std::vector<ast::SignalParam> signal_params;
};

struct StreamConn {
    std::string src_kernel;
    int         src_port = 1;
    std::string dst_kernel;
    int         dst_port = 1;
    bool        lossy = false;
    int         depth = 64;
    int         width_bits = 512;
};

struct HostConn {
    // Streaming connection between a kernel port and the host (slot bridge).
    std::string kernel;
    int         port = 1;
    bool        kernel_to_host = true;   // direction
    int         slot_id = -1;
};

struct MemConn {
    std::string kernel;
    std::string mem_bank;        // e.g. "HBM[0]"
};

struct Build {
    Platform                  platform = Platform::U50_XDMA;
    int                       user_clock_hz = 322265625;   // 322.265625 MHz
    std::vector<KernelHls>    kernels;
    std::vector<StreamConn>   stream_conns;
    std::vector<HostConn>     host_conns;
    std::vector<MemConn>      mem_conns;

    // Special boundary connections (tor/nic CMAC + host_in/out)
    std::vector<StreamConn>   tor_conns;
    std::vector<StreamConn>   nic_conns;
    std::vector<HostConn>     host_streams;

    // Signal-dispatch table (gid → kernel name + AXI-Lite base addr)
    struct SignalEntry {
        int gid = -1;
        std::string kernel;
        int axilite_base = 0;
    };
    std::vector<SignalEntry>  signal_table;

    // Source path of the original .clnp (for header comments)
    std::string source_path;
};

}  // namespace openclicknp::be
