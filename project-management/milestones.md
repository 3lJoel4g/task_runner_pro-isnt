# Hitos — JobRunner

| Hito | Nombre                                             | Estado      | Gate |
|------|----------------------------------------------------|-------------|------|
| 0    | Inicio y línea base                                | ✅ Cerrado  | G0   |
| 1    | Núcleo local (submit/status/list/cancel/exit code) | ✅ Cerrado  | G1   |
| 2    | Cola + límite + persistencia + recuperación        | ✅ Cerrado  | G2   |
| 3    | Protocolo TCP + cliente remoto + whitelist         | ⏳ Pendiente | G3   |
| 4    | Resiliencia (señales, duplicados)                  | ⏳ Pendiente | G4   |
| 5    | Documentación final y auditoría                    | ⏳ Pendiente | G5   |

## Historial de releases a main

| Fecha      | Hito | Tag sugerido | Evidencia |
|------------|------|--------------|-----------|
| 2026-10-30 | 1    | v0.1.0       | verif/results/20261030-094031/ |
| 2026-10-30 | 2    | v0.2.0       | verif/results/20261030-223039/ |

## Estado actual
Hito 2 cerrado con G2 superado. `make -C src test` → PASS=6 FAIL=0.
