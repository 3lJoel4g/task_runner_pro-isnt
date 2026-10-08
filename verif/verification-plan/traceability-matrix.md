# Matriz de Trazabilidad — JobRunner

Cada fila asocia un requisito con su(s) caso(s) de prueba, evidencia y estado.

## Requisitos Funcionales

| RF    | Descripción breve                                  | TC(s)       | Evidencia                          | Estado |
|-------|----------------------------------------------------|-------------|------------------------------------|--------|
| RF-01 | Aceptar comando+args, devolver ID                  | TC-001      | (pendiente)                        | ⏳     |
| RF-02 | Validar solicitudes malformadas                    | TC-001      | (pendiente)                        | ⏳     |
| RF-03 | Cola de espera                                     | TC-002      | (pendiente)                        | ⏳     |
| RF-04 | Ejecución en procesos separados                    | TC-006      | verif/results/20261008-031343/     | ✅     |
| RF-05 | Límite configurable de concurrencia                | TC-002      | (pendiente)                        | ⏳     |
| RF-06 | Estados obligatorios del ciclo de vida             | TC-003, TC-006 | verif/results/20261008-031343/  | ✅     |
| RF-07 | Timestamps y exit code                             | TC-003, TC-006 | verif/results/20261008-031343/  | ✅     |
| RF-08 | Consultar estado por ID                            | TC-004      | (pendiente)                        | ⏳     |
| RF-09 | Listar con filtros                                 | TC-004      | (pendiente)                        | ⏳     |
| RF-10 | Cancelación                                        | TC-005      | (pendiente)                        | ⏳     |
| RF-11 | Captura separada stdout/stderr                     | TC-006      | verif/results/20261008-031343/     | ✅     |
| RF-12 | Persistencia al reinicio                           | TC-007      | (pendiente)                        | ⏳     |
| RF-13 | Recuperación al arranque                           | TC-007      | (pendiente)                        | ⏳     |
| RF-14 | Bitácora con timestamp e ID                        | TC-013      | (pendiente)                        | ⏳     |
| RF-15 | Graceful shutdown                                  | TC-009      | (pendiente)                        | ⏳     |
| RF-16 | Archivo de configuración                           | TC-013      | (pendiente)                        | ⏳     |
| RF-17 | Cliente CLI                                        | TC-015      | (pendiente)                        | ⏳     |
| RF-18 | Protocolo cliente-servidor documentado             | TC-010      | (pendiente)                        | ⏳     |
| RF-19 | Misma semántica local/remoto                       | TC-010      | (pendiente)                        | ⏳     |
| RF-20 | Whitelist IPs/subredes                             | TC-011      | (pendiente)                        | ⏳     |
| RF-21 | Framing + I/O parcial                              | TC-012      | (pendiente)                        | ⏳     |
| RF-22 | Tolerar desconexiones                              | TC-012      | (pendiente)                        | ⏳     |
| RF-23 | Rechazar fuera de límites                          | TC-016      | (pendiente)                        | ⏳     |
| RF-24 | Health summary                                     | TC-013      | (pendiente)                        | ⏳     |
| RF-25 | Rechazo explícito cola saturada                    | TC-016      | (pendiente)                        | ⏳     |
| RF-26 | Cancelaciones concurrentes coherentes              | TC-017      | (pendiente)                        | ⏳     |
| RF-27 | Solicitudes duplicadas                             | TC-018      | (pendiente)                        | ⏳     |
| RF-28 | Desconexión durante solicitud                      | TC-019      | (pendiente)                        | ⏳     |
| RF-29 | Detección caída hijo (SIGSEGV/SIGKILL)             | TC-020      | (pendiente)                        | ⏳     |
| RF-30 | Escalamiento SIGTERM→SIGKILL                       | TC-021      | (pendiente)                        | ⏳     |

## Requisitos No Funcionales (resumen)

| RNF        | Descripción breve                                | TC(s)       | Estado |
|------------|--------------------------------------------------|-------------|--------|
| RNF-01..03 | Linux, reproducible, sin root                    | TC-014      | ⏳     |
| RNF-04..07 | Concurrencia, latencia, 500 registros            | TC-002      | ⏳     |
| RNF-08..11 | Aislamiento, persistencia atómica                | TC-006, 007 | 🟡     |
| RNF-12..16 | Seguridad                                        | TC-011      | ⏳     |
| RNF-17..20 | Modularidad, -Werror, tests 1 comando            | TC-014      | ✅     |
| RNF-21..26 | Mensajes, logs, guía, sockets                    | varios      | ⏳     |
| RNF-27..34 | Transiciones, atomicidad, cero huérfanos         | varios      | ⏳     |

Leyenda: ✅ completado y verificado · 🟡 parcial · ⏳ pendiente
