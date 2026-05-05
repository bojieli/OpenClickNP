# OpenClickNP — Verification levels

Five concentric layers, slowest to fastest:

| Level | Tool | Catches | Time per run | Hardware needed |
| --- | --- | --- | --- | --- |
| **L1** Unit  | g++ + GoogleTest | logic bugs, state-machine mistakes | seconds | none |
| **L2** SW emu | g++ + std::thread | topology bugs, deadlock, backpressure | seconds | none |
| **L3** HLS cosim | Vitis HLS | pragma issues, II surprises, latency drift | minutes/kernel | none |
| **L4** Verilator | Verilator | RTL integration, cross-kernel CDC under load | tens of minutes | none |
| **L5** Real FPGA | XRT + Alveo U50 | timing closure, real Eth, real perf | minutes (after 4–8 h build) | U50 |

## L1 — Unit tests

`scripts/sim/run_unit.sh` builds and runs the per-component tests:

- compiler frontend: `tests/unit/test_lexer.cpp`,
  `test_parser.cpp`, `test_resolver.cpp`, `test_analyses.cpp`,
  `test_compiler_smoke.cpp`.
- runtime: `runtime/tests/test_runtime_basic.cpp`,
  `test_pcap_roundtrip.cpp`, `test_slot_bridge.cpp`.

These are pure software, deterministic, and run in CI on every push.

## L2 — Software emulator

`scripts/sim/run_emu.sh examples/<name>`:

1. Compiles `<name>/topology.clnp` with the SW-emu backend → produces
   `generated/sw_emu/topology.cpp`.
2. Compiles that with g++ + the runtime to a single binary
   `<name>_swemu`.
3. Runs the binary; if a PCAP path is given, replays it through
   `tor_in`/`nic_in`.

Each kernel runs as its own `std::thread`; channels are
`openclicknp::SwStream` SPSC FIFOs that share the same `read_nb` /
`write_nb` API as Vitis HLS streams. So **the same element body
compiles unchanged in both contexts.**

## L3 — Vitis HLS C/RTL cosimulation

`scripts/sim/run_cosim.sh examples/<name> [kernel]`:

For each kernel:
1. `vitis_hls -f <kernel>_cosim.tcl` runs `csynth_design` then
   `cosim_design -tool xsim`.
2. The same testbench used for L1 unit tests drives the synthesized RTL.
3. Pass/fail comes from cosim's exit code.

This catches the gap between "compiles with HLS" and "produces correct
RTL" — pragma errors, II≠1 surprises, off-by-one latency mismatches.

## L4 — Verilator full-system simulation

`scripts/sim/run_verilator.sh examples/<name>`:

1. Reuses the per-kernel Verilog produced by L3's `csynth_design`.
2. Builds the auto-generated `topology.v` wrapper that wires those
   kernels together with AXIS FIFOs.
3. Compiles + runs Verilator against `tb.cpp`, dumping `topology.fst`.

This is the **highest-fidelity test that runs without an FPGA** — it
catches integration-level deadlocks that show up only under realistic
backpressure plus the actual synthesized RTL.

## L5 — Real Alveo U50

```
scripts/build/synth_kernels.sh examples/<name>      # ~min/kernel
scripts/build/link.sh           examples/<name>     # 5–10 min, runs CDC #1
scripts/build/implement.sh      examples/<name>     # 4–8 h, runs CDC #2
scripts/run/program_fpga.sh     build/<name>/<name>.xclbin
scripts/run/run_example.sh      examples/<name>
scripts/run/perf_pps.sh                              # live counters
```

Cable check: `scripts/run/loopback_test.sh` for QSFP28-to-QSFP28
loopback validation.

## Cycle-accurate alternative — SystemC

`scripts/sim/run_systemc.sh` builds and runs the SystemC backend. It's
slower than SW-emu but cycle-accurate (each handler iteration takes one
simulated cycle), useful for FIFO-depth tuning and latency budgeting
without HLS synthesis.

## CI policy

Recommended GitHub Actions matrix:

- **every push**: L1 + L2 (no Xilinx tooling required).
- **every PR**: L4 (Verilator only, no Xilinx tooling required).
- **nightly**: L3 (Vitis HLS license required).
- **release tags**: full L5 with hardware-in-the-loop tests.
