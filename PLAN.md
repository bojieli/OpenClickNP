# OpenClickNP — Clean-Room Implementation Plan

## 0. Scope and clean-room boundary

OpenClickNP is a clean-room reimplementation of the system described in the paper:

> Bojie Li, Kun Tan, Layong (Larry) Luo, Yanqing Peng, Renqian Luo, Ningyi Xu,
> Yongqiang Xiong, Peng Cheng, Enhong Chen.
> **"ClickNP: Highly Flexible and High-performance Network Processing with
> Reconfigurable Hardware."** ACM SIGCOMM 2016.

The published paper is the **only** specification this implementation is allowed
to derive from. No part of this project is to be informed by, copied from, or
cross-checked against any prior implementation. The user-facing DSL preserves
the syntax forms the paper documents — element declarations, the topology
language, host-control annotations — because those are part of the paper's
public artifact. Implementation details (lexer architecture, IR shape,
codegen, runtime layout, build pipeline) are entirely original to this
project.

This document is the contract. If a feature is not motivated by the paper,
it does not belong in v1.

---

## 1. Goals

### 1.1 Functional goals (from the paper)

1. **Element model.** Each element has five components: states, ports, an
   initializer, a data handler, and a signal handler. The data handler reads
   one 64-byte message per input port per cycle and writes zero or one 64-byte
   message per output port per cycle.
2. **Two channel kinds.** "Flit" channels carry 32-byte packet payloads with
   `sop`/`eop` flags. "Metadata" channels carry user-defined parsed-header
   structures. The compiler is type-aware about the distinction.
3. **Topology language.** A network application is described as a directed
   graph of element instances connected by lossless (`->`) or lossy (`=>`)
   channels. Element groups are reusable subgraphs. Special pseudo-elements
   `tor_in`, `tor_out`, `nic_in`, `nic_out`, `host_in`, `host_out`, `Drop`,
   `begin`, `end` represent the boundary of the user graph.
4. **Host control.** An element annotated with `@` has its signal handler
   bound to a host-callable RPC. The compiler emits a per-element C++ class
   exposing `launch`, `signal`, `send`, `receive`, and `setCallback` methods.
5. **Compile-time elaboration.** A `.repeat(var, lo, hi){...}` directive
   generates `(hi - lo)` substituted copies of a code block, and `BREAK`
   exits the elaborated region. This is the paper's mechanism for
   parameterized depth/width without runtime loops.
6. **Performance target.** 100 Gbps line rate on the supported network
   ports at any packet size, with sub-microsecond latency for stateless
   elements. The original paper reports 40 Gbps; we lift this to 100 G
   because the chosen target hardware has native 100 G CMACs and the
   internal datapath naturally supports it.

### 1.2 Non-goals (v1)

- Multi-FPGA scale-out
- Dynamic kernel reconfiguration (partial reconfig)
- Replacement of the vendor HLS toolchain (deferred research direction)
- Windows host support (Linux only)

---

## 2. System architecture

```
┌───────────────────────────── Host (Linux x86_64) ──────────────────────────────┐
│                                                                                │
│   user app                                                                     │
│      │                                                                         │
│      ▼                                                                         │
│   ┌──────────────────────────┐                                                 │
│   │ libopenclicknp_runtime   │   per-element C++ classes (generated)           │
│   │  - Platform              │   - launch / signal / send / receive            │
│   │  - SignalDispatcher      │   - setCallback                                 │
│   │  - SlotBridge (XDMA|QDMA)│                                                 │
│   │  - Counters              │                                                 │
│   └────┬─────────────────────┘                                                 │
│        │ XRT native API (xrt::device, xrt::kernel, xrt::ip)                    │
└────────┼───────────────────────────────────────────────────────────────────────┘
         │ PCIe Gen3x16 (XDMA) or Gen3x16 (QDMA)
┌────────▼───────────────────── FPGA (Alveo U50) ────────────────────────────────┐
│                                                                                │
│   ┌──────────────────────┐                                                     │
│   │ XDMA / QDMA shell    │   AXI4-MM + AXIS H2C/C2H + AXI-Lite control         │
│   └──────┬───────────────┘                                                     │
│          │                                                                     │
│   ┌──────▼───────────────────────────────┐                                     │
│   │ openclicknp_top (user region @322MHz)│                                     │
│   │                                      │                                     │
│   │   slot_bridge ──┐  ┌── signal_disp.  │                                     │
│   │                 ▼  ▼                 │                                     │
│   │   nic_in ──► [Element graph] ──► tor_in                                    │
│   │                                      │                                     │
│   │   nic_out ◄── [Element graph] ◄── tor_out                                  │
│   │                                      │                                     │
│   │   AXI-MM ──► HBM (8 GB, 460 GB/s)    │                                     │
│   └─────────┬────────────────────────────┘                                     │
│             │                                                                  │
│   ┌─────────▼────────┐         ┌──────────────────┐                            │
│   │  CMAC 100G #0    │ ──────► │  QSFP28 cage A   │   (tor_in / tor_out)       │
│   ├──────────────────┤         ├──────────────────┤                            │
│   │  CMAC 100G #1    │ ──────► │  QSFP28 cage B   │   (nic_in / nic_out)       │
│   └──────────────────┘         └──────────────────┘                            │
└────────────────────────────────────────────────────────────────────────────────┘
```

### 2.1 Hardware target

- **Card:** AMD/Xilinx Alveo U50 (XCU50LP-FSVH2104).
- **Platforms supported:**
  - `xilinx_u50_gen3x16_xdma_*` (XDMA streaming + memory-mapped)
  - `xilinx_u50_gen3x16_qdma_*` (QDMA queue-based streaming)
- **Network:** 2 × QSFP28 in 100 GbE mode (one labeled "tor", the other
  "nic", matching the paper's nomenclature).
- **Memory:** 8 GB HBM2, exposed as 32 pseudo-channels via AXI smartconnect,
  primary store for global tables.
- **User clock:** 322.265625 MHz (= CMAC user clock). All user kernels run
  at this single clock to minimize CDC.

### 2.2 Software stack

| Component | Version (pinned) |
| --- | --- |
| Vitis / Vivado | 2025.2 |
| XRT | ≥ 2.16 |
| Open NIC Shell | git submodule, pinned commit |
| Verilator | ≥ 5.018 |
| GCC / Clang (host) | gcc-11+ or clang-14+ |
| C++ standard | C++17 |
| Build system | CMake 3.22+ |
| Unit-test framework | GoogleTest 1.14+ |
| OS | Ubuntu 22.04 LTS |

---

## 3. The DSL (the user-facing language)

### 3.1 File extension and structure

OpenClickNP source files use the `.clnp` extension. A file may contain:

- `import "path/to/lib.clnp";` — bring another file's element definitions
  and groups into scope.
- `.element Name <inports, outports> { ... }` — element definition.
- `.element_group Name { ... }` — reusable subgraph.
- Top-level connection statements forming the application graph.

### 3.2 Element definition

```
.element Name <num_in_ports, num_out_ports> {
    .state {
        // Persistent variables. Plain C++17 declarations.
        // Special form: constexpr NAME = expression;
        //               .repeat(idx, 0, N) { ... $idx ... }
    }
    .init {
        // Runs once at element launch. Plain C++17.
    }
    .handler {
        // Runs each iteration. Plain C++17.
        // Built-ins: input_ready(PORT_n), input_data[n], read_input_port(PORT_n),
        //            clear_input_ready(PORT_n), set_output_port(n, value),
        //            return PORT_n  -- which input ports to wait on next iter.
    }
    .signal {
        // Optional. Plain C++17.
        // Built-ins: event (incoming), outevent (response).
    }
}
```

The body inside each `.state/.init/.handler/.signal` block is **opaque
C++17**, passed through verbatim to the HLS backend with a fixed prologue
that supplies the built-ins above. The compiler does not parse user C++
code; it only frames it.

### 3.3 Topology language

```
// Element instance:
//   Type :: name [@] [&] (in_ports, out_ports [, params...])
//   @ enables the signal handler (host control)
//   & marks the element as autorun (no host launch parameters)

PushHeader :: encap (2, 2)
HashTable  :: ht @ (1, 1, 14)         // 14 is a passed parameter

// Connection (ranges and lossy/lossless):
//   nodeA[port] -> [port]nodeB    -- lossless, backpressure
//   nodeA[port] => [port]nodeB    -- lossy, drop on full

nic_in -> Parser(1,2) -> NVGRE_Encap(2,2) -> ratelimit[1] -> tor_in
ratelimit[2] -> Drop(1,0)
```

Anonymous instances `Type(in,out)` are auto-named.

`.element_group` blocks are inlined: each group instance becomes a fresh
copy of the inner graph with `begin`/`end` rewired to the group's outer
ports.

### 3.4 Code-generation directives

- `constexpr NAME = expression;` inside `.state` — compile-time constant
  visible to `.repeat`, array sizes, and the C++ body.
- `.repeat(var, lo, hi) { ... $var ... }` — generates `(hi - lo)` copies
  with `$var` textually substituted with the loop value. Allowed inside
  `.state`, `.init`, `.handler`, and `.signal`.
- `BREAK` — inside a `.repeat` body, jumps out of the elaborated region
  via a generated label. Outside `.repeat`, illegal.

### 3.5 Special pseudo-elements

| Name | Direction | Maps to |
| --- | --- | --- |
| `tor_in`  | input  | CMAC #0 RX path (one QSFP28) |
| `tor_out` | output | CMAC #0 TX path |
| `nic_in`  | input  | CMAC #1 RX path |
| `nic_out` | output | CMAC #1 TX path |
| `host_in`  | input  | host streaming → kernel |
| `host_out` | output | kernel → host streaming |
| `Drop`     | terminal sink | discard |
| `Idle`     | terminal source | never produce |
| `begin` / `end` | inside `.element_group` | group I/O ports |

---

## 4. Compiler

### 4.1 Architecture

```
.clnp source ──► Lexer ──► Parser ──► Surface AST
                                          │
                                          ▼
                            Resolver + Template expansion
                                          │
                                          ▼
                            Element-Graph IR (typed)
                                          │
                                          ▼  Analyses:
                                          │  - cycle detection
                                          │  - port-arity check
                                          │  - bandwidth feasibility
                                          │  - autorun classification
                                          │  - signal-dispatch table
                                          │
                                          ▼
                            Backend IR (build plan)
                                          │
                ┌───────────┬─────────────┼─────────────┬──────────────┐
                │           │             │             │              │
                ▼           ▼             ▼             ▼              ▼
            HLS C++     SystemC      SW emulator   Verilator      v++ link cfg
            backend     backend      backend       harness        + XRT host
            (per-       (cycle-      (std::thread  (Verilator     stub
             kernel     accurate     per kernel)   testbench)
             .cpp)      sim)
```

- **Language:** C++17, no external parser generator, no LLVM/MLIR
  dependency. Single binary `openclicknp-cc`.
- **Reasoning:** the grammar is small (≈ 30 productions); recursive
  descent gives the best diagnostics; matches the FPGA research
  community's working language.

### 4.2 Compiler stages

#### Stage 1 — Lexer (`compiler/src/lex/`)

Hand-written, single-pass. Token kinds:

- Keywords: `import`, `.element`, `.element_group`, `.state`, `.init`,
  `.handler`, `.signal`, `.repeat`, `BREAK`, `constexpr`, `host`,
  `verilog`.
- Punctuation: `{ } [ ] ( ) < > , ; : :: -> => @ & * $`.
- Identifier, integer literal, string literal.
- Special: `OPAQUE_CPP_BLOCK` — the lexer recognizes the `{` after
  `.state`/`.init`/`.handler`/`.signal`, then consumes raw characters
  until the matching `}` (with brace-depth tracking and string-literal
  awareness), emitting one big token. The C++ inside is never lexed
  by us — it goes straight to Vitis HLS.

Source locations (file, line, column) carried on every token for
diagnostics.

#### Stage 2 — Parser (`compiler/src/parse/`)

Recursive descent into the surface AST defined in
`compiler/include/openclicknp/ast.hpp`. AST node kinds:

- `Module` (top-level)
- `ImportDecl`
- `ElementDecl` { name, n_in, n_out, state, init, handler, signal,
  parameters }
- `ElementGroupDecl` { name, body }
- `InstanceDecl` { type, name, host_ctrl, autorun, n_in, n_out,
  params, channel_depth }
- `Connection` { source, source_port_range, dest, dest_port_range,
  lossy }
- `RawCpp` (the opaque blocks)

Errors carry source locations and highlight the offending token plus
context.

#### Stage 3 — Resolver (`compiler/src/ir/resolver.cpp`)

- Resolve element-type references (forward and through imports).
- Inline `.element_group` instances (rename `begin`/`end` to the group's
  outer ports, fresh-name internal nodes).
- Anonymous instance auto-naming (`Type_anon_42` style).
- Build the global symbol table.
- Detect duplicate instance names, undefined types, port-count
  mismatches.

#### Stage 4 — Element-Graph IR (`compiler/include/openclicknp/eg_ir.hpp`)

```cpp
struct EGKernel {
    std::string name;        // instance name
    std::string type;        // element type
    int n_in_ports, n_out_ports;
    bool host_control;       // @ flag
    bool autorun;            // & flag
    std::vector<std::string> params;
    std::vector<std::string> raw_state_cpp;
    std::vector<std::string> raw_init_cpp;
    std::vector<std::string> raw_handler_cpp;
    std::vector<std::string> raw_signal_cpp;
    SourceRange src;
};

struct EGEdge {
    std::string src_kernel; int src_port;
    std::string dst_kernel; int dst_port;
    bool lossy;              // -> vs =>
    int  depth;              // FIFO depth (default 64)
    ChannelKind kind;        // FLIT or METADATA
    SourceRange src;
};

struct EGGraph {
    std::vector<EGKernel> kernels;
    std::vector<EGEdge> edges;
    std::vector<EGSpecial> specials;  // tor_in/out, nic_in/out, host_in/out
};
```

#### Stage 5 — Static analyses (`compiler/src/analyses/`)

Each analysis is a function `(const EGGraph&) → DiagnosticReport`.

1. **Port-arity check.** Every port number used in a connection lies
   within `[1, n_ports]` for that kernel.
2. **Cycle detection (Tarjan SCC).** Pure-dataflow cycles are flagged
   as warnings (potential deadlock); cycles passing through a `@`
   element are allowed (signal-driven feedback).
3. **Bandwidth feasibility.** For each edge, compute
   `width_bits × clock_hz` and compare to declared platform headroom
   (e.g., 100 G CMAC on `tor_*`/`nic_*` boundary edges). Warn on
   over-subscription.
4. **Autorun classification.** A kernel is autorun if it has no `@`
   and is not on a host-mediated path; emit it with `ap_ctrl_none`.
5. **Signal-dispatch table.** Enumerate `@`-marked kernels, assign
   each a 16-bit GID, build the AXI-Lite address map.

#### Stage 6 — Backend IR (`compiler/include/openclicknp/be_ir.hpp`)

The build plan. Per-target slices:

```cpp
struct BEKernelHls {
    std::string top_name;
    std::string cpp_source_path;
    std::vector<std::string> pragmas;      // HLS pragma list
    std::vector<BEPort> in_ports, out_ports;
    bool autorun;
    int  axilite_base;                      // if has signal handler
};

struct BELink {
    std::string platform_name;             // u50_xdma or u50_qdma
    std::vector<BEStreamConn> stream_conns;// kernel↔kernel AXIS
    std::vector<BEHostConn> host_conns;    // kernel↔host streams
    std::vector<BEMemConn> mem_conns;      // kernel↔HBM banks
    std::map<std::string,int> clock_assignments;
};

struct BEHost {
    std::vector<BEHostKernel> kernels;     // for kernel_table.cpp
    BESignalMap signal_map;                // GID → AXI-Lite addr
    BESlotMap   slot_map;                  // slot_id → kernel/port
};
```

The Backend IR is the **stable contract** all codegen backends consume.
Adding a new backend is implementing one consumer of `BackendIR`.

### 4.3 Codegen backends

#### B1. Vitis HLS C++ (`compiler/src/backends/hls_cpp/`)

Per-kernel C++ file pattern:

```cpp
#include "openclicknp/hls_runtime.hpp"

void NAME(
    hls::stream<flit_t> &in_1, hls::stream<flit_t> &in_2,
    hls::stream<flit_t> &out_1, hls::stream<flit_t> &out_2,
    /* signal AXI-Lite ports if @ */
    /* m_axi global memory if used */
) {
#pragma HLS INTERFACE axis port=in_1
#pragma HLS INTERFACE axis port=out_1
#pragma HLS INTERFACE ap_ctrl_none port=return     // if autorun
    static /* state vars */ ;
    /* init code on first iteration via static bool */
    while (1) {
#pragma HLS PIPELINE II=1
        /* prologue: read input_data, set _input_port mask */
        /* user .handler body */
        /* epilogue: write output_data on output_port mask */
    }
}
```

The HLS runtime header provides typed `flit_t`, `metadata_t`, `port_mask_t`,
and the `input_ready/read_input_port/set_output_port` macros — all
HLS-friendly (no recursion, no dynamic allocation).

#### B2. SystemC (`compiler/src/backends/systemc/`)

Each kernel becomes an `SC_MODULE` with `sc_fifo<flit_t>` input/output
ports and a `SC_THREAD` running the handler loop with `wait()` between
iterations. This gives **cycle-accurate** behavior — the handler body
is executed in 1 simulated cycle, FIFO read/write blocks/non-blocks
respect simulated time.

Use cases:
- Cycle-accurate latency measurements without HLS synthesis.
- FIFO-depth tuning without RTL synthesis.
- Educational / debugging — SystemC is single-threaded and easy to
  trace.

Output: per-design `topology_systemc.cpp` + a `Makefile.systemc` that
links against `libsystemc` and produces a binary
`<example>_systemc_sim` writing waveforms to `topology.vcd`.

#### B3. Software emulator (`compiler/src/backends/sw_emu/`)

Each kernel becomes an `std::thread`. Inter-kernel channels are bounded
SPSC FIFOs (`openclicknp::SpscQueue<flit_t>`). Handler bodies run as
plain C++17, with the same built-ins shimmed via header. No simulated
time; runs as fast as the host CPU permits.

Use cases:
- L2 verification — full-graph functional behavior.
- CI on every push (no FPGA, no SystemC, no Verilator needed).
- Bug bisection.

Output: per-design `topology_sw.cpp` linked into `<example>_sw_sim`.

#### B4. Verilator harness (`compiler/src/backends/verilator_sim/`)

Generates a C++ testbench that drives the *RTL* version of the user
graph. Workflow:

1. We have already run Vitis HLS (B1) per kernel → produces
   `<kernel>.v` (the synthesized RTL).
2. Verilator backend emits a top-level Verilog wrapper that
   instantiates each kernel module, wires their AXIS interfaces with
   `axis_data_fifo` IP-equivalent FIFOs, and exposes the boundary
   ports (`tor_*`, `nic_*`, `host_*`).
3. A C++ Verilator testbench (`tb.cpp`) drives the boundary ports
   from PCAPs, checks output against goldens.
4. CMake target `verilator_<example>` runs `verilator --binary` and
   then the resulting binary, dumping `topology.fst`.

#### B5. v++ link config (`compiler/src/backends/vpp_link/`)

Emits `link/connectivity.cfg`, `link/clocks.cfg`, `link/slr.cfg`:

```
# connectivity.cfg
[connectivity]
nk=Pass:1:tor_rx_cnt
nk=Counter:1:nic_rx_cnt
stream_connect=tor_rx_cnt.out_1:nic_rx_cnt.in_1:64
slr=tor_rx_cnt:SLR0

# clocks.cfg
[clock]
freqHz=322265625:tor_rx_cnt.ap_clk
freqHz=322265625:nic_rx_cnt.ap_clk
```

#### B6. XRT host stub (`compiler/src/backends/xrt_host/`)

Emits `generated/host/kernel_table.cpp` and one
`<KernelName>.hpp` per element type. Each kernel header defines:

```cpp
class HashTable : public openclicknp::Element {
public:
    HashTable(openclicknp::Platform&, std::string name);
    void launch();
    SignalResponse signal(const SignalRequest&);
    void send(const HostMessage&);
    bool receive(HostMessage&, bool blocking = true);
    void setCallback(std::function<void(const HostMessage&)>);

    // Element-specific RPCs derived from .signal arg list:
    void addRule(uint8_t depth, uint32_t pos, uint32_t value,
                 uint32_t left, uint32_t right);
    /* ... */
};
```

The element-specific RPC methods are derived from the `.signal` arg list
documented in the paper:
`.signal (uchar cmd, uchar depth, uint pos, uint value, uint left, uint right)`.
The compiler parses the parameter list and produces typed C++ wrappers.

### 4.4 Diagnostics

Every error carries:
- A primary source location (file, line, column, length)
- A message
- An optional secondary location ("note: previously declared here")
- A pretty-printed source-line excerpt with caret

Implemented via `compiler/include/openclicknp/diagnostic.hpp`.

---

## 5. Runtime library (`runtime/`)

A single shared library `libopenclicknp_runtime.so`, built with CMake,
links against XRT.

### 5.1 Public API

```cpp
namespace openclicknp {

class Platform {
public:
    Platform(const std::string& xclbin_path,
             const std::string& bdf = "");        // PCIe BDF, auto if empty
    ~Platform();

    void launchAll();
    void shutdownAll();

    // Counters mirroring the paper's hardware counters
    uint64_t cycleCounter() const;
    NetworkCounters networkCounters() const;
};

class Element {
public:
    virtual void launch() = 0;
    virtual SignalResponse signal(const SignalRequest&) = 0;
    virtual void send(const HostMessage&) = 0;
    virtual bool receive(HostMessage&, bool blocking) = 0;
    virtual void setCallback(std::function<void(const HostMessage&)>) = 0;
};

}  // namespace openclicknp
```

### 5.2 Slot bridge

Two implementations selected at build time by the chosen platform:

#### XDMA slot bridge (`runtime/src/slot_bridge_xdma.cpp`)

- Multiplexes `N` virtual ClickNP streaming "slots" onto 4 physical
  XDMA H2C and 4 C2H AXIS channels.
- Each flit on a virtual slot carries a 16-bit `slot_id` in the
  payload header; FPGA-side `slot_demux` IP fans out by `slot_id`,
  host-side software does the reverse.
- Up to 31 virtual slots supported (matches the paper's slot count).

#### QDMA slot bridge (`runtime/src/slot_bridge_qdma.cpp`)

- Each ClickNP slot becomes a real QDMA streaming queue.
- No software multiplexing; one queue per slot.
- Better latency, better isolation, but requires the QDMA shell.

The compiler selects the right header at host-stub generation time
based on the `--platform` flag.

### 5.3 Signal dispatcher (`runtime/src/signal_rpc.cpp`)

For each `@`-marked element, the compiler assigns:
- A 16-bit GID (0..65535)
- An AXI-Lite slave register block (16 bytes per kernel)

The `Platform::signal(GID, request)` method writes the request bytes
into the kernel's slave registers, raises a `signal_pending` bit, and
polls until the kernel clears it (writing back the response).

### 5.4 Counters

Three small AXI-Lite registers exposed by `openclicknp_top.v`:

| Address | Name | Description |
| --- | --- | --- |
| 0x00 | `cycle_lo` | Free-running cycle counter, low 32 bits |
| 0x04 | `cycle_hi` | Free-running cycle counter, high 32 bits |
| 0x08 | `tor_rx_cnt` | Packets received on tor side |
| 0x0C | `tor_tx_cnt` | Packets transmitted on tor side |
| 0x10 | `nic_rx_cnt` | Packets received on nic side |
| 0x14 | `nic_tx_cnt` | Packets transmitted on nic side |
| 0x18 | `tor_link_up` | Link-state bitmap |
| 0x1C | `nic_link_up` | Link-state bitmap |

---

## 6. Standard element library (`elements/`)

All written from the paper's textual descriptions; **no reference is
made to any prior implementation**. Phase 1 set:

| Element | Description (from paper) | Phase |
| --- | --- | --- |
| `Pass` | Identity. One in, one out, copies through. | 1 |
| `Tee` | Duplicate input to two outputs. One in, two out. | 1 |
| `Mux` | N-to-1, round-robin or strict-priority. | 1 |
| `Demux` | 1-to-N, round-robin. | 1 |
| `Counter` | Count flits and packets, expose via signal. | 1 |
| `Idle` | Never produces. | 1 |
| `Drop` | Discards everything. | 1 |
| `IP_Lookup` | Binary-tree IP classifier (the paper's worked example). | 2 |
| `HashTable` | Generic hashtable, signal-controlled rule update. | 2 |
| `FlowParser` | 5-tuple extractor producing metadata. | 2 |
| `IP_Checksum` | Compute and emit IPv4 checksum action. | 2 |
| `PacketModifier` | Apply checksum/header-modification actions. | 2 |
| `RateLimiter` | Token-bucket policer. | 3 |
| `NVGRE_Encap` | Element group from the paper's Fig. application. | 3 |

Phase 1 elements are implemented in v1 of OpenClickNP.

---

## 7. Verification levels

### 7.1 L1 — Pure software unit tests

- Each element is compiled with the SW emulator backend's stream shim.
- One GoogleTest per element, replays canned inputs, checks outputs.
- Runtime: seconds. Run on every commit.

### 7.2 L2 — Software emulator (whole topology)

- Compile each example with `--backend=sw_emu`, get
  `<example>_sw_sim`.
- Replay PCAPs through `tor_in` / `nic_in`, capture outputs at
  `tor_out` / `nic_out`.
- Reference: a Python golden model in `tests/golden/<example>.py`.
- Runtime: seconds. Run on every commit.

### 7.3 L3 — Vitis HLS C/RTL co-simulation

- Per kernel: `vitis_hls -f compiler/scripts/cosim.tcl <kernel>`.
- Reuses the L1 testbench against the synthesized RTL.
- Catches HLS-pragma issues, II surprises, latency miscounts.
- Runtime: minutes per kernel. Run nightly in CI.

### 7.4 L4 — Verilator full-system simulation

- Compile all kernels via Vitis HLS, get one `.v` per kernel.
- Compiler emits a top-level Verilog wrapper that wires kernels with
  AXIS FIFOs and exposes boundary ports.
- Verilator testbench replays PCAPs, dumps `.fst` waveforms.
- Catches integration-level bugs at RTL fidelity, no FPGA required.
- Runtime: tens of minutes. Run on every PR.

### 7.5 L5 — Real Alveo U50 hardware

- Build full bitstream (`.xclbin`).
- `xbutil program` the U50 with the bitstream.
- Run example host binary.
- Measure pps, Gbps, latency via XRT counters and host-side packet
  capture.
- Cable test: QSFP28 loopback (port A → port B).
- Runtime: 4–8 h build, minutes to run. Run on release tags.

---

## 8. Build pipeline

```
.clnp source
    │
    │ openclicknp-cc --backend=hls_cpp,systemc,sw_emu,
    │                  verilator_sim,vpp_link,xrt_host
    ▼
generated/
    ├── kernels/<name>.cpp              (HLS C++)
    ├── kernels/<name>_tb.cpp           (cosim testbench)
    ├── systemc/topology.cpp            (SystemC)
    ├── sw_emu/topology.cpp             (SW emulator)
    ├── verilator/topology.v + tb.cpp   (Verilator harness)
    ├── link/connectivity.cfg           (v++ link)
    ├── link/clocks.cfg
    └── host/kernel_table.cpp + Element subclass headers
    │
    │ scripts/build/synth_kernels.sh   (parallel vitis_hls)
    ▼
build/kernels/<name>.xo
    │
    │ scripts/build/link.sh    (v++ -l)
    │   • CDC CHECK #1: report_cdc on linked design
    │   • TIMING CHECK #1: warn on negative slack
    ▼
build/<example>.xclbin (linked, pre-impl)
    │
    │ scripts/build/implement.sh  (Vivado synth + impl + bit)
    │   • CDC CHECK #2: report_cdc -severity error → fail on violation
    │   • TIMING CHECK #2: WNS ≥ 0, WHS ≥ 0 → fail otherwise
    ▼
build/<example>.xclbin (deployable)
```

### 8.1 CDC policy

- All user kernels run at one clock (322.265625 MHz). No CDC
  internally.
- Three known cross-domain interfaces, all in vendor IP:
  - CMAC ↔ user clock (handled by CMAC IP)
  - HBM ↔ user clock (handled by AXI smartconnect)
  - PCIe/XDMA/QDMA ↔ user clock (handled by shell)
- After place_design, run `report_cdc -severity error`. Any reported
  violation outside vendor-IP boundaries fails the build.
- A documented `cdc_waivers.xdc` lists every intentional async path
  (must be paired with `set_clock_groups -asynchronous`).

### 8.2 Timing policy

- Target: WNS ≥ 0 ns, WHS ≥ 0 ns at 322.265625 MHz.
- Build script auto-retries impl with
  `-strategy Performance_ExplorePostRoutePhysOpt` on first failure.
- Second failure surfaces the failing path and exits non-zero.

---

## 9. Repository layout

```
OpenClickNP/
├── compiler/             — openclicknp-cc compiler
├── runtime/              — libopenclicknp_runtime.so
├── elements/             — standard element library (.clnp)
├── shell/                — U50 platform integration
│   ├── u50_xdma/         — XDMA shell + ONS glue
│   └── u50_qdma/         — QDMA shell + ONS glue
├── tests/                — L1/L2 tests, goldens, PCAPs
├── examples/             — PassTraffic, Firewall, L4LoadBalancer
├── scripts/              — build/run/sim/platform shell scripts
├── docs/                 — architecture, compiler internals, spec
├── ci/                   — GitHub Actions / Jenkins configs
├── third_party/          — git submodules (Open NIC Shell, etc.)
├── PLAN.md               — this document
├── README.md
├── LICENSE               — Apache-2.0
└── CMakeLists.txt
```

---

## 10. Phased delivery

| Phase | Deliverable |
| --- | --- |
| 0 | Repo skeleton, CMake, top-level docs, license |
| 1 | Compiler frontend: lexer, parser, AST, diagnostics |
| 2 | Compiler middle: resolver, EG-IR, analyses |
| 3 | Compiler back: HLS C++, SystemC, SW emulator backends |
| 4 | Runtime library v1: Platform, slot bridge (XDMA), signal dispatcher |
| 5 | Runtime v2: QDMA slot bridge, counters, callbacks |
| 6 | Standard library: Pass, Tee, Mux, Demux, Counter, Idle, Drop |
| 7 | L1 unit + L2 emulator tests, golden models |
| 8 | First example end-to-end in SW emulator: PassTraffic |
| 9 | Verilator backend + L4 system sim |
| 10 | Vitis HLS scripts, L3 cosim |
| 11 | v++ link, Vivado impl, CDC check pipeline |
| 12 | Real-FPGA scripts (`program`, `run`, `perf`) |
| 13 | Phase-2 elements: IP_Lookup, HashTable, FlowParser |
| 14 | Firewall + L4LoadBalancer examples |
| 15 | Documentation polish, public release prep |

This document defines what v1 is. Additions beyond this list are
explicitly out of scope until the v1 release ships.
