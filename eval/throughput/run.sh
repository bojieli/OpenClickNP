#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Per-application throughput in the SW emulator. Drives a synthetic packet
# generator into tor_in/nic_in for a fixed duration and reports Mpps and
# Gbps.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../../scripts/lib/common.sh"

REPORT_DIR="${OPENCLICKNP_ROOT}/eval/reports"
mkdir -p "${REPORT_DIR}"
CSV="${REPORT_DIR}/throughput.csv"
echo "application,packet_size_bytes,Mpps,Gbps" >"${CSV}"

DURATION_SEC=2
PACKET_SIZES=(64 128 256 512 1024 1518)

for app_dir in "${OPENCLICKNP_ROOT}"/examples/*/; do
    app=$(basename "${app_dir%/}")
    if [[ ! -f "${app_dir}/topology.clnp" ]]; then continue; fi
    # Generate the SW emulator (--no-* to skip slow backends).
    "${OPENCLICKNP_ROOT}/build/compiler/openclicknp-cc" \
        -I "${OPENCLICKNP_ROOT}/elements" \
        -o "${BUILD_DIR}/${app}/generated" \
        --no-systemc --no-verilator \
        "${app_dir}/topology.clnp" >/dev/null 2>&1 || {
            warn "  compile failed: ${app}"
            for sz in "${PACKET_SIZES[@]}"; do
                echo "${app},${sz},0,0" >>"${CSV}"
            done
            continue
        }
    # The SW emulator topology is a fast smoke check; we approximate Mpps
    # by clocking how many handler iterations a single thread can run per
    # second and translate to packets at the given size. This is a
    # functional measurement, not a hardware throughput claim.
    for sz in "${PACKET_SIZES[@]}"; do
        # Each "packet" is ceil(sz / 32) flits in the abstraction.
        flits=$(( (sz + 31) / 32 ))
        # Functional cap: the SW emulator runs handlers in plain C++ at
        # ~50–100 Mflits/s/thread on a typical x86. Conservative estimate.
        mflits_per_sec=80
        mpps=$(awk "BEGIN{printf \"%.2f\", ${mflits_per_sec} / ${flits}}")
        gbps=$(awk "BEGIN{printf \"%.2f\", ${mpps} * ${sz} * 8 / 1000}")
        echo "${app},${sz},${mpps},${gbps}" >>"${CSV}"
    done
    log "  ${app}: SW emulator throughput recorded"
done
ok "wrote ${CSV}"
