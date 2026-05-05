# OpenClickNP

A clean-room, open-source implementation of the system described in:

> Bojie Li, Kun Tan, Layong (Larry) Luo, Yanqing Peng, Renqian Luo,
> Ningyi Xu, Yongqiang Xiong, Peng Cheng, Enhong Chen.
> **"ClickNP: Highly Flexible and High-performance Network Processing
> with Reconfigurable Hardware."** ACM SIGCOMM 2016.

OpenClickNP is a high-level FPGA programming framework for network
functions. You write a network application as a graph of communicating
elements in a small DSL; OpenClickNP compiles it to AMD/Xilinx Alveo
U50 hardware, with optional software, SystemC, and Verilator
simulation backends.

This project is a clean-room reimplementation: every line of source is
written from the published paper as the only specification. It is
licensed under Apache-2.0.

## Status

v0.1 — under active development. See `PLAN.md` for the full design.

## Quick start

### Prerequisites

- Ubuntu 22.04 LTS
- CMake ≥ 3.22
- gcc-11+ or clang-14+
- (Optional, for L3/L4/L5) AMD Vivado/Vitis 2025.2, XRT ≥ 2.16,
  Verilator ≥ 5.018, SystemC ≥ 2.3.4

### Build the compiler and runtime

```bash
git clone https://example.invalid/OpenClickNP.git
cd OpenClickNP
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build
```

This builds `openclicknp-cc`, the runtime library, and runs the L1 unit
+ L2 emulator tests.

### Compile and simulate an example (no FPGA needed)

```bash
./scripts/sim/run_emu.sh examples/PassTraffic
```

### Build a real bitstream (Vitis 2025.2 + Alveo U50 required)

```bash
./scripts/build/compile.sh   examples/PassTraffic --platform u50_xdma
./scripts/build/synth_kernels.sh examples/PassTraffic
./scripts/build/link.sh      examples/PassTraffic
./scripts/build/implement.sh examples/PassTraffic
```

Output: `build/PassTraffic/PassTraffic.xclbin`.

### Run on a real Alveo U50

```bash
./scripts/run/program_fpga.sh build/PassTraffic/PassTraffic.xclbin
./scripts/run/run_example.sh examples/PassTraffic
```

## Repository tour

```
compiler/    — openclicknp-cc, the .clnp DSL → multi-target compiler
runtime/     — libopenclicknp_runtime, host-side library
elements/    — standard element library (Pass, Tee, Counter, ...)
shell/       — Vivado/Vitis platform integration for U50 (XDMA + QDMA)
tests/       — L1 unit, L2 emulator, golden references, PCAPs
examples/    — end-to-end demo applications
scripts/     — build / run / sim / platform-install scripts
docs/        — architecture, compiler internals, language reference
```

See [`PLAN.md`](PLAN.md) for the full design plan.

## License

Apache-2.0. See [LICENSE](LICENSE).

## Citation

If you use OpenClickNP in academic work, please cite both the original
ClickNP paper (linked above) and this implementation.
