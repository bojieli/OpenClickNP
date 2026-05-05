# OpenClickNP — Compiler internals

The compiler binary `openclicknp-cc` is a single-process C++17 tool
implemented as a static library plus a thin `main`. It deliberately
avoids LLVM, MLIR, ANTLR, and parser generators: the grammar is small
enough that a hand-written recursive-descent parser is the right size,
and gives the best diagnostics with the fewest dependencies.

## Pipeline

```
   .clnp source
       │
       ▼
   ┌─────────┐    Token stream (Tok + range)
   │ Lexer   │     hand-written, single pass; recognizes directive tokens
   │ (lex/)  │     (.element, .handler, ...) and emits OpaqueCpp blocks
   └────┬────┘     for the four element-body sections
        ▼
   ┌─────────┐    Surface AST  (ast::Module, ast::ElementDecl, ...)
   │ Parser  │     recursive-descent over the topology grammar
   │ (parse/)│     element bodies are passed through verbatim
   └────┬────┘
        ▼
   ┌────────────┐  Element-Graph IR  (eg::Graph)
   │ Resolver   │  inlines element-groups, auto-names anonymous instances,
   │ (ir/)      │  synthesizes pseudo-elements (tor_*, nic_*, host_*,
   └────┬───────┘  Drop, Idle), assigns signal GIDs and AXI-Lite bases
        ▼
   ┌────────────┐  Analyses
   │ Analyses   │   • port_arity   — every connected port exists
   │ (analyses/)│   • cycles       — Tarjan SCC, warn on dataflow cycles
   └────┬───────┘   • bandwidth    — boundary throughput sanity
        │           • autorun      — classifier for ap_ctrl_none vs s_axilite
        ▼
   ┌────────────┐  Backend IR  (be::Build)
   │ Lower      │   the build plan: kernels, stream connections, host
   │ (ir/lower) │   streams, signal table, platform pin
   └────┬───────┘
        ▼
   ┌─────────────────────────────────────────┐
   │  Backends (compiler/src/backends/)      │
   │   • hls_cpp/     Vitis HLS C++ + tb     │
   │   • systemc/    SystemC SC_MODULEs      │
   │   • sw_emu/     std::thread emulator    │
   │   • verilator_sim/  Verilator harness   │
   │   • vpp_link/   v++ connectivity.cfg    │
   │   • xrt_host/   per-element C++ classes │
   └─────────────────────────────────────────┘
```

## Why hand-written and three-IR?

**Hand-written** — better error messages than bison/yacc; no extra
languages contributors must learn; one language for the whole compiler;
single binary; small grammar (~30 productions) doesn't motivate
generation.

**Three IRs** — one shape per audience:
- *Surface AST* is for diagnostics (line/column/length on every node).
- *Element-Graph IR* is for analyses and is the **stable contract** for
  alternative front-ends (a future Python eDSL, for instance).
- *Backend IR* is for codegen and is the **stable contract** for
  alternative backends (today: HLS/SystemC/SW emu/Verilator/v++/XRT;
  future: an MLIR / CIRCT lowering).

## Element-body opacity

Inside `.state/.init/.handler/.signal`, the lexer recognizes the opening
`{`, then consumes the content as a single `OpaqueCpp` token by walking
balanced braces (with awareness of `// ... \n`, `/* ... */`, `"..."`,
and `'...'`). The token's text is stored verbatim and emitted into the
backend's generated source unchanged.

This is the key design choice that makes the compiler small **and**
makes user code source-compatible across HLS, SystemC, and SW-emu: each
backend wraps the same opaque block with a backend-specific prologue
and epilogue providing the built-ins (`input_ready`, `read_input_port`,
`set_output_port`, ...). The macros are defined in
`runtime/include/openclicknp/flit.hpp` and have identical semantics in
all three contexts.

## Diagnostics

Every error/warning carries a `SourceRange` (file_id + line/col + offset
+ end). The `DiagnosticEngine` buffers diagnostics until `render(os)` is
called, which prints:

```
PATH:LINE:COL: error: message
  source line text
            ^~~~~~
```

Each Diagnostic optionally has child notes for cross-references
("previously declared here").

## Adding a new backend

1. Add `compiler/src/backends/<name>/emit.cpp` defining
   `bool emit<Name>(const be::Build&, const std::string& outdir,
                    DiagnosticEngine&)`.
2. Declare it in `passes.hpp`.
3. Add a switch in `driver.cpp` (an `emit_<name>` flag in
   `DriverOptions` and a corresponding command-line flag in `main.cpp`).
4. Add the source path to `compiler/CMakeLists.txt`.

## Adding a new analysis

1. Add `compiler/src/analyses/<name>.cpp`.
2. Declare in `passes.hpp`.
3. Call from `driver.cpp` between the resolver and lowering steps.
4. Use `DiagnosticEngine::error/warn/note` with a `SourceRange` so
   diagnostics get pretty-printed automatically.
