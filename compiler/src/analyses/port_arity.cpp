// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/passes.hpp"

#include <sstream>

namespace openclicknp {

bool analyzePortArity(const eg::Graph& g, DiagnosticEngine& d) {
    bool ok = true;
    for (const auto& e : g.edges) {
        const auto* sk = g.find(e.src_kernel);
        const auto* dk = g.find(e.dst_kernel);
        if (sk && e.src_port > sk->n_out_ports) {
            std::ostringstream os;
            os << "kernel `" << sk->name << "` has " << sk->n_out_ports
               << " output port(s), but connection uses port " << e.src_port;
            d.error(e.src, os.str());
            ok = false;
        }
        if (dk && e.dst_port > dk->n_in_ports) {
            std::ostringstream os;
            os << "kernel `" << dk->name << "` has " << dk->n_in_ports
               << " input port(s), but connection uses port " << e.dst_port;
            d.error(e.src, os.str());
            ok = false;
        }
        if (e.src_port < 1) {
            d.error(e.src, "source port must be >= 1");
            ok = false;
        }
        if (e.dst_port < 1) {
            d.error(e.src, "destination port must be >= 1");
            ok = false;
        }
    }
    return ok;
}

}  // namespace openclicknp
