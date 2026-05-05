#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# install_xrt.sh — install Xilinx Runtime (XRT) on Ubuntu 22.04.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"

if ! grep -q '22.04' /etc/os-release 2>/dev/null; then
    warn "this script targets Ubuntu 22.04; proceeding anyway"
fi

XRT_DEB_URL="${XRT_DEB_URL:-https://www.xilinx.com/bin/public/openDownload?filename=xrt_202410.2.16.204_22.04-amd64-xrt.deb}"
TMP_DEB="$(mktemp --suffix=.deb)"
log "downloading XRT package"
curl -fL "${XRT_DEB_URL}" -o "${TMP_DEB}"
log "installing"
sudo apt-get update
sudo apt-get install -y "${TMP_DEB}"
rm -f "${TMP_DEB}"
ok "XRT installed; source /opt/xilinx/xrt/setup.sh in your shell"
