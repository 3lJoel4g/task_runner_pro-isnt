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
