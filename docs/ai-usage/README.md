# Registro de uso de IA

Bitácora de interacciones con asistentes de IA usadas durante el desarrollo
de JobRunner. Se registran tres categorías: aceptado, modificado, rechazado.

---

### Entrada 001 — Rechazada: diseño original de captura de stdout/stderr

- **Contexto:** El motor base original capturaba stdout y stderr secuencialmente.
- **Resultado bruto:** Lectura de stdout completa antes de stderr.
- **Estado:** RECHAZADO.
- **Justificación:** Viola RF-11 y RNF-08. Deadlock si el hijo llena el pipe
  de stderr mientras el padre lee stdout.
- **Corrección:** `poll()` multiplexando ambos descriptores (TC-006).

---

### Entrada 002 — Modificada: recuperación tras merge accidental a main

- **Contexto:** El PR #8 se mergeó a `main` en lugar de `develop`.
- **Resultado bruto:** `git push --force origin main` para revertir.
- **Estado:** MODIFICADO.
- **Justificación:** `main` es rama pública; reescribir historia viola RNF-33.
- **Corrección:** Estrategia conservadora + branch protection (INC-001).

---

### Entrada 003 — Aceptada: arquitectura CLI + daemon

- **Contexto:** El milestone pide un binario con subcomandos (`submit`, etc.).
- **Prompt:** "Diseña la arquitectura mínima cliente/servicio en Linux sin usar TCP."
- **Resultado:** CLI + daemon con socket Unix (`AF_UNIX`), protocolo de una
  línea por petición, un request = una conexión.
- **Estado:** ACEPTADO tras verificación con TC-002.
- **Trazabilidad:** RF-19, RF-21, TC-002.

---

### Entrada 004 — Modificada: bug del cliente colgado en SUBMIT largo

- **Contexto:** `submit sleep 300` tardaba 5 minutos en responder.
- **Resultado bruto:** Aumentar timeouts.
- **Estado:** MODIFICADO.
- **Justificación técnica:** El hijo de `fork()` heredaba el socket aceptado
  y el cliente esperaba EOF hasta que el job terminara.
- **Corrección:** `FD_CLOEXEC` en `cfd` y `listen_fd`.
- **Trazabilidad:** INC-002, TC-002.

---

### Entrada 005 — Aceptada: cola FIFO con límite de workers

- **Contexto:** Implementar la cola del Hito 2 sin librerías externas.
- **Prompt:** "Diseña la cola y el control de workers en C++17."
- **Resultado:** `std::deque<int>` de IDs, `count_running()`, `count_queued()`,
  `can_spawn()`, `queue_has_space()`, `dequeue_next()`.
- **Estado:** ACEPTADO tras verificación con TC-002 y TC-016.
- **Trazabilidad:** RF-03, RF-05, RF-23, RF-25.

---

### Entrada 006 — Modificada: bug del `set_limits` que ignoraba max_queue_size

- **Contexto:** El test TC-016 fallaba porque el daemon no rechazaba con cola llena.
- **Resultado bruto de la IA:** `max_queue_size = ...` (sin guion bajo).
- **Estado:** MODIFICADO.
- **Justificación técnica:** Asignaba al parámetro local, no al miembro
  `max_queue_size_`. El miembro siempre conservaba el default (100).
- **Corrección:** `max_queue_size_ = max_queue_size > 0 ? max_queue_size : 1;`
- **Trazabilidad:** RF-25, TC-016.
