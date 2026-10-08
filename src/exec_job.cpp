#include "exec_job.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>

#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

std::int64_t now_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(
               steady_clock::now().time_since_epoch())
        .count();
}

// Cierra un fd si es válido y lo marca como -1.
void safe_close(int& fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

} // namespace

bool exec_job(Job& job) {
    job.state          = JobState::RUNNING;
    job.started_at_ns  = now_ns();
    job.stdout_data.clear();
    job.stderr_data.clear();
    job.exit_code      = -1;
    job.term_signal    =  0;
    job.pid            = -1;

    int out_pipe[2] = {-1, -1};
    int err_pipe[2] = {-1, -1};

    if (::pipe(out_pipe) == -1) {
        std::fprintf(stderr, "[exec_job] pipe(stdout) fallo: %s\n",
                     std::strerror(errno));
        job.state = JobState::FAILED;
        job.finished_at_ns = now_ns();
        return false;
    }
    if (::pipe(err_pipe) == -1) {
        std::fprintf(stderr, "[exec_job] pipe(stderr) fallo: %s\n",
                     std::strerror(errno));
        safe_close(out_pipe[0]);
        safe_close(out_pipe[1]);
        job.state = JobState::FAILED;
        job.finished_at_ns = now_ns();
        return false;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        std::fprintf(stderr, "[exec_job] fork fallo: %s\n",
                     std::strerror(errno));
        safe_close(out_pipe[0]); safe_close(out_pipe[1]);
        safe_close(err_pipe[0]); safe_close(err_pipe[1]);
        job.state = JobState::FAILED;
        job.finished_at_ns = now_ns();
        return false;
    }

    if (pid == 0) {
        // ===================== PROCESO HIJO =====================
        safe_close(out_pipe[0]);
        safe_close(err_pipe[0]);

        if (::dup2(out_pipe[1], STDOUT_FILENO) == -1 ||
            ::dup2(err_pipe[1], STDERR_FILENO) == -1) {
            std::perror("[exec_job][child] dup2");
            ::_exit(127);
        }
        safe_close(out_pipe[1]);
        safe_close(err_pipe[1]);

        // Construir argv del hijo. Los strings viven en `job`, que sigue
        // válido en el hijo hasta el execvp (mismo address space tras fork).
        std::vector<char*> argv;
        argv.reserve(job.args.size() + 2);
        argv.push_back(const_cast<char*>(job.command.c_str()));
        for (const auto& a : job.args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);

        ::execvp(argv[0], argv.data());

        // Si llegamos aquí, execvp falló.
        std::fprintf(stderr, "[exec_job][child] execvp(%s) fallo: %s\n",
                     argv[0], std::strerror(errno));
        ::_exit(127);
    }

    // ===================== PROCESO PADRE =====================
    job.pid = pid;
    safe_close(out_pipe[1]);
    safe_close(err_pipe[1]);

    // RF-11: multiplexar lecturas con poll() para no bloquearnos si el
    // hijo escribe >PIPE_BUF en uno de los dos streams antes de cerrar el
    // otro. Este era el deadlock del código original.
    struct pollfd fds[2];
    fds[0].fd     = out_pipe[0];
    fds[0].events = POLLIN;
    fds[1].fd     = err_pipe[0];
    fds[1].events = POLLIN;

    int open_fds = 2;
    char buf[4096];

    while (open_fds > 0) {
        const int rv = ::poll(fds, 2, -1);
        if (rv < 0) {
            if (errno == EINTR) continue;
            std::fprintf(stderr, "[exec_job] poll fallo: %s\n",
                         std::strerror(errno));
            break;
        }

        for (int i = 0; i < 2; ++i) {
            if (fds[i].fd < 0) continue;
            if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;

            const ssize_t n = ::read(fds[i].fd, buf, sizeof(buf));
            if (n > 0) {
                if (i == 0) job.stdout_data.append(buf, static_cast<size_t>(n));
                else        job.stderr_data.append(buf, static_cast<size_t>(n));
            } else if (n == 0) {
                // EOF: el hijo cerró su extremo de escritura.
                safe_close(fds[i].fd);
                --open_fds;
            } else if (errno != EINTR) {
                // Error real de lectura: cerramos y seguimos con el otro.
                safe_close(fds[i].fd);
                --open_fds;
            }
        }
    }

    safe_close(fds[0].fd);
    safe_close(fds[1].fd);

    // Recolectar al hijo (bucle por si llega EINTR).
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        // reintentar
    }

    job.finished_at_ns = now_ns();

    if (WIFEXITED(status)) {
        job.exit_code = WEXITSTATUS(status);
        job.state     = (job.exit_code == 0) ? JobState::SUCCEEDED
                                             : JobState::FAILED;
    } else if (WIFSIGNALED(status)) {
        job.term_signal = WTERMSIG(status);
        job.exit_code   = -1;
        job.state       = JobState::FAILED;   // RF-29: caída por señal
    } else {
        job.exit_code = -1;
        job.state     = JobState::FAILED;
    }

    return true;
}

const char* job_state_to_string(JobState s) {
    switch (s) {
        case JobState::QUEUED:    return "QUEUED";
        case JobState::RUNNING:   return "RUNNING";
        case JobState::SUCCEEDED: return "SUCCEEDED";
        case JobState::FAILED:    return "FAILED";
        case JobState::CANCELED:  return "CANCELED";
    }
    return "UNKNOWN";
}

