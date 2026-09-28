# ADR-0001: Modelo de Concurrencia e IPC para ejecución de trabajos

## Estado
Aceptada

## Contexto
El sistema debe ejecutar trabajos en segundo plano sin bloquear la atención de nuevas solicitudes (RF-04) y capturar las salidas de error y estándar por separado (RF-11). Se requiere definir el mecanismo de concurrencia y la comunicación entre procesos (IPC) entre el demonio principal y los trabajos.

## Alternativas consideradas
| # | Alternativa | Pros | Contras | Riesgos |
|---|-------------|------|---------|---------|
| 1 | `fork()` + `execvp()` con `pipe()` anónimos | Nativo en POSIX, aísla fallos completamente, permite redirección limpia con `dup2()`. | El demonio debe usar `epoll`/`select` para evitar bloqueos al leer. | Alto consumo de descriptores de archivo bajo carga máxima. |
| 2 | Hilos (`pthreads`) + `system()` | Más ligero en memoria que hacer un fork completo. | Un fallo en el trabajo puede corromper o matar el servicio principal. | Incumple RNF-09 (Aislamiento de fallo) y dificulta separar stdout/stderr. |
| 3 | Sockets de Dominio Unix (`socketpair`) | Comunicación bidireccional avanzada. | Mayor complejidad de implementación. | Sobreingeniería, ya que el servicio solo necesita leer la salida unidireccional del trabajo. |

## Decisión
Se elige la **Alternativa 1 (`fork()` + `execvp()` con tuberías anónimas)**. Se crearán dos pipes unidireccionales por cada trabajo antes de hacer fork. El proceso hijo redirigirá su salida estándar y de error a los extremos de escritura de los pipes mediante `dup2()` y luego ejecutará el comando con `execvp()`. 

## Consecuencias
* **Positivas:** Cumplimiento total de RF-04, RF-11 y RNF-09. La falla de un trabajo no afectará al demonio.
* **Negativas/Mitigaciones:** El proceso principal (JobRunner) deberá implementar multiplexación I/O (`epoll` o `select`) o manejar hilos de lectura dedicados para no bloquearse esperando la salida de un solo pipe.
