# Definición de Roles del Proyecto JobRunner

Al ser un desarrollo enfocado en la programación de sistemas de alto nivel, la responsabilidad técnica se distribuye en los siguientes roles principales (asumidos por el desarrollador líder en esta fase):

1. **Arquitecto de Sistemas:** Responsable del diseño de concurrencia, selección de mecanismos IPC (Pipes, Sockets) y prevención de interbloqueos/zombies.
2. **Desarrollador C/C++:** Codificación del demonio y el cliente bajo estándares estrictos (-Wall -Wextra -Werror).
3. **Ingeniero de Verificación (QA):** Diseño del plan de pruebas, ejecución de pruebas de estrés y validación de la matriz de trazabilidad.
