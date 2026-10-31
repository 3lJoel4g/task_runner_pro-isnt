# ADR-0007: Política de cancelación

## Estado
Aceptada

## Contexto
El sistema debe poder cancelar trabajos tanto en cola como en ejecución
(RF-10) y manejar el caso de trabajos que no responden a una señal suave
(RF-30). Se necesita una política que sea determinista y auditable.

## Alternativas consideradas

| # | Alternativa | Pros | Contras |
|---|---|---|---|
| 1 | SIGKILL directo | Simple, mata siempre | No permite cleanup en el proceso; viola la convención POSIX |
| 2 | SIGTERM y esperar indefinidamente | Respetuoso | Puede colgar el daemon si el proceso ignora la señal |
| 3 | SIGTERM → 3 s → SIGKILL | Respetuoso + acotado | Requiere tracking de tiempo por job |

## Decisión
Se elige la **Alternativa 3**:
- Cancelar en `QUEUED`: pasa directo a `CANCELED` sin enviar señal (no hay proceso).
- Cancelar en `RUNNING`: se envía SIGTERM. Si tras 3 segundos el proceso
  sigue vivo, se escala a SIGKILL.
- Al recibir SIGINT/SIGTERM en el daemon, se cancela y recolecta todo
  antes de salir.

## Consecuencias
- Cumple RF-10 y RF-30.
- El daemon nunca queda bloqueado esperando un proceso que ignora señales.
- Verificado por TC-021.
