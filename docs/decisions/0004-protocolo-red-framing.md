# ADR-0004: Protocolo de red y framing

## Estado
Aceptada

## Contexto
RF-18 exige un protocolo cliente-servidor remoto documentado; RF-19 exige
misma semántica local y remota; RF-21 exige framing, delimitación de
mensajes, control de errores y manejo de I/O parcial; RNF-14 soporte de
mensajes fragmentados; RNF-32 compatibilidad de versión.

## Alternativas consideradas
| # | Alternativa | Pros | Contras | Riesgos |
|---|-------------|------|---------|---------|
| 1 | TCP + framing longitud-prefijo + payload JSON-RPC | Framing explícito, no depende de `\n`; versionable con campo `v`; JSON es auditable y universal; fácil de testear con `nc` o scripts. | Overhead de parseo JSON en cada mensaje. | Aceptable para el alcance. |
| 2 | TCP + JSON delimitado por `\n` (NDJSON) | Trivial de parsear. | Se rompe si un string contiene `\n` sin escape; requiere disciplina de serialización. | Frágil ante clientes no conformes. |
| 3 | gRPC / Protobuf | Tipado fuerte, codegen. | Dependencia pesada (protoc, libs), overkill para el alcance. | Rompe RNF-01 (clon limpio). |
| 4 | Sockets Unix locales + TCP solo para remoto | Mejor rendimiento local. | Duplica caminos de código; RF-19 pide misma semántica. | Aumenta superficie de test. |

## Decisión
**TCP + framing longitud-prefijo + payload JSON-RPC.** Formato de trama:

    [ 4 bytes: uint32 big-endian = N ][ N bytes: JSON UTF-8 ]

El JSON incluye siempre:
- `"v": 1` (versión de protocolo, RNF-32)
- `"id"`: identificador de correlación (int)
- `"method"`: string (`"submit"`, `"status"`, `"list"`, `"cancel"`, `"health"`)
- `"params"`: objeto
- `"result"` o `"error"`: objeto

El receptor debe leer exactamente 4 bytes de cabecera y luego leer el
payload en bucle hasta completar N bytes (manejo de I/O parcial, RF-21).
Se impone un límite máximo de payload configurable (RF-23).

## Consecuencias
* **Positivas:** Cumple RF-18, RF-19, RF-21, RNF-14, RNF-32. Framing
  robusto ante fragmentación TCP; parseo con `json` (a decidir: nlohmann
  header-only o parser propio mínimo).
* **Negativas/Mitigaciones:** Riesgo de "length prefix attack" → límite
  máximo obligatorio en configuración.
* **Trazabilidad:** RF-18, RF-19, RF-21, RF-22, RF-23, RNF-14, RNF-32.
