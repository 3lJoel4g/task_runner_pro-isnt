#!/usr/bin/env bash
# TC-002: CLI + daemon con cola FIFO y límite de workers.
# Cubre RF-01, RF-03, RF-04, RF-05, RF-06, RF-07, RF-08, RF-09, RF-10, RF-15, RF-17, RF-23, RF-25.
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
[ -S "$SOCK" ] || fail "daemon no creó el socket"

# ---------- 1. Submit + IDs únicos ----------
out=$("$SERVER" submit echo hola) || fail "submit 1 falló"
echo "$out" | grep -q "OK id=1" || fail "no devolvió id=1"
out=$("$SERVER" submit bash -c 'exit 3') || fail "submit 2 falló"
echo "$out" | grep -q "OK id=2" || fail "no devolvió id=2"
sleep 0.5

out=$("$SERVER" status 2)
echo "$out" | grep -q "state=FAILED" || fail "job 2 no FAILED"
echo "$out" | grep -q "exit=3"       || fail "job 2 exit code no propagado"
ok "submit + ID + exit code"

# ---------- 2. Cola FIFO con límite de workers ----------
"$SERVER" submit sleep 600 >/dev/null
"$SERVER" submit sleep 600 >/dev/null
"$SERVER" submit sleep 600 >/dev/null
out=$("$SERVER" submit sleep 600)      # el 4to debe quedar QUEUED
echo "$out" | grep -q "state=QUEUED" || fail "4to submit no quedó QUEUED (límite no aplicado): $out"
out=$("$SERVER" submit sleep 600)      # el 5to debe quedar QUEUED
echo "$out" | grep -q "state=QUEUED" || fail "5to submit no quedó QUEUED: $out"

sleep 0.5
out=$("$SERVER" health)
echo "$out" | grep -q "running=3" || fail "health no reporta 3 running: $out"
echo "$out" | grep -q "queued=2"  || fail "health no reporta 2 queued: $out"
ok "cola FIFO + límite de workers (3 RUNNING + 2 QUEUED)"

# ---------- 3. Cancelar en cola (sin señal, directo a CANCELED) ----------
# Obtener el primer ID que esté QUEUED (no asumir el 4)
out=$("$SERVER" list QUEUED)
queued_id=$(echo "$out" | grep "state=QUEUED" | head -1 | grep -o "id=[0-9]*" | cut -d= -f2)
[ -n "$queued_id" ] || fail "no hay jobs en QUEUED para cancelar"
echo "Cancelando job QUEUED con id=$queued_id"

out=$("$SERVER" cancel "$queued_id")
echo "$out" | grep -q "cancel requested (QUEUED)" || fail "cancel de QUEUED falló (id=$queued_id): $out"
sleep 0.5
out=$("$SERVER" status "$queued_id")
echo "$out" | grep -q "state=CANCELED" || fail "job $queued_id no quedó CANCELED: $out"
ok "cancel de QUEUED (sin señal)"

# ---------- 3b. Cancelar en ejecución (SIGTERM) ----------
out=$("$SERVER" list RUNNING)
running_id=$(echo "$out" | grep "state=RUNNING" | head -1 | grep -o "id=[0-9]*" | cut -d= -f2)
[ -n "$running_id" ] || fail "no hay jobs en RUNNING para cancelar"

out=$("$SERVER" cancel "$running_id")
echo "$out" | grep -q "cancel requested (RUNNING)" || fail "cancel de RUNNING falló (id=$running_id): $out"
sleep 1.5
out=$("$SERVER" status "$running_id")
echo "$out" | grep -qE "state=(CANCELED|FAILED)" || fail "job $running_id no quedó terminal: $out"
ok "cancel de RUNNING (SIGTERM)"



# ---------- 5. Comando inexistente no tumba el servicio ----------
"$SERVER" submit /ruta/que/no/existe >/dev/null
sleep 0.5
out=$("$SERVER" submit echo sigue-vivo)
echo "$out" | grep -q "OK id=" || fail "servicio se cayó tras comando inexistente"
ok "comando inválido no tumba el servicio"

# ---------- 6. Shutdown limpio ----------
out=$("$SERVER" shutdown)
echo "$out" | grep -q "OK shutting down" || fail "shutdown no respondió OK"

ok "TC-002 completo"
echo
echo "PASS: TC-002 (RF-01, RF-03, RF-04, RF-05, RF-06, RF-07, RF-08, RF-09, RF-10, RF-15, RF-17, RF-23, RF-25)"
exit 0
