#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# run_all_tests.sh — run every test that doesn't need an FPGA.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/lib/common.sh"

if [[ ! -d "${BUILD_DIR}" ]]; then
    log "configuring fresh build"
    cmake -B "${BUILD_DIR}" -DOPENCLICKNP_BUILD_TESTS=ON
fi
log "building"
cmake --build "${BUILD_DIR}" -j

log "L1 unit tests"
ctest --test-dir "${BUILD_DIR}" --output-on-failure

log "L2 SW-emu — PassTraffic"
"${SCRIPT_DIR}/sim/run_emu.sh" examples/PassTraffic

ok "All software tests passed"
