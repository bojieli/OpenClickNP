#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Shared helpers used by all OpenClickNP scripts.

set -euo pipefail

# Repository root, regardless of where the script is invoked from.
OPENCLICKNP_ROOT="${OPENCLICKNP_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
export OPENCLICKNP_ROOT

# Pinned platform/board defaults.
: "${VITIS_VERSION:=2025.2}"
: "${XILINX_DIR:=/opt/Xilinx/${VITIS_VERSION}}"
: "${PLATFORM:=u50_xdma}"
: "${PLATFORM_VITIS:=xilinx_u50_gen3x16_xdma_5_202210_1}"
: "${USER_CLOCK_HZ:=322265625}"
: "${BUILD_DIR:=${OPENCLICKNP_ROOT}/build}"
: "${GENERATED_DIR_NAME:=generated}"

c_red()   { printf '\033[1;31m%s\033[0m\n' "$*"; }
c_green() { printf '\033[1;32m%s\033[0m\n' "$*"; }
c_yel()   { printf '\033[1;33m%s\033[0m\n' "$*"; }
c_blue()  { printf '\033[1;34m%s\033[0m\n' "$*"; }

log()  { c_blue  "[openclicknp] $*" >&2; }
warn() { c_yel   "[openclicknp WARN ] $*" >&2; }
err()  { c_red   "[openclicknp ERROR] $*" >&2; }
ok()   { c_green "[openclicknp OK   ] $*" >&2; }

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        err "required command not found: $1"
        return 1
    fi
}

source_vivado_env() {
    if [[ -z "${VIVADO_SOURCED:-}" ]]; then
        if [[ -f "${XILINX_DIR}/Vitis/settings64.sh" ]]; then
            # shellcheck disable=SC1091
            source "${XILINX_DIR}/Vitis/settings64.sh"
            export VIVADO_SOURCED=1
        else
            warn "Vitis settings64.sh not found at ${XILINX_DIR}/Vitis/settings64.sh"
            warn "Vivado/Vitis steps will be unavailable"
        fi
    fi
}

example_dir_for() {
    local example="$1"
    if [[ -d "$example" ]]; then
        echo "$(cd "$example" && pwd)"
    else
        echo "${OPENCLICKNP_ROOT}/examples/${example}"
    fi
}

example_name_of() {
    basename "$(example_dir_for "$1")"
}

generated_dir() {
    local example="$1"
    echo "${BUILD_DIR}/$(example_name_of "$example")/${GENERATED_DIR_NAME}"
}

build_dir_for() {
    local example="$1"
    echo "${BUILD_DIR}/$(example_name_of "$example")"
}
