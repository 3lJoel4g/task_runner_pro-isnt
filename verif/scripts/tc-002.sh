#!/usr/bin/env bash
# TC-002: CLI + daemon: submit/status/list/cancel vía socket Unix.
set -uo pipefail

ROOT_DIR="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
SERVER="$ROOT_DIR/src/server"
SOCK="/tmp/jobrunner-test-$$.sock"
export JOBRUNNER_SOCKET="$SOCK"

fail() { echo "FAIL: $*"; exit 1; }
ok()   { echo "OK: $*"; }

cleanup() {
    "$SERVER" shutdown >/dev/null 2>&1 || true
    wait "$DAEMON_PID" 2>/dev/null || true
    rm -f "$SOCK"
}
trap cleanup EXIT

# Arrancar daemon en background
"$SERVER" daemon >/dev/null 2>&1 &
DAEMON_PID=$!

for _ in $(seq 1 50); do [ -S "$SOCK" ] && break; sleep 0.05; done
[ -S "$SOCK" ] || fail "daemon no creó el socket"

# 1) submit + IDs únicos
out=$("$SERVER" submit echo hola) || fail "submit falló"
echo "$out" | grep -q "OK id=1" || fail "no devolvió id=1"
out=$("$SERVER" submit bash -c 'exit 3') || fail "submit 2 falló"
echo "$out" | grep -q "OK id=2" || fail "no devolvió id=2"
sleep 0.3

# 2) status refleja exit code
out=$("$SERVER" status 2)
echo "$out" | grep -q "state=FAILED" || fail "job 2 no FAILED"
echo "$out" | grep -q "exit=3"       || fail "job 2 exit code no propagado"

# 3) list
out=$("$SERVER" list)
echo "$out" | grep -q "OK count=2" || fail "list no reportó 2 jobs"

# 4) filtro
out=$("$SERVER" list FAILED)
echo "$out" | grep -q "OK count=1" || fail "filtro FAILED incorrecto"

# 5) cancelación
"$SERVER" submit sleep 30 >/dev/null
sleep 0.2
out=$("$SERVER" cancel 3)
echo "$out" | grep -q "OK cancel requested" || fail "cancel no aceptado"
sleep 0.5
out=$("$SERVER" status 3)
echo "$out" | grep -qE "state=(CANCELED|FAILED)" || fail "job 3 no quedó terminal"

# 6) comando inválido no tumba servicio
"$SERVER" submit /ruta/que/no/existe >/dev/null
sleep 0.3
out=$("$SERVER" status 4)
echo "$out" | grep -q "state=FAILED" || fail "job 4 no FAILED"
out=$("$SERVER" submit echo sigue-vivo)
echo "$out" | grep -q "OK id=5" || fail "servicio se cayó"

# 7) shutdown
out=$("$SERVER" shutdown)
echo "$out" | grep -q "OK shutting down" || fail "shutdown no respondió OK"

ok "TC-002 completo"
echo
echo "PASS: TC-002 (RF-01, RF-04, RF-06, RF-07, RF-08, RF-09, RF-10)"
exit 0
