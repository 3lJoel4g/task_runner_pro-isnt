# Riesgos Iniciales (Hito 0)

| ID | Riesgo | Impacto | Probabilidad | Estrategia de Mitigación |
|---|---|---|---|---|
| R-01 | Fuga de recursos por procesos "zombie" o pipes no cerrados. | Alto | Media | Uso estricto de `waitpid()` y revisión manual de descriptores de archivo en cada `fork()`. |
| R-02 | Saturación del servidor por ráfagas de clientes (DDoS local). | Crítico | Baja | Implementar un límite de concurrencia en la cola del servidor (Estado QUEUED). |
| R-03 | Condiciones de carrera al acceder al diccionario de trabajos. | Medio | Baja | Evitar multithreading; usar diseño basado en procesos independientes y bucle de eventos asíncronos. |
