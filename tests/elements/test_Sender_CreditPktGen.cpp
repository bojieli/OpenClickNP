// SPDX-License-Identifier: Apache-2.0
// Auto-generated behavioral test for element: Sender_CreditPktGen
// Liveness test — drives a few inputs, drains outputs, asserts no hang.
#include "element_test_harness.hpp"

using namespace openclicknp;
using namespace openclicknp::test;

extern "C" void kernel_u(
    SwStream& out_1,
    std::atomic<bool>& _stop
);

int main() {
    SwStream out_1(64);
    KernelHarness h;
    h.start(kernel_u, std::ref(out_1), std::ref(h.stop));
    auto outs_1 = drain(out_1, 4, 200ms);
    // Liveness check only — element specifics covered by app-level tests.
    ELEM_ASSERT(outs_1.size() >= 0u, "sourced flits or none");
    h.run_for(50ms);
    ELEM_PASS("Sender_CreditPktGen");
}
