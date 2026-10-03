# Casos de prueba — JobRunner (Avance 1)

Todos los casos están **automatizados** en `verif/scripts/verify.sh`, que construye el
servicio, los ejecuta contra el binario real y guarda la evidencia en `verif/results/`.

```bash
./verif/scripts/verify.sh        # código de salida 0 = todo pasa
```

Cada caso arranca un servidor nuevo, le manda comandos por stdin y revisa lo que imprime.
La columna **Requisito** usa los nombres de la funcionalidad mínima del avance; completen
la columna **RF/RNF** con los identificadores de su documento de requisitos (la matriz de
trazabilidad se arma a partir de aquí).

| ID | Requisito | RF/RNF | Descripción | Pasos | Resultado esperado |
|----|-----------|--------|-------------|-------|--------------------|
| TC-001 | Enviar trabajo / ID único | RF-01, RF-02 | Tres envíos seguidos | `submit echo a`, `b`, `c` | `OK id=1`, `id=2`, `id=3` |
| TC-002 | Ejecución, código de salida | RF-06 | Trabajo exitoso | `submit echo hola mundo`, `status 1`, `output 1` | `SUCCEEDED`, `exit=0`, stdout = `hola mundo` |
| TC-003 | Ejecución, código de salida | RF-06, RF-11 | Trabajo que falla | `submit ls /ruta_que_no_existe` | `FAILED`, `exit=2`, stderr con el mensaje de `ls` |
| TC-004 | Comandos inválidos | RNF-09 | Comando inexistente | `submit comando_que_no_existe_xyz`, luego `submit echo sigo_vivo` | primero `FAILED` con `exit=127`; el segundo `SUCCEEDED`; el servicio no cae |
| TC-005 | Consultar estado | — | Estado de job existente e inexistente | `submit sleep 3`, `status 1`, `status 99` | `RUNNING` con pid y `exit=-`; `ERROR: no existe el trabajo 99` |
| TC-006 | Listar trabajos | — | Tabla con varios estados | tres `submit` distintos, `list` | encabezado y una fila por trabajo con su estado |
| TC-007 | Cancelación | — | Cancelar uno en ejecución | `submit sleep 30`, `cancel 1`, `status 1` | `CANCELED`, `exit=143 (señal 15)` |
| TC-008 | Cancelación | — | Trabajo que ignora SIGTERM | script con `trap '' TERM`, `cancel 1` | a 1 s sigue `RUNNING`; a los ~3 s `CANCELED`, `exit=137 (señal 9)` |
| TC-009 | Cancelación / errores | — | Cancelar terminado, inexistente, id inválido | `cancel 1`, `cancel 99`, `cancel abc` | tres `ERROR` distintos; el estado del job terminado no cambia |
| TC-010 | Comandos inválidos | RNF-09 | Entradas basura | `foo bar`, `submit`, `status`, `status abc`, `cancel -1`, comillas sin cerrar | un `ERROR` por cada una, no consumen IDs, el servicio sigue |
| TC-011 | Cola / estados | RF-03, RF-06 | Límite de concurrencia = 1 | `./server 1`, `submit sleep 2`, `submit sleep 3`, `cancel 2` | el 1 `RUNNING`, el 2 `QUEUED` sin pid; cancelado en cola pasa a `CANCELED` |
| TC-012 | Captura de salida | RF-11 | Salida grande (300 KB a stderr, 2 MB a stdout) | `submit sh -c "head -c 300000 /dev/zero >&2"` y otro con `yes x \| head -c 2000000` | ambos `SUCCEEDED` (no hay deadlock); stderr completo (300000 bytes) |
| TC-013 | Duplicados | RF-27 (ADR-0002) | Mismo comando activo y luego ya terminado | `submit sleep 20` ×2, `cancel 1`, `submit sleep 20` | segundo envío = `OK id=1 (duplicado…)`; tercero = `OK id=2` |
| TC-014 | Captura de salida | RF-11 | stdout y stderr separados | `submit sh -c "echo uno; echo dos >&2; exit 7"` | `exit=7`; `uno` solo en stdout; `dos` solo en stderr |
| TC-015 | Proceso separado | RNF-09 | El trabajo es un proceso hijo | `submit sh -c "echo MIPID=$$ PADRE=$PPID; cat /proc/$PPID/comm"` | el padre se llama `server`; pid del hijo ≠ pid del servidor; coincide con el que muestra `status` |
| TC-016 | Señales / código de salida | — | Muerte por señal sin cancel | `submit sh -c "kill -9 $$"` | `FAILED`, `exit=137 (señal 9)` (no `CANCELED`: nadie pidió cancelar) |
| TC-017 | Procesos huérfanos | R-01 | `quit` con trabajos activos (incluye nietos) | `submit sleep 7123`, `submit sh -c "sleep 7124; echo fin"`, `quit` | el servicio termina y no queda ningún `sleep 7123/7124` |
| TC-018 | Señales / resiliencia | R-01 | SIGINT al servidor | `submit sleep 7125`, `kill -INT <pid del servidor>` | apagado ordenado y sin `sleep 7125` huérfano |
| TC-019 | Arranque | — | Argumento inválido | `./server abc` | muestra el uso y termina con código 2 |

## Convenciones que verifican estos casos

- **Código de salida:** salida normal = el código del programa; muerte por señal = `128 + señal`
  (SIGTERM → 143, SIGKILL → 137); comando que no se pudo ejecutar → 127.
- **Estados:** `CANCELED` solo si se pidió cancelar **y** el proceso murió por una señal.
  Una muerte por señal sin cancelación es `FAILED` (TC-016).

## Cobertura que falta (para el siguiente avance)

- Operación remota por sockets (RF-18): no se pide todavía.
- Persistencia del estado ante caída del servicio (ADR de persistencia).
- Carrera "el job termina justo antes de llegar SIGTERM" (limitación conocida; difícil de automatizar).
- Pruebas de estrés con muchos trabajos simultáneos.
