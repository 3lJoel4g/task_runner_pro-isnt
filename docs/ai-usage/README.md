# Registro de uso de IA

Bitácora de interacciones con asistentes de IA usadas durante el desarrollo
de JobRunner (prompts clave, decisiones sugeridas, y validación humana aplicada).

### Entrada 001 — Rechazo del diseño original de captura de stdout/stderr

- **Contexto:** El motor base original (`src/server.cpp`, commit `a9d1b78`)
  capturaba stdout y stderr con dos pipes anónimos, pero leía primero
  `stdout` completo y **después** empezaba a leer `stderr`.
- **Herramienta:** Asistente IA (auditoría técnica del Hito 1).
- **Prompt:** "Audita `execute_job()`: ¿es correcto leer stdout y stderr
  secuencialmente con dos pipes anónimos sin multiplexar?"
- **Resultado bruto:** Sugerencia de la IA (y aceptada en su momento por
  el desarrollador) de leer secuencialmente.
- **Estado:** **RECHAZADO**.
- **Justificación técnica:** Viola RF-11 y RNF-08. Si el hijo escribe más
  de `PIPE_BUF` (~64 KiB) en `stderr` mientras el padre aún está bloqueado
  en `read(stdout_pipe)`, se produce un interbloqueo: el hijo queda
  bloqueado en `write(stderr_pipe)` y el padre en `read(stdout_pipe)`.
- **Acción correctiva:** Refactor a bucle `poll()` que multiplexa ambos
  descriptores (`src/exec_job.cpp`). El caso queda reproducido y
  verificado por `TC-006`, que provoca explícitamente >PIPE_BUF en ambos
  streams.
- **Trazabilidad:** RF-07, RF-11, RNF-08, ADR-0002, TC-006.
