# OpenClickNP — Documentation index

Pick the entry point that matches what you want to do.

## I want to use OpenClickNP

- [`getting_started.md`](getting_started.md) — install, build the
  toolchain, compile and run the first example, then build a real
  bitstream on Alveo U50.
- [`language.md`](language.md) — reference for the `.clnp` DSL:
  `.element`, `.state/.init/.handler/.signal`, topology connections,
  element groups, pseudo-elements (`tor_in`, `host_in`, ...).

## I want to understand how it works

- [`architecture.md`](architecture.md) — the layers (compiler, runtime,
  elements, shell, scripts) and how they fit together. Clocking and CDC
  policy. What the build outputs look like.
- [`compiler_internals.md`](compiler_internals.md) — pipeline from
  `.clnp` source to backend code: lexer → parser → resolver → analyses
  → backend IR → six backends (HLS C++, SystemC, SW emu, Verilator,
  v++ link, XRT host).
- [`verification_levels.md`](verification_levels.md) — the L1–L5 test
  pyramid: unit → SW emu → HLS cosim → Verilator → real FPGA, and the
  CI policy that uses them.

## I want the design rationale and measured results

- [`../PLAN.md`](../PLAN.md) — full design plan: target hardware,
  toolchain, IR contracts, backend split, clocking/CDC strategy,
  verification ladder, roadmap.
- [`../FINAL_REPORT.md`](../FINAL_REPORT.md) — measured numbers:
  per-element resource usage from Vitis HLS, per-app place-and-route
  results on the U50 die at 322 MHz, CDC report status, throughput
  and latency from the SW emulator.

## I want to extend OpenClickNP

- Adding a new element: write `elements/<category>/<Name>.clnp`, add a
  test in `tests/elements/test_<Name>.cpp`, register it in
  `tests/elements/CMakeLists.txt`. See `compiler_internals.md` for the
  body-opacity contract.
- Adding a new backend: see "Adding a new backend" in
  [`compiler_internals.md`](compiler_internals.md).
- Adding a new application: copy any directory under `examples/` and
  edit `topology.clnp`. The `PassTraffic` example is the minimum.
