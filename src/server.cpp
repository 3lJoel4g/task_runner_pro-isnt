#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <time.h>

#include "exec_job.h"
#include "job.h"

namespace {

std::int64_t now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000LL
         + static_cast<std::int64_t>(ts.tv_nsec);
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

} // namespace

int main(int argc, char** argv) {
    std::cout << "=== JobRunner Server Init (Hito 1, RF-11 con poll) ===\n";

    // Modo demo (sin argumentos): reproduce los dos casos originales.
    if (argc == 1) {
        Job ok{};
        ok.id = 1;
        ok.command = "ls";
        ok.args = {"-la"};
        ok.received_at_ns = now_ns();
        exec_job(ok);
        print_report(ok);

        Job fail{};
        fail.id = 2;
        fail.command = "ls";
        fail.args = {"/ruta_que_no_existe"};
        fail.received_at_ns = now_ns();
        exec_job(fail);
        print_report(fail);

        return 0;
    }

    // Modo un comando: ./server <cmd> [args...]
    Job job{};
    job.id = 1;
    job.command = argv[1];
    for (int i = 2; i < argc; ++i) {
        job.args.emplace_back(argv[i]);
    }
    job.received_at_ns = now_ns();

    exec_job(job);
    print_report(job);

    return (job.state == JobState::SUCCEEDED) ? 0 : 1;
}
