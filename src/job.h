#ifndef JOB_H
#define JOB_H

#include <sys/types.h>   // pid_t
#include <string>
#include <vector>

// RF-06: Estados obligatorios
enum class JobState {
    QUEUED,     // aceptado, esperando un "hueco" de ejecución
    RUNNING,    // proceso hijo vivo
    SUCCEEDED,  // terminó con exit code 0
    FAILED,     // terminó con exit code != 0, o murió por señal sin que se pidiera cancelar
    CANCELED    // se pidió cancelar y el trabajo no llegó a completarse
};

inline const char* state_name(JobState s) {
    switch (s) {
        case JobState::QUEUED:    return "QUEUED";
        case JobState::RUNNING:   return "RUNNING";
        case JobState::SUCCEEDED: return "SUCCEEDED";
        case JobState::FAILED:    return "FAILED";
        case JobState::CANCELED:  return "CANCELED";
    }
    return "UNKNOWN";
}

inline bool is_terminal(JobState s) {
    return s == JobState::SUCCEEDED || s == JobState::FAILED || s == JobState::CANCELED;
}

struct Job {
    int id = 0;
    JobState state = JobState::QUEUED;
    std::string command;
    std::vector<std::string> args;

    pid_t pid = -1;          // -1 mientras no haya proceso
    int exit_code = -1;      // -1 = "todavía no hay código"
    int term_signal = 0;     // != 0 si el proceso murió por una señal

    bool cancel_requested = false;  // el usuario pidió cancelar
    bool kill_sent = false;         // ya escalamos a SIGKILL
    double cancel_time = 0;         // instante (monotónico) del SIGTERM

    int out_fd = -1;         // extremo de lectura del pipe de stdout del hijo
    int err_fd = -1;         // extremo de lectura del pipe de stderr del hijo
    std::string out;         // stdout capturado
    std::string err;         // stderr capturado
};

#endif
