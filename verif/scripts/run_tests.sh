#!/usr/bin/env bash
# Orquestador de pruebas de aceptación JobRunner.
# Compila y ejecuta todos los verif/scripts/tc-*.sh, guardando evidencia
# en verif/results/<run-id>/.
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RUN_ID="$(date +%Y%m%d-%H%M%S)"
RESULT_DIR="$ROOT_DIR/verif/results/$RUN_ID"

mkdir -p "$RESULT_DIR"
echo "==> JobRunner test run: $RUN_ID"
echo "==> Evidencia: $RESULT_DIR"

echo "==> make -C src"
if ! make -C "$ROOT_DIR/src" >"$RESULT_DIR/build.log" 2>&1; then
    echo "FAIL: build falló. Ver $RESULT_DIR/build.log"
    cat "$RESULT_DIR/build.log"
    exit 1
fi
echo "    build OK"

PASSED=0
FAILED=0

shopt -s nullglob
for tc in "$ROOT_DIR"/verif/scripts/tc-*.sh; do
    name="$(basename "$tc" .sh)"
    log="$RESULT_DIR/$name.log"
    echo "==> $name"
    if bash "$tc" "$ROOT_DIR" >"$log" 2>&1; then
        echo "    PASS"
        PASSED=$((PASSED + 1))
    else
        echo "    FAIL (ver $log)"
        FAILED=$((FAILED + 1))
    fi
done

echo
echo "==> Resumen: PASS=$PASSED FAIL=$FAILED"
echo "==> Evidencia: $RESULT_DIR"
[ "$FAILED" -eq 0 ]
