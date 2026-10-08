#include <cstring>
#include <ctime>
#include <iostream>
#include <string>

#include "client.h"
#include "daemon.h"
#include "exec_job.h"
#include "job.h"

namespace {

constexpr std::size_t kMaxCommandLen = 1024;
constexpr std::size_t kMaxArgs       = 256;

std::int64_t now_ns() {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (std::int64_t)ts.tv_sec * 1'000'000'000LL + (std::int64_t)ts.tv_nsec;
}

void print_usage() {
    std::cout
        << "JobRunner — uso:\n"
        << "  server <comando> [args...]     ejecuta un trabajo (modo legacy)\n"
        << "  server daemon                  arranca el servicio (foreground)\n"
        << "  server submit <cmd> [args...]  envía un trabajo al servicio\n"
        << "  server status <id>             consulta estado\n"
        << "  server list [STATE]            lista trabajos\n"
        << "  server cancel <id>             solicita cancelación\n"
        << "  server shutdown                detiene el servicio\n"
        << "  server help                    esta ayuda\n";
}

void print_report(const Job& j) {
    std::cout << "=== Job ID " << j.id << " ===\n"
              << "  command   : " << j.command;
    for (const auto& a : j.args) std::cout << ' ' << a;
    std::cout << "\n  state     : " << job_state_to_string(j.state) << "\n"
              << "  pid       : " << j.pid << "\n"
              << "  exit_code : " << j.exit_code << "\n";
    if (j.term_signal) std::cout << "  signal    : " << j.term_signal << "\n";
    if (!j.stdout_data.empty()) {
        std::cout << "  --- stdout ---\n" << j.stdout_data;
        if (j.stdout_data.back() != '\n') std::cout << "\n";
    }
    if (!j.stderr_data.empty()) {
        std::cout << "  --- stderr ---\n" << j.stderr_data;
        if (j.stderr_data.back() != '\n') std::cout << "\n";
    }
}

bool validate_legacy(int argc, char** argv, std::string& err) {
    if (argc < 2) { err = "se requiere un comando"; return false; }
    const std::string cmd = argv[1];
    if (cmd.empty()) { err = "el comando no puede estar vacío"; return false; }
    if (cmd.size() > kMaxCommandLen) {
        err = "el comando excede la longitud máxima"; return false;
    }
    if ((std::size_t)(argc - 2) > kMaxArgs) {
        err = "número de argumentos excede el máximo"; return false;
    }
    return true;
}

int run_legacy(int argc, char** argv) {
    std::string err;
    if (!validate_legacy(argc, argv, err)) {
        std::cerr << "[server] solicitud inválida: " << err << "\n\n"
                  << "uso: " << argv[0] << " <comando> [args...]\n";
        return 2;
    }
    static int counter = 0;
    Job job{};
    job.id = ++counter;
    job.command = argv[1];
    for (int i = 2; i < argc; ++i) job.args.emplace_back(argv[i]);
    job.received_at_ns = now_ns();
    if (!exec_job(job)) return 3;
    print_report(job);
    return (job.state == JobState::SUCCEEDED) ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { print_usage(); return 2; }

    const std::string cmd = argv[1];
    if (cmd == "daemon")   return run_daemon();
    if (cmd == "submit")   return client_submit(argc, argv);
    if (cmd == "status")   return client_status(argc, argv);
    if (cmd == "list")     return client_list(argc, argv);
    if (cmd == "cancel")   return client_cancel(argc, argv);
    if (cmd == "shutdown") return client_shutdown();
    if (cmd == "help" || cmd == "--help" || cmd == "-h") { print_usage(); return 0; }

    return run_legacy(argc, argv);
}
