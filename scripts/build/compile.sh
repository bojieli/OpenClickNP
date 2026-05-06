#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# compile.sh — run openclicknp-cc on an example's topology.clnp and emit
# all backends (HLS C++, SystemC, SW emu, Verilator, v++ link, XRT host).
#
# Usage: scripts/build/compile.sh <example-dir> [--platform u50_xdma|u50_qdma]
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"

if [[ $# -lt 1 ]]; then
    err "usage: compile.sh <example-dir> [--platform u50_xdma|u50_qdma]"
    exit 1
fi

EXAMPLE="$1"; shift
EXAMPLE_DIR="$(example_dir_for "${EXAMPLE}")"
PLATFORM_ARG="${PLATFORM}"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --platform) PLATFORM_ARG="$2"; shift 2;;
        *) err "unknown option $1"; exit 1;;
    esac
done

CC="${OPENCLICKNP_ROOT}/build/compiler/openclicknp-cc"
if [[ ! -x "${CC}" ]]; then
    err "openclicknp-cc not built. Run: cmake --build ${BUILD_DIR}"
    exit 1
fi

OUT="$(generated_dir "${EXAMPLE_DIR}")"
mkdir -p "${OUT}"

log "compiling ${EXAMPLE_DIR}/topology.clnp → ${OUT} (platform=${PLATFORM_ARG})"
"${CC}" "${EXAMPLE_DIR}/topology.clnp" \
    -o "${OUT}" \
    -I "${OPENCLICKNP_ROOT}/elements" \
    --platform "${PLATFORM_ARG}"
ok "compile complete"
