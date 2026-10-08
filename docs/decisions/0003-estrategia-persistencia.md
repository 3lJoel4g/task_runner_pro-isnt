# ADR-0003: Estrategia de persistencia de estado

## Estado
Aceptada

## Contexto
RF-12 exige conservar metadatos y resultados al reiniciar el servicio;
RF-13 exige recuperar historial al arrancar sin dejar estados fantasma
`RUNNING`. RNF-10 y RNF-11 exigen persistencia atómica anti-corrupción
ante fallos de energía o reinicios abruptos. El volumen esperado es de
500 registros sin pérdida (RNF-06).

## Alternativas consideradas
| # | Alternativa | Pros | Contras | Riesgos |
|---|-------------|------|---------|---------|
| 1 | SQLite | ACID, consultas, índice por ID, WAL para durabilidad. | Dependencia externa (o amalgamación ~250 KB); sobredimensionado para 500 registros; complejidad de esquema y migraciones. | Añade dependencia y peso que RNF-01 (clon limpio) no justifica. |
| 2 | JSON plano con escritura atómica (`tmp + fsync + rename + fsync(dir)`) | Sin dependencias; formato auditable a ojo; `rename(2)` es atómico en POSIX; fácil de testear y de recuperar. | Toda la base se reescribe en cada cambio; no escala a millones de registros. | Pérdida si el disco miente y no respeta fsync (aceptable para el alcance). |
| 3 | Append-only log binario + checkpoint | Muy rápido en escritura, tolera truncamientos. | Formato opaco; recuperación compleja; difícil de auditar a mano. | Complejidad desproporcionada para el alcance. |

## Decisión
**JSON plano con escritura atómica**. Un único archivo `jobs.json` en el
directorio de datos configurado (RF-16). Toda mutación se realiza así:
1. Escribir el JSON completo en `jobs.json.tmp`.
2. `fflush` + `fsync(fd_tmp)`.
3. `rename("jobs.json.tmp", "jobs.json")`.
4. `fsync(dir_fd)` para asegurar la entrada de directorio.
El demonio mantiene el estado en memoria y persiste tras cada transición
de estado relevante (encolado, inicio, fin, cancelación).

## Consecuencias
* **Positivas:** Cumple RF-12, RF-13, RNF-10, RNF-11 sin dependencias.
  Recuperación trivial: leer el JSON entero al arrancar.
* **Negativas/Mitigaciones:** Coste O(n) por escritura → aceptable hasta
  ~500 registros. Si el alcance crece, se reevalúa con un nuevo ADR.
* **Trazabilidad:** RF-12, RF-13, RF-16, RNF-10, RNF-11, RNF-28.
