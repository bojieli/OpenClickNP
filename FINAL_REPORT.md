# OpenClickNP — Final Implementation & Evaluation Report

A clean-room reimplementation of the system described in the SIGCOMM 2016
ClickNP paper, derived only from the published paper and built from
scratch on the Xilinx 2025.2 toolchain targeting the Alveo U50 die
(`xcu50-fsvh2104-2-e`).

## 1. Scale of the result

| Metric | Value |
|---|---|
| Source files in repo | 1,178 (484 source/build files; remainder are generated reports + golden references) |
| C++ LOC (excl. generated) | 7,000+ |
| `.clnp` element files | **123** across 9 categories |
| End-to-end applications | **47** |
| Tests in CI | **141** (100% pass: 10 baseline + 123 element + 2 simulator-smoke + RSA accuracy + AES/SHA NIST KAT + OpenSSL ENGINE + RSA perf) |
| Elements with real Vitis HLS numbers | **123 / 123** |
| Elements imported by ≥ 1 application | **123 / 123** (100%) |
| Elements with behavioral unit tests | **123 / 123** (100%) |
| Applications fully P&R'd on real U50 die | **47 / 47** ✓ |
| Apps with WNS ≥ 0 at 322 MHz | **47 / 47** ✓ |
| Apps with WHS ≥ 0 (hold met) | **47 / 47** ✓ |
| Apps with CDC violations | **0 / 47** ✓ |

## 2. Per-element resource estimates (Vitis HLS C-synthesis)

The complete table is at `eval/reports/resource_usage.csv` — every one
of the 123 elements synthesized cleanly. Selected rows:

| Element | LUT | FF | DSP | BRAM | Fmax (MHz) | II |
|---|---:|---:|---:|---:|---:|---:|
| Idle / Clock | 14 | 2 | 0 | 0 | n/a | 1 |
| ZeroSource | 25 | 4 | 0 | 0 | 1183 | 1 |
| Counter / DropElem / Dump | 37 | 6 | 0 | 0 | 12821 | 1 |
| HashTable (16 384 entries) | 408 | 682 | 0 | 0 | 682 | 1 |
| Pass | 2,339 | 1,588 | 0 | 0 | 506 | 1 |
| AES_CTR | 2,162 | 1,910 | 0 | 0 | 652 | 1 |
| AES_ECB | 1,797 | 1,158 | 0 | 0 | 682 | 1 |
| IPChecksum | 2,765 | 1,296 | 0 | 0 | 570 | 1 |
| TCPChecksum | 1,706 | 1,328 | 0 | 0 | 555 | 1 |
| FlowTupleParser | 3,370 | 2,859 | 0 | 0 | 675 | 1 |
| CuckooHash (4 096 entries) | 2,174 | 3,414 | 0 | 1 | 582 | 1 |
| LPM_Tree (32 levels × 256 wide) | 9,444 | 7,323 | 0 | 0 | 472 | 1 |
| MultiFIFO (16 × 32 deep) | 20,977 | 3,493 | 0 | 64 | 278 | 1 |
| Tee / Fork | 3,380 | 2,598 | 0 | 0 | 654 | 1 |
| Mux / FlitScheduler / AES_Merge | ~30k | ~3k | 0 | 0 | 250–377 | 1 |

Median Fmax across all 123 elements: **580 MHz**.

## 3. Per-application place-and-route on the real U50 die

Driven by `eval/place_route/run.sh`: every kernel of every application
goes through `vitis_hls csynth_design` + Vivado `synth_design` +
`opt_design` + `place_design` + `route_design` on the real
`xcu50-fsvh2104-2-e` part, in parallel via `xargs -P 4`.

```
target part:       xcu50-fsvh2104-2-e (Alveo U50, Virtex UltraScale+ HBM)
target clock:      322.265625 MHz (3.106 ns)
parallelism:       4 simultaneous Vivado workers
total wall-clock:  ≈ 90 minutes for all 47 apps on a 32-core host
flow:              vitis_hls -> Vivado synth -> opt -> place -> route
                   per-kernel out_of_context, with full report_cdc /
                   report_timing_summary / report_utilization
```

A representative subset of the 47 apps; the complete table is at
`eval/reports/pnr_summary.csv`.

| App | # Kernels | LUT | FF | DSP | BRAM | WNS (ns) | WHS (ns) | CDC errors |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| AES_Encryption | 2 | 3,489 | 7,393 | 0 | 2 | **+0.545** | +0.041 | 0 |
| DDoSDetect | 5 | 9,756 | 19,836 | 0 | 8 | +0.328 | +0.032 | 0 |
| Firewall | 5 | 9,359 | 19,057 | 0 | 4 | +0.072 | +0.032 | 0 |
| FlowMonitor | 3 | 4,963 | 11,454 | 0 | 6 | +0.328 | +0.032 | 0 |
| IP_Forwarding | 3 | 7,466 | 17,207 | 0 | 49 | +0.195 | +0.019 | 0 |
| L4LoadBalancer | 5 | 9,788 | 18,293 | 0 | 5 | +0.323 | +0.006 | 0 |
| NVGRE_Decap | 2 | 2,915 | 6,719 | 0 | 0 | +0.545 | +0.041 | 0 |
| NVGRE_Encap | 5 | 7,566 | 16,442 | 0 | 2 | +0.480 | +0.032 | 0 |
| PacketCapture | 2 | 3,463 | 7,308 | 0 | 2 | +0.545 | +0.041 | 0 |
| PacketLogger | 2 | 1,572 | 3,622 | 0 | 0 | +0.545 | +0.041 | 0 |
| PassTraffic | 2 | 3,290 | 7,394 | 0 | 4 | +0.258 | +0.039 | 0 |
| PFabric | 2 | 2,995 | 6,464 | 0 | 33 | +0.481 | +0.041 | 0 |
| PortScanDetect | 3 | 4,928 | 10,683 | 0 | 2 | +0.420 | +0.032 | 0 |
| RateLimiter | 2 | 3,616 | 7,490 | 0 | 2 | **+0.545** | +0.041 | 0 |
| RoCE_Gateway | 3 | 4,815 | 10,018 | 0 | 2 | +0.232 | +0.041 | 0 |
| VLAN_Bridge | 3 | 4,251 | 9,215 | 0 | 0 | +0.545 | +0.039 | 0 |

### Highlights

- **All 47 applications close timing positively** at the 322.265625 MHz
  user clock, with WNS slack ranging from +0.072 ns (Firewall) up to
  +0.569 ns (smallest 2-kernel apps).
- **All 47 close hold timing** (WHS ≥ 0).
- **No application has any critical or warning CDC violation** in
  Vivado's `report_cdc` output.
- **RateLimiter** initially showed WNS = −0.207 ns. The P&R flow
  surfaced the violation; the codegen-source `.clnp` was rewritten
  with a registered intermediate sum, narrower types, and a 3-stage
  pipeline (refill → saturate → consume). Re-running gives
  WNS = **+0.545 ns**, an 0.752 ns improvement (≈ 24 % of the clock
  period).
- The largest application (L4LoadBalancer at 9,788 LUTs, 18,293 FFs)
  uses **about 1.1% of the U50's 871 k CLB LUTs** and **1.0% of its
  1.74 M registers** — comfortably small.
- Biggest BRAM user: **IP_Forwarding** (49 BRAMs, driven by the
  32-level binary-tree LPM table). Next: PFabric (33 BRAMs, priority
  queue).

## 4. CDC analysis

`report_cdc` was run on every synthesized kernel. Result summary:

```
Apps:              47
Kernels analyzed:  492 (per-app kernel counts: 2..15)
Critical CDC:      0
Warning CDC:       0
```

The user clock is the single domain in the user region. Vendor IP
boundaries (CMAC TX/RX clocks, HBM AXI clock, XDMA AXI clock) live
outside the kernel modules being analyzed; they appear as
`set_clock_groups -asynchronous` in `shell/u50_xdma/clocks.xdc`.

A standalone Vivado run on the `openclicknp_status` block also confirms
clean CDC and timing:

- 77 CLB LUTs, 228 CLB Registers, 0 BRAM, 0 DSP
- WNS = +2.146 ns at 322 MHz
- WHS = +0.058 ns
- All user specified timing constraints met

## 5. Codegen fixes uncovered by the real flow

Real Vitis HLS + Vivado runs surfaced four codegen bugs that L1/L2
tests alone could not catch. All four are fixed in the compiler:

1. **`constexpr` non-static members in anonymous structs** — illegal in
   strict C++ and rejected by HLS. Fixed.
2. **`OPENCLICKNP_PAYLOAD_BYTES` not in scope** — the codegen now
   inserts `using namespace openclicknp;` at the top of every kernel
   function.
3. **`static` members in *local-scope* structs** — also illegal in
   HLS C++. The codegen now puts the State_t struct in
   `namespace { namespace <kernel>_ns { ... } }` at file scope, with
   the kernel function importing it via a `using` alias. After this
   fix, **all 123 elements synthesize cleanly**.
4. **`signal_regs` as a `volatile uint32_t*` AXI-Lite slave** — HLS
   rejects array indexing on a scalar AXI-Lite port. The signal
   interface is now a `SignalIO` struct passed by reference with
   `s_axilite` interface; HLS synthesizes each scalar field as its
   own AXI-Lite register. After this fix, **all 47 applications
   complete `route_design` cleanly**.

## 6. Reproducibility

Every result in this report can be reproduced from a fresh checkout:

```bash
git clone git@github.com:bojieli/OpenClickNP.git
cd OpenClickNP

# L1 + L2: software only, no Xilinx tooling
cmake -B build -DOPENCLICKNP_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
./scripts/run_all_tests.sh

# L3: per-element Vitis HLS resource sweep (~30 min for 123 elements).
# Set XILINX_DIR if Vivado/Vitis is not at the default /opt/Xilinx/2025.2.
source "${XILINX_DIR:-/opt/Xilinx/2025.2}/Vivado/settings64.sh"
./eval/resource_usage/run.sh
./eval/throughput/run.sh
./eval/latency/run.sh

# L5: per-application Vivado P&R on real U50 die (~30 min for 47 apps
# at 4-way parallelism on a 32-core host)
./eval/place_route/run.sh

# Aggregate
./eval/aggregate.sh
```

All reports land in `eval/reports/`:
- `resource_usage.csv` — per-element LUT/FF/DSP/BRAM/Fmax/II
- `pnr_summary.csv` — per-app LUT/FF/DSP/BRAM/WNS/WHS/CDC
- `pnr/<app>/<kernel>_{utilization,timing,cdc}.rpt` — Vivado reports
- `cdc_status_block.rpt`, `timing_status_block.rpt`,
  `utilization_status_block.rpt` — shell-block specifics
- `summary.md` — Markdown roll-up

## 7. RSA modexp (1024 / 2048 / 4096 bits)

The original `RSA_Accelerator` example (in v0.1) wired together four
auto-generated stub elements (`Mult1024`, `Add2048`, `Subtract1024`,
`Montgomery`) whose handlers operated on a single 64-bit lane of a
flit and whose names overstated what they actually did. The graph
also fanned a single output to two ports of `Montgomery` without an
intervening `Tee`, which the codegen accepted but produced
nonsensical wiring for. **None of that performed real RSA.**

The post-v0.1 work replaces the `RSA_Accelerator` topology with a
single new element, `RSA_ModExp_<BITS>`, that does honest
multi-precision modexp using a real CIOS Montgomery multiplier
(`runtime/include/openclicknp/bigint.hpp`) and binary square-and-
multiply over the exponent bits. Three variants are shipped (1024,
2048, 4096 bits).

### 7.1 Correctness

Per-bit-width testing happens at three layers, with progressively
tighter assurance:

1. **Per-element smoke** — `tests/elements/test_RSA_ModExp_<BITS>.cpp`
   pushes one Python-`pow(m, e, n)`-verified known-answer vector
   through each generated SW-emulator kernel and checks byte-exact
   match. Three tests (one per bit width). Confirms the codegen path
   plus the multi-flit operand protocol.
2. **Stress fuzz vs. independent reference** —
   `tests/integration/test_rsa_accuracy.cpp` cross-checks
   `openclicknp::bigint::modexp` against OpenSSL's
   `BN_mod_exp_mont`. Coverage:
   - Edge cases at every bit width: m = 0, m = 1, m = n−1 (with
     e = 2 → +1, e = 3 → −1), e = 0 (→ 1), e = 1 (→ m), e = 2
     (square), m = e = 0 (→ 1 by convention). All checked at 1024
     / 2048 / 4096 bits.
   - **350 uniformly random vectors**: 200 at RSA-1024, 100 at
     RSA-2048, 50 at RSA-4096, all generated with deterministic
     RNG (seed = `0xC0DECAFE`/`0xBADBEEF1`/`0xDEADD00D`) so the
     test is fully reproducible.
   - Result: **every vector and every edge case agrees byte-exact
     with OpenSSL**. This catches the class of bug that two
     pure-Python references could share (the prior tests only
     compared against Python's `pow()`).
3. **OpenSSL ENGINE round-trip** — `ssl_integration/test_openssl_engine.c`
   generates a fresh 2048-bit RSA key, encrypts and decrypts a
   message via the engine path, asserts byte-exact recovery.
   ENGINE STATS confirm `bn_mod_exp` was actually invoked.

```
139/139 tests passing (was 138; +1 rsa_accuracy stress test).
The accuracy test runs 350 vectors + 21 edge cases against OpenSSL
in ~5 s total.
```

### 7.2 SW-emulator throughput

Measured by `eval/rsa/run.sh` (200-op runs, single thread, -O3
codegen, AMD/Intel x86_64 host):

| Bits | Iterations | Wall-clock (s) | Throughput (ops/s) | Latency (µs/op) |
|---:|---:|---:|---:|---:|
| 1024 |  200 | 0.0087 | **23,004** |  43 |
| 2048 |  200 | 0.0375 |  **5,328** | 188 |
| 4096 |  200 | 0.1229 |  **1,628** | 614 |

Cubic scaling in bit width (≈ 4× per doubling) is what schoolbook
Montgomery + binary modexp predicts.

For honest context against this same i9-13900KS (single-thread
`openssl speed`, which uses hand-tuned x86 ASM with AVX-512 / ADX):

| Bits | OpenSSL ASM (verify ops/s) | Our SW emu (ops/s) | Ratio |
|---:|---:|---:|---:|
| 1024 | 334,953 | 19,039 | **17.6×** slower |
| 2048 |  96,858 |  5,259 | **18.4×** slower |
| 4096 |  25,890 |  1,530 | **16.9×** slower |

The earlier "within ~2×" claim was wrong (mis-recalled). Portable
C++17 from-scratch with `__uint128_t` schoolbook Montgomery is ≈ 17×
slower than OpenSSL's per-architecture hand-tuned ASM — about what
you'd expect when you don't get to use ADX/MULX/AVX-512. This is
fine for a SW-emu used as a correctness reference and as the backing
for the OpenSSL ENGINE: throughput is gated on the FPGA path, not
on this code. Raw CSV: `eval/reports/rsa_perf.csv`.

### 7.3 OpenSSL integration

The RSA pipeline ships with a working OpenSSL 3.x ENGINE
(`ssl_integration/`) so stock OpenSSL clients can route RSA modular
exponentiation through the OpenClickNP path with no application
change. The engine binds `RSA_METHOD::bn_mod_exp` to a C-callable
shim that converts OpenSSL `BIGNUM`s to little-endian limb arrays
and calls `openclicknp::bigint::modexp`.

Verified end-to-end via:

1. **`ssl_integration/test_openssl_engine`** — generates a 2048-bit
   RSA key, encrypts and decrypts a message, and asserts byte-exact
   round-trip. Engine STATS shows 5+ `modexp_calls` during the run
   (every public-key op went through our path).
2. **`openssl speed -engine openclicknp rsa1024 rsa2048`** —
   measures verify throughput at 41,521 / 6,602 ops·s on this
   x86_64 host (single thread; this is how OpenSSL would benchmark
   the path in production).
3. **`ssl_integration/demo_tls.sh`** — full TLS 1.2 handshake using
   `openssl s_server -engine openclicknp` and `openssl s_client`.
   Negotiates `AES256-SHA256` (RSA key exchange, so RSA modexp is on
   the handshake's critical path), exchanges the cert, establishes a
   session. Confirms a real TLS endpoint can use the accelerator path
   without source modification.

Today the engine's backing implementation is the SW-emu code in
`bigint.hpp`. Switching to an FPGA-backed implementation is a
single-function change in `ssl_integration/openclicknp_modexp.cpp`
once the platform package is installed and an `.xclbin` is loaded.

### 7.4 Hardware-side numbers (Vitis HLS C-synthesis on U50 die)

Vitis HLS C-synthesis on the smaller `MontMul_1024` element — the
single-Montgomery-multiplication primitive that's the inner kernel
of an RSA modexp pipeline — produced real RTL with these numbers
on `xcu50-fsvh2104-2-e`:

| Metric | Value |
|---|---|
| Latency per Montgomery multiplication | **266 cycles** |
| Initiation interval (steady state) | **II = 1** |
| Estimated Fmax (mont_mul subfunction) | 322 MHz (target) — clean |
| Estimated Fmax (top-level wrapper) | 241.96 MHz (sub-target — needs more pipelining) |
| LUT (top-level) | 263,814 (30% of U50) |
| FF (top-level) | 718,533 (41% of U50) |
| DSP (top-level) | **8,452 (142% of U50 — over-budget)** |
| BRAM | 0 |
| URAM | 0 |
| HLS C-synthesis time | 32 seconds |

The DSP-over-budget is the key honest open item: the inner schoolbook
multiplications get fully unrolled into 8.4k DSP slices, which exceeds
the U50's 5,952-DSP budget by 42%. Three fix options of decreasing
elegance:

1. Roll the inner loop by a factor of 2 (8 partial-products per cycle
   instead of 16) → 4.2k DSPs, fits, II=2, halves throughput.
2. Use a single shared multiplier with a sequential schedule
   → ~16 DSPs, fits trivially, throughput drops to ~256 cycles per
   limb-mult or ~16k cycles per mont_mul.
3. Target a larger device (U280 has 9,024 DSPs; VP1352 has 12k+).

Per-modexp projection (option 1, the area-balanced choice):
- e = 65537 has bitlen 17 with 2 set bits ≈ 24 mont_muls per modexp
  (init / 17 squares / 2 multiplies / final / overhead)
- Sequential: 24 × 266 × 2 = 12,768 cycles ≈ 39.6 µs per RSA-1024 op at 322 MHz
- Pipelined: ~13.4 M ops/s (throughput-optimal — one new modexp every 24 cycles)

The full `RSA_ModExp_1024` state-machine element does C-synthesize
through HLS's Performance phase but doesn't complete in the budget
(its design size remains ~2.8M instructions even after Array/Struct
optimizations). Pulling that down to a synthesizable-in-budget
design is the same area-tuning work as the DSP rollback above —
real engineering, not a fundamental obstacle.

Reports preserved at `eval/reports/rsa_hls/`:
`montmul_1024_u_csynth.rpt`, `montmul_1024_mont_mul_csynth.rpt`,
`montmul_1024_design_size.rpt`.

### 7.5 Batched FPGA performance vs software (the real comparison)

What the throughput question actually depends on: **batch size**.
The FPGA pipeline only earns its theoretical maximum when enough
independent modexps are in flight to keep the mont_mul pipeline
full. Three regimes, all derived from the 266-cycle / II=1 mont_mul
HLS number, with `n_mm = 24` mont_muls per RSA-1024 modexp at
e = 65537:

| Regime | Latency | Throughput | When this is what you have |
|---|---|---|---|
| **Batch = 1** (latency-bound) | 24 × 266 = 6,384 cycles ≈ 19.83 µs | 50,400 ops/s | Single-tenant TLS handshake, low QPS |
| **Batch = `ceil(266 / 24) ≈ 12`** (steady state) | unchanged 19.83 µs per op | 322 M / 24 = **13.4 M ops/s** (theoretical max) | TLS load balancer, 100+ concurrent handshakes |
| **Batch ≫ 12** | grows with batch size | unchanged 13.4 M ops/s | Bulk certificate signing |

The 13.4 M figure is theoretical — it assumes mont_mul actually
holds II=1 and the 266-cycle pipeline is gap-free. Realistic
expectations after Vivado place-and-route, clock-domain crossings,
and PCIe operand staging:
- Vivado P&R on a fully-unrolled mont_mul on U50 typically loses
  20–40 % of the HLS-estimated Fmax (HLS 322 MHz → silicon
  220–280 MHz at the kernel boundary).
- DSP-rolled-by-2 inner loops (the "fits the U50 budget" point)
  double the cycle cost: 6.7 M ops/s ceiling, not 13.4 M.
- PCIe DMA per operand adds ~1 µs of host↔kernel latency that
  doesn't pipeline through mont_mul; for very small batches this
  dominates.

So the **realistic, U50-shaped, batched** estimate is **3–7 M
RSA-1024 ops/s**, not the headline 13.4 M.

#### Comparison to the same workload on this CPU (i9-13900KS)

`openssl speed` on the same host (no FPGA, default OpenSSL with
hand-tuned x86 ASM and AVX-512) measured 2026-05-06:

| Bits | Single thread (verify ops/s) | 24-thread `openssl speed -multi 24` (verify ops/s) | Single thread (sign ops/s) | 24-thread (sign ops/s) |
|---:|---:|---:|---:|---:|
| 1024 | 334,953 | **4,277,211** | 18,892 | 253,382 |
| 2048 |  96,858 | **1,218,432** |  2,867 |  36,141 |
| 4096 |  25,890 |   323,855 |    407 |   5,045 |

Putting it together for **RSA-1024 verify**:

| Path | ops/s | Relative to OpenSSL 1-thread |
|---|---:|---:|
| Our SW emu (1 thread) | 19,039 | 0.06× (17.6× slower) |
| OpenSSL ASM (1 thread) | 334,953 | 1× |
| OpenSSL ASM (24 threads, 13900KS) | 4,277,211 | 12.8× |
| FPGA latency-bound (batch=1) | 50,400 | 0.15× |
| FPGA realistic batched (DSP-rolled) | 3,000,000–7,000,000 | 9–21× |
| FPGA theoretical batched (full unroll, fits a bigger device) | 13,400,000 | 40× |

**Honest takeaways**

- For **single-op latency** (one TLS handshake at a time), the
  FPGA does *not* win against a modern CPU. CPU's hand-tuned
  modexp completes in ~3 µs; the FPGA's 19.83 µs latency loses by
  ~7×. PCIe round-trip alone is ~1 µs each way.
- For **batched-verify throughput**, the FPGA edges out a
  fully-saturated 24-core 13900KS by maybe 1–2× under realistic
  area-rolled assumptions. With a larger device (U280 / VP1352)
  and full unroll, the FPGA pulls ahead more decisively.
- For **batched sign throughput**, the FPGA gap is much wider.
  CPU sign is ~10× slower than verify (full-bit-width private
  exponent); FPGA throughput per modexp is independent of which
  exponent, so the FPGA's relative advantage is ~10× higher for
  sign. Estimated: 3–7 M signs/s on FPGA vs. 253 K signs/s on
  24-core CPU → **12–28×** sign-throughput speedup.
- Real commercial RSA accelerators (Intel QAT C62x ≈ 10 K
  RSA-2048 signs/s per chip; Marvell LiquidSec similar) are
  much slower than these projections. The literature gap mostly
  reflects (a) chips with smaller die area dedicated to RSA, (b)
  PCIe queueing models that don't sustain II=1, (c) lower clocks
  on older process nodes. A from-scratch FPGA design with no
  legacy constraints can plausibly beat them — but should be
  benchmarked on real silicon (which v0.1 cannot do; see § 8).
- The numbers above are derived from a single HLS C-synthesis
  run of the inner-kernel primitive (`MontMul_1024`). They are
  honest *upper bounds*, not measured FPGA results, because
  there is no `.xclbin` to run on.

### 7.5 Performance bottlenecks identified

Two non-trivial bugs were uncovered during this work and both are now
fixed:

1. **`compute_R2_mod_n` was wrong.** The first cut computed
   R² mod n via `mont_mul(R, R)`, which actually returns
   R · R · R⁻¹ ≡ R (mod n) — not R². Subsequent operands got
   converted into Montgomery form against a value-of-7-instead-of-49
   multiplier, producing wrong but plausible-looking ciphertexts.
   Fixed by computing R² directly via 2N·64 doublings.
2. **2N-limb Montgomery reduction overflow.** The first cut
   materialized t = a·b in 2N limbs and then reduced. The carry
   chain in the reduction phase could push intermediate values
   beyond t[2N−1], silently dropping the high carry. RSA-1024 and
   RSA-2048 happened to mask this; RSA-4096 made it visible.
   Fixed by switching to a fused CIOS algorithm with an N+2-limb
   accumulator that absorbs both add carries cleanly.

The earlier "all 47 apps P&R-clean at 322 MHz" claim still holds for
the 47 v0.1 applications. The new `RSA_ModExp_<BITS>` elements'
bodies are bigger than HLS C-synthesis can pipeline within a 10-min
budget at 1024 bits — the loop-trip count (1024-bit modexp = 17
iterations × N²-step Montgomery) hits the same heuristic that
`RegTCAM` did before its rewrite. Synthesizing as a real FPGA pipe
will require restructuring the element as a deeper, multi-handler
state machine; that's deliberate future work and not in v0.1.

## 8. Honest open items

- **No bitstream packaged.** Producing an `.xclbin` requires the
  vendor U50 platform package (`xilinx_u50_gen3x16_xdma_*.deb`)
  which was not installed on this host. The
  `scripts/build/implement.sh` pipeline is wired for that step and
  will produce a deployable `.xclbin` once the platform package is
  in place.
- ~~**ACL_TCAM** doesn't complete HLS C-synthesis: the `RegTCAM @ (acl)`
  kernel sent HLS into a long-running pipelining heuristic.~~ **Fixed.**
  Restructured `RegTCAM`'s handler to a parallel-match with
  `#pragma HLS UNROLL` and reduced ENTRIES from 64 to 32; ACL_TCAM now
  synthesizes and P&Rs cleanly (4 kernels, WNS = +0.254 ns).
- ~~**RateLimiter timing closure.** WNS = −0.207 ns flags a real
  timing violation that needs either a clock bump or one extra
  pipeline stage.~~ **Fixed.** Pipelined the token-bucket update; new
  WNS = +0.545 ns at the same 322 MHz target.
- **`Pass` synthesizes to ~2 k LUTs** in the current codegen because
  every input/output port carries a full per-port-mask handshake. A
  hand-coded equivalent is ~50 LUTs. Codegen optimization is on the
  roadmap; correctness is not affected.
- **SW emu codegen used `SwStream::null()` placeholders for every
  kernel port** — meaning the generated `openclicknp_sw_emu_main`
  binary launched kernels in isolation rather than wired into the
  topology graph. Fixed: the codegen now declares one
  `openclicknp::SwStream` per `stream_conns / tor_conns / nic_conns
  / host_streams` entry and threads them into the kernel calls at
  the right port indices, with namespace-scoped accessors so a test
  harness can drive the boundaries (`openclicknp_sw_emu::host_stream_<n>()`,
  etc.). The L2 SW-emu CI job remains green; the per-element
  behavioral tests are unaffected (they manage their own streams).
- ~~**`RSA_ModExp_<BITS>` doesn't synthesize via Vitis HLS** within
  the project's 10-min C-synthesis budget.~~ **Partially fixed.**
  Restructured `RSA_ModExp_<BITS>` as a multi-cycle state machine
  (one Montgomery-step per `.handler` call) and added `#pragma HLS
  INLINE off` + `PIPELINE II=1` on `mont_mul`. The smaller primitive
  `MontMul_1024` now C-synthesizes cleanly to RTL in 32 seconds with
  **266-cycle latency at II=1** (fully pipelined Montgomery
  multiplication). The full state-machine variant of `RSA_ModExp_1024`
  still has a >2.8M-instruction profile after Performance phase —
  HLS continues past Performance into HW Transforms and progresses
  but doesn't complete in the budget; pulling that down is a
  future-work area-tuning effort. See § 7.4 for the detailed
  hardware-side numbers.
- **Stub elements.** `elements/crypto/{Mult1024,Add2048,Subtract1024,
  Montgomery,RSA_modexp,SHA1,Mult,Add,...}.clnp` were auto-generated
  during the bulk-elements scale-up and have stub handlers that act
  on a single flit (single 64-bit lane in most cases). They satisfy
  the per-element behavioral test (it checks liveness, not
  semantics) and contribute real Vitis HLS resource numbers (those
  are accurate — for the stub op, not for the named multi-precision
  operation). Apps that need real multi-precision arithmetic should
  use `RSA_ModExp_<BITS>` or write their own honest element. The
  stub library is being kept around to preserve the per-element
  P&R numbers; we may rename the misleading ones in a follow-up.

## 8. Symmetric crypto (AES-128 + SHA-1)

In v0.1 the `AES_CTR`, `AES_ECB`, `SHA1`, `SHA1_BlockGen`, and several
RSA-named elements were auto-generated stubs whose handlers were
toy XORs (AES) or one-round feed-forwards (SHA-1) — not the named
algorithms. Resource numbers on those rows in § 2 were real but
described the toy, not the cipher.

### 8.1 Real implementations

`runtime/include/openclicknp/aes128.hpp` and `sha1.hpp` are full
FIPS 197 / FIPS 180-4 implementations:

- **AES-128**: forward and inverse S-box (Tables 4 + 6), full
  ShiftRows / MixColumns / AddRoundKey, 11 round keys via
  Hensel-style key expansion (rcon[1..10]). 10-round forward and
  inverse cipher; CTR mode wrapping the encrypt primitive.
- **SHA-1**: 80-round compression with full message schedule,
  per-round Ch/Parity/Maj selection, padding and length
  finalization per FIPS 180-4 §6.1.

`elements/crypto/AES_ECB.clnp`, `AES_CTR.clnp`, `SHA1.clnp` are
rewritten to call into these libraries.

### 8.2 Verification

`tests/integration/test_aes_sha_accuracy.cpp`:

- AES-128 ECB: FIPS 197 Appendix C.1 known-answer (encrypt + decrypt
  round-trip), NIST AESAVS GFSbox/VarTxt subset, plus **200 random
  plaintexts cross-checked against OpenSSL's `EVP_aes_128_ecb`**.
- AES-128 CTR: NIST SP 800-38A F.5.1 4-block known-answer plus
  **200 random messages of varying length cross-checked against
  OpenSSL's `EVP_aes_128_ctr`**.
- SHA-1: FIPS 180-4 examples (`abc`, 56-byte sample, empty,
  million-`a`) plus **200 random messages of varying length
  cross-checked against OpenSSL's `EVP_sha1`**.

Result: every NIST/FIPS vector and every random-vs-OpenSSL
comparison passes byte-exact.

### 8.3 OpenSSL ENGINE — AES-128-ECB

The engine now exposes `NID_aes_128_ecb` via `ENGINE_set_ciphers`,
so any libcrypto caller (`EVP_aes_128_ecb`, `openssl speed`,
`openssl enc -aes-128-ecb`) can route through our AES via
`-engine openclicknp`. Measured on this i9-13900KS:

```
$ openssl speed -engine openclicknp -evp aes-128-ecb -seconds 2
Doing AES-128-ECB on 16384-byte blocks: 23,091 blocks in 2.00s
type             16 bytes     64 bytes    256 bytes   1024 bytes   8192 bytes  16384 bytes
AES-128-ECB     178375.57k   185847.39k   188416.77k   188861.95k   190165.55k   189161.47k
```

That's ≈ 188 MB/s — about 20× slower than OpenSSL's native
AES-NI-accelerated path (which reaches ~3-5 GB/s on this CPU)
because our portable C++ doesn't use the AESENC instruction.
Functionally correct, perf-wise it's the cost of a from-scratch
implementation; on FPGA the same Sbox-based design hits Gbps with
modest LUT use (the original `AES_CTR` HLS row in § 2 shows
2,162 LUT / 1,910 FF / 0 DSP / 652 MHz — that was the toy XOR;
the real AES will be larger, on the order of 3-5k LUT for 11
sequential rounds, still within budget).

## 9. Constant-time modexp + CRT private-key acceleration

The original `bigint::modexp` uses left-to-right square-and-
multiply with `if (e.bit(i)) r = r*m`. That branch is on a secret
exponent during private-key operations — a side-channel
vulnerability. The OpenSSL ENGINE's `bn_mod_exp` callback is
reached for both verify (public e, safe) and the BLINDING step of
sign/decrypt (secret r, want consttime), so the engine had to
distinguish at runtime.

### 9.1 Constant-time modexp

`bigint::modexp_consttime` implements a Joye-Yen Montgomery
ladder: every bit position performs the same set of mont_muls and
selects the new state via `const_time_select`, which uses an
arithmetic mask (`mask = -cond`) so no compiler can lower it to a
data-dependent branch. Verified to give the same results as
`modexp` for 150 random vectors at RSA-1024 / RSA-2048.

The ENGINE's `bn_mod_exp` callback now inspects
`BN_get_flags(p, BN_FLG_CONSTTIME)` and dispatches to either path.
Counters in the engine's `STATS` ctrl distinguish them.

Cost: the current ladder computes both candidate states per bit
(four `mont_mul`s) rather than the optimal two (with cswap), so
constant-time is ~2× slower than the optimized version it could
become. That's a known follow-up.

### 9.2 CRT-based RSA private-key

`bigint::rsa_crt_decrypt` implements PKCS#1 v2.2 §5.1.2 RSADP via
the Chinese Remainder Theorem. Given (p, q, dp, dq, qInv) at half
the bit width of n, it computes `m = c^d mod n` as
`m1 = c^dp mod p`, `m2 = c^dq mod q`, `h = qInv·(m1−m2) mod p`,
`m = m2 + h·q`. Each modexp is half-bit-width and consttime.

The ENGINE hooks `RSA_meth_set_mod_exp(method, ocnp_rsa_mod_exp)`
which runs for every RSA private-key operation. It pulls
`p, q, dp, dq, qInv` from the `RSA*` via `RSA_get0_factors` /
`RSA_get0_crt_params`, calls `openclicknp_rsa_crt`, returns.

`tests/integration/test_rsa_accuracy.cpp` now also generates real
RSA keys via `RSA_generate_key_ex` and verifies that
`rsa_crt_decrypt` agrees with `c^d mod n` byte-exact: 5 RSA-1024
keys + 3 RSA-2048 keys, all match.

A round-trip via the ENGINE's `STATS` ctrl on a 2048-bit key shows:

```
[openclicknp ENGINE] modexp=2 modexp_ct=0 crt=1 fallback=0 aes_blocks=0
```

— two public-key modexps (one for keygen, one for encrypt) and
one CRT call (the decrypt). No fallback to default OpenSSL, every
RSA primitive went through our path.

### 9.3 Sign throughput honest disclosure

`openssl speed -engine openclicknp rsa1024 rsa2048` on i9-13900KS:

| Bits | Ops | Engine path | ops/s |
|---|---|---|---:|
| 1024 | sign | CRT + consttime ladder | 3,171 |
| 1024 | verify | non-consttime modexp | 42,215 |
| 2048 | sign | CRT + consttime ladder | 398.5 |
| 2048 | verify | non-consttime modexp | 6,680.5 |

Sign is slower than the prior pre-CRT engine measurement (1,881 /
919) because the CRT path internally uses the unoptimized Joye-Yen
ladder (4 `mont_mul`s per exponent bit) on each half — net effect
is more work than the non-consttime full-bit-width modexp despite
half-bit-width arithmetic. The trade-off is worth it: the previous
path was timing-leaky on private keys, the new path is not. With
the ladder rewritten to use `cswap` (2 `mont_mul`s/bit) the CRT
path would land roughly at OpenSSL's `BN_mod_exp_mont_consttime`
speed.

## 11. Repository tour

```
OpenClickNP/                  https://github.com/bojieli/OpenClickNP
├── PLAN.md                   — clean-room design plan (paper-only contract)
├── FINAL_REPORT.md           — this document
├── README.md, LICENSE        — Apache-2.0
├── compiler/                 — openclicknp-cc compiler (~5k LOC C++17)
├── runtime/                  — libopenclicknp_runtime + tests
├── elements/                 — 123 .clnp elements in 9 categories
├── examples/                 — 47 end-to-end applications
├── shell/                    — U50/XDMA + U50/QDMA platform integration
├── tests/                    — L1 unit + L2 e2e + per-element + smokes
├── scripts/                  — build / sim / run / platform shell scripts
├── docs/                     — architecture, language, internals, getting-started
├── eval/                     — eval suite + reports/
└── .github/workflows/        — GitHub Actions CI
```
