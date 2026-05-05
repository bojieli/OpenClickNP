#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# run_unit.sh — L1 unit tests (compiler + runtime).
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"

if [[ ! -d "${BUILD_DIR}" ]]; then
    log "configuring build"
    cmake -B "${BUILD_DIR}" -DOPENCLICKNP_BUILD_TESTS=ON
fi
log "building"
cmake --build "${BUILD_DIR}" -j
log "running L1 unit tests"
ctest --test-dir "${BUILD_DIR}" --output-on-failure
ok "L1 done"
