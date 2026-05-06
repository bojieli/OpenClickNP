# Contributing to OpenClickNP

Thanks for your interest in OpenClickNP. This document covers the
basics: what to install, how to build and test, and the conventions
contributions should follow.

## Reporting bugs and asking questions

- **Bug reports & feature requests** — open an issue at
  <https://github.com/bojieli/OpenClickNP/issues>. Please include the
  OpenClickNP commit you're on (`git rev-parse HEAD`), the OS / compiler
  / Vitis version if relevant, and the smallest `.clnp` topology that
  reproduces the problem.
- **General questions** — open a GitHub Discussion rather than an issue.

## Development setup

OpenClickNP builds with no external dependencies for L1/L2 work
(unit tests + software emulator). L3–L5 work needs AMD/Xilinx tools.

```bash
# Required
sudo apt-get install -y build-essential cmake ninja-build

# Optional, enables auto-discovered SystemC + Verilator smoke tests
sudo apt-get install -y verilator libsystemc-dev

# Build the compiler + runtime + tests
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Run the full test suite (~135 tests, ~20 s)
ctest --test-dir build --output-on-failure
```

For L3 (Vitis HLS cosim), L4 (full Verilator sim of HLS RTL), and L5
(real Alveo U50), see [`docs/verification_levels.md`](docs/verification_levels.md).

## Pull-request workflow

1. Fork the repo and create a topic branch from `main`.
2. Make your change. Keep commits focused — one logical change per
   commit.
3. Run `ctest --test-dir build --output-on-failure` and make sure all
   tests pass before pushing.
4. Open a PR against `main`. CI runs automatically on every PR; it
   must be green before merge.
5. Be prepared to rebase if `main` has moved.

## What kind of contributions we want

- **New elements** — anything from the original ClickNP paper that
  isn't yet implemented, or new building blocks useful to multiple
  applications. See [Adding an element](#adding-an-element).
- **New end-to-end applications** — under `examples/`. The goal is
  realistic NF demos that exercise multiple elements together.
- **Codegen optimizations** — see the open items in
  [`FINAL_REPORT.md`](FINAL_REPORT.md). The biggest one is shrinking
  the per-port handshake overhead so `Pass`-class elements synthesize
  to dozens of LUTs instead of thousands.
- **Backend extensions** — additional simulator targets or platforms
  (e.g., U200, U250, U280, V80, VPK series). See
  [Adding a backend](#adding-a-backend).
- **Documentation improvements** — corrections, clarifications,
  worked examples.

## What kind of contributions we don't want

- Code copied or paraphrased from the original (proprietary) ClickNP
  Microsoft Research codebase. OpenClickNP is a clean-room
  reimplementation derived only from the published SIGCOMM 2016 paper
  and the public ClickNP-related literature. If you have read the
  original source, please don't contribute to this project.
- Vendor-specific glue that locks the design to a non-Xilinx FPGA
  family without a clear path to portability.
- Backwards-compatibility shims for unreleased APIs, dead-code feature
  flags, or speculative abstractions for hypothetical future needs.

## Coding conventions

- **Language** — C++17 throughout, no LLVM/MLIR/ANTLR dependencies.
- **License headers** — every source file starts with
  `// SPDX-License-Identifier: Apache-2.0` (or `# ...` for shell/CMake).
- **Style** — match the surrounding code. We don't use a formatter; the
  codebase is small enough for human review. Names use `snake_case`
  for variables/functions and `CamelCase` for types. Indent with four
  spaces.
- **Comments** — explain *why*, not *what*. The code should be readable
  on its own; reserve comments for hidden invariants, workarounds for
  specific tool bugs, and surprising design choices. No multi-paragraph
  docstrings.
- **Tests** — every new element gets a behavioral unit test under
  `tests/elements/`; every new compiler feature gets a unit test under
  `tests/unit/`. PRs without tests will not be merged.

## Adding an element

1. Write `elements/<category>/<Name>.clnp` with `.element/.state/.init/
   .handler[/.signal]` blocks. See `elements/core/Pass.clnp` for the
   smallest meaningful example.
2. Write `tests/elements/test_<Name>.cpp` that drives the
   `extern "C" void kernel_<Name>(...)` symbol the SW-emu backend
   exports. See `tests/elements/test_Pass.cpp` for the pattern.
3. Register the test in `tests/elements/CMakeLists.txt` with
   `openclicknp_element_test(<Name> <category>)`.
4. Run `ctest -R element_<Name>` and confirm it passes.
5. (Optional but encouraged) Add the element to one of the existing
   `examples/<App>/topology.clnp` files so it gets exercised in the
   end-to-end SW-emu CI job, OR add a new `examples/<App>/` that
   demonstrates a realistic use of the element.

## Adding a backend

See "Adding a new backend" in
[`docs/compiler_internals.md`](docs/compiler_internals.md). In short:

1. Add `compiler/src/backends/<name>/emit.cpp` defining
   `bool emit<Name>(const be::Build&, const std::string& outdir,
                    DiagnosticEngine&)`.
2. Declare it in `compiler/include/openclicknp/passes.hpp`.
3. Wire a `--no-<name>` / `--<name>-only` CLI flag in
   `compiler/src/driver.cpp` and `compiler/src/main.cpp`.
4. Add the source file to `compiler/CMakeLists.txt`.
5. Add an integration smoke test under `tests/integration/` mirroring
   `test_systemc_smoke.cpp` and `test_verilator_smoke.cpp`.

## License

OpenClickNP is licensed under the Apache License 2.0 — see
[`LICENSE`](LICENSE). By submitting a contribution, you certify that:

- You have the right to submit the work under the project's license.
- You agree that your contribution will be distributed under the same
  license.
- The contribution does not derive from material covered by an
  incompatible license, and in particular does not derive from the
  proprietary ClickNP source code.

We don't require a separate CLA. Standard GitHub PR submission is
sufficient.
