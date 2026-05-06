// SPDX-License-Identifier: Apache-2.0
//
// RSA modexp throughput benchmark.
//
// Drives N (m, e, n) operands through the chosen RSA_ModExp_BITS kernel
// in the SW emulator, times the run, prints a CSV row to stdout:
//   bits,iterations,wall_time_s,ops_per_sec,latency_us_per_op
//
// Build via the integration tests target. Invoke as:
//   test_rsa_perf <bits=1024|2048|4096> <iterations>
//
// We deliberately use random but reproducible (seed=42) operands so
// successive runs produce comparable numbers.
#include "openclicknp/bigint.hpp"
#include "openclicknp/sw_runtime.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <thread>

using namespace openclicknp;

extern "C" void kernel_RSA_ModExp_1024(
    SwStream& in_1, SwStream& in_2, SwStream& in_3, SwStream& out_1,
    std::atomic<bool>& _stop);
extern "C" void kernel_RSA_ModExp_2048(
    SwStream& in_1, SwStream& in_2, SwStream& in_3, SwStream& out_1,
    std::atomic<bool>& _stop);
extern "C" void kernel_RSA_ModExp_4096(
    SwStream& in_1, SwStream& in_2, SwStream& in_3, SwStream& out_1,
    std::atomic<bool>& _stop);

template <int LIMBS, int FLITS>
static void push_operand(SwStream& s, const bigint::U<LIMBS>& x) {
    for (int i = 0; i < FLITS; ++i) {
        flit_t f{};
        for (int j = 0; j < 4; ++j) f.set(j, x.limbs[i * 4 + j]);
        f.set_sop(i == 0); f.set_eop(i == FLITS - 1);
        s.write(f);
    }
}

template <int LIMBS, int FLITS, typename Fn>
static int run_bench(Fn kernel, int iters) {
    SwStream in_m(128), in_e(128), in_n(128), out(128);
    std::atomic<bool> stop{false};
    std::thread w(kernel, std::ref(in_m), std::ref(in_e), std::ref(in_n),
                  std::ref(out), std::ref(stop));

    // Build deterministic random operands. We don't need cryptographic
    // randomness for a perf benchmark — same RNG seed gives same ops.
    std::mt19937_64 rng(42);
    std::vector<bigint::U<LIMBS>> ms(iters), es(iters), ns(iters);
    for (int k = 0; k < iters; ++k) {
        for (int i = 0; i < LIMBS; ++i) {
            ms[k].limbs[i] = rng();
            ns[k].limbs[i] = rng();
        }
        // Force n to be odd (Montgomery requires it) and large.
        ns[k].limbs[0] |= 1;
        ns[k].limbs[LIMBS - 1] |= (1ULL << 63);
        // Force m < n (cheap): clear m's top bit.
        ms[k].limbs[LIMBS - 1] &= ~(1ULL << 63);
        es[k].set_zero();
        es[k].limbs[0] = 65537;
    }

    // Producer thread feeds operands as fast as possible; consumer thread
    // (this main thread) drains FLITS flits per op and counts.
    std::thread producer([&](){
        for (int k = 0; k < iters; ++k) {
            push_operand<LIMBS, FLITS>(in_m, ms[k]);
            push_operand<LIMBS, FLITS>(in_e, es[k]);
            push_operand<LIMBS, FLITS>(in_n, ns[k]);
        }
    });

    auto t0 = std::chrono::steady_clock::now();
    int got = 0;
    while (got < iters * FLITS) {
        flit_t f;
        if (out.read_nb(f)) { ++got; continue; }
        std::this_thread::yield();
    }
    auto t1 = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(t1 - t0).count();
    double ops_per_sec = iters / secs;
    double us_per_op = (secs * 1e6) / iters;

    int bits = LIMBS * 64;
    std::printf("%d,%d,%.6f,%.2f,%.2f\n",
                bits, iters, secs, ops_per_sec, us_per_op);

    producer.join();
    stop.store(true);
    if (w.joinable()) w.join();
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: test_rsa_perf <1024|2048|4096> <iterations>\n");
        return 1;
    }
    int bits = std::atoi(argv[1]);
    int iters = std::atoi(argv[2]);
    if (iters < 1) iters = 1;

    if (bits == 1024) return run_bench<16, 4>(kernel_RSA_ModExp_1024, iters);
    if (bits == 2048) return run_bench<32, 8>(kernel_RSA_ModExp_2048, iters);
    if (bits == 4096) return run_bench<64, 16>(kernel_RSA_ModExp_4096, iters);

    std::fprintf(stderr, "unknown bit width %d\n", bits);
    return 1;
}
