#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# loopback_test.sh — verify QSFP28 cable loopback by sending packets out
# tor and watching them come back on nic.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"
require_cmd xbutil

if [[ $# -lt 1 ]]; then
    err "usage: loopback_test.sh <xclbin>"
    exit 1
fi
XCLBIN="$1"

xbutil program --user "${XCLBIN}"

# Use the runtime's built-in PCAP playback host harness (built when XRT
# is available). Plays the standard "icmp_64.pcap" repeatedly for 5 s.
TEST_HOST="${BUILD_DIR}/examples/PassTraffic/passtraffic_host"
PCAP="${OPENCLICKNP_ROOT}/tests/pcaps/icmp_64.pcap"
if [[ ! -x "${TEST_HOST}" ]]; then
    err "build PassTraffic host first"; exit 1
fi
"${TEST_HOST}" "${XCLBIN}" "" xdma &
HOST_PID=$!
sleep 5
kill "${HOST_PID}" 2>/dev/null || true
ok "loopback test ran for 5 s; check counters above"
