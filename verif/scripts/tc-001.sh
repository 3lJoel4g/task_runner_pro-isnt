#!/usr/bin/env bash
# TC-001: Aceptar trabajo con argumentos y validar la solicitud.
# Cubre RF-01 (aceptar comando + ID) y RF-02 (validar entradas).
set -uo pipefail

ROOT_DIR="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
SERVER="$ROOT_DIR/src/server"

fail() { echo "FAIL: $*"; exit 1; }
ok()   { echo "OK: $*"; }

[ -x "$SERVER" ] || fail "no existe binario $SERVER"

# ---------- Caso 1: comando válido → exit 0 + SUCCEEDED + ID ----------
out="$("$SERVER" echo hola 2>&1)"; rc=$?
[ "$rc" -eq 0 ]                    || fail "caso 1: exit esperado 0, obtenido $rc"
echo "$out" | grep -q "hola"       || fail "caso 1: stdout no contiene 'hola'"
echo "$out" | grep -q "SUCCEEDED"  || fail "caso 1: no reporta SUCCEEDED"
echo "$out" | grep -q "Job ID 1"   || fail "caso 1: no reporta ID 1 (RF-01)"
ok "caso 1: comando válido aceptado con ID (RF-01)"

# ---------- Caso 2: comando con exit != 0 → server exit 1, FAILED ----------
out="$("$SERVER" bash -c 'exit 7' 2>&1)"; rc=$?
[ "$rc" -eq 1 ]                    || fail "caso 2: exit esperado 1 (job falló), obtenido $rc"
echo "$out" | grep -q "FAILED"     || fail "caso 2: no reporta FAILED"
echo "$out" | grep -q "exit_code : 7" || fail "caso 2: no propaga exit_code 7 (RF-07)"
ok "caso 2: exit code del job propagado (RF-07)"

# ---------- Caso 3: sin argumentos → exit 2 + mensaje de uso ----------
out="$("$SERVER" 2>&1)"; rc=$?
[ "$rc" -eq 2 ]                    || fail "caso 3: exit esperado 2 (uso), obtenido $rc"
echo "$out" | grep -qiE "uso|usage" || fail "caso 3: no muestra mensaje de uso"
echo "$out" | grep -qi "comando"    || fail "caso 3: mensaje no menciona 'comando'"
ok "caso 3: rechazo sin argumentos con mensaje útil (RF-02)"

# ---------- Caso 4: comando vacío → exit 2 + mensaje específico ----------
out="$("$SERVER" "" 2>&1)"; rc=$?
[ "$rc" -eq 2 ]                    || fail "caso 4: exit esperado 2, obtenido $rc"
echo "$out" | grep -qi "vac"        || fail "caso 4: no informa comando vacío (RF-02)"
ok "caso 4: rechazo de comando vacío (RF-02)"

# ---------- Caso 5: comando inexistente → job FAILED con exit 127 ----------
out="$("$SERVER" /ruta/que/no/existe 2>&1)"; rc=$?
[ "$rc" -eq 1 ]                    || fail "caso 5: exit esperado 1 (job FAILED), obtenido $rc"
echo "$out" | grep -q "FAILED"     || fail "caso 5: no reporta FAILED"
echo "$out" | grep -q "127"        || fail "caso 5: no propaga exit 127 del child"
ok "caso 5: comando inexistente tratado como job FAILED (RF-02)"

echo
echo "PASS: TC-001 (RF-01, RF-02)"
exit 0
