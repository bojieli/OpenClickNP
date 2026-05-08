# OpenClickNP DSL — Language Reference

OpenClickNP source files use the `.clnp` extension. A file may contain:

- `import "path/to/lib.clnp";` — pull definitions from another file.
- `.element Name <inports, outports> { ... }` — element type.
- `.element_group Name { ... }` — reusable subgraph.
- Top-level instance and connection statements.

## Element

```
.element Name <num_in_ports, num_out_ports> {
    .state {
        // Persistent variables. Plain C++17.
        constexpr uint32_t SIZE = 1024;     // compile-time constant
        struct Entry { ... };
        Entry table[SIZE];
        uint64_t counter;
    }

    .init {
        // Runs once at element launch. Plain C++17.
        for (uint32_t i = 0; i < _state.SIZE; ++i) _state.table[i] = {};
        _state.counter = 0;
    }

    .handler {
        // Runs each iteration.
        if (test_input_port(PORT_1)) {
            openclicknp::flit_t f = read_input_port(PORT_1);
            // ...
            set_output_port(2, f);
        }
        return PORT_1;     // which input ports to wait on next iteration
    }

    .signal (uint cmd, uint sparam) {
        // Optional. Host-callable RPC.
        outevent.lparam[0] = _state.counter;
    }
    .timing {
        // Optional. Override per-element HLS pipeline initiation interval.
        // Default is ii=1 (one new flit accepted every cycle). Use ii=2
        // for state-heavy elements whose handler can't fit in a single
        // 3.106 ns cycle at 322 MHz — typically those that combine an
        // AXI input read with a multi-step state update.
        ii = 2;
    }
}
```

### Built-ins inside `.handler` and `.signal`

| Name | Description |
| --- | --- |
| `input_ready(MASK)` / `test_input_port(MASK)` | true iff that input has a flit ready |
| `_input_port` | bitmask of currently-ready inputs |
| `_input_data[i]` | the flit on input port `i` (1-based) |
| `read_input_port(MASK)` | clear-and-return the flit on the named port |
| `clear_input_ready(MASK)` | mark a port as consumed without reading |
| `set_output_port(i, value)` | enqueue `value` on output port `i` |
| `last_output_failed(MASK)` | non-zero if the last cycle's lossy write was dropped |
| `last_output_success(MASK)` | non-zero if the last cycle's lossy write was accepted |
| `event` | incoming `ClSignal` (in `.signal`) |
| `outevent` | response `ClSignal` (in `.signal`) |
| `last_rport` | the value `.handler` returned last iteration |
| `PORT_1`, `PORT_2`, ..., `PORT_8` | bit masks |
| `PORT_ALL` | wait on all inputs next iteration |

The handler returns a port-mask: the next iteration only reads from
inputs whose bit is set.

## Topology

### Instances

```
ElementType :: instance_name [@] [&] [(in_ports, out_ports[*depth][, params...])]
```

- `@` enables host-control (signal RPC).
- `&` declares the instance as autorun.
- `(in, out)` overrides port counts; `*depth` overrides the default 64-flit
  channel depth on outgoing edges.
- Additional numeric or string parameters propagate to the element body
  (for elements that read them via `_state` `constexpr`).

Anonymous form: `ElementType(in, out)` inside a connection chain creates
an unnamed instance.

### Connections

```
A -> B           // lossless, single input/output ports default to 1
A -> [2]B        // explicit destination port
A[3] -> B        // explicit source port
A => B           // lossy: source drops on full
A -> B[1*128]    // override channel depth
A -> Drop        // discard
```

A chain `A -> B -> C` expands to two connections.

### Special pseudo-elements

| Name | Maps to |
| --- | --- |
| `tor_in`, `tor_out` | CMAC #0 RX / TX (QSFP28 cage A) |
| `nic_in`, `nic_out` | CMAC #1 RX / TX (QSFP28 cage B) |
| `host_in`, `host_out` | Streaming host↔kernel via slot bridge |
| `Drop` | Discard sink |
| `Idle` | Never-producing source |
| `begin`, `end` | Group-internal aliases for the outer ports |

## Element groups

```
.element_group NVGRE_Encap <2, 2> {
    PushHeader :: encap (2, 2)
    begin[1] -> [1]encap -> IPChecksum(2, 2) -> PacketModifier(2, 2) -> end
    begin[2] -> ExtractCA(1, 1) -> HashTable @ (1, 1, 14) -> PA_Table @ (1, 1) -> [2]encap
}

NVGRE_Encap :: encap_inst (2, 2)
```

Group instantiation inlines the body, replacing `begin`/`end` with the
outer instance's port references.

## Code-generation directives (planned)

- `constexpr NAME = expr;` inside `.state` — compile-time constant.
- `.repeat(var, lo, hi) { ... $var ... }` — generates `(hi - lo)` copies
  with `$var` substituted.
- `BREAK` — exit the elaborated `.repeat` region.

These are the paper's mechanism for parameterized depth/width pipelines
(used by `IP_Lookup` for tree depth, etc.). The v0.1 compiler accepts
them syntactically and forwards them; full elaboration of `.repeat` is
on the roadmap.

## Examples

The smallest meaningful example: `examples/PassTraffic/topology.clnp`.
A more advanced example with `host_control` rule installation:
`examples/Firewall/topology.clnp`.
