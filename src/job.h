#ifndef JOB_H
#define JOB_H

// RF-06, RF-07: modelo de un trabajo y su ciclo de vida.
// Autocontenido: incluye todo lo que necesita para compilar solo.

#include <sys/types.h>   // pid_t

#include <cstdint>
#include <string>
#include <vector>

// RF-06: estados obligatorios del ciclo de vida.
enum class JobState {
    QUEUED,
    RUNNING,
    SUCCEEDED,
    FAILED,
    CANCELED
};

// Representación textual para logs y reportes.
const char* job_state_to_string(JobState s);

struct Job {
    // Identificación
    int         id      = 0;
    std::string command;
    std::vector<std::string> args;

    // Ciclo de vida (RF-06)
    JobState    state   = JobState::QUEUED;

    // RF-07: marcas de tiempo en nanosegundos (clock monotónico).
    std::int64_t received_at_ns = 0;
    std::int64_t started_at_ns  = 0;
    std::int64_t finished_at_ns = 0;

    // Resultado
    pid_t       pid         = -1;
    int         exit_code   = -1;
    int         term_signal =  0;   // != 0 si terminó por señal (RF-29)

    // RF-11: salidas capturadas y separadas.
    std::string stdout_data;
    std::string stderr_data;
};

#endif // JOB_H
