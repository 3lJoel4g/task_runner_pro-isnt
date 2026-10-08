#!/usr/bin/env bash
# TC-006: Captura separada de stdout/stderr (RF-11) y propagación de
# exit code (RF-07). Provoca el caso que antes causaba deadlock:
# el hijo escribe >PIPE_BUF en stderr mientras el padre aún lee stdout.
set -uo pipefail

ROOT_DIR="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
SERVER="$ROOT_DIR/src/server"

fail() { echo "FAIL: $*"; exit 1; }
pass() { echo "PASS: $*"; }

[ -x "$SERVER" ] || fail "no existe binario $SERVER"

# ---------- Caso 1: captura básica separada ----------
out="$("$SERVER" bash -c 'echo OUT-LINE; echo ERR-LINE 1>&2' 2>/dev/null)"
echo "$out" | grep -q "OUT-LINE" || fail "stdout no capturado"
echo "$out" | grep -q "ERR-LINE" || fail "stderr no capturado"
echo "Caso 1 OK: captura separada"

# ---------- Caso 2: estrés de I/O concurrente (deadlock original) ----------
# ~800 KB por cada stream. Con la lectura secuencial antigua, este caso
# bloquea el padre para siempre (timeout → falla).
timeout 15s "$SERVER" bash -c \
    'yes STDOUT-AAAAAAAAAAAAAAAAAAAAAAAAAAAA | head -n 20000
     yes STDERR-BBBBBBBBBBBBBBBBBBBBBBBBBBBB | head -n 20000 1>&2' \
    >"${TMPDIR:-/tmp}/tc006_stress.out" 2>/dev/null \
    || fail "timeout/exit != 0 en caso de estrés (deadlock en pipes?)"

grep -q "STDOUT-A" "${TMPDIR:-/tmp}/tc006_stress.out" \
    || fail "no capturó stdout grande"
grep -q "STDERR-B" "${TMPDIR:-/tmp}/tc006_stress.out" \
    || fail "no capturó stderr grande"
echo "Caso 2 OK: sin deadlock con >PIPE_BUF en ambos streams"

# ---------- Caso 3: propagación de exit code ----------
"$SERVER" bash -c 'exit 42' >/dev/null 2>&1
rc=$?
[ "$rc" -eq 1 ] || fail "servidor debió salir 1 ante job fallido, salió $rc"
echo "Caso 3 OK: exit code propagado"

pass "TC-006 completo (RF-07, RF-11)"
exit 0
