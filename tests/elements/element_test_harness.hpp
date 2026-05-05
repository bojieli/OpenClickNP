// SPDX-License-Identifier: Apache-2.0
//
// Generic behavioral-test harness for individual elements.
//
// Each test_<element>.cpp:
//   1. Includes the auto-generated SW-emu kernel function for ONE element
//      (compiled as `extern "C" void kernel_<NAME>(streams..., stop, sigch)`).
//   2. Spawns the kernel in its own thread.
//   3. Pushes test flits into the input streams.
//   4. Pulls outputs and checks invariants.
//   5. Stops the kernel and joins the thread.
#pragma once

#include "openclicknp/flit.hpp"
#include "openclicknp/sw_runtime.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace openclicknp::test {

using namespace std::chrono_literals;

// Helpers for building canned test flits.
inline flit_t make_flit(uint64_t l0 = 0, uint64_t l1 = 0,
                        uint64_t l2 = 0, uint64_t l3 = 0,
                        bool sop = true, bool eop = true) {
    flit_t f{};
    f.set(0, l0); f.set(1, l1); f.set(2, l2); f.set(3, l3);
    f.set_sop(sop); f.set_eop(eop);
    return f;
}

// Run a kernel for `timeout_ms` and call `stop` afterward. The kernel
// thread joins before the function returns.
struct KernelHarness {
    std::atomic<bool> stop{false};
    std::thread       worker;
    SignalChannel     sigch;

    // Caller passes the full argument list, including any trailing
    // std::ref(h.stop) / std::ref(h.sigch) references the kernel expects.
    template <typename Fn, typename... Args>
    void start(Fn&& fn, Args&&... args) {
        worker = std::thread(std::forward<Fn>(fn), std::forward<Args>(args)...);
    }

    void run_for(std::chrono::milliseconds d) {
        std::this_thread::sleep_for(d);
        stop.store(true);
        if (worker.joinable()) worker.join();
    }

    ~KernelHarness() {
        stop.store(true);
        if (worker.joinable()) worker.join();
    }
};

// Drain up to `n_flits` from a stream, with timeout.
inline std::vector<flit_t> drain(SwStream& s, int n_flits,
                                 std::chrono::milliseconds timeout = 200ms) {
    std::vector<flit_t> out;
    auto t0 = std::chrono::steady_clock::now();
    while (static_cast<int>(out.size()) < n_flits &&
           std::chrono::steady_clock::now() - t0 < timeout) {
        flit_t f{};
        if (s.read_nb(f)) out.push_back(f);
        else std::this_thread::sleep_for(50us);
    }
    return out;
}

// Helper for tests that want to push then drain.
inline std::vector<flit_t> push_and_drain(SwStream& in, SwStream& out,
                                          const std::vector<flit_t>& inputs,
                                          int expected_outputs,
                                          std::chrono::milliseconds timeout = 500ms) {
    std::thread pusher([&]() {
        for (const auto& f : inputs) in.write(f);
    });
    auto outs = drain(out, expected_outputs, timeout);
    pusher.join();
    return outs;
}

#define ELEM_ASSERT(cond, msg)                                                   \
    do { if (!(cond)) {                                                          \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg));     \
        return 1;                                                                \
    } } while (0)

#define ELEM_ASSERT_EQ(a, b, msg)                                                \
    do { auto _A = (a); auto _B = (b);                                           \
        if (_A != _B) {                                                          \
            std::fprintf(stderr,                                                 \
                "FAIL %s:%d: %s (got %llu, want %llu)\n",                        \
                __FILE__, __LINE__, (msg),                                       \
                (unsigned long long)_A, (unsigned long long)_B);                 \
            return 1;                                                            \
        }                                                                        \
    } while (0)

#define ELEM_PASS(name) do {                                                     \
    std::printf("[ELEM-OK] %s\n", (name));                                       \
    return 0;                                                                    \
} while (0)

}  // namespace openclicknp::test
