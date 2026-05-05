#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# flash_shell.sh — one-time U50 shell flash. Reboot required afterwards.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"
require_cmd xbutil

PLATFORM_NAME="${1:-${PLATFORM_VITIS}}"
warn "About to flash the U50 with platform ${PLATFORM_NAME}."
warn "This is a one-time operation. The host must be REBOOTED afterwards."
read -rp "Continue? [y/N] " ans
[[ "${ans}" == "y" ]] || exit 1

xbutil program --base --user "${PLATFORM_NAME}"
ok "Flash complete. Reboot the host to load the new shell."
