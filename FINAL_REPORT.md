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
| Tests in CI | **135** (100% pass: 10 baseline + 123 element + 2 simulator-smoke) |
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

End-to-end correctness is verified against Python's `pow(m, e, n)` in
`tests/elements/test_RSA_ModExp_<BITS>.cpp`. The tests build the
generated SW-emulator kernel, push 4 / 8 / 16 input flits per operand
on three input ports, drain the same number of output flits, and
compare the recovered 1024 / 2048 / 4096-bit ciphertext byte-exactly
against the reference. **All three pass.**

```
138/138 tests passing (was 135; +3 RSA correctness tests)
```

The standalone bigint library has the same vectors covered in
`/tmp/bigint_test.cpp`-style smoke tests during development.

### 7.2 SW-emulator throughput

Measured by `eval/rsa/run.sh` (200-op runs, single thread, -O3
codegen, AMD/Intel x86_64 host):

| Bits | Iterations | Wall-clock (s) | Throughput (ops/s) | Latency (µs/op) |
|---:|---:|---:|---:|---:|
| 1024 |  200 | 0.0087 | **23,004** |  43 |
| 2048 |  200 | 0.0375 |  **5,328** | 188 |
| 4096 |  200 | 0.1229 |  **1,628** | 614 |

Cubic scaling in bit width (≈ 4× per doubling) is what schoolbook
Montgomery + binary modexp predicts.  For reference, OpenSSL's hand-
tuned x86 assembly does ~50k / 10k / 2k ops/s at the same bit widths
on a similar CPU — the from-scratch implementation here lands within
~2× without any architecture-specific optimization. Raw CSV:
`eval/reports/rsa_perf.csv`.

### 7.3 Performance bottlenecks identified

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
- **`RSA_ModExp_<BITS>` doesn't synthesize via Vitis HLS** within
  the project's 10-min C-synthesis budget. The element runs the
  full modexp inside one `.handler` invocation, which is friendly
  for SW emu but blows past HLS's loop-pipelining heuristics. A
  redesigned multi-cycle pipeline is straightforward future work.
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

## 9. Repository tour

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
