// SPDX-License-Identifier: Apache-2.0
// Counters helper — currently a thin wrapper over Platform::networkCounters.
#include "openclicknp/platform.hpp"

namespace openclicknp {

uint64_t cycleCounterOf(const Platform& p) { return p.cycleCounter(); }
NetworkCounters networkCountersOf(const Platform& p) { return p.networkCounters(); }

}  // namespace openclicknp
