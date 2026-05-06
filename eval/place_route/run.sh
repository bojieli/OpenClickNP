#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Full Vivado place-and-route on every application, in parallel.
#
# For each app:
#   1) openclicknp-cc compiles .clnp to per-kernel HLS C++.
#   2) vitis_hls produces per-kernel synthesized RTL (csynth_design).
#   3) For each kernel, Vivado synth_design -mode out_of_context produces
#      a real per-kernel utilization, timing, and CDC report on the
#      xcu50-fsvh2104-2-e part.
#   4) Per-app totals = sum across all the kernels in the app.
#
# Output: eval/reports/pnr/<app>/<kernel>_{utilization,timing,cdc}.rpt
#         eval/reports/pnr_summary.csv
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../../scripts/lib/common.sh"

if [[ -f "${XILINX_DIR}/Vivado/settings64.sh" ]]; then
    # shellcheck disable=SC1091
    source "${XILINX_DIR}/Vivado/settings64.sh" >/dev/null 2>&1 || true
fi

PNR_REPORT="${OPENCLICKNP_ROOT}/eval/reports/pnr"
WORK="${BUILD_DIR}/eval_pnr"
mkdir -p "${PNR_REPORT}" "${WORK}"

JOBS="${JOBS:-$(nproc)}"
PARALLEL="${PARALLEL:-4}"
TARGET_PART="${TARGET_PART:-xcu50-fsvh2104-2-e}"
CLOCK_NS="${CLOCK_NS:-3.106}"

CSV="${OPENCLICKNP_ROOT}/eval/reports/pnr_summary.csv"
echo "app,kernels,LUT,FF,DSP,BRAM,WNS_ns,WHS_ns,CDC_critical,CDC_warning,impl_seconds" >"${CSV}"

extract_util_field() {
    # Extract the "Used" column (column 3) for a given site type from a
    # report_utilization output.
    local rpt="$1" site="$2"
    awk -v site="${site}" -F'|' '
        $0 ~ site && $0 ~ /^\|/ {
            gsub(/ /, "", $3);
            if ($3 ~ /^[0-9]+$/) { print $3; exit }
        }' "${rpt}"
}

run_one_app() {
    local app="$1"
    local app_dir="${OPENCLICKNP_ROOT}/examples/${app}"
    local app_work="${WORK}/${app}"
    mkdir -p "${app_work}"

    [[ -f "${app_dir}/topology.clnp" ]] || { echo "${app}: no topology.clnp"; return 0; }

    local t0 t1
    t0=$(date +%s)

    # 1) Compile to HLS C++.
    local gen="${app_work}/generated"
    rm -rf "${gen}"
    "${OPENCLICKNP_ROOT}/build/compiler/openclicknp-cc" \
        -I "${OPENCLICKNP_ROOT}/elements" \
        -o "${gen}" \
        --no-systemc --no-verilator \
        "${app_dir}/topology.clnp" >"${app_work}/cc.log" 2>&1 || {
            echo "${app},0,compile_failed,,,,,,,," >>"${CSV}"
            return 0
        }

    local kernels=()
    while IFS= read -r f; do
        kernels+=("$(basename "$f" .cpp)")
    done < <(find "${gen}/kernels" -maxdepth 1 -name '*.cpp' ! -name '*_tb.cpp' -print | sort)
    local nk="${#kernels[@]}"

    local app_lut=0 app_ff=0 app_dsp=0 app_bram=0
    local app_wns_min="" app_whs_min="" app_cdc_c=0 app_cdc_w=0
    local mkdir_app="${PNR_REPORT}/${app}"
    mkdir -p "${mkdir_app}"

    # 2) For each kernel, vitis_hls -> Verilog, then Vivado synth.
    for k in "${kernels[@]}"; do
        local proj="${app_work}/${k}_proj"
        local hls_tcl="${app_work}/${k}_hls.tcl"
        rm -rf "${proj}"
        cat >"${hls_tcl}" <<EOF
open_project ${proj}
add_files ${gen}/kernels/${k}.cpp -cflags "-I ${OPENCLICKNP_ROOT}/runtime/include -std=c++17"
set_top ${k}
open_solution -reset solution1
set_part ${TARGET_PART}
create_clock -period ${CLOCK_NS}
config_compile -ignore_long_run_time
csynth_design
exit
EOF
        if ! (cd "${app_work}" && \
              "${XILINX_DIR}/Vitis/bin/loader" -exec vitis_hls \
              -f "${hls_tcl}" >"${app_work}/${k}_hls.log" 2>&1); then
            warn "  ${app}/${k}: HLS failed"
            continue
        fi

        # Vivado: synth_design + place_design + route_design on the kernel.
        local rtl_dir="${proj}/solution1/syn/verilog"
        local viv_tcl="${app_work}/${k}_viv.tcl"
        local viv_proj="${app_work}/${k}_viv_proj"
        rm -rf "${viv_proj}"
        cat >"${viv_tcl}" <<EOF
create_project ${k}_proj ${viv_proj} -part ${TARGET_PART} -force
foreach f [glob -nocomplain ${rtl_dir}/*.v] {
    add_files -fileset sources_1 \$f
}
set_property top ${k} [current_fileset]
update_compile_order -fileset sources_1

set xdc ${app_work}/${k}.xdc
set fp [open \$xdc w]
puts \$fp "create_clock -period ${CLOCK_NS} -name ap_clk \[get_ports ap_clk\]"
close \$fp
add_files -fileset constrs_1 \$xdc

# OOC synth + p+r
synth_design -top ${k} -part ${TARGET_PART} -mode out_of_context
opt_design
place_design
route_design

report_utilization        -file ${mkdir_app}/${k}_utilization.rpt
report_timing_summary     -file ${mkdir_app}/${k}_timing.rpt
report_cdc -severity Info -file ${mkdir_app}/${k}_cdc.rpt
exit
EOF
        if ! (cd "${app_work}" && \
              vivado -nojournal -nolog -mode batch -source "${viv_tcl}" \
              >"${app_work}/${k}_viv.log" 2>&1); then
            warn "  ${app}/${k}: Vivado P&R failed"
            continue
        fi

        # Aggregate per-kernel into app totals.
        local k_lut k_ff k_dsp k_bram k_wns k_whs k_cdc_c k_cdc_w
        k_lut=$(extract_util_field "${mkdir_app}/${k}_utilization.rpt" "CLB LUTs")
        k_ff=$( extract_util_field "${mkdir_app}/${k}_utilization.rpt" "CLB Registers")
        k_dsp=$(extract_util_field "${mkdir_app}/${k}_utilization.rpt" "DSPs")
        k_bram=$(extract_util_field "${mkdir_app}/${k}_utilization.rpt" "Block RAM Tile")

        # WNS / WHS extraction. The report has a column header line, then
        # a dashes separator, then the data line — skip the dashes with
        # two getlines, then read the first numeric line.
        k_wns=$(awk '/WNS\(ns\)/{getline; getline; sub(/^ +/, ""); print $1; exit}' \
                "${mkdir_app}/${k}_timing.rpt")
        k_whs=$(awk '/WNS\(ns\)/{getline; getline; sub(/^ +/, ""); print $5; exit}' \
                "${mkdir_app}/${k}_timing.rpt")

        k_cdc_c=$(grep -c "^Critical" "${mkdir_app}/${k}_cdc.rpt" 2>/dev/null | head -1)
        k_cdc_w=$(grep -c "^Warning"  "${mkdir_app}/${k}_cdc.rpt" 2>/dev/null | head -1)
        [[ -z "${k_cdc_c}" ]] && k_cdc_c=0
        [[ -z "${k_cdc_w}" ]] && k_cdc_w=0

        # Integer aggregation via bash. (Block RAM / DSP can be fractional
        # in some reports; we coerce to int for the totals.)
        if [[ -n "${k_lut}"  ]] && [[ "${k_lut}"  =~ ^[0-9]+$ ]]; then app_lut=$((app_lut  + k_lut));  fi
        if [[ -n "${k_ff}"   ]] && [[ "${k_ff}"   =~ ^[0-9]+$ ]]; then app_ff=$((app_ff   + k_ff));   fi
        if [[ -n "${k_dsp}"  ]] && [[ "${k_dsp}"  =~ ^[0-9]+$ ]]; then app_dsp=$((app_dsp + k_dsp));  fi
        if [[ -n "${k_bram}" ]] && [[ "${k_bram}" =~ ^[0-9]+$ ]]; then app_bram=$((app_bram + k_bram)); fi
        if [[ -n "${k_wns}" ]]; then
            if [[ -z "${app_wns_min}" ]] || awk "BEGIN{exit !(${k_wns} < ${app_wns_min})}"; then
                app_wns_min="${k_wns}"
            fi
        fi
        if [[ -n "${k_whs}" ]]; then
            if [[ -z "${app_whs_min}" ]] || awk "BEGIN{exit !(${k_whs} < ${app_whs_min})}"; then
                app_whs_min="${k_whs}"
            fi
        fi
        app_cdc_c=$((app_cdc_c + k_cdc_c))
        app_cdc_w=$((app_cdc_w + k_cdc_w))
        log "    ${app}/${k}: LUT=${k_lut} FF=${k_ff} WNS=${k_wns}"
    done

    t1=$(date +%s)
    local secs=$((t1 - t0))
    echo "${app},${nk},${app_lut},${app_ff},${app_dsp},${app_bram},${app_wns_min:-?},${app_whs_min:-?},${app_cdc_c},${app_cdc_w},${secs}" >>"${CSV}"
    ok "  ${app}: LUT=${app_lut} FF=${app_ff} WNS=${app_wns_min} ns (${secs}s)"
}
export -f run_one_app extract_util_field log warn err ok c_blue c_yel c_red c_green
export OPENCLICKNP_ROOT WORK PNR_REPORT TARGET_PART CLOCK_NS BUILD_DIR CSV

APPS=()
for d in "${OPENCLICKNP_ROOT}"/examples/*/; do
    APPS+=("$(basename "${d%/}")")
done

log "running per-kernel P&R on ${#APPS[@]} apps with ${PARALLEL}-way parallelism on ${TARGET_PART}"
printf '%s\n' "${APPS[@]}" \
    | xargs -n1 -P "${PARALLEL}" -I{} bash -c 'run_one_app "$@"' _ {}

ok "all apps done; per-app reports in ${PNR_REPORT}, summary at ${CSV}"
