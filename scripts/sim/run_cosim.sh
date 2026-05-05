#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# run_cosim.sh — L3 Vitis HLS C/RTL co-simulation, per kernel.
#
# Usage: scripts/sim/run_cosim.sh <example-dir> [kernel_name]
#   kernel_name optional: if omitted, runs cosim for every kernel.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"
source_vivado_env

if [[ $# -lt 1 ]]; then
    err "usage: run_cosim.sh <example-dir> [kernel_name]"
    exit 1
fi
EXAMPLE_DIR="$(example_dir_for "$1")"
ONLY="${2:-}"

GEN="$(generated_dir "${EXAMPLE_DIR}")"
BD="$(build_dir_for "${EXAMPLE_DIR}")/cosim"
mkdir -p "${BD}"

if ! command -v vitis_hls >/dev/null 2>&1; then
    err "vitis_hls not in PATH"
    exit 1
fi

KERNELS=()
while IFS= read -r f; do
    k="$(basename "$f" .cpp)"
    if [[ -n "${ONLY}" && "${k}" != "${ONLY}" ]]; then continue; fi
    KERNELS+=("${k}")
done < <(find "${GEN}/kernels" -maxdepth 1 -name '*.cpp' ! -name '*_tb.cpp' -print)

for k in "${KERNELS[@]}"; do
    log "cosim ${k}"
    tcl="${BD}/${k}_cosim.tcl"
    cat >"${tcl}" <<EOF
open_project ${BD}/${k}_proj -reset
add_files ${GEN}/kernels/${k}.cpp -cflags "-I ${OPENCLICKNP_ROOT}/runtime/include -std=c++17"
add_files -tb ${GEN}/kernels/${k}_tb.cpp -cflags "-I ${OPENCLICKNP_ROOT}/runtime/include -std=c++17"
set_top ${k}
open_solution -reset solution1
set_part xcu50-fsvh2104-2-e
create_clock -period $(awk "BEGIN{printf \"%.3f\", 1e9/${USER_CLOCK_HZ}}")
csynth_design
cosim_design -trace_level all -wave_debug -tool xsim
exit
EOF
    (cd "${BD}" && vitis_hls -f "${tcl}") || {
        err "cosim FAILED for ${k}"
        exit 1
    }
done
ok "L3 cosim done"
