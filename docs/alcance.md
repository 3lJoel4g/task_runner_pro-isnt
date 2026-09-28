# Alcance Aprobado y Línea Base (Hito 0)

El proyecto consiste en desarrollar **JobRunner**, un servicio ligero para registrar, ejecutar, supervisar y controlar trabajos del SO en entornos Linux mediante C/C++.

**Baseline de Requerimientos Principales:**
* **RF-01 al RF-06:** Recepción, asignación de ID, cola y gestión de estados (QUEUED, RUNNING, SUCCEEDED, FAILED, CANCELED).
* **RF-11:** Captura independiente de stdout y stderr mediante tuberías.
* **RF-18:** Interfaz de red privada (Sockets) entre cliente y servidor.
* **RNF-09:** Aislamiento de fallos mediante multiprocesamiento nativo (`fork`).
