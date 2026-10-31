# JobRunner — Guía técnica

## Compilación

    make -C src clean
    make -C src

Requisitos: g++ ≥ 9, Linux, sin privilegios de root. Compila sin
warnings con `-Wall -Wextra -Werror -std=c++17`.

## Ejecución de pruebas

    make -C src test

Ejecuta `verif/scripts/run_tests.sh`, que corre todos los `tc-*.sh`
y deja evidencia en `verif/results/<run-id>/`.

## Configuración — `config.json`

    {
      "max_workers": 3,
      "max_queue_size": 100,
      "data_dir": "./data",
      "log_file": "jobrunner.log",
      "socket_path": "/tmp/jobrunner.sock"
    }

Si el archivo no existe, el daemon lo crea con valores por defecto.

## Arquitectura

    CLIENTE ── socket Unix ──► DAEMON ── fork/execvp ──► hijo
                                  │
                                  ├─ Cola FIFO (deque<int>)
                                  ├─ Persistencia (data/jobs.json)
                                  └─ Bitácora (jobrunner.log)

## Persistencia atómica

1. Escribe en `data/jobs.json.tmp`.
2. `fsync(fd)`.
3. `rename(tmp, final)` (operación atómica POSIX).
4. `fsync(dir)`.

Al arrancar, los `RUNNING` huérfanos pasan a `FAILED` con
`note="interrupted by restart"`.

## Logs

Formato: `[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [job=N] mensaje`.
Niveles: INFO, WARN, ERROR. Se abre en modo append.

## Cancelación

- `QUEUED`: pasa a `CANCELED` sin señal.
- `RUNNING`: SIGTERM, y tras 3 s SIGKILL (escalamiento).

## Variables de entorno

- `JOBRUNNER_SOCKET`: ruta del socket (prioritario sobre config.json
  en el cliente).
