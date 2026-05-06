#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# RSA modexp throughput / latency benchmark.
#
# Drives a stream of (m, e, n) operands through the SW-emulated
# RSA_ModExp_{1024,2048,4096} kernels, times the result, and emits a
# CSV row per bit width. Reproduces the perf side of the FINAL_REPORT
# tables.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../../scripts/lib/common.sh"

REPORT_DIR="${OPENCLICKNP_ROOT}/eval/reports"
WORK_DIR="${BUILD_DIR}/eval_rsa"
mkdir -p "${REPORT_DIR}" "${WORK_DIR}"

BENCH="${OPENCLICKNP_ROOT}/build/tests/integration/test_rsa_perf"
if [[ ! -x "${BENCH}" ]]; then
    err "perf binary not built. Run: cmake --build build -j"
    exit 1
fi

CSV="${REPORT_DIR}/rsa_perf.csv"
echo "bits,iterations,wall_time_s,ops_per_sec,latency_us_per_op" >"${CSV}"

for bits in 1024 2048 4096; do
    log "RSA-${bits}: 200 ops"
    "${BENCH}" "${bits}" 200 >>"${CSV}"
done

ok "wrote ${CSV}"
column -t -s, "${CSV}"
