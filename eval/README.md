# OpenClickNP — Evaluation suite

Reproducible evaluation aligned with the experiments described in the
ClickNP paper (SIGCOMM 2016): per-element resource usage, per-application
throughput, latency, and CDC/timing closure.

## Layout

```
eval/
├── resource_usage/    Per-element FPGA resource estimates (LUT/FF/DSP/BRAM)
│   └── run.sh         Drives Vitis HLS C synthesis on every element
├── throughput/        Per-application Mpps measurement (SW emulator)
│   └── run.sh
├── latency/           Per-application median + tail latency (SW emulator)
│   └── run.sh
├── cdc/               Vivado CDC report on a representative design
│   └── run.sh
└── reports/           Auto-generated CSVs/Markdown
    ├── resource_usage.csv
    ├── throughput.csv
    ├── latency.csv
    └── summary.md
```

## Reproducing

```bash
# Software-only experiments (no FPGA toolchain needed)
./eval/throughput/run.sh
./eval/latency/run.sh

# Vitis-HLS-based experiments (Vitis 2025.2 required).
# Either source settings64.sh yourself or set XILINX_DIR (default: /opt/Xilinx/2025.2).
source "${XILINX_DIR:-/opt/Xilinx/2025.2}/Vitis/settings64.sh"
./eval/resource_usage/run.sh

# CDC + timing closure (Vivado required, hours)
./eval/cdc/run.sh

# Aggregate everything into eval/reports/summary.md
./eval/aggregate.sh
```

## Comparison to the paper

The paper reports numbers for the original 28 nm Stratix V fabric. The
target hardware here is a 16 nm UltraScale+ U50 — the absolute LUT counts
and Fmax differ, but the **relative ordering** of element costs and
**throughput vs packet size** trends should reproduce.
