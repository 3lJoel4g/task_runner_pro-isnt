#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <array>
#include "job.h"

std::vector<char*> prepare_args(const std::string& cmd, const std::vector<std::string>& args) {
    std::vector<char*> c_args;
    c_args.push_back(const_cast<char*>(cmd.c_str()));
    for (const auto& arg : args) {
        c_args.push_back(const_cast<char*>(arg.c_str()));
    }
    c_args.push_back(nullptr);
    return c_args;
}

void execute_job(Job& job) {
    job.state = JobState::RUNNING;
    std::cout << "[Servidor] Iniciando Job ID " << job.id << ": " << job.command << "\n";

    int out_pipe[2], err_pipe[2];

    if (pipe(out_pipe) == -1 || pipe(err_pipe) == -1) {
        std::cerr << "[Servidor] Error al crear pipes\n";
        job.state = JobState::FAILED;
        return;
    }

    pid_t pid = fork();

    if (pid < 0) {
        std::cerr << "[Servidor] Error al hacer fork()\n";
        job.state = JobState::FAILED;
        return;
    }

    if (pid == 0) {
        // --- PROCESO HIJO (El Clon) ---
        close(out_pipe[0]);
        close(err_pipe[0]);

        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);

        close(out_pipe[1]);
        close(err_pipe[1]);

        std::vector<char*> c_args = prepare_args(job.command, job.args);
        execvp(c_args[0], c_args.data());
        
        std::cerr << "Error interno: Comando no encontrado\n";
        exit(EXIT_FAILURE); 
    } else {
        // --- PROCESO PADRE (El Creador) ---
        job.pid = pid;
        
        close(out_pipe[1]);
        close(err_pipe[1]);

        char buffer[256];
        ssize_t bytes_read;
        std::string captured_stdout;
        while ((bytes_read = read(out_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            captured_stdout += buffer;
        }

        std::string captured_stderr;
        while ((bytes_read = read(err_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            captured_stderr += buffer;
        }

        close(out_pipe[0]);
        close(err_pipe[0]);

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            job.exit_code = WEXITSTATUS(status);
            job.state = (job.exit_code == 0) ? JobState::SUCCEEDED : JobState::FAILED;
        } else {
            job.state = JobState::FAILED;
            job.exit_code = -1;
        }

        if (!captured_stdout.empty()) {
            std::cout << "[Servidor] --- SALIDA NORMAL CAPTURADA ---\n" << captured_stdout;
        }
        if (!captured_stderr.empty()) {
            std::cout << "[Servidor] --- ERRORES CAPTURADOS ---\n" << captured_stderr;
        }
        std::cout << "[Servidor] Job ID " << job.id << " finalizado. Código: " << job.exit_code << "\n";
    }
}

int main() {
    std::cout << "=== JobRunner Server Init ===\n";

    // Trabajo 1: Comando exitoso
    Job test_job;
    test_job.id = 1;
    test_job.state = JobState::QUEUED;
    test_job.command = "ls";
    test_job.args = {"-l", "-a"};
    execute_job(test_job);

    std::cout << "\n";

    // Trabajo 2: Comando que va a fallar a propósito
    Job fail_job;
    fail_job.id = 2;
    fail_job.state = JobState::QUEUED;
    fail_job.command = "ls";
    fail_job.args = {"/ruta_que_no_existe"};
    execute_job(fail_job);

    return 0;
}
