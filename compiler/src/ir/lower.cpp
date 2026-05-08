// SPDX-License-Identifier: Apache-2.0
// EG IR → BE IR lowering.
#include "openclicknp/passes.hpp"

namespace openclicknp {

namespace be { /* avoid name lookup ambiguity */ }

bool lowerToBackend(const eg::Graph& g,
                    DiagnosticEngine& /*d*/,
                    be::Platform platform,
                    int user_clock_hz,
                    const std::string& source_path,
                    be::Build& out) {
    out.platform = platform;
    out.user_clock_hz = user_clock_hz;
    out.source_path   = source_path;

    // Kernels: emit a BE KernelHls for each non-special kernel.
    for (const auto& k : g.kernels) {
        if (k.special != eg::SpecialKind::None) continue;
        be::KernelHls bk;
        bk.name = k.name;
        bk.element_type = k.type;
        bk.autorun = k.autorun;
        bk.has_signal = k.host_control;
        bk.axilite_base = k.axilite_base;
        for (int i = 1; i <= k.n_in_ports; ++i) {
            be::Port p; p.index = i; p.kind = eg::ChannelKind::Flit; p.width_bits = 512;
            bk.in_ports.push_back(p);
        }
        for (int i = 1; i <= k.n_out_ports; ++i) {
            be::Port p; p.index = i; p.kind = eg::ChannelKind::Flit; p.width_bits = 512;
            bk.out_ports.push_back(p);
        }
        for (const auto& p : k.params) bk.params.push_back(p.text);
        bk.state_cpp   = k.state_cpp;
        bk.init_cpp    = k.init_cpp;
        bk.handler_cpp = k.handler_cpp;
        bk.signal_cpp  = k.signal_cpp;
        bk.signal_params = k.signal_params;
        bk.pipeline_ii = k.pipeline_ii;
        bk.hls_pragmas = k.hls_pragmas;
        out.kernels.push_back(std::move(bk));

        if (k.host_control) {
            out.signal_table.push_back(
                be::Build::SignalEntry{k.gid, k.name, k.axilite_base});
        }
    }

    // Edges: classify into stream_conns / tor / nic / host.
    int slot_id = 32;   // host slot IDs start from 32 by convention
    for (const auto& e : g.edges) {
        const auto* sk = g.find(e.src_kernel);
        const auto* dk = g.find(e.dst_kernel);
        be::StreamConn sc;
        sc.src_kernel = e.src_kernel;
        sc.src_port   = e.src_port;
        sc.dst_kernel = e.dst_kernel;
        sc.dst_port   = e.dst_port;
        sc.lossy      = e.lossy;
        sc.depth      = e.depth;
        sc.width_bits = 512;

        bool src_special = sk && sk->special != eg::SpecialKind::None;
        bool dst_special = dk && dk->special != eg::SpecialKind::None;

        if (src_special) {
            switch (sk->special) {
                case eg::SpecialKind::TorIn:  out.tor_conns.push_back(sc); continue;
                case eg::SpecialKind::NicIn:  out.nic_conns.push_back(sc); continue;
                case eg::SpecialKind::HostIn: {
                    be::HostConn hc;
                    hc.kernel = e.dst_kernel;
                    hc.port = e.dst_port;
                    hc.kernel_to_host = false;  // host -> kernel
                    hc.slot_id = slot_id++;
                    out.host_streams.push_back(hc);
                    continue;
                }
                case eg::SpecialKind::Idle:
                    continue;     // never produces; drop edge
                default: break;
            }
        }
        if (dst_special) {
            switch (dk->special) {
                case eg::SpecialKind::TorOut: out.tor_conns.push_back(sc); continue;
                case eg::SpecialKind::NicOut: out.nic_conns.push_back(sc); continue;
                case eg::SpecialKind::HostOut: {
                    be::HostConn hc;
                    hc.kernel = e.src_kernel;
                    hc.port = e.src_port;
                    hc.kernel_to_host = true;
                    hc.slot_id = slot_id++;
                    out.host_streams.push_back(hc);
                    continue;
                }
                case eg::SpecialKind::Drop:
                    continue;     // discard sink; drop edge
                default: break;
            }
        }
        out.stream_conns.push_back(std::move(sc));
    }
    return true;
}

}  // namespace openclicknp
