# OpenClickNP — Architecture

## What this document covers

A high-level walk-through of the system pieces: how a `.clnp` source file
becomes a running design (or a simulation), what each layer does, and how
they fit together. For the language reference see
[`language.md`](language.md); for compiler internals see
[`compiler_internals.md`](compiler_internals.md); for the verification
hierarchy see [`verification_levels.md`](verification_levels.md).

## The picture

```
┌────────────── front-end ──────────────┐  ┌────────── back-ends ──────────┐
│                                       │  │                               │
│   .clnp source                        │  │   HLS C++  ─→ vitis_hls       │
│       │                               │  │   SystemC  ─→ g+++libsystemc  │
│       ▼                               │  │   SW emu   ─→ g++ + threads   │
│   lexer → parser → surface AST        │  │   v++ link ─→ connectivity.cfg│
│       │                               │  │   XRT host ─→ kernel_table.cpp│
│       ▼                               │  │   Verilator harness           │
│   resolver + group inlining           │  └───────────────────────────────┘
│       │
│       ▼                                            ┌── runtime ──┐
│   Element-Graph IR  ─── analyses    ◀──── EG IR    │  Platform   │
│   (kernels + edges)   • port arity                 │  Element    │
│       │               • cycles                     │  SlotBridge │
│       │               • bandwidth                  │  Signal RPC │
│       ▼               • autorun                    └─────────────┘
│   Backend IR (build plan)
└────────────────────────────────────────────────────────────────────────┘
```

## Layers

### 1. Compiler (`compiler/`)

The `openclicknp-cc` binary is a single C++17 process. It parses `.clnp`
sources with a hand-written recursive-descent parser, lowers through two
typed IRs (Element-Graph IR and Backend IR), runs four static analyses,
and writes one or more code-generation outputs.

The most important property: **element bodies are passed through
opaquely.** The parser never analyses C++ inside `.state/.init/.handler/
.signal` blocks; it just brackets them. The downstream backends (HLS C++,
SystemC, SW emu) inject identical macro-level shims (defined in
`runtime/include/openclicknp/flit.hpp`) so user code is source-compatible
across all three.

### 2. Runtime (`runtime/`)

A shared library (`libopenclicknp_runtime.so`) the host program links
against. It wraps:

- **XRT** — `xrt::device`, `xrt::kernel`, `xrt::ip` for opening a
  programmed FPGA, launching kernels, peek/poking AXI-Lite registers.
- **Slot bridge** — for streaming host↔kernel data. Two backends:
  - `slot_bridge_xdma.cpp` multiplexes virtual slots over a small number
    of XDMA H2C/C2H AXIS channels.
  - `slot_bridge_qdma.cpp` maps each slot to its own QDMA streaming
    queue.
- **Signal RPC** — stop-and-wait host→kernel calls implemented via
  AXI-Lite.
- **Counters** — cycle counter and per-port packet counters exposed by
  the platform `openclicknp_status` block.
- **PCAP** — minimal libpcap-format reader/writer for testing; converts
  packets to/from 64 B flits with `sop`/`eop` flags.

Generated code (specifically `host/kernel_table.cpp`) calls
`registerGenerated()` at static-init time so the runtime knows what
kernels exist, which have signal handlers, etc.

### 3. Element library (`elements/`)

123 standard `.clnp` files organized into 9 categories that mirror the
paper's taxonomy:

| Category | Examples |
| --- | --- |
| `core/` | `Pass`, `Tee`, `Mux`, `Demux`, `Counter`, `Idle`, `DropElem`, `Fork` |
| `lookups/` | `HashTable`, `CuckooHash`, `LPM_Tree`, `FlowCache`, `RegTCAM` |
| `parsers/` | `FlowTupleParser`, `IP_Parser`, `TCP_Parser`, `UDP_Parser` |
| `actions/` | `IPChecksum`, `TCPChecksum`, `NVGRE_Encap`, `PacketModifier` |
| `crypto/` | `AES_CTR`, `AES_ECB`, `SHA1`, `Mult1024`, `Montgomery` |
| `queues/` | `MinHeap`, `PriorityQueue`, `RateLimit`, `ChannelShaping` |
| `traffic/` | `TrafficGen`, `PacketGen`, `RateLimiter`, `RoCE_Gen` |
| `networking/` | `L4LoadBalancer`, `Firewall_5tuple`, `Regex` |
| `testing/` | `LatencyTest`, `DelayTester`, `XorInputs` |

Each is a single self-contained file with `.state/.init/.handler/.signal`
blocks.

### 4. Shell (`shell/`)

The U50 platform integration: a Vivado Tcl script that adds an
OpenClickNP-specific top-level wrapper (`openclicknp_top.v`) plus three
constraint files (`clocks.xdc`, `pinout.xdc`, `cdc_waivers.xdc`) on top
of either the stock XDMA or QDMA platform from AMD.

The wrapper exposes boundary AXIS ports for `tor_in/out`, `nic_in/out`,
host streams, and AXI-Lite control. A small `openclicknp_status` IP
block exposes a free-running cycle counter and per-port packet
counters at fixed offsets.

For QDMA platforms, two helpers (`slot_demux.v` / `slot_mux.v`) are
omitted — each slot becomes a real QDMA queue.

### 5. Examples (`examples/`)

47 end-to-end applications, ranging from the smallest meaningful demo
(`PassTraffic`) to full network functions (`Firewall` 5-tuple filter
with host-installed rules via signal RPC, `ECMP_Router`, `L4LoadBalancer`,
`NVGRE_Gateway`, `IPsec_ESP`, `RoCE_Gateway`, `AES_Pipeline_4x`, etc.).
Every element in the library is imported by ≥ 1 application.

### 6. Scripts (`scripts/`)

Bash wrappers around the toolchain stages:

- `build/`: compile, synth_kernels, link, implement.
- `sim/`: run_unit, run_emu, run_systemc, run_cosim, run_verilator.
- `run/`: program_fpga, run_example, perf_pps, loopback_test.
- `platform/`: install_xrt, install_platform, flash_shell.
- `lib/common.sh`: shared helpers, version pins.

## Clocking and CDC

User-side everywhere runs at 322.265625 MHz. Three known cross-domain
boundaries, all in vendor IP:

1. CMAC tx/rx clocks ↔ user clock (handled by CMAC IP).
2. HBM AXI clock ↔ user clock (handled by AXI smartconnect).
3. PCIe/XDMA/QDMA AXI clock ↔ user clock (handled by shell IP).

`clocks.xdc` declares all three as `set_clock_groups -asynchronous`.
`cdc_waivers.xdc` documents the vendor-IP-internal CDC paths the tools
otherwise flag. After `place_design`, `report_cdc -severity error` must
return zero entries — any user-logic CDC violation fails the build.

## Build outputs

Compiling an example produces the full set of artifacts the rest of the
toolchain needs:

```
build/<example>/generated/
├── kernels/<name>.cpp        ← per-kernel HLS C++ source
├── kernels/<name>_tb.cpp     ← cosim testbench
├── systemc/topology.cpp      ← cycle-accurate SystemC sim
├── sw_emu/topology.cpp       ← std::thread emulator
├── verilator/topology.v      ← top-level Verilog wrapper for L4
├── verilator/tb.cpp          ← Verilator C++ testbench
├── link/connectivity.cfg     ← v++ stream_connect manifest
├── link/clocks.cfg           ← v++ kernel-clock assignments
├── host/kernel_table.cpp     ← runtime registration table
└── host/<ElementType>.hpp    ← per-element C++ class
```

Then synth_kernels.sh produces `build/<example>/kernels/<name>.xo`,
link.sh produces a linked `.xclbin` plus a CDC report, and implement.sh
produces the final `.xclbin` plus CDC and timing-summary reports.
