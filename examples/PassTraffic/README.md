# PassTraffic

Smallest meaningful OpenClickNP demo: every flit arriving on the ToR-side
QSFP28 is forwarded to the NIC-side QSFP28 (and vice versa), with two
counter elements counting flits and packets in each direction. Counters
are queryable via the host signal RPC.

```
tor_in --> [tor_rx_cnt] --> nic_out
nic_in --> [nic_rx_cnt] --> tor_out
```

## Build

```bash
# Compile .clnp → generated/
openclicknp-cc topology.clnp -o generated -I ../../elements

# Software emulator (no FPGA)
./scripts/sim/run_emu.sh examples/PassTraffic

# Real Alveo U50 (Vitis 2025.2 + XRT installed)
./scripts/build/synth_kernels.sh examples/PassTraffic
./scripts/build/link.sh           examples/PassTraffic
./scripts/build/implement.sh      examples/PassTraffic
./scripts/run/program_fpga.sh     build/PassTraffic/PassTraffic.xclbin
./scripts/run/run_example.sh      examples/PassTraffic
```

## Expected output

```
ToR rx: 12345 pkts (12345 pps)   NIC rx: 11823 pkts (11823 pps)
ToR rx: 24500 pkts (12155 pps)   NIC rx: 23800 pkts (11977 pps)
...
```
