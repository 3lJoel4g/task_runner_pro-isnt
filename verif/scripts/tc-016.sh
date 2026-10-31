#!/usr/bin/env bash
# TC-016: rechazo explícito cuando la cola está llena (RF-25, RF-23).
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
  "max_workers": 1,
  "max_queue_size": 2,
  "data_dir": "$DATA_DIR",
  "log_file": "/tmp/jobrunner-test-$$.log",
  "socket_path": "$SOCK"
}
JSON

fail() { echo "FAIL: $*"; exit 1; }
ok()   { echo "OK: $*"; }

cleanup() {
    "$SERVER" shutdown >/dev/null 2>&1 || true
    for _i in $(seq 1 20); do ps -p "$DAEMON_PID" >/dev/null 2>&1 || break; sleep 0.1; done
    kill -9 "$DAEMON_PID" 2>/dev/null || true
    pkill -9 -x server 2>/dev/null || true
    rm -f "$SOCK"
    rm -rf "$DATA_DIR"
    rm -f "$CONFIG"
    [ -f "$ORIG_CONFIG_BACKUP" ] && mv "$ORIG_CONFIG_BACKUP" "$CONFIG"
}
trap cleanup EXIT

cd "$ROOT_DIR" || fail "no puedo entrar a ROOT_DIR"
"$SERVER" daemon >/dev/null 2>&1 &
DAEMON_PID=$!
for _ in $(seq 1 50); do [ -S "$SOCK" ] && break; sleep 0.05; done
[ -S "$SOCK" ] || fail "daemon no arrancó"

# ---------- Llenar el sistema: 1 RUNNING + 2 QUEUED ----------
out=$("$SERVER" submit sleep 600)
echo "$out" | grep -q "state=RUNNING" || fail "1er submit no RUNNING: $out"
sleep 0.2

out=$("$SERVER" submit sleep 600)
echo "$out" | grep -q "state=QUEUED" || fail "2do submit no QUEUED: $out"

out=$("$SERVER" submit sleep 600)
echo "$out" | grep -q "state=QUEUED" || fail "3er submit no QUEUED: $out"

# Verificar estado con health (1 RUNNING + 2 QUEUED)
sleep 0.3
out=$("$SERVER" health)
echo "$out" | grep -q "running=1" || fail "esperaba 1 running antes del 4to submit"
echo "$out" | grep -q "queued=2"  || fail "esperaba 2 queued antes del 4to submit"
ok "sistema lleno (1 RUNNING + 2 QUEUED)"

# ---------- 4to debe ser rechazado ----------
out=$("$SERVER" submit sleep 600)
echo "$out" | grep -q "ERR queue full"      || fail "4to submit no fue rechazado: $out"
echo "$out" | grep -q "max_queue_size=2"    || fail "rechazo no reporta max_queue_size: $out"
echo "$out" | grep -q "running=1"           || fail "rechazo no reporta running: $out"
echo "$out" | grep -q "queued=2"            || fail "rechazo no reporta queued: $out"
ok "rechazo explícito con diagnóstico (ERR queue full)"

# ---------- 5. Tras liberar un slot, debe aceptar de nuevo ----------
"$SERVER" cancel 1 >/dev/null
sleep 1.5
out=$("$SERVER" submit sleep 600)
echo "$out" | grep -q "OK id=" || fail "no se recuperó tras liberar slot: $out"
ok "acepta de nuevo al liberarse un slot"

"$SERVER" shutdown >/dev/null 2>&1

echo
echo "PASS: TC-016 (RF-23, RF-25)"
exit 0
