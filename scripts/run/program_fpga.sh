#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# program_fpga.sh — load an .xclbin onto the U50 via xbutil.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"

if [[ $# -lt 1 ]]; then
    err "usage: program_fpga.sh <xclbin> [bdf]"
    exit 1
fi
XCLBIN="$1"
BDF="${2:-}"

require_cmd xbutil

if [[ -n "${BDF}" ]]; then
    xbutil program --device "${BDF}" --user "${XCLBIN}"
else
    xbutil program --user "${XCLBIN}"
fi
ok "FPGA programmed with ${XCLBIN}"
