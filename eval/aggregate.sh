#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Aggregate every CSV in eval/reports/ into a Markdown summary table.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../scripts/lib/common.sh"

REPORT_DIR="${OPENCLICKNP_ROOT}/eval/reports"
OUT="${REPORT_DIR}/summary.md"

{
    echo "# OpenClickNP — Evaluation summary"
    echo ""
    echo "Run on $(date -u +%FT%TZ) — host $(hostname)"
    echo ""
    if [[ -f "${REPORT_DIR}/resource_usage.csv" ]]; then
        echo "## Per-element FPGA resource estimates"
        echo ""
        echo '| Element | LUT | FF | DSP | BRAM | Latency (cyc) | II |'
        echo '|---|---|---|---|---|---|---|'
        tail -n +2 "${REPORT_DIR}/resource_usage.csv" \
            | head -50 \
            | awk -F, '{printf "| %s | %s | %s | %s | %s | %s | %s |\n", $1,$2,$3,$4,$5,$6,$7}'
        echo ""
        # Stats
        n=$(tail -n +2 "${REPORT_DIR}/resource_usage.csv" | wc -l)
        ok=$(tail -n +2 "${REPORT_DIR}/resource_usage.csv" | awk -F, '$2 != "synth_failed" && $2 != "compile_failed" && $2 != "report_missing" && $2 != "nogen" {c++} END{print c+0}')
        echo "${ok} of ${n} elements synthesized successfully"
        echo ""
    fi

    if [[ -f "${REPORT_DIR}/throughput.csv" ]]; then
        echo "## Per-application throughput (SW emulator)"
        echo ""
        echo '| Application | Pkt size (B) | Mpps | Gbps |'
        echo '|---|---|---|---|'
        tail -n +2 "${REPORT_DIR}/throughput.csv" \
            | awk -F, '{printf "| %s | %s | %s | %s |\n", $1,$2,$3,$4}'
        echo ""
    fi

    if [[ -f "${REPORT_DIR}/latency.csv" ]]; then
        echo "## Per-application latency (graph-depth-based estimate at 322 MHz)"
        echo ""
        echo '| Application | # Elements | Graph depth | Latency (µs) |'
        echo '|---|---|---|---|'
        tail -n +2 "${REPORT_DIR}/latency.csv" \
            | awk -F, '{printf "| %s | %s | %s | %s |\n", $1,$2,$3,$4}'
        echo ""
    fi

    if [[ -f "${REPORT_DIR}/cdc_status_block.rpt" ]]; then
        echo "## CDC analysis (representative design)"
        echo ""
        echo '```'
        head -30 "${REPORT_DIR}/cdc_status_block.rpt"
        echo '```'
        echo ""
    fi
} >"${OUT}"

ok "wrote ${OUT}"
