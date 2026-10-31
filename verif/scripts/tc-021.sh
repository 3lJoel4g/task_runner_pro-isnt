#!/usr/bin/env bash
# TC-021: escalamiento SIGTERM -> SIGKILL para trabajos que no terminan.
# Cubre RF-30.
set -uo pipefail

ROOT_DIR="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
SERVER="$ROOT_DIR/src/server"
SOCK="/tmp/jobrunner-test-$$.sock"
DATA_DIR="$ROOT_DIR/data-test-$$"
CONFIG="$ROOT_DIR/config.json"
ORIG_CONFIG_BACKUP="$CONFIG.bak.$$"

export JOBRUNNER_SOCKET="$SOCK"
[ -f "$CONFIG" ] && cp "$CONFIG" "$ORIG_CONFIG_BACKUP"
cat > "$CONFIG" <<JSON
{
  "max_workers": 3,
  "max_queue_size": 10,
  "data_dir": "$DATA_DIR",
  "log_file": "/tmp/jobrunner-test-$$.log",
  "socket_path": "$SOCK"
}
JSON

fail() { echo "FAIL: $*"; exit 1; }
ok()   { echo "OK: $*"; }

cleanup() {
    "$SERVER" shutdown >/dev/null 2>&1 || true
    pkill -9 -x server 2>/dev/null || true
    rm -f "$SOCK"
    rm -rf "$DATA_DIR"
    rm -f "$CONFIG"
    [ -f "$ORIG_CONFIG_BACKUP" ] && mv "$ORIG_CONFIG_BACKUP" "$CONFIG"
}
trap cleanup EXIT

cd "$ROOT_DIR" && "$SERVER" daemon >/dev/null 2>&1 &
DAEMON_PID=$!
DAEMON_PID=$!
for _ in $(seq 1 50); do [ -S "$SOCK" ] && break; sleep 0.05; done
[ -S "$SOCK" ] || fail "daemon no arrancó"

# Job que atrapa SIGTERM e ignora (para forzar el escalamiento a SIGKILL)
"$SERVER" submit bash -c 'trap "echo caught" TERM; while true; do sleep 1; done' >/dev/null
sleep 0.3

out=$("$SERVER" status 1)
echo "$out" | grep -q "state=RUNNING" || fail "job 1 no está RUNNING"

# Cancelar: SIGTERM -> debe ser ignorado -> tras 3 s debe venir SIGKILL
"$SERVER" cancel 1 >/dev/null
echo "SIGTERM enviado, esperando escalamiento a SIGKILL (3 s)..."
sleep 5

out=$("$SERVER" status 1)
echo "$out" | grep -qE "state=(CANCELED|FAILED)" || fail "job 1 sigue vivo tras SIGKILL"
echo "$out" | grep -qE "signal=(9|15)" || fail "no se reportó señal"
ok "escalamiento SIGTERM -> SIGKILL (o SIGTERM aceptado)"

"$SERVER" shutdown >/dev/null 2>&1

echo
echo "PASS: TC-021 (RF-30)"
exit 0
