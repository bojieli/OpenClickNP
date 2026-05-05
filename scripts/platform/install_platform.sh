#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# install_platform.sh — install the U50 Vitis platform package.
#
# The exact URL and checksum will depend on the Vitis release. This is a
# documented placeholder; users should download the platform package from
# AMD's Alveo support page corresponding to their chosen Vitis version.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"

PLATFORM_NAME="${1:-${PLATFORM_VITIS}}"

if [[ -d "/opt/xilinx/platforms/${PLATFORM_NAME}" ]]; then
    ok "${PLATFORM_NAME} already installed at /opt/xilinx/platforms/"
    exit 0
fi

err "Platform ${PLATFORM_NAME} not installed."
cat <<'NOTE'
Download the U50 platform package from AMD's Alveo support page:

  https://www.xilinx.com/products/boards-and-kits/alveo/u50.html#getting-started

Files needed:
  - xilinx-u50-gen3x16-xdma-base_*.deb
  - xilinx-u50-gen3x16-xdma-shell_*.deb
  - xilinx-u50-gen3x16-xdma-validate_*.deb

Then:
  sudo apt install ./xilinx-u50-gen3x16-xdma-*_amd64.deb

For QDMA platform: replace `xdma` with `qdma` in the file names.
NOTE
exit 1
