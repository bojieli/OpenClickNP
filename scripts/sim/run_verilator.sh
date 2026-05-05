#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# run_verilator.sh — L4 full-system Verilator simulation.
#
# Builds Verilator binary from the generated topology.v wrapper plus each
# kernel's RTL (which must already exist in build/<example>/kernels — run
# synth_kernels.sh first).
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"

if [[ $# -lt 1 ]]; then
    err "usage: run_verilator.sh <example-dir>"
    exit 1
fi
EXAMPLE_DIR="$(example_dir_for "$1")"

if ! command -v verilator >/dev/null 2>&1; then
    err "verilator not in PATH"
    exit 1
fi

GEN="$(generated_dir "${EXAMPLE_DIR}")"
BD="$(build_dir_for "${EXAMPLE_DIR}")/verilator"
mkdir -p "${BD}"

# Find Verilog produced by Vitis HLS (in solution1/syn/verilog/ for each kernel).
RTL_DIRS=()
while IFS= read -r d; do RTL_DIRS+=("$d"); done \
    < <(find "$(build_dir_for "${EXAMPLE_DIR}")/kernels" \
              -path '*solution1/syn/verilog' -type d 2>/dev/null)

VERILATOR_ARGS=( --binary --threads "$(nproc)" --trace-fst -O3 -Wno-fatal --top-module openclicknp_sim_top )
for d in "${RTL_DIRS[@]}"; do VERILATOR_ARGS+=( -y "${d}" ); done
VERILATOR_ARGS+=( "${GEN}/verilator/topology.v" "${GEN}/verilator/tb.cpp" )

log "building Verilator simulator"
(cd "${BD}" && verilator "${VERILATOR_ARGS[@]}")
log "running simulation"
(cd "${BD}/obj_dir" && ./Vopenclicknp_sim_top)
ok "L4 done — waveform: ${BD}/obj_dir/topology.fst"
