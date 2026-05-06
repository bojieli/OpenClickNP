# Getting started with OpenClickNP

This guide walks you from a fresh Ubuntu 22.04 install to a running
OpenClickNP example, software-only first, then on real Alveo U50 hardware.

## 1. Prerequisites

```bash
sudo apt update
sudo apt install -y build-essential cmake git
```

For the L3/L4 simulators (still software-only, but require additional
tooling):

```bash
sudo apt install -y verilator     # >=5.018; build from source if your
                                  # distro lags
# SystemC (optional)
# https://accellera.org/downloads/standards/systemc
```

For real-hardware flow add:

```bash
./scripts/platform/install_xrt.sh         # XRT >= 2.16
./scripts/platform/install_platform.sh    # U50 platform package
```

And install Vivado/Vitis 2025.2 from AMD (see their official installer).

## 2. Build the toolchain

```bash
git clone https://github.com/bojieli/OpenClickNP.git
cd OpenClickNP
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Expect ≈135 tests to pass: 5 compiler-frontend unit tests, 4 runtime
unit tests, 1 e2e integration test, 123 per-element behavioral tests,
and 2 simulator-smoke tests (SystemC, Verilator) that auto-enable when
those tools are installed. If any fail, it's a bug — please file an issue.

## 3. Compile your first design

```bash
./scripts/build/compile.sh examples/PassTraffic
```

This produces `build/PassTraffic/generated/` with kernel sources, link
configs, and host stubs.

## 4. Run in software (no FPGA)

```bash
./scripts/sim/run_emu.sh examples/PassTraffic
```

You should see the SW emulator binary launch, run for a moment, and
exit cleanly.

## 5. Build a real bitstream (Vitis required)

```bash
./scripts/build/synth_kernels.sh examples/PassTraffic    # ~minutes
./scripts/build/link.sh           examples/PassTraffic   # 5–10 min
./scripts/build/implement.sh      examples/PassTraffic   # 4–8 hours
```

Output: `build/PassTraffic/PassTraffic.xclbin`.

## 6. Run on real hardware

```bash
./scripts/run/program_fpga.sh build/PassTraffic/PassTraffic.xclbin
./scripts/run/run_example.sh  examples/PassTraffic
```

In another terminal, watch live throughput:

```bash
./scripts/run/perf_pps.sh
```

## 7. Write your own design

Pattern your `.clnp` after `examples/PassTraffic/topology.clnp`:

```
import "core/Pass.clnp";
import "core/Counter.clnp";

Pass    :: rx
Counter :: cnt @

tor_in -> rx -> cnt
```

Build it the same way:

```bash
./scripts/build/compile.sh path/to/your/design
./scripts/sim/run_emu.sh    path/to/your/design
```

## Where to go next

- [`language.md`](language.md) — the full DSL reference.
- [`architecture.md`](architecture.md) — how the pieces fit together.
- [`compiler_internals.md`](compiler_internals.md) — for compiler hackers.
- [`verification_levels.md`](verification_levels.md) — the test pyramid.
- [`PLAN.md`](../PLAN.md) — the design plan and roadmap.
