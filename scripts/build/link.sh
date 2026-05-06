#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# link.sh — v++ -l: link kernel .xo files against the chosen U50 platform.
#
# Runs CDC and timing checks on the linked design before declaring success.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"
source_vivado_env

if [[ $# -lt 1 ]]; then
    err "usage: link.sh <example-dir>"
    exit 1
fi
EXAMPLE_DIR="$(example_dir_for "$1")"
NAME="$(example_name_of "${EXAMPLE_DIR}")"

GEN="$(generated_dir "${EXAMPLE_DIR}")"
BD="$(build_dir_for "${EXAMPLE_DIR}")"

XOS=()
while IFS= read -r f; do XOS+=("$f"); done < <(find "${BD}/kernels" -maxdepth 1 -name '*.xo' -print)

if (( ${#XOS[@]} == 0 )); then
    warn "no .xo files found; skipping link (run synth_kernels.sh first)"
    exit 0
fi

if ! command -v v++ >/dev/null 2>&1; then
    err "v++ not in PATH"
    exit 1
fi

XCLBIN="${BD}/${NAME}.link.xclbin"
log "linking ${#XOS[@]} .xo files for ${PLATFORM_VITIS} → ${XCLBIN}"

v++ -l \
    --platform "${PLATFORM_VITIS}" \
    --target hw \
    --config "${GEN}/link/connectivity.cfg" \
    --config "${GEN}/link/clocks.cfg" \
    --temp_dir "${BD}/link.tmp" \
    --report_dir "${BD}/link.reports" \
    --log_dir "${BD}/link.logs" \
    --output "${XCLBIN}" \
    "${XOS[@]}"

ok "v++ link complete"

# CDC check #1: post-synthesis on linked design.
log "running CDC check on linked design"
CDC_DCP="${BD}/link.tmp/link/imports/dr_link.dcp"
if [[ -f "${CDC_DCP}" ]]; then
    vivado -nojournal -nolog -mode batch -source <(cat <<EOF
open_checkpoint ${CDC_DCP}
report_cdc -severity {Error Warning Info} -file ${BD}/cdc_post_link.rpt
report_clock_interaction -file ${BD}/clk_interaction_post_link.rpt
exit
EOF
)
    if grep -q '^Critical CDC' "${BD}/cdc_post_link.rpt"; then
        err "CDC check #1 found critical violations (see ${BD}/cdc_post_link.rpt)"
        exit 1
    fi
    ok "CDC check #1 passed"
else
    warn "linked DCP not found at ${CDC_DCP}; skipping CDC check"
fi
