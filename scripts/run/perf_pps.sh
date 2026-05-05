#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# perf_pps.sh — print pps and Gbps measured via XRT counters every second.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"
require_cmd xbutil
xbutil examine --report kernel-stats --watch
