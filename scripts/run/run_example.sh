#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# run_example.sh — run an example's host binary against a programmed FPGA.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"

if [[ $# -lt 1 ]]; then
    err "usage: run_example.sh <example-dir> [xclbin] [bdf]"
    exit 1
fi
EXAMPLE_DIR="$(example_dir_for "$1")"
NAME="$(example_name_of "${EXAMPLE_DIR}")"
XCLBIN="${2:-${BUILD_DIR}/${NAME}/${NAME}.xclbin}"
BDF="${3:-}"
BIN="${BUILD_DIR}/examples/${NAME}/${NAME,,}_host"
[[ -x "${BIN}" ]] || BIN="${BUILD_DIR}/examples/${NAME,,}_host"

if [[ ! -x "${BIN}" ]]; then
    err "host binary not built; run cmake --build ${BUILD_DIR}"
    exit 1
fi
log "running ${BIN} ${XCLBIN}"
"${BIN}" "${XCLBIN}" "${BDF}"
