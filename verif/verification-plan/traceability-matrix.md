# Matriz de Trazabilidad — JobRunner

Cada fila asocia un requisito con su(s) caso(s) de prueba, evidencia y estado.

## Requisitos Funcionales

| RF    | Descripción breve                                  | TC(s)          | Evidencia                                  | Estado |
|-------|----------------------------------------------------|----------------|--------------------------------------------|--------|
| RF-01 | Aceptar comando+args, devolver ID                  | TC-001, TC-002 | verif/results/20261030-223039/             | ✅     |
| RF-02 | Validar solicitudes malformadas                    | TC-001         | verif/results/20261030-223039/tc-001.log   | ✅     |
| RF-03 | Cola de espera                                     | TC-002         | verif/results/20261030-223039/tc-002.log   | ✅     |
| RF-04 | Ejecución en procesos separados                    | TC-002, TC-006 | verif/results/20261030-223039/             | ✅     |
| RF-05 | Límite configurable de concurrencia                | TC-002         | verif/results/20261030-223039/tc-002.log   | ✅     |
| RF-06 | Estados obligatorios del ciclo de vida             | TC-001, TC-002, TC-006 | verif/results/20261030-223039/    | ✅     |
| RF-07 | Timestamps y exit code                             | TC-001, TC-002, TC-006 | verif/results/20261030-223039/    | ✅     |
| RF-08 | Consultar estado por ID                            | TC-002         | verif/results/20261030-223039/tc-002.log   | ✅     |
| RF-09 | Listar trabajos con filtros                        | TC-002         | verif/results/20261030-223039/tc-002.log   | ✅     |
| RF-10 | Cancelación (SIGTERM → SIGKILL)                    | TC-002         | verif/results/20261030-223039/tc-002.log   | ✅     |
| RF-11 | Captura separada stdout/stderr                     | TC-006         | verif/results/20261030-223039/tc-006.log   | ✅     |
| RF-12 | Persistencia de metadatos                          | TC-007         | verif/results/20261030-223039/tc-007.log   | ✅     |
| RF-13 | Recuperación al arranque                           | TC-007         | verif/results/20261030-223039/tc-007.log   | ✅     |
| RF-14 | Bitácora con timestamp e ID                        | (manual)       | jobrunner.log                              | ✅     |
| RF-15 | Cierre controlado (graceful shutdown)              | TC-002         | verif/results/20261030-223039/tc-002.log   | ✅     |
| RF-16 | Archivo de configuración                           | (manual)       | config.json                                | ✅     |
| RF-17 | Cliente CLI con ayuda y códigos                    | TC-002         | verif/results/20261030-223039/tc-002.log   | ✅     |
| RF-18 | Protocolo cliente-servidor documentado             | TC-010         | (Hito 3)                                   | ⏳     |
| RF-19 | Misma semántica local/remoto                       | TC-010         | (Hito 3)                                   | ⏳     |
| RF-20 | Whitelist IPs/subredes                             | TC-011         | (Hito 3)                                   | ⏳     |
| RF-21 | Framing + I/O parcial                              | TC-012         | (Hito 3)                                   | ⏳     |
| RF-22 | Tolerar desconexiones                              | TC-012         | (Hito 3)                                   | ⏳     |
| RF-23 | Rechazar solicitudes fuera de límites              | TC-016         | verif/results/20261030-223039/tc-016.log   | ✅     |
| RF-24 | Health summary                                     | TC-002         | verif/results/20261030-223039/tc-002.log   | ✅     |
| RF-25 | Rechazo explícito con cola llena                   | TC-016         | verif/results/20261030-223039/tc-016.log   | ✅     |
| RF-26 | Cancelaciones concurrentes coherentes              | TC-017         | (Hito 4)                                   | ⏳     |
| RF-27 | Solicitudes duplicadas                             | TC-018         | (Hito 4)                                   | ⏳     |
| RF-28 | Desconexión durante solicitud                      | TC-019         | (Hito 4)                                   | ⏳     |
| RF-29 | Detección caída hijo (SIGSEGV/SIGKILL)             | TC-020         | (Hito 4)                                   | ⏳     |
| RF-30 | Escalamiento SIGTERM→SIGKILL                       | TC-021         | verif/results/20261030-223039/tc-021.log   | ✅     |

## Requisitos No Funcionales

| RNF        | Descripción breve                                | Evidencia                          | Estado |
|------------|--------------------------------------------------|------------------------------------|--------|
| RNF-01..03 | Linux, reproducible, sin root                    | build.log                          | ✅     |
| RNF-04..07 | Concurrencia, latencia, 500 registros            | TC-002                             | ✅     |
| RNF-08..11 | Aislamiento, persistencia atómica                | TC-002, TC-006, TC-007             | ✅     |
| RNF-12..16 | Seguridad                                        | (Hito 3)                           | ⏳     |
| RNF-17..20 | Modularidad, -Werror, tests 1 comando            | build.log, make test               | ✅     |
| RNF-21..26 | Mensajes, logs, guía, sockets                    | jobrunner.log, TC-002              | 🟡     |
| RNF-27..34 | Transiciones, atomicidad, cero huérfanos         | TC-007, TC-021                     | ✅     |

Leyenda: ✅ completado y verificado · 🟡 parcial · ⏳ pendiente
