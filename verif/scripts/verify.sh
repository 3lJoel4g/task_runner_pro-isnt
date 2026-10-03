#!/usr/bin/env bash
# =============================================================================
# verif/scripts/verify.sh  —  Script de verificación de JobRunner (Avance 1)
#
# Qué hace:
#   1. Construye el servicio desde cero (make clean && make).
#   2. Ejecuta los casos de prueba TC-001..TC-019 contra el binario real,
#      alimentándolo con comandos por stdin y revisando su salida.
#   3. Guarda la evidencia en verif/results/ (bitácora + salida cruda de cada caso).
#   4. Termina con código 0 si TODO pasó, 1 si algo falló, 2 si no compiló.
#
# Uso (desde cualquier carpeta):   ./verif/scripts/verify.sh
# Los casos están descritos en:    verif/test-cases/casos_de_prueba.md
# =============================================================================
set -u
export LC_ALL=C                       # mensajes de ls/strerror en inglés, estables

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="$ROOT/src"
SERVER="$SRC/server"
RES="$ROOT/verif/results"
STAMP="$(date +%Y%m%d_%H%M%S)"
OUT="$RES/$STAMP"                     # salida cruda de cada caso
SUMMARY="$RES/verify_$STAMP.txt"      # bitácora legible (.txt: el .gitignore ignora *.log)
mkdir -p "$OUT"

# --- utilidades --------------------------------------------------------------
send() { printf '%s\n' "$@"; }        # manda cada argumento como una línea al servidor

run() {                               # run <archivo_salida> [args del server]; stdin = comandos
    local f="$1"; shift
    timeout 60 "$SERVER" "$@" > "$f" 2>&1
}

TOTAL=0; PASSED=0; FAILED_LIST=(); TC_ID=""; TC_FAIL=0

begin() { TC_ID="$1"; TC_FAIL=0; echo; echo "== $1: $2"; }

expect() {                            # expect <archivo> <regex> <descripción>
    if grep -Eq -- "$2" "$1"; then
        echo "   ok    - $3"
    else
        echo "   FALLA - $3   (no se encontró /$2/ en $(basename "$1"))"
        TC_FAIL=1
    fi
}

expect_not() {                        # expect_not <archivo> <regex> <descripción>
    if grep -Eq -- "$2" "$1"; then
        echo "   FALLA - $3   (apareció /$2/ en $(basename "$1"))"
        TC_FAIL=1
    else
        echo "   ok    - $3"
    fi
}

assert() {                            # assert <descripción> <comando...>
    local d="$1"; shift
    if "$@"; then echo "   ok    - $d"; else echo "   FALLA - $d"; TC_FAIL=1; fi
}

finish() {
    TOTAL=$((TOTAL + 1))
    if [ "$TC_FAIL" -eq 0 ]; then
        PASSED=$((PASSED + 1)); echo "   => $TC_ID PASA"
    else
        FAILED_LIST+=("$TC_ID"); echo "   => $TC_ID FALLA"
    fi
}

main() {
    echo "JobRunner — verificación $STAMP"
    echo "commit: $(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo 'n/d')  |  $(uname -sr)  |  $(g++ --version 2>/dev/null | head -1)"

    # --- 0. Construcción desde cero -----------------------------------------
    echo; echo "== BUILD: make clean && make"
    if ! ( make -C "$SRC" clean && make -C "$SRC" ) > "$OUT/build.txt" 2>&1; then
        cat "$OUT/build.txt"; echo "FALLA: no compiló"; return 2
    fi
    echo "   ok    - compila con -Wall -Wextra -Werror"

    # script auxiliar: un trabajo que IGNORA SIGTERM (para probar SIGKILL)
    cat > "$OUT/ignora_term.sh" <<'SH'
#!/bin/bash
trap '' TERM
while true; do sleep 1; done
SH
    chmod +x "$OUT/ignora_term.sh"

    # ------------------------------------------------------------------ TC-001
    begin TC-001 "Enviar trabajos devuelve un ID único e incremental"
    f="$OUT/TC-001.txt"
    { send 'submit echo a' 'submit echo b' 'submit echo c'; sleep 1; } | run "$f"
    expect "$f" '^OK id=1$' "primer trabajo recibe id=1"
    expect "$f" '^OK id=2$' "segundo trabajo recibe id=2"
    expect "$f" '^OK id=3$' "tercer trabajo recibe id=3"
    finish

    # ------------------------------------------------------------------ TC-002
    begin TC-002 "Trabajo exitoso: SUCCEEDED, exit 0, stdout capturado"
    f="$OUT/TC-002.txt"
    { send 'submit echo hola mundo'; sleep 1; send 'status 1' 'output 1'; } | run "$f"
    expect "$f" 'id=1 estado=SUCCEEDED' "estado SUCCEEDED"
    expect "$f" 'exit=0 ' "código de salida 0"
    expect "$f" '^hola mundo$' "stdout capturado"
    finish

    # ------------------------------------------------------------------ TC-003
    begin TC-003 "Trabajo que falla: FAILED, exit distinto de 0, stderr capturado"
    f="$OUT/TC-003.txt"
    { send 'submit ls /ruta_que_no_existe'; sleep 1; send 'status 1' 'output 1'; } | run "$f"
    expect "$f" 'id=1 estado=FAILED' "estado FAILED"
    expect "$f" 'exit=2 ' "código de salida 2 (el de ls)"
    expect "$f" 'No such file or directory' "stderr capturado"
    finish

    # ------------------------------------------------------------------ TC-004
    begin TC-004 "Comando inexistente: FAILED/127 y el servicio sigue vivo"
    f="$OUT/TC-004.txt"
    { send 'submit comando_que_no_existe_xyz'; sleep 1
      send 'status 1' 'submit echo sigo_vivo'; sleep 1; send 'status 2' 'output 2'; } | run "$f"
    expect "$f" 'id=1 estado=FAILED' "el comando inexistente termina FAILED"
    expect "$f" 'id=1 estado=FAILED.*exit=127 ' "código 127 (convención 'command not found')"
    expect "$f" 'id=2 estado=SUCCEEDED' "el servicio siguió aceptando y ejecutando trabajos"
    expect "$f" '^sigo_vivo$' "el trabajo posterior produjo su salida"
    expect "$f" 'JobRunner terminado' "el servicio terminó de forma ordenada"
    finish

    # ------------------------------------------------------------------ TC-005
    begin TC-005 "Consultar estado: trabajo existente e inexistente"
    f="$OUT/TC-005.txt"
    { send 'submit sleep 3'; sleep 0.5; send 'status 1' 'status 99' 'cancel 1'; sleep 0.5; } | run "$f"
    expect "$f" 'id=1 estado=RUNNING pid=[0-9]+ exit=- ' "RUNNING muestra pid y exit '-'"
    expect "$f" 'ERROR: no existe el trabajo 99' "id inexistente => ERROR sin caerse"
    finish

    # ------------------------------------------------------------------ TC-006
    begin TC-006 "Listar trabajos"
    f="$OUT/TC-006.txt"
    { send 'submit echo uno' 'submit ls /no_existe' 'submit sleep 3'; sleep 1; send 'list' 'cancel 3'; sleep 0.5; } | run "$f"
    expect "$f" '^ID +ESTADO +PID +EXIT +COMANDO' "encabezado de la tabla"
    expect "$f" '1 +SUCCEEDED .*echo uno' "fila del trabajo 1"
    expect "$f" '2 +FAILED .*ls /no_existe' "fila del trabajo 2"
    expect "$f" '3 +RUNNING .*sleep 3' "fila del trabajo 3"
    finish

    # ------------------------------------------------------------------ TC-007
    begin TC-007 "Cancelar un trabajo en ejecución (SIGTERM => CANCELED, 143)"
    f="$OUT/TC-007.txt"
    { send 'submit sleep 30'; sleep 0.5; send 'cancel 1'; sleep 0.7; send 'status 1'; } | run "$f"
    expect "$f" 'cancelación solicitada para el trabajo 1 \(SIGTERM enviado\)' "se envió SIGTERM"
    expect "$f" 'id=1 estado=CANCELED' "estado final CANCELED"
    expect "$f" 'exit=143 \(señal 15' "código 143 = 128 + SIGTERM(15)"
    finish

    # ------------------------------------------------------------------ TC-008
    begin TC-008 "Cancelar un trabajo que ignora SIGTERM (escala a SIGKILL, 137)"
    f="$OUT/TC-008.txt"
    { send "submit $OUT/ignora_term.sh"; sleep 0.5; send 'cancel 1'; sleep 1; send 'status 1'
      sleep 3.5; send 'status 1'; } | run "$f"
    expect "$f" 'estado=RUNNING' "1 s después del SIGTERM sigue vivo (lo ignoró)"
    expect "$f" 'estado=CANCELED pid=[0-9]+ exit=137 \(señal 9' "tras el plazo muere por SIGKILL (137)"
    finish

    # ------------------------------------------------------------------ TC-009
    begin TC-009 "Cancelar trabajo ya terminado o inexistente => error, sin caerse"
    f="$OUT/TC-009.txt"
    { send 'submit echo x'; sleep 1; send 'cancel 1' 'cancel 99' 'cancel abc' 'list'; } | run "$f"
    expect "$f" 'ERROR: el trabajo 1 ya terminó \(SUCCEEDED\)' "cancelar uno terminado"
    expect "$f" 'ERROR: no existe el trabajo 99' "cancelar uno inexistente"
    expect "$f" 'ERROR: uso: cancel <id>' "id no numérico"
    expect "$f" '1 +SUCCEEDED' "el estado del trabajo terminado no cambió"
    finish

    # ------------------------------------------------------------------ TC-010
    begin TC-010 "Entradas inválidas no terminan el servicio"
    f="$OUT/TC-010.txt"
    { send 'foo bar' 'submit' 'status' 'status abc' 'cancel -1' 'submit sh -c "sin cerrar' '   ' 'submit echo vivo'
      sleep 1; send 'output 1'; } | run "$f"
    expect "$f" "comando desconocido 'foo'" "comando desconocido"
    expect "$f" 'ERROR: uso: submit' "submit sin argumentos"
    expect "$f" 'ERROR: uso: status' "status sin id / id inválido"
    expect "$f" 'ERROR: comillas sin cerrar' "comillas sin cerrar"
    expect "$f" '^OK id=1$' "las entradas inválidas NO consumieron IDs"
    expect "$f" '^vivo$' "tras todo eso el servicio sigue ejecutando trabajos"
    finish

    # ------------------------------------------------------------------ TC-011
    begin TC-011 "Cola con límite de concurrencia (QUEUED) y cancelación en cola"
    f="$OUT/TC-011.txt"
    { send 'submit sleep 2' 'submit sleep 3'; sleep 0.4; send 'list' 'cancel 2' 'status 2'
      sleep 2.2; send 'status 1'; } | run "$f" 1
    expect "$f" '1 +RUNNING .*sleep 2' "con límite 1, el primero corre"
    expect "$f" '2 +QUEUED +- .*sleep 3' "el segundo espera en QUEUED (sin pid)"
    expect "$f" 'trabajo 2 cancelado \(estaba en cola\)' "cancelar uno en cola"
    expect "$f" 'id=2 estado=CANCELED pid=- ' "CANCELED sin haber tenido proceso"
    expect "$f" 'id=1 estado=SUCCEEDED' "el primero terminó normalmente"
    finish

    # ------------------------------------------------------------------ TC-012
    begin TC-012 "Salida grande no bloquea al servicio (sin deadlock de pipes)"
    f="$OUT/TC-012.txt"
    { send 'submit sh -c "head -c 300000 /dev/zero >&2"' 'submit sh -c "yes x | head -c 2000000"'
      sleep 2.5; send 'list' 'output 1'; } | run "$f"
    expect "$f" '1 +SUCCEEDED' "300 KB a stderr: el trabajo termina"
    expect "$f" '2 +SUCCEEDED' "2 MB a stdout: el trabajo termina"
    expect "$f" 'stderr \(300000 bytes\)' "stderr capturado completo"
    finish

    # ------------------------------------------------------------------ TC-013
    begin TC-013 "Solicitud duplicada de un trabajo activo (ADR-0002)"
    f="$OUT/TC-013.txt"
    { send 'submit sleep 20' 'submit sleep 20'; sleep 0.3; send 'cancel 1'; sleep 0.7
      send 'submit sleep 20'; sleep 0.3; send 'cancel 2'; sleep 0.7; send 'list'; } | run "$f"
    expect "$f" '^OK id=1 \(duplicado' "idéntico y activo => devuelve el ID existente"
    expect "$f" '^OK id=2$' "idéntico pero el previo ya terminó => ID nuevo"
    finish

    # ------------------------------------------------------------------ TC-014
    begin TC-014 "stdout y stderr se capturan por separado"
    f="$OUT/TC-014.txt"
    { send 'submit sh -c "echo uno; echo dos >&2; exit 7"'; sleep 1; send 'status 1' 'output 1'; } | run "$f"
    awk '/^--- stdout/{s=1;next} /^--- stderr/{s=2;next} s==1' "$f" > "$OUT/TC-014.stdout.txt"
    awk '/^--- stdout/{s=1;next} /^--- stderr/{s=2;next} s==2' "$f" > "$OUT/TC-014.stderr.txt"
    expect "$f" 'estado=FAILED.*exit=7 ' "exit code 7 propagado tal cual"
    expect "$OUT/TC-014.stdout.txt" '^uno$' "'uno' llegó a stdout"
    expect_not "$OUT/TC-014.stdout.txt" 'dos' "'dos' NO está en stdout"
    expect "$OUT/TC-014.stderr.txt" '^dos$' "'dos' llegó a stderr"
    finish

    # ------------------------------------------------------------------ TC-015
    begin TC-015 "Cada trabajo corre como proceso separado (padre = servidor)"
    f="$OUT/TC-015.txt"
    { send 'submit sh -c "echo MIPID=$$ PADRE=$PPID; cat /proc/$PPID/comm"'; sleep 1; send 'status 1' 'output 1'; } | run "$f"
    mipid="$(sed -n 's/^MIPID=\([0-9]*\) PADRE=.*/\1/p' "$f")"
    padre="$(sed -n 's/^MIPID=[0-9]* PADRE=\([0-9]*\)$/\1/p' "$f")"
    stpid="$(sed -n 's/.*estado=[A-Z]* pid=\([0-9]*\) .*/\1/p' "$f" | head -1)"
    expect "$f" '^server$' "el proceso padre del trabajo se llama 'server'"
    assert "el trabajo tiene PID propio, distinto del servidor ($mipid vs $padre)" \
        test -n "$mipid" -a -n "$padre" -a "$mipid" != "$padre"
    assert "el pid que reporta 'status' ($stpid) es el del proceso hijo ($mipid)" \
        test -n "$stpid" -a "$stpid" = "$mipid"
    finish

    # ------------------------------------------------------------------ TC-016
    begin TC-016 "Muerte por señal ajena (sin cancel) => FAILED, 128+señal"
    f="$OUT/TC-016.txt"
    { send 'submit sh -c "kill -9 $$"'; sleep 1; send 'status 1'; } | run "$f"
    expect "$f" 'id=1 estado=FAILED pid=[0-9]+ exit=137 \(señal 9' "murió por SIGKILL sin que se pidiera cancelar"
    finish

    # ------------------------------------------------------------------ TC-017
    begin TC-017 "quit termina los trabajos activos (sin procesos huérfanos)"
    f="$OUT/TC-017.txt"
    { send 'submit sleep 7123' 'submit sh -c "sleep 7124; echo fin"'; sleep 1; send 'quit'; } | run "$f"
    sleep 0.5
    expect "$f" 'JobRunner terminado' "el servicio terminó"
    assert "no quedan procesos 'sleep 7123/7124' (ni los nietos)" \
        bash -c '! pgrep -xf "sleep 712[34]" >/dev/null'
    finish

    # ------------------------------------------------------------------ TC-018
    begin TC-018 "SIGINT al servidor: apagado ordenado, sin huérfanos"
    f="$OUT/TC-018.txt"
    { send 'submit sleep 7125'; sleep 4; } | "$SERVER" > "$f" 2>&1 &
    spid=$!
    sleep 1
    kill -INT "$spid" 2>/dev/null
    sleep 1
    expect "$f" 'JobRunner terminado' "el servicio atendió SIGINT y terminó"
    assert "no queda 'sleep 7125' huérfano" bash -c '! pgrep -xf "sleep 7125" >/dev/null'
    wait 2>/dev/null
    finish

    # ------------------------------------------------------------------ TC-019
    begin TC-019 "Argumento inválido de arranque no deja el servicio en estado raro"
    f="$OUT/TC-019.txt"
    "$SERVER" abc < /dev/null > "$f" 2>&1; rc=$?
    expect "$f" 'uso:' "muestra el uso"
    assert "termina con código 2 (rc=$rc)" test "$rc" -eq 2
    finish

    # --- Resumen --------------------------------------------------------------
    echo; echo "================================================================"
    echo "RESULTADO: $PASSED/$TOTAL casos pasan"
    if [ "${#FAILED_LIST[@]}" -gt 0 ]; then
        echo "FALLARON: ${FAILED_LIST[*]}"
        return 1
    fi
    echo "TODOS LOS CASOS PASAN"
    echo "Evidencia: $SUMMARY"
    echo "Salida cruda por caso: $OUT/"
    return 0
}

main "$@" 2>&1 | tee "$SUMMARY"
rc=${PIPESTATUS[0]}
cp "$SUMMARY" "$RES/latest.txt" 2>/dev/null
exit "$rc"
