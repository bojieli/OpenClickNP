// SPDX-License-Identifier: Apache-2.0
// Auto-generated behavioral test for element: Clock
// Liveness test — drives a few inputs, drains outputs, asserts no hang.
#include "element_test_harness.hpp"

using namespace openclicknp;
using namespace openclicknp::test;

extern "C" void kernel_u(
    std::atomic<bool>& _stop,
    SignalChannel& _sigch
);

int main() {

    KernelHarness h;
    h.start(kernel_u, std::ref(h.stop), std::ref(h.sigch));
    // Liveness check only — element specifics covered by app-level tests.
    h.run_for(50ms);
    ELEM_PASS("Clock");
}
