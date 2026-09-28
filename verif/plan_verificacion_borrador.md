# Plan de Verificación (Borrador Hito 0)

## Objetivos de Prueba
Asegurar que el sistema cumple con la línea base de Requisitos Funcionales (RF) y No Funcionales (RNF) definidos en el alcance.

## Estrategia
1. **Pruebas Unitarias:** Validación de parseo de comandos y encolamiento.
2. **Pruebas de Integración:** Verificación de IPC (el padre recibe stdout/stderr del hijo correctamente).
3. **Pruebas de Sistema:** El cliente envía trabajo por red local (socket TCP) y el servidor ejecuta y responde.
4. **Pruebas de Resiliencia:** Enviar señales SIGINT/SIGKILL y verificar que el servidor no colapsa ni deja zombies.
