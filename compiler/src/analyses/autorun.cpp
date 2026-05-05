// SPDX-License-Identifier: Apache-2.0
// Autorun classification: any non-host-controlled, non-special kernel that
// has at least one input port is implicitly autorun (free-running) unless
// the user has explicitly marked it. The paper's element model is naturally
// free-running: the handler loop runs forever.
#include "openclicknp/passes.hpp"

namespace openclicknp {

bool analyzeAutorun(eg::Graph& g, DiagnosticEngine&) {
    for (auto& k : g.kernels) {
        if (k.special != eg::SpecialKind::None) continue;
        if (k.host_control) {
            // host_control kernels can still be autorun for the data path;
            // the signal handler is invoked on demand.
            k.autorun = true;
        } else {
            k.autorun = true;
        }
    }
    return true;
}

}  // namespace openclicknp
