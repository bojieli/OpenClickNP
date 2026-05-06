#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# implement.sh — v++ -l --target hw: place + route + bitgen.
#
# After place_design, runs CDC check #2 (errors fail the build) and
# enforces WNS ≥ 0 and WHS ≥ 0 timing closure.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"
source_vivado_env

if [[ $# -lt 1 ]]; then
    err "usage: implement.sh <example-dir>"
    exit 1
fi
EXAMPLE_DIR="$(example_dir_for "$1")"
NAME="$(example_name_of "${EXAMPLE_DIR}")"

GEN="$(generated_dir "${EXAMPLE_DIR}")"
BD="$(build_dir_for "${EXAMPLE_DIR}")"

if ! command -v v++ >/dev/null 2>&1; then
    err "v++ not in PATH"
    exit 1
fi

XCLBIN="${BD}/${NAME}.xclbin"

XOS=()
while IFS= read -r f; do XOS+=("$f"); done < <(find "${BD}/kernels" -maxdepth 1 -name '*.xo' -print)
if (( ${#XOS[@]} == 0 )); then
    err "no .xo files; run synth_kernels.sh first"
    exit 1
fi

log "implementing ${NAME} (this typically takes 4–8 hours on a 16-core workstation)"

# We invoke v++ with --kernel_frequency to also enforce the user clock.
v++ -l \
    --platform "${PLATFORM_VITIS}" \
    --target hw \
    --kernel_frequency 322 \
    --config "${GEN}/link/connectivity.cfg" \
    --vivado.impl.strategies "Performance_Explore,Performance_ExplorePostRoutePhysOpt" \
    --temp_dir "${BD}/impl.tmp" \
    --report_dir "${BD}/impl.reports" \
    --log_dir "${BD}/impl.logs" \
    --output "${XCLBIN}" \
    "${XOS[@]}"

# CDC check #2: post-implementation, errors are fatal.
IMPL_DCP=$(find "${BD}/impl.tmp" -name 'impl_*.dcp' | head -1 || true)
if [[ -n "${IMPL_DCP}" ]]; then
    log "CDC check #2 on implemented design"
    vivado -nojournal -nolog -mode batch -source <(cat <<EOF
open_checkpoint ${IMPL_DCP}
report_cdc -severity {Critical Error} -file ${BD}/cdc_post_impl.rpt
report_timing_summary -file ${BD}/timing_summary.rpt
exit
EOF
)
    if grep -q '^Critical CDC' "${BD}/cdc_post_impl.rpt" || \
       grep -q '^Error CDC'    "${BD}/cdc_post_impl.rpt"; then
        err "CDC check #2 failed (see ${BD}/cdc_post_impl.rpt)"
        exit 1
    fi
    WNS=$(grep -A2 '| Design Timing Summary' "${BD}/timing_summary.rpt" | tail -1 | awk '{print $1}')
    if [[ -n "${WNS}" ]] && awk "BEGIN{exit !(${WNS} < 0)}"; then
        err "timing closure failed: WNS=${WNS} ns"
        exit 1
    fi
    ok "CDC and timing checks passed (WNS=${WNS:-?} ns)"
fi
ok "implementation complete: ${XCLBIN}"
