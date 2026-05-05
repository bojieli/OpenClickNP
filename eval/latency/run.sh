#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Per-application end-to-end latency. Without an FPGA, we report the
# *element-graph depth* (longest path from input to output) and convert it
# to a wall-clock estimate at the design clock (322.265625 MHz).
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../../scripts/lib/common.sh"

REPORT_DIR="${OPENCLICKNP_ROOT}/eval/reports"
mkdir -p "${REPORT_DIR}"
CSV="${REPORT_DIR}/latency.csv"
echo "application,element_count,graph_depth,latency_us" >"${CSV}"

CLOCK_NS=$(awk "BEGIN{printf \"%.4f\", 1000.0 / 322.265625}")

for app_dir in "${OPENCLICKNP_ROOT}"/examples/*/; do
    app=$(basename "${app_dir%/}")
    [[ -f "${app_dir}/topology.clnp" ]] || continue
    # element count: lines containing `:: name`
    elem_n=$(grep -cE '::[[:space:]]*[A-Za-z_]' "${app_dir}/topology.clnp" || true)
    # graph depth proxy: count distinct -> arrows in the longest line
    depth=$(grep -E '^[a-zA-Z_]+' "${app_dir}/topology.clnp" \
            | awk -F'->' '{print NF-1}' | sort -n | tail -1)
    [[ -z "${depth}" ]] && depth=1
    # Each element introduces ~3 cycles of latency in HLS II=1 dataflow.
    cycles=$((depth * 3))
    lat_us=$(awk "BEGIN{printf \"%.3f\", ${cycles} * ${CLOCK_NS} / 1000}")
    echo "${app},${elem_n},${depth},${lat_us}" >>"${CSV}"
done

ok "wrote ${CSV}"
