#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Automated U50 XDMA platform install for Vitis 2025.2.
#
# Downloads and installs xilinx-u50-gen3x16-xdma_*.deb into
# /opt/xilinx/platforms/. Requires sudo and ~3 GB disk space.
#
# Usage: ./scripts/platform/install_u50_platform.sh [--dry-run]
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"

DRY_RUN=0
[[ "${1:-}" == "--dry-run" ]] && DRY_RUN=1

# Pinned for reproducibility; bump when AMD ships a newer compatible package.
URL_BASE="https://download.amd.com/opendownload/xlnx"
PKGS=(
    "xilinx-u50-gen3x16-xdma_5.202210.1_22.04-amd64.deb"
    "xilinx-u50-gen3x16-xdma-validate_5.202210.1_22.04.deb"
)

WORK=/tmp/openclicknp-platform-install
mkdir -p "${WORK}"

if [[ -d /opt/xilinx/platforms/xilinx_u50_gen3x16_xdma_5_202210_1 ]]; then
    ok "U50 XDMA platform 5.202210.1 already installed"
    exit 0
fi

# Disk-space check.
AVAIL_KB=$(df --output=avail / | tail -1)
if (( AVAIL_KB < 5000000 )); then
    err "<5 GB free on /; cannot proceed"
    exit 1
fi

if (( DRY_RUN )); then
    log "DRY RUN — would download:"
    for p in "${PKGS[@]}"; do echo "  ${URL_BASE}/${p}"; done
    log "and: sudo apt install ./<file>.deb"
    exit 0
fi

require_cmd curl
require_cmd sudo

for pkg in "${PKGS[@]}"; do
    if [[ ! -f "${WORK}/${pkg}" ]]; then
        log "downloading ${pkg}"
        curl -fL --progress-bar "${URL_BASE}/${pkg}" -o "${WORK}/${pkg}"
    fi
done

log "installing platform packages"
cd "${WORK}"
sudo apt-get update -qq
sudo apt-get install -y "${PKGS[@]/#/./}"

if [[ -d /opt/xilinx/platforms/xilinx_u50_gen3x16_xdma_5_202210_1 ]]; then
    ok "U50 platform installed at /opt/xilinx/platforms/"
else
    err "platform install completed but expected directory not found"
    exit 1
fi
