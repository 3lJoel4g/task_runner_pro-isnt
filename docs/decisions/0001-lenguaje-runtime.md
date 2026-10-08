# ADR-0001: Lenguaje y runtime del servicio JobRunner

## Estado
Aceptada

## Contexto
JobRunner requiere control fino sobre fork/exec (RF-04), captura separada
de stdout/stderr (RF-11), manejo de señales POSIX (RF-15, RF-29, RF-30),
IPC multiplexado (RF-18, RF-21) y persistencia atómica (RF-12, RF-13).
Se necesita un lenguaje que exponga primitivas POSIX sin capas de
abstracción que oculten el comportamiento del SO.

## Alternativas consideradas
| # | Alternativa | Pros | Contras | Riesgos |
|---|-------------|------|---------|---------|
| 1 | C++17 (g++) | Acceso nativo a fork/exec/pipe/poll/epoll/signals; RAII para gestión de FDs; strings y vectores sin sacrificar control; soporta `-Wall -Wextra -Werror`. | Disciplina para no introducir excepciones en zonas críticas (fork, handlers). | Complejidad innecesaria si se abusa de templates. |
| 2 | C11 | Máximo control, binario mínimo. | Gestión manual de strings y memoria; más código para lo mismo. | Mayor probabilidad de bugs de memoria. |
| 3 | Go | Concurrencia sencilla, GC. | Runtime pesado; fork/exec y señales se sienten foráneos al modelo POSIX. | Acoplamiento al runtime. |
| 4 | Python | Prototipado rápido. | No apto para control fino de procesos/señales; overhead; dependencia de intérprete. | Incumple RNF-01 (binario reproducible sin runtime adicional). |
| 5 | Node.js | Ecosistema amplio. | Modelo de concurrencia inadecuado; event loop oculta señales POSIX. | Igual que Python. |

## Decisión
**C++17 con g++**, compilado con `-Wall -Wextra -Werror -std=c++17`.
Se prohíbe el uso de excepciones en zonas críticas (fork, handlers de señales,
callbacks de poll/select). El binario es autocontenido (sin runtime externo).

## Consecuencias
* **Positivas:** Control total sobre el SO; cumple RF-04, RF-11, RF-15,
  RF-29, RF-30, RNF-09. RAII simplifica el cierre de FDs y reduce leaks.
* **Negativas/Mitigaciones:** Riesgo de fugas de descriptores → política
  "un FD, un RAII"; code review obligatorio en zonas de fork.
* **Trazabilidad:** RF-04, RF-11, RF-15, RF-18, RF-29, RF-30,
  RNF-01, RNF-17, RNF-19.
