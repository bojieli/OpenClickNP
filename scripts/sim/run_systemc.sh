#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# run_systemc.sh — build & run the cycle-accurate SystemC simulation.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"

if [[ $# -lt 1 ]]; then
    err "usage: run_systemc.sh <example-dir>"
    exit 1
fi
EXAMPLE_DIR="$(example_dir_for "$1")"
NAME="$(example_name_of "${EXAMPLE_DIR}")"

GEN="$(generated_dir "${EXAMPLE_DIR}")"
BD="$(build_dir_for "${EXAMPLE_DIR}")/systemc"
mkdir -p "${BD}"

: "${SYSTEMC_HOME:=/usr/local/systemc}"
if [[ ! -d "${SYSTEMC_HOME}/include" ]]; then
    err "SystemC not found at ${SYSTEMC_HOME}; install or set SYSTEMC_HOME"
    exit 1
fi

log "compiling SystemC simulation"
g++ -std=c++17 -O2 -DSC_INCLUDE_DYNAMIC_PROCESSES \
    -I "${SYSTEMC_HOME}/include" \
    -I "${OPENCLICKNP_ROOT}/runtime/include" \
    "${GEN}/systemc/topology.cpp" \
    -L "${SYSTEMC_HOME}/lib" -lsystemc -lpthread \
    -o "${BD}/${NAME}_sc"
log "running"
"${BD}/${NAME}_sc"
ok "L-cycle SystemC sim done — waveform: ${BD}/topology.vcd"
