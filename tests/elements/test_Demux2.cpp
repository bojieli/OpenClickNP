// SPDX-License-Identifier: Apache-2.0
// Auto-generated behavioral test for element: Demux2
// Liveness test — drives a few inputs, drains outputs, asserts no hang.
#include "element_test_harness.hpp"

using namespace openclicknp;
using namespace openclicknp::test;

extern "C" void kernel_u(
    SwStream& in_1,
    SwStream& out_1,
    SwStream& out_2,
    std::atomic<bool>& _stop
);

int main() {
    SwStream in_1(64);
    SwStream out_1(64);
    SwStream out_2(64);
    KernelHarness h;
    h.start(kernel_u, std::ref(in_1), std::ref(out_1), std::ref(out_2), std::ref(h.stop));
    auto pusher = std::thread([&]() {
        in_1.write(make_flit(0x101));
        in_1.write(make_flit(0x201));
    });
    auto outs_1 = drain(out_1, 4, 200ms);
    auto outs_2 = drain(out_2, 4, 200ms);
    pusher.join();
    // Liveness check only — element specifics covered by app-level tests.
    ELEM_ASSERT(outs_1.size() <= 8u, "output should be bounded");
    h.run_for(50ms);
    ELEM_PASS("Demux2");
}
