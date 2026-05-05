# OpenClickNP — Final Implementation & Evaluation Report

A clean-room reimplementation of the system described in the SIGCOMM 2016
ClickNP paper, derived only from the published paper and built from
scratch on the Xilinx 2025.2 toolchain targeting the Alveo U50 die
(`xcu50-fsvh2104-2-e`).

## 1. Scale of the result

| Metric | Value |
|---|---|
| Source files in repo | 305+ |
| C++ LOC (excl. generated) | 6,000+ |
| `.clnp` element files | **123** across 9 categories |
| End-to-end applications | **16** |
| Tests in CI | **10** (100% pass) |
| Elements with real Vitis HLS numbers | **123 / 123** |
| Applications fully P&R'd on real U50 die | **16 / 16** |
| Apps with WNS ≥ 0 at 322 MHz | **15 / 16** |
| Apps with WHS ≥ 0 (hold met) | **16 / 16** |
| Apps with CDC violations | **0 / 16** |

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
total wall-clock:  ≈ 30 minutes for all 16 apps on a 32-core host
flow:              vitis_hls -> Vivado synth -> opt -> place -> route
                   per-kernel out_of_context, with full report_cdc /
                   report_timing_summary / report_utilization
```

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
| **RateLimiter** | 2 | 3,719 | 7,570 | 0 | 2 | **−0.207** | +0.041 | 0 |
| RoCE_Gateway | 3 | 4,815 | 10,018 | 0 | 2 | +0.232 | +0.041 | 0 |
| VLAN_Bridge | 3 | 4,251 | 9,215 | 0 | 0 | +0.545 | +0.039 | 0 |

### Highlights

- **15 of 16 applications close timing positively** at the
  322.265625 MHz user clock, with WNS slack ranging from +0.072 ns
  (Firewall) up to +0.545 ns (PacketLogger / NVGRE_Decap / etc.).
- **All 16 close hold timing** (WHS ≥ 0).
- **No application has any critical or warning CDC violation** in
  Vivado's `report_cdc` output.
- **RateLimiter shows WNS = −0.207 ns** — a real timing violation
  surfaced by the P&R flow. Either bump the clock period to ~3.3 ns
  (≈ 303 MHz) or split the token-bucket update into two pipeline
  stages.
- The largest application (L4LoadBalancer at 9,788 LUTs, 18,293 FFs)
  uses **about 1.1% of the U50's 871 k CLB LUTs** and **1.0% of its
  1.74 M registers** — comfortably small.
- Biggest BRAM user: **IP_Forwarding** (49 BRAMs, driven by the
  32-level binary-tree LPM table). Next: PFabric (33 BRAMs, priority
  queue).

## 4. CDC analysis

`report_cdc` was run on every synthesized kernel. Result summary:

```
Apps:              16
Kernels analyzed:  ~50 (per-app kernel counts: 2..5)
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
   own AXI-Lite register. After this fix, **all 16 applications
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

# L3: per-element Vitis HLS resource sweep (~30 min for 123 elements)
source /home/ubuntu/Xilinx/2025.2/Vivado/settings64.sh
./eval/resource_usage/run.sh
./eval/throughput/run.sh
./eval/latency/run.sh

# L5: per-application Vivado P&R on real U50 die (~30 min for 16 apps
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

## 7. Honest open items

- **No bitstream packaged.** Producing an `.xclbin` requires the
  vendor U50 platform package (`xilinx_u50_gen3x16_xdma_*.deb`)
  which was not installed on this host. The
  `scripts/build/implement.sh` pipeline is wired for that step and
  will produce a deployable `.xclbin` once the platform package is
  in place.
- **RateLimiter timing closure.** WNS = −0.207 ns flags a real timing
  violation that needs either a clock bump or one extra pipeline
  stage.
- **`Pass` synthesizes to ~2 k LUTs** in the current codegen because
  every input/output port carries a full per-port-mask handshake. A
  hand-coded equivalent is ~50 LUTs. Codegen optimization is on the
  roadmap; correctness is not affected.

## 8. Repository tour

```
OpenClickNP/                  https://github.com/bojieli/OpenClickNP (private)
├── PLAN.md                   — clean-room design plan (paper-only contract)
├── FINAL_REPORT.md           — this document
├── README.md, LICENSE        — Apache-2.0
├── compiler/                 — openclicknp-cc compiler (~5k LOC C++17)
├── runtime/                  — libopenclicknp_runtime + tests
├── elements/                 — 123 .clnp elements in 9 categories
├── examples/                 — 16 end-to-end applications
├── shell/                    — U50/XDMA + U50/QDMA platform integration
├── tests/                    — L1 unit + L2 e2e + integration
├── scripts/                  — build / sim / run / platform shell scripts
├── docs/                     — architecture, language, internals, getting-started
├── eval/                     — eval suite + reports/
└── ci/                       — GitHub Actions config
```
