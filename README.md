# task-runner-pro'isnt

> *"Un JobRunner que intenta ser Pro... pero honestamente hace lo que puede."*

## Propósito del Proyecto
Implementación de un sistema de gestión y ejecución de tareas en segundo plano (JobRunner) enfocado en el procesamiento asíncrono, concurrente, escalable y con tolerancia a fallos.

## Integrantes y Contacto Institucional
* **Elisea Saavedra Samuel Alejandro** - samuel.elisea1143@alumnos.udg.mx
* **Galindo Parra Javier Alberto** - javier.galindo2212@alumnos.udg.mx
* **Pánuco Rodriguez David de Jesús** - david.panuco@alumnos.udg.mx
* **Gonzalez Aguilar Joel Alejandro** - gonzalez.joel@alumnos.udg.mx

## Matriz de Responsabilidades y Roles
| Área | Responsable Principal | Revisor / Backup |
| :--- | :--- | :--- |
| **Producto** | Elisea Saavedra Samuel Alejandro | Gonzalez Aguilar Joel Alejandro |
| **Ingeniería / Arquitectura** | Galindo Parra Javier Alberto | Elisea Saavedra Samuel Alejandro |
| **Verificación / QA** | Pánuco Rodriguez David de Jesús | Galindo Parra Javier Alberto |

## Estado del Proyecto
* **Fase:** Arranque e Inicialización (Hito 0).
* **Estado actual:** Estructura base de repositorio, documentación inicial y asignación de actividades lista.

## Cronograma Inicial (Hitos Principales)
| Hito / Fase | Entregables Clave | Responsable Principal | Estado |
| :--- | :--- | :--- | :--- |
| **Hito 0: Setup & Organización** | Repo, matriz de roles, estructura inicial, ADRs 001-003 | Equipo completo | Completado |
| **Hito 1: Avance 1 (Core)** | Encolamiento base, interfaz de Jobs, persistencia inicial | Galindo Parra Javier Alberto | En progreso |
| **Hito 2: Avance 2 (Workers)** | Concurrencia de workers, reintentos y timeouts | Elisea Saavedra Samuel Alejandro | Pendiente |
| **Hito 3: QA & Pruebas** | Suite de pruebas unitarias/integración, benchmarks | Pánuco Rodriguez David de Jesús | Pendiente |
| **Hito 4: Entrega Final** | Documentación técnica, empaquetado y release estable | Equipo completo | Pendiente |

## Registro de Riesgos y Dependencias
* **Riesgo 1 (Concurrencia):** Bloqueos de recursos o condiciones de carrera al despachar múltiples tareas simultáneas. *(Mitigación: pruebas de estrés tempranas y ADR de concurrencia).*
* **Riesgo 2 (Persistencia):** Sobrecarga o pérdida de estado de tareas en caso de caída del proceso. *(Mitigación: elección rigurosa de cola persistente).*
* **Dependencia Crítica:** Disponibilidad de entorno de ejecución/broker compatible (Redis/DB/Sockets) para la sincronización de estados.

## Registro de Decisiones de Arquitectura (ADR Iniciales)
1. **ADR-001:** Selección de lenguaje y runtime para el JobRunner (Python vs. Go vs. Node.js).
2. **ADR-002:** Mecanismo de persistencia y encolamiento de tareas (In-Memory persistente vs. Redis vs. SQLite/PostgreSQL).
3. **ADR-003:** Modelo de concurrencia y ejecución de workers (Multiprocessing vs. Hilos vs. Async Event Loop).

## Instrucciones de Construcción Provisional
```bash
# Clonar el repositorio
git clone [https://github.com/3lJoel4g/task_runner_pro-isnt.git](https://github.com/3lJoel4g/task_runner_pro-isnt.git)

# Entrar a la carpeta del proyecto
cd task_runner_pro-isnt
