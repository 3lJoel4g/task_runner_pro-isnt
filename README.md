# JobRunner

Servicio ligero para Linux que ejecuta y supervisa trabajos en segundo plano
con IPC, procesos, señales, persistencia y red privada.

## Estructura del repositorio
- `src/` — Código fuente
- `docs/` — Documentación (usuario, técnica, ADRs, uso de IA)
- `verif/` — Plan de verificación, casos de prueba, scripts y evidencia
- `project-management/` — Seguimiento del proyecto (hitos, backlog)

## Estado
🚧 Hito 0 — Inicio y línea base

## Stack
- Lenguaje: C++17 (g++)
- Compilador: g++ ≥ 9 con -Wall -Wextra -Werror
- Plataforma: Linux (POSIX)
- Suite de pruebas: Bash, orquestada por `make test`

## Uso

### Arrancar el servicio (terminal 1)

    ./src/server daemon

Escucha en `/tmp/jobrunner.sock` (configurable con `$JOBRUNNER_SOCKET`).

### Enviar comandos (terminal 2)

    ./src/server submit echo hola
    ./src/server submit sleep 30
    ./src/server status 1
    ./src/server list
    ./src/server list RUNNING
    ./src/server cancel 2
    ./src/server shutdown

### Modo legacy (un solo trabajo síncrono)

    ./src/server echo hola

### Pruebas

    make -C src test

## Hito 2 — Estado

- **Cola FIFO** con límite configurable de concurrencia.
- **Persistencia atómica** en `data/jobs.json` con `tmp + fsync + rename`.
- **Recuperación** tras caída: `RUNNING` → `FAILED` con nota.
- **Bitácora** en `jobrunner.log` con timestamp e ID.
- **Rechazo explícito** de cola llena con diagnóstico.
- **Cancelación escalonada** SIGTERM → SIGKILL.

**Suite:** `make -C src test` → **PASS=6 FAIL=0**
**Tag:** `v0.2.0` (sugerido)

## Comandos

    server daemon
    server submit <cmd> [args...]
    server status <id>
    server list [STATE]
    server cancel <id>
    server health
    server shutdown
