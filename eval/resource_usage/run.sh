#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Per-element FPGA resource estimates via Vitis HLS C synthesis.
#
# Output: eval/reports/resource_usage.csv with one row per element:
#   element_name,LUT,FF,DSP,BRAM,Fmax_MHz,II
#
# In Vitis 2025.2, vitis_hls is launched via Vitis loader and needs Vivado
# settings sourced. By default we target xcvu9p (Virtex UltraScale+) since
# the U50 device libraries may not be present on every install — the LUT
# numbers are representative of the same family on Alveo U50.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../../scripts/lib/common.sh"

if [[ -f /home/ubuntu/Xilinx/2025.2/Vivado/settings64.sh ]]; then
    source /home/ubuntu/Xilinx/2025.2/Vivado/settings64.sh >/dev/null 2>&1 || true
fi

REPORT_DIR="${OPENCLICKNP_ROOT}/eval/reports"
WORK_DIR="${BUILD_DIR}/eval_resource_usage"
mkdir -p "${REPORT_DIR}" "${WORK_DIR}"

CSV="${REPORT_DIR}/resource_usage.csv"
echo "element,LUT,FF,DSP,BRAM,Fmax_MHz,II" >"${CSV}"

HLS_LAUNCHER="/home/ubuntu/Xilinx/2025.2/Vitis/bin/loader"
TARGET_PART="${TARGET_PART:-xcvu9p-flga2104-2-i}"
CLOCK_NS="${CLOCK_NS:-3.106}"   # 322.265625 MHz

if [[ ! -x "${HLS_LAUNCHER}" ]]; then
    err "Vitis HLS loader not found at ${HLS_LAUNCHER}"
    exit 1
fi

# Limit which elements to synthesize via the SUBSET env var. Default: all.
SUBSET="${SUBSET:-}"

elements=()
while IFS= read -r f; do
    name="$(basename "$f" .clnp)"
    if [[ -n "${SUBSET}" ]] && ! [[ ",${SUBSET}," == *",${name},"* ]]; then continue; fi
    elements+=("${name}")
done < <(find "${OPENCLICKNP_ROOT}/elements" -name '*.clnp' | sort)

log "evaluating ${#elements[@]} element(s) on ${TARGET_PART}"

extract_field() {
    # Pull "Total | ... | FF | LUT | URAM" from the resource summary table.
    local rpt="$1" field="$2"
    awk -v F="${field}" '
        /^\|Total/ {
            gsub(/^\|/,""); gsub(/\|$/,"");
            n=split($0,a,"|");
            for(i=1;i<=n;i++) gsub(/^ *| *$/,"",a[i]);
            # column order in the table: Name | BRAM_18K | DSP | FF | LUT | URAM
            if (F == "BRAM") print a[2]
            else if (F == "DSP")  print a[3]
            else if (F == "FF")   print a[4]
            else if (F == "LUT")  print a[5]
            exit
        }' "${rpt}"
}

for elem in "${elements[@]}"; do
    src="${WORK_DIR}/${elem}.clnp"
    elem_path=$(find "${OPENCLICKNP_ROOT}/elements" -name "${elem}.clnp" -printf '%P\n' | head -1)
    cat >"${src}" <<EOF
import "${elem_path}";

${elem} :: u__inst
EOF
    out="${WORK_DIR}/${elem}_gen"
    rm -rf "${out}"
    if ! "${OPENCLICKNP_ROOT}/build/compiler/openclicknp-cc" \
            -I "${OPENCLICKNP_ROOT}/elements" -o "${out}" "${src}" >/dev/null 2>&1; then
        echo "${elem},compile_failed,,,,," >>"${CSV}"
        warn "  ${elem}: compile failed"
        continue
    fi

    proj="${WORK_DIR}/${elem}_proj"
    rm -rf "${proj}"
    tcl="${WORK_DIR}/${elem}.tcl"
    cat >"${tcl}" <<EOF
open_project ${proj}
add_files ${out}/kernels/u__inst.cpp -cflags "-I ${OPENCLICKNP_ROOT}/runtime/include -std=c++17"
set_top u__inst
open_solution -reset solution1
set_part ${TARGET_PART}
create_clock -period ${CLOCK_NS}
config_compile -ignore_long_run_time
csynth_design
exit
EOF

    if (cd "${WORK_DIR}" && ${HLS_LAUNCHER} -exec vitis_hls -f "${tcl}" >"${WORK_DIR}/${elem}.log" 2>&1); then
        # Vitis HLS sanitizes the top function name (collapses __ → _).
        rpt="${proj}/solution1/syn/report/u_inst_csynth.rpt"
        if [[ -f "${rpt}" ]]; then
            LUT=$(extract_field "${rpt}" LUT)
            FF=$(extract_field "${rpt}" FF)
            DSP=$(extract_field "${rpt}" DSP)
            BRAM=$(extract_field "${rpt}" BRAM)
            FMAX=$( (grep -oE 'Estimated Fmax: [0-9.]+ MHz' "${WORK_DIR}/${elem}.log" || true) \
                   | tail -1 | awk '{print $3}')
            II=$( (grep -m1 -oE 'achieved \| *[0-9]+' "${rpt}" || true) | tail -1 | awk '{print $NF}')
            [[ -n "${II}" ]] || II="1"
            echo "${elem},${LUT:-?},${FF:-?},${DSP:-?},${BRAM:-?},${FMAX:-?},${II:-?}" >>"${CSV}"
            ok "  ${elem}: LUT=${LUT:-?} FF=${FF:-?} DSP=${DSP:-?} BRAM=${BRAM:-?} Fmax=${FMAX:-?}"
        else
            echo "${elem},report_missing,,,,," >>"${CSV}"
        fi
    else
        echo "${elem},synth_failed,,,,," >>"${CSV}"
        warn "  ${elem}: synth failed"
    fi
done

ok "wrote ${CSV}"
