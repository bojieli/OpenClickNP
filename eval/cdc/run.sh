#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Run Vivado CDC analysis on a representative synthesized design.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../../scripts/lib/common.sh"
source_vivado_env

REPORT_DIR="${OPENCLICKNP_ROOT}/eval/reports"
WORK="${BUILD_DIR}/eval_cdc"
mkdir -p "${REPORT_DIR}" "${WORK}"

if ! command -v vivado >/dev/null 2>&1; then
    err "vivado not in PATH"
    exit 1
fi

# Synthesize the openclicknp_status block (small, fast — finishes in a
# minute) and run report_cdc on the result.
log "synthesizing openclicknp_status for CDC analysis"

cd "${WORK}"
cat >cdc_run.tcl <<EOF
create_project cdc_proj cdc_proj -part xcu50-fsvh2104-2-e -force
add_files ${OPENCLICKNP_ROOT}/shell/common/openclicknp_status.v
set_property top openclicknp_status [current_fileset]
synth_design -top openclicknp_status -part xcu50-fsvh2104-2-e -mode out_of_context

# Generate constraints for the synthesized CDC analysis.
create_clock -period 3.106 -name clk_user [get_ports ap_clk]

report_cdc -severity Info -file ${REPORT_DIR}/cdc_status_block.rpt
report_clock_interaction -delay_type max -file ${REPORT_DIR}/clock_interaction_status_block.rpt
report_timing_summary -file ${REPORT_DIR}/timing_status_block.rpt
report_utilization   -file ${REPORT_DIR}/utilization_status_block.rpt
exit
EOF

vivado -nojournal -nolog -mode batch -source cdc_run.tcl >"${WORK}/vivado.log" 2>&1
ok "CDC report at ${REPORT_DIR}/cdc_status_block.rpt"

# Summarize.
SUMMARY="${REPORT_DIR}/cdc_summary.csv"
echo "rule,severity,count" >"${SUMMARY}"
if [[ -f "${REPORT_DIR}/cdc_status_block.rpt" ]]; then
    awk '/^CDC-/ { sev=$2; rule=$1; key=rule"|"sev; cnt[key]++ }
         END { for (k in cnt) { split(k, a, "|"); print a[1]","a[2]","cnt[k] } }' \
        "${REPORT_DIR}/cdc_status_block.rpt" >>"${SUMMARY}"
fi
ok "summary: ${SUMMARY}"
