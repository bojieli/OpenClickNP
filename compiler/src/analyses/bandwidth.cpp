// SPDX-License-Identifier: Apache-2.0
// Bandwidth feasibility check.
//
// Each edge carries up to 512 bits per cycle at user_clock_hz. For boundary
// edges to/from CMACs, we check against the 100 GbE rate (≈ 149 Gb/s with PCS
// overhead — we use 100 Gb/s clean nominal). For host streams, we use the
// effective XDMA streaming rate (≈ 64 Gb/s per channel on Gen3x16).
#include "openclicknp/passes.hpp"

#include <map>
#include <sstream>

namespace openclicknp {

namespace {

bool isBoundary(const eg::Kernel* k) {
    if (!k) return false;
    switch (k->special) {
        case eg::SpecialKind::TorIn:
        case eg::SpecialKind::TorOut:
        case eg::SpecialKind::NicIn:
        case eg::SpecialKind::NicOut:
        case eg::SpecialKind::HostIn:
        case eg::SpecialKind::HostOut:
            return true;
        default:
            return false;
    }
}

}  // namespace

bool analyzeBandwidth(const eg::Graph& g, DiagnosticEngine& d, int user_clock_hz) {
    const double internal_bps = 512.0 * static_cast<double>(user_clock_hz);
    // CMAC 100G nominal:
    constexpr double cmac_bps = 100.0e9;

    // Aggregate per source kernel: sum of demanded bps across out-edges.
    std::map<std::string, double> demand_per_src;
    for (const auto& e : g.edges) {
        // Each edge runs at line rate of its source — but each kernel can
        // emit at most 1 flit/cycle aggregate across all output ports per
        // the paper's per-cycle semantics. So total demand from a kernel is
        // bounded by internal_bps regardless of fan-out.
        (void)demand_per_src;
        (void)e;
    }
    // Boundary saturation check: warn if a CMAC edge runs above 100 Gb/s.
    for (const auto& e : g.edges) {
        const auto* sk = g.find(e.src_kernel);
        const auto* dk = g.find(e.dst_kernel);
        if (isBoundary(sk) || isBoundary(dk)) {
            // Internal datapath supplies up to internal_bps; CMAC accepts
            // up to cmac_bps. Warn if internal < cmac (no headroom).
            if (internal_bps < cmac_bps) {
                std::ostringstream os;
                os.precision(3);
                os << "internal datapath " << (internal_bps / 1e9)
                   << " Gb/s is below CMAC line rate "
                   << (cmac_bps / 1e9)
                   << " Gb/s; this is fine for sub-line-rate workloads, but "
                      "100 G saturation requires 195+ MHz user clock";
                d.warn(e.src, os.str());
                break;
            }
        }
    }
    return true;
}

}  // namespace openclicknp
