#!/usr/bin/env bash
# TC-007: persistencia atómica + recuperación tras reinicio.
# Cubre RF-12, RF-13, RNF-10, RNF-11, RNF-28.
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
    pkill -9 -x server 2>/dev/null || true
    rm -f "$SOCK"
    rm -rf "$DATA_DIR"
    rm -f "$CONFIG"
    [ -f "$ORIG_CONFIG_BACKUP" ] && mv "$ORIG_CONFIG_BACKUP" "$CONFIG"
}
trap cleanup EXIT

cd "$ROOT_DIR" || fail "no puedo entrar a ROOT_DIR"

# ========== PARTE 1: recuperación de RUNNING ==========
"$SERVER" daemon >/dev/null 2>&1 &
DAEMON_PID=$!
for _ in $(seq 1 50); do [ -S "$SOCK" ] && break; sleep 0.05; done
[ -S "$SOCK" ] || fail "daemon no arrancó (1ra vez)"

"$SERVER" submit sleep 600 >/dev/null
"$SERVER" submit sleep 600 >/dev/null
sleep 0.5

out=$("$SERVER" list)
echo "$out" | grep -q "count=2" || fail "no hay 2 jobs RUNNING"
ok "2 jobs RUNNING persistidos"

[ -f "$DATA_DIR/jobs.json" ] || fail "jobs.json no existe"
ok "jobs.json existe en disco"

# Kill -9 al daemon (usar pkill -9 -f para matar el proceso real)
pkill -9 -f "src/server daemon" 2>/dev/null || true
wait "$DAEMON_PID" 2>/dev/null
sleep 0.5
ps -p "$DAEMON_PID" >/dev/null 2>&1 && fail "daemon sigue vivo tras kill -9"

grep -q "RUNNING" "$DATA_DIR/jobs.json" || fail "jobs.json no conserva RUNNING tras caída"
ok "jobs.json conserva RUNNING tras caída (persistencia atómica)"

# Relanzar
rm -f "$SOCK"
"$SERVER" daemon >/dev/null 2>&1 &
DAEMON_PID=$!
for _ in $(seq 1 50); do [ -S "$SOCK" ] && break; sleep 0.05; done
[ -S "$SOCK" ] || fail "daemon no relanzó"

out=$("$SERVER" list)
echo "$out" | grep -q "state=RUNNING" && fail "hay RUNNING fantasma tras reinicio"
echo "$out" | grep -q "state=FAILED"  || fail "no hay FAILED tras reinicio"
echo "$out" | grep -q "note=interrupted by restart" || fail "no se marcó 'interrupted by restart'"
ok "recuperación: RUNNING -> FAILED (interrupted by restart)"

# ========== PARTE 2: QUEUED se preserva ==========
# Saturar con 4 jobs para que el 4to quede QUEUED
"$SERVER" submit sleep 600 >/dev/null
"$SERVER" submit sleep 600 >/dev/null
"$SERVER" submit sleep 600 >/dev/null
out=$("$SERVER" submit sleep 600)
echo "$out" | grep -q "state=QUEUED" || fail "4to submit no quedó QUEUED: $out"
sleep 0.5

grep -q "QUEUED" "$DATA_DIR/jobs.json" || fail "no hay QUEUED en jobs.json"

# Kill -9 y relanzar
pkill -9 -f "src/server daemon" 2>/dev/null || true
wait "$DAEMON_PID" 2>/dev/null
sleep 0.5
rm -f "$SOCK"

"$SERVER" daemon >/dev/null 2>&1 &
DAEMON_PID=$!
for _ in $(seq 1 50); do [ -S "$SOCK" ] && break; sleep 0.05; done
[ -S "$SOCK" ] || fail "daemon no relanzó (2da vez)"

# El QUEUED debe reencolarse (puede estar RUNNING si había slots, o seguir QUEUED)
out=$("$SERVER" list)
echo "$out" | grep -qE "state=(QUEUED|RUNNING)" || fail "QUEUED no se preservó ni se relanzó"
ok "QUEUED se preserva tras reinicio"

"$SERVER" shutdown >/dev/null 2>&1 || true

echo
echo "PASS: TC-007 (RF-12, RF-13, RNF-10, RNF-11, RNF-28)"
exit 0
