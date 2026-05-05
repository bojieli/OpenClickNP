// SPDX-License-Identifier: Apache-2.0
// v++ link configuration emitter.
//   generated/link/connectivity.cfg
//   generated/link/clocks.cfg

#include "openclicknp/passes.hpp"

#include <filesystem>
#include <fstream>

namespace openclicknp::backends {

namespace {

void emitConnectivity(std::ostream& os, const be::Build& b) {
    os << "# Auto-generated v++ link connectivity\n";
    os << "[connectivity]\n";
    // Number of kernel instances per binary kernel.
    for (const auto& k : b.kernels) {
        os << "nk=" << k.element_type << ":1:" << k.name << "\n";
    }
    // Stream connections kernel ↔ kernel.
    for (const auto& sc : b.stream_conns) {
        os << "stream_connect="
           << sc.src_kernel << ".out_" << sc.src_port << ":"
           << sc.dst_kernel << ".in_"  << sc.dst_port << ":"
           << sc.depth << "\n";
    }
    // tor / nic / host boundary streams: wire to platform-supplied
    // streaming-IP interfaces by name.
    int idx = 0;
    for (const auto& sc : b.tor_conns) {
        if (sc.dst_kernel.find("tor_out") == std::string::npos) {
            // tor_in -> kernel
            os << "stream_connect=tor_in" << idx << ":" << sc.dst_kernel
               << ".in_" << sc.dst_port << ":" << sc.depth << "\n";
        } else {
            os << "stream_connect=" << sc.src_kernel << ".out_" << sc.src_port
               << ":tor_out" << idx << ":" << sc.depth << "\n";
        }
        ++idx;
    }
    idx = 0;
    for (const auto& sc : b.nic_conns) {
        if (sc.dst_kernel.find("nic_out") == std::string::npos) {
            os << "stream_connect=nic_in" << idx << ":" << sc.dst_kernel
               << ".in_" << sc.dst_port << ":" << sc.depth << "\n";
        } else {
            os << "stream_connect=" << sc.src_kernel << ".out_" << sc.src_port
               << ":nic_out" << idx << ":" << sc.depth << "\n";
        }
        ++idx;
    }
    for (const auto& hs : b.host_streams) {
        if (hs.kernel_to_host) {
            os << "stream_connect=" << hs.kernel << ".out_" << hs.port
               << ":host_c2h_" << hs.slot_id << "\n";
        } else {
            os << "stream_connect=host_h2c_" << hs.slot_id << ":"
               << hs.kernel << ".in_" << hs.port << "\n";
        }
    }
}

void emitClocks(std::ostream& os, const be::Build& b) {
    os << "# Auto-generated v++ clock configuration\n";
    os << "[clock]\n";
    for (const auto& k : b.kernels) {
        os << "freqHz=" << b.user_clock_hz << ":" << k.name << ".ap_clk\n";
    }
}

}  // namespace

bool emitVppLink(const be::Build& build, const std::string& outdir,
                 DiagnosticEngine& d) {
    namespace fs = std::filesystem;
    fs::path dir = fs::path(outdir) / "link";
    fs::create_directories(dir);
    {
        std::ofstream f(dir / "connectivity.cfg");
        if (!f) { d.error({}, "cannot write connectivity.cfg"); return false; }
        emitConnectivity(f, build);
    }
    {
        std::ofstream f(dir / "clocks.cfg");
        if (!f) { d.error({}, "cannot write clocks.cfg"); return false; }
        emitClocks(f, build);
    }
    return true;
}

}  // namespace openclicknp::backends
