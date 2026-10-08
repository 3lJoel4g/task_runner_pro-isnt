#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <time.h>

#include "exec_job.h"
#include "job.h"

namespace {

// RF-02 / RF-23: límites de validación de la solicitud.
constexpr std::size_t kMaxCommandLen = 1024;
constexpr std::size_t kMaxArgs       = 256;

// Códigos de salida del binario server (documentados en README/user-guide).
enum ExitCode : int {
    EXIT_OK         = 0,   // job ejecutado y SUCCEEDED
    EXIT_JOB_FAILED = 1,   // job ejecutado y FAILED
    EXIT_USAGE      = 2,   // invocación inválida (RF-02)
    EXIT_INTERNAL   = 3,   // error interno (fork/pipe/...)
};

std::int64_t now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000LL
         + static_cast<std::int64_t>(ts.tv_nsec);
}

// RF-01: asignación de ID único dentro del proceso servidor.
// Será reemplazado por el registro persistente en Hito 2 (RF-12/RF-13).
int next_job_id() {
    static int counter = 0;
    return ++counter;
}

void print_usage(const char* prog) {
    std::cerr
        << "JobRunner server — uso:\n"
        << "  " << prog << " <comando> [args...]   ejecuta un trabajo\n"
        << "\n"
        << "Límites: comando no vacío y <= " << kMaxCommandLen
        << " caracteres; máximo " << kMaxArgs << " argumentos.\n";
}

// RF-02: validar la solicitud. Devuelve true si es aceptable.
bool validate_request(int argc, char** argv, std::string& err) {
    if (argc < 2) {
        err = "se requiere un comando";
        return false;
    }
    const std::string cmd = argv[1];
    if (cmd.empty()) {
        err = "el comando no puede estar vacío";
        return false;
    }
    if (cmd.size() > kMaxCommandLen) {
        err = "el comando excede la longitud máxima (" +
              std::to_string(kMaxCommandLen) + " caracteres)";
        return false;
    }
    if (static_cast<std::size_t>(argc - 2) > kMaxArgs) {
        err = "número de argumentos excede el máximo (" +
              std::to_string(kMaxArgs) + ")";
        return false;
    }
    return true;
}

void print_report(const Job& j) {
    std::cout << "=== Job ID " << j.id << " ===\n"
              << "  command   : " << j.command;
    for (const auto& a : j.args) std::cout << ' ' << a;
    std::cout << "\n"
              << "  state     : " << job_state_to_string(j.state) << "\n"
              << "  pid       : " << j.pid << "\n"
              << "  exit_code : " << j.exit_code << "\n";
    if (j.term_signal != 0) {
        std::cout << "  signal    : " << j.term_signal << "\n";
    }
    const std::int64_t dur_ms =
        (j.finished_at_ns - j.started_at_ns) / 1'000'000LL;
    std::cout << "  duration  : " << dur_ms << " ms\n"
              << "  stdout    : [" << j.stdout_data.size() << " bytes]\n"
              << "  stderr    : [" << j.stderr_data.size() << " bytes]\n";

    if (!j.stdout_data.empty()) {
        std::cout << "  --- stdout ---\n" << j.stdout_data;
        if (j.stdout_data.back() != '\n') std::cout << "\n";
    }
    if (!j.stderr_data.empty()) {
        std::cout << "  --- stderr ---\n" << j.stderr_data;
        if (j.stderr_data.back() != '\n') std::cout << "\n";
    }
}

int run_single_job(int argc, char** argv) {
    std::string err;
    if (!validate_request(argc, argv, err)) {
        std::cerr << "[server] solicitud inválida: " << err << "\n\n";
        print_usage(argv[0]);
        return EXIT_USAGE;
    }

    Job job{};
    job.id             = next_job_id();     // RF-01
    job.command        = argv[1];
    for (int i = 2; i < argc; ++i) {
        job.args.emplace_back(argv[i]);
    }
    job.received_at_ns = now_ns();

    if (!exec_job(job)) {
        std::cerr << "[server] error interno al ejecutar job\n";
        return EXIT_INTERNAL;
    }

    print_report(job);

    return (job.state == JobState::SUCCEEDED) ? EXIT_OK : EXIT_JOB_FAILED;
}

} // namespace

int main(int argc, char** argv) {
    std::cout << "=== JobRunner Server Init (Hito 1, RF-11 con poll) ===\n";
    return run_single_job(argc, argv);
}
