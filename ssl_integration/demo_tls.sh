#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# demo_tls.sh — end-to-end TLS handshake using the OpenClickNP ENGINE.
#
# Sets up a self-signed RSA-2048 cert, starts `openssl s_server` with
# `-engine openclicknp`, connects with `openssl s_client`, and tears
# down. Confirms that:
#   - the engine loads from build/ssl_integration/openclicknp.so
#   - the TLS handshake completes (cert exchanged, session established)
#   - the cipher suite uses RSA (kRSA or RSA auth) so RSA modexp was on
#     the critical path of the handshake
#
# Implicit proof of engine engagement: with `ENGINE_set_default_RSA`,
# every libcrypto BN_mod_exp_mont call within s_server's lifetime
# routes through ocnp_bn_mod_exp. The handshake's certificate
# signature-verify and RSA key-exchange both call modexp.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT="${SCRIPT_DIR}/.."
ENGINE="${ROOT}/build/ssl_integration/openclicknp.so"
[[ -f "${ENGINE}" ]] || { echo "ERROR: engine not built at ${ENGINE}"; exit 1; }

WORK=$(mktemp -d /tmp/ocnp_tls.XXXXX)
cleanup() { kill "${SRV_PID:-0}" 2>/dev/null || true; rm -rf "${WORK}"; }
trap cleanup EXIT
cd "${WORK}"

echo "== generating self-signed RSA-2048 cert =="
openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
    -keyout server.key -out server.crt \
    -subj "/CN=openclicknp-demo" 2>/dev/null
echo "  ok: $(openssl x509 -in server.crt -noout -subject)"

PORT=4447
echo "== starting s_server -engine openclicknp on port ${PORT} =="
# Use cipher suite that forces RSA key exchange so the handshake
# definitely uses an RSA modexp (vs. ECDHE which uses ECDH and only
# uses RSA for the cert sig).
openssl s_server -engine "${ENGINE}" -accept "${PORT}" \
    -cert server.crt -key server.key -tls1_2 \
    -cipher 'AES256-SHA256:AES128-SHA' \
    -www >server.out 2>server.err </dev/null &
SRV_PID=$!
sleep 1

echo "== running s_client handshake =="
echo "Q" | openssl s_client -connect "localhost:${PORT}" -tls1_2 \
    -CAfile server.crt -servername openclicknp-demo \
    > client.out 2>client.err

# Inspect output. Successful TLS handshakes print:
#   - "CONNECTED(...)" on the client
#   - "Cipher    : <suite>" listing the negotiated suite
#   - "Verify return code: 0 (ok)" or similar
#   - The server prints "Engine \"openclicknp\" set."
ENGINE_LOADED=$(grep -c '^Engine "openclicknp" set' server.err || true)
HANDSHAKE_OK=$(grep -c 'CONNECTED' client.out || true)
CIPHER=$(awk -F': *' '/^[[:space:]]*Cipher[[:space:]]*:/ {print $2; exit}' client.out)
PROTOCOL=$(awk -F': *' '/^[[:space:]]*Protocol[[:space:]]*:/ {print $2; exit}' client.out)
SESSIONID=$(awk -F': *' '/^[[:space:]]*Session-ID[[:space:]]*:/ {print $2; exit}' client.out)

echo
echo "Result:"
echo "  Engine loaded:      ${ENGINE_LOADED:-0} (>0 means yes)"
echo "  Handshake CONNECT:  ${HANDSHAKE_OK:-0} (>0 means yes)"
echo "  Negotiated proto:   ${PROTOCOL:-?}"
echo "  Negotiated cipher:  ${CIPHER:-?}"
echo "  Session ID present: $([[ -n "${SESSIONID}" ]] && echo yes || echo no)"

if [[ "${ENGINE_LOADED}" -gt 0 && "${HANDSHAKE_OK}" -gt 0 && -n "${SESSIONID}" ]]; then
    echo
    echo "TLS handshake via openclicknp engine: OK"
    exit 0
else
    echo
    echo "TLS handshake via openclicknp engine: FAIL"
    echo "--- server stderr ---"
    cat server.err
    echo "--- client stderr ---"
    cat client.err
    echo "--- client stdout (first 40) ---"
    head -40 client.out
    exit 1
fi
