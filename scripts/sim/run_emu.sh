#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# run_emu.sh — L2 software emulator: compile and run the SW-emu topology
# for an example, replaying a PCAP if provided.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../lib/common.sh"

if [[ $# -lt 1 ]]; then
    err "usage: run_emu.sh <example-dir> [pcap]"
    exit 1
fi
EXAMPLE_DIR="$(example_dir_for "$1")"
NAME="$(example_name_of "${EXAMPLE_DIR}")"
PCAP="${2:-}"

# Step 1: compile to generated/.
"${SCRIPT_DIR}/../build/compile.sh" "${EXAMPLE_DIR}"

GEN="$(generated_dir "${EXAMPLE_DIR}")"
BD="$(build_dir_for "${EXAMPLE_DIR}")"
mkdir -p "${BD}/swemu"

# Step 2: compile the generated SW emu topology against the runtime.
log "compiling SW emulator binary"
SHIM="${BD}/swemu/main.cpp"
cat >"${SHIM}" <<'EOF'
extern int openclicknp_sw_emu_main(int, char**);
int main(int argc, char** argv) { return openclicknp_sw_emu_main(argc, argv); }
EOF
g++ -std=c++17 -O2 -Wall \
    -I "${OPENCLICKNP_ROOT}/runtime/include" \
    -DOPENCLICKNP_HAS_XRT=0 \
    "${SHIM}" \
    "${GEN}/sw_emu/topology.cpp" \
    "${OPENCLICKNP_ROOT}/runtime/src/pcap.cpp" \
    -o "${BD}/swemu/${NAME}_swemu" \
    -lpthread

ok "built ${BD}/swemu/${NAME}_swemu"
log "running"
"${BD}/swemu/${NAME}_swemu" "${PCAP}" || true
ok "L2 done"
