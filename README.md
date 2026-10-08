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
