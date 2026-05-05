// SPDX-License-Identifier: Apache-2.0
// Helpers that bridge between the registered generated tables and the
// runtime classes that consume them.
#include "openclicknp/platform.hpp"

#include <string>
#include <vector>

namespace openclicknp {

// Defined in platform.cpp via a static; expose accessor for cross-TU use.
namespace internal {

extern std::vector<SignalEntry>& generatedSignalsRef();   // implemented inline below

int findSignalGid(const std::string& kernel_name);

}  // namespace internal
}  // namespace openclicknp

// We can't reach the anonymous namespace inside platform.cpp directly, so we
// re-derive the table by walking through registerGenerated()'s last input.
// To keep things simple, we intercept registerGenerated() via a side table:
namespace openclicknp::internal {

namespace {
struct SideTable {
    std::vector<SignalEntry> signals;
};
SideTable& side() { static SideTable s; return s; }
}  // namespace

int findSignalGid(const std::string& kernel_name) {
    for (const auto& s : side().signals) {
        if (kernel_name == s.kernel) return s.gid;
    }
    return -1;
}

void recordSignals(const SignalEntry* s, size_t n) {
    side().signals.assign(s, s + n);
}

}  // namespace openclicknp::internal
