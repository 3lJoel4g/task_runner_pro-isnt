# ADR-0005: Estrategia de recuperación tras reinicio

## Estado
Aceptada

## Contexto
RF-13 exige recuperar el historial al arrancar y actualizar coherentemente
los trabajos interrumpidos, evitando estados fantasma `RUNNING`. RNF-28
exige transiciones de estado consistentes. Al reiniciar el demonio, los
procesos hijos previamente vivos ya no son supervisables por nosotros
(no hacemos double-fork ni daemonización de hijos).

## Alternativas consideradas
| # | Alternativa | Pros | Contras | Riesgos |
|---|-------------|------|---------|---------|
| 1 | Marcar todo `RUNNING` como `FAILED` con motivo "interrumpido por reinicio" | Simple, honesto, sin estados fantasma. | Se pierde información de trabajos que quizá seguían vivos. | Ninguno relevante en nuestro modelo (hijos mueren con el padre o quedan huérfanos re-parentados a init sin forma de re-adoptarlos). |
| 2 | Double-fork + deamonización de hijos + PID file por trabajo | Los hijos sobreviven al padre. | Complejidad alta: re-adopción, re-attach a pipes imposible tras reinicio, gestión de zombies huérfanos. | Frágil; incumple simplicidad de ADR-0002. |
| 3 | Log de transiciones + reconciliación con `ps` | Podría detectar hijos aún vivos. | Depende de nombres de proceso, PIDs reciclados, race conditions. | Complejo y propenso a falsos positivos. |

## Decisión
**Alternativa 1.** Al arrancar, el demonio:
1. Lee `jobs.json`.
2. Cualquier trabajo en estado `RUNNING` se transiciona a `FAILED` con
   `exit_code = -1` y campo `note = "interrupted_by_restart"`.
3. Se registra un evento en la bitácora (RF-14).
4. Cualquier trabajo en `QUEUED` se mantiene en `QUEUED` y se reencola.
5. Los estados terminales (`SUCCEEDED`, `FAILED`, `CANCELED`) se preservan.

## Consecuencias
* **Positivas:** Cumple RF-13, RNF-28. Sin estados fantasma. Sin
  complejidad de re-adopción. Historial recuperable y auditable.
* **Negativas/Mitigaciones:** Trabajos que pudieran haber seguido vivos
  se reportan como fallidos → aceptable y documentado.
* **Trazabilidad:** RF-12, RF-13, RF-14, RNF-28, RNF-31.
