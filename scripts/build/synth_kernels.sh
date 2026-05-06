#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# synth_kernels.sh — run vitis_hls per-kernel C synthesis + IP package.
#
# Each generated/kernels/<name>.cpp produces build/<example>/kernels/<name>.xo
# (Vitis HLS object archive) which v++ can later link.
#
# Usage: scripts/build/synth_kernels.sh <example-dir> [--jobs N]
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"
source_vivado_env

if [[ $# -lt 1 ]]; then
    err "usage: synth_kernels.sh <example-dir> [--jobs N]"
    exit 1
fi
EXAMPLE_DIR="$(example_dir_for "$1")"; shift
JOBS=$(nproc)
while [[ $# -gt 0 ]]; do
    case "$1" in
        --jobs) JOBS="$2"; shift 2;;
        *) err "unknown option $1"; exit 1;;
    esac
done

GEN="$(generated_dir "${EXAMPLE_DIR}")"
BD="$(build_dir_for "${EXAMPLE_DIR}")/kernels"
mkdir -p "${BD}"

if ! command -v vitis_hls >/dev/null 2>&1; then
    warn "vitis_hls not in PATH; skipping HLS synthesis (this is OK for SW-only flow)"
    exit 0
fi

KERNELS=()
while IFS= read -r f; do
    KERNELS+=("$(basename "$f" .cpp)")
done < <(find "${GEN}/kernels" -maxdepth 1 -name '*.cpp' ! -name '*_tb.cpp' -print)

log "synthesizing ${#KERNELS[@]} kernels with ${JOBS} parallel jobs"

run_one() {
    local k="$1"
    local tcl="${BD}/${k}_synth.tcl"
    cat >"${tcl}" <<EOF
open_project -reset ${BD}/${k}_proj
add_files ${GEN}/kernels/${k}.cpp -cflags "-I ${OPENCLICKNP_ROOT}/runtime/include -std=c++17"
add_files -tb ${GEN}/kernels/${k}_tb.cpp -cflags "-I ${OPENCLICKNP_ROOT}/runtime/include -std=c++17"
set_top ${k}
open_solution -reset solution1
set_part xcu50-fsvh2104-2-e
create_clock -period $(awk "BEGIN{printf \"%.3f\", 1e9/${USER_CLOCK_HZ}}") -name default
config_export -format ip_catalog -output ${BD}/${k}.xo -display_name ${k} -vendor openclicknp -library hls -version 1.0
csynth_design
export_design -format ip_catalog -output ${BD}/${k}.xo
exit
EOF
    log "  vitis_hls ${k}"
    (cd "${BD}" && vitis_hls -f "${tcl}" >"${BD}/${k}.log" 2>&1) || {
        err "HLS failed for ${k} (see ${BD}/${k}.log)"
        return 1
    }
}

export -f run_one log err warn ok
export OPENCLICKNP_ROOT GEN BD USER_CLOCK_HZ

if (( ${#KERNELS[@]} == 0 )); then
    warn "no kernels found in ${GEN}/kernels"
else
    printf '%s\n' "${KERNELS[@]}" | xargs -n1 -P "${JOBS}" -I{} bash -c 'run_one "$@"' _ {}
fi

ok "kernel synthesis complete"
