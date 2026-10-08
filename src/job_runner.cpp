#include "job_runner.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unordered_map>

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace job_runner {

namespace {
constexpr int kCancelGraceMs = 3000;

std::int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
               steady_clock::now().time_since_epoch()).count();
}

bool make_tmpfile(std::string& path_out) {
    char tmpl[] = "/tmp/jobrunner-XXXXXX";
    const int fd = ::mkstemp(tmpl);
    if (fd < 0) return false;
    ::close(fd);
    path_out = tmpl;
    return true;
}

bool read_file_into(int fd, std::string& out) {
    if (fd < 0) return false;
    if (::lseek(fd, 0, SEEK_SET) == (off_t)-1) return false;
    char buf[4096];
    for (;;) {
        const ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n > 0) { out.append(buf, static_cast<size_t>(n)); continue; }
        if (n == 0) break;
        if (errno == EINTR) continue;
        return false;
    }
    return true;
}

std::unordered_map<int, std::int64_t> g_cancel_requested_ms;
} // namespace

bool spawn(Job& job) {
    std::string out_p, err_p;
    if (!make_tmpfile(out_p) || !make_tmpfile(err_p)) {
        std::fprintf(stderr, "[job_runner] mkstemp fallo: %s\n",
                     std::strerror(errno));
        job.state = JobState::FAILED;
        return false;
    }
    const int out_fd = ::open(out_p.c_str(), O_RDWR);
    const int err_fd = ::open(err_p.c_str(), O_RDWR);
    if (out_fd < 0 || err_fd < 0) {
        std::fprintf(stderr, "[job_runner] open tmp fallo\n");
        if (out_fd >= 0) ::close(out_fd);
        if (err_fd >= 0) ::close(err_fd);
        ::unlink(out_p.c_str());
        ::unlink(err_p.c_str());
        job.state = JobState::FAILED;
        return false;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(out_fd); ::close(err_fd);
        ::unlink(out_p.c_str()); ::unlink(err_p.c_str());
        job.state = JobState::FAILED;
        return false;
    }

    if (pid == 0) {
        ::dup2(out_fd, STDOUT_FILENO);
        ::dup2(err_fd, STDERR_FILENO);
        ::close(out_fd);
        ::close(err_fd);

        std::vector<char*> argv;
        argv.reserve(job.args.size() + 2);
        argv.push_back(const_cast<char*>(job.command.c_str()));
        for (const auto& a : job.args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        ::execvp(argv[0], argv.data());
        std::fprintf(stderr, "execvp(%s) fallo: %s\n",
                     argv[0], std::strerror(errno));
        ::_exit(127);
    }

    job.pid         = pid;
    job.stdout_fd   = out_fd;
    job.stderr_fd   = err_fd;
    job.stdout_path = out_p;
    job.stderr_path = err_p;
    job.state       = JobState::RUNNING;
    return true;
}

int reap_finished(JobRegistry& reg) {
    int reaped = 0;
    for (;;) {
        int status = 0;
        const pid_t pid = ::waitpid(-1, &status, WNOHANG);
        if (pid <= 0) break;

        Job* job = nullptr;
        for (Job* j : reg.all()) if (j->pid == pid) { job = j; break; }
        if (!job) continue;

        if (WIFEXITED(status)) {
            job->exit_code = WEXITSTATUS(status);
            job->state = (job->exit_code == 0) ? JobState::SUCCEEDED
                                               : JobState::FAILED;
        } else if (WIFSIGNALED(status)) {
            job->term_signal = WTERMSIG(status);
            job->exit_code   = -1;
            if (g_cancel_requested_ms.count(job->id)) {
                job->state = JobState::CANCELED;
                g_cancel_requested_ms.erase(job->id);
            } else {
                job->state = JobState::FAILED;
            }
        } else {
            job->state     = JobState::FAILED;
            job->exit_code = -1;
        }

        read_file_into(job->stdout_fd, job->stdout_data);
        read_file_into(job->stderr_fd, job->stderr_data);
        if (job->stdout_fd >= 0) { ::close(job->stdout_fd); job->stdout_fd = -1; }
        if (job->stderr_fd >= 0) { ::close(job->stderr_fd); job->stderr_fd = -1; }
        if (!job->stdout_path.empty()) { ::unlink(job->stdout_path.c_str()); job->stdout_path.clear(); }
        if (!job->stderr_path.empty()) { ::unlink(job->stderr_path.c_str()); job->stderr_path.clear(); }
        ++reaped;
    }
    return reaped;
}

bool request_cancel(Job& job) {
    if (job.state != JobState::RUNNING && job.state != JobState::QUEUED) return false;
    if (job.pid <= 0) return false;
    if (::kill(job.pid, SIGTERM) == -1) {
        std::fprintf(stderr, "[job_runner] kill(%d, SIGTERM) fallo: %s\n",
                     job.pid, std::strerror(errno));
        return false;
    }
    g_cancel_requested_ms[job.id] = now_ms();
    return true;
}

void escalate_cancels(JobRegistry& reg) {
    const auto now = now_ms();
    for (auto it = g_cancel_requested_ms.begin();
         it != g_cancel_requested_ms.end(); ) {
        if (now - it->second < kCancelGraceMs) { ++it; continue; }
        Job* j = reg.find(it->first);
        if (!j || j->pid <= 0 || j->state != JobState::RUNNING) {
            it = g_cancel_requested_ms.erase(it);
            continue;
        }
        ::kill(j->pid, SIGKILL);
        ++it;
    }
}

} // namespace job_runner
