# ADR-0002: Tratamiento de Solicitudes Duplicadas

## Estado
Aceptada

## Contexto
El RF-27 exige documentar la semántica para identificar o tratar solicitudes duplicadas o reenviadas, garantizando que no se creen estados contradictorios en el sistema.

## Alternativas consideradas
| # | Alternativa | Pros | Contras | Riesgos |
|---|-------------|------|---------|---------|
| 1 | Rechazo estricto por duplicidad | Ahorra recursos al máximo. | Frustrante para el usuario si realmente necesita correr el comando de nuevo. | Bloqueo permanente de comandos recurrentes legítimos. |
| 2 | Ejecución independiente (asignar ID nuevo siempre) | Muy fácil de implementar, el sistema no necesita buscar comandos previos. | Un cliente malicioso o un script en bucle puede saturar la cola rápidamente. | Incumplimiento implícito de protección de recursos bajo carga. |
| 3 | Idempotencia temporal (Reutilizar ID si está activo) | Protege la cola y el límite de concurrencia. Permite reejecuciones legítimas. | Requiere lógica de búsqueda y comparación de cadenas en la tabla de trabajos. | Ligero impacto en rendimiento al encolar. |

## Decisión
Se elige la **Alternativa 3 (Idempotencia temporal basada en estado)**. Cuando llegue una solicitud, JobRunner buscará en su registro si existe un trabajo con exactamente el mismo comando que se encuentre en estado `QUEUED` o `RUNNING`. 
* Si existe, no se creará un trabajo nuevo; el sistema devolverá el ID del trabajo existente. 
* Si el trabajo idéntico previo ya está en un estado terminal (`SUCCEEDED`, `FAILED`, `CANCELED`), se tratará como una solicitud nueva, se le asignará un nuevo ID y entrará a la cola.

## Consecuencias
* **Positivas:** Cumplimiento del RF-27. Optimización de los recursos del sistema operativo al no duplicar procesos idénticos en ejecución simultánea.
* **Negativas/Mitigaciones:** El demonio principal deberá recorrer la lista de trabajos activos y comparar las cadenas de texto (argumentos incluidos) por cada solicitud entrante, lo cual es manejable para el tamaño de carga esperado.
