#include "daemon.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include "ipc.h"
#include "job.h"
#include "job_registry.h"
#include "job_runner.h"

namespace {

volatile std::sig_atomic_t g_stop = 0;
void on_signal(int) { g_stop = 1; }

std::int64_t now_ns() {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (std::int64_t)ts.tv_sec * 1'000'000'000LL + (std::int64_t)ts.tv_nsec;
}

std::vector<std::string> split_tabs(const std::string& s) {
    std::vector<std::string> out;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == '\t') {
            out.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return out;
}

std::string state_line(const Job& j) {
    std::ostringstream o;
    o << "id=" << j.id
      << " state=" << job_state_to_string(j.state)
      << " cmd=" << j.command
      << " pid=" << j.pid
      << " exit=" << j.exit_code;
    if (j.term_signal) o << " signal=" << j.term_signal;
    return o.str();
}

std::string handle_request(JobRegistry& reg, std::string line,
                           bool& shutdown_flag) {
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.pop_back();
    const auto parts = split_tabs(line);
    if (parts.empty()) return "ERR empty\n";
    const std::string& verb = parts[0];

    if (verb == "PING") return "OK pong\n";

    if (verb == "SUBMIT") {
        if (parts.size() < 2 || parts[1].empty())
            return "ERR submit requires a command\n";
        Job job{};
        job.id = reg.next_id();
        job.command = parts[1];
        for (std::size_t i = 2; i < parts.size(); ++i) job.args.push_back(parts[i]);
        job.received_at_ns = now_ns();
        job.started_at_ns  = job.received_at_ns;
        if (!job_runner::spawn(job))
            return "ERR spawn failed\n";
        std::ostringstream o;
        o << "OK id=" << job.id << " pid=" << job.pid << "\n";
        reg.add(std::move(job));
        return o.str();
    }

    if (verb == "STATUS") {
        if (parts.size() < 2) return "ERR status requires id\n";
        int id = std::atoi(parts[1].c_str());
        job_runner::reap_finished(reg);
        Job* j = reg.find(id);
        if (!j) return "ERR job not found\n";
        std::ostringstream o;
        o << "OK\n" << state_line(*j) << "\n";
        if (!j->stdout_data.empty()) o << "--stdout--\n" << j->stdout_data;
        if (!j->stderr_data.empty()) o << "--stderr--\n" << j->stderr_data;
        return o.str();
    }

    if (verb == "LIST") {
        job_runner::reap_finished(reg);
        std::vector<Job*> rows;
        if (parts.size() >= 2 && !parts[1].empty()) {
            JobState s;
            const std::string& f = parts[1];
            if      (f == "QUEUED")    s = JobState::QUEUED;
            else if (f == "RUNNING")   s = JobState::RUNNING;
            else if (f == "SUCCEEDED") s = JobState::SUCCEEDED;
            else if (f == "FAILED")    s = JobState::FAILED;
            else if (f == "CANCELED")  s = JobState::CANCELED;
            else return "ERR invalid state filter\n";
            rows = reg.by_state(s);
        } else {
            rows = reg.all();
        }
        std::ostringstream o;
        o << "OK count=" << rows.size() << "\n";
        for (Job* j : rows) o << state_line(*j) << "\n";
        return o.str();
    }

    if (verb == "CANCEL") {
        if (parts.size() < 2) return "ERR cancel requires id\n";
        int id = std::atoi(parts[1].c_str());
        Job* j = reg.find(id);
        if (!j) return "ERR job not found\n";
        if (j->state != JobState::RUNNING && j->state != JobState::QUEUED)
            return "ERR job already terminal\n";
        if (!job_runner::request_cancel(*j)) return "ERR kill failed\n";
        return "OK cancel requested\n";
    }

    if (verb == "SHUTDOWN") {
        shutdown_flag = true;
        return "OK shutting down\n";
    }

    return "ERR unknown verb\n";
}

} // namespace

int run_daemon() {
    const std::string sock_path = ipc::default_socket_path();
    const int listen_fd = ipc::listen_unix(sock_path);
    if (listen_fd < 0) {
        std::fprintf(stderr, "[daemon] no se pudo escuchar en %s: %s\n",
                     sock_path.c_str(), std::strerror(errno));
        return 2;
    }
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);
    std::signal(SIGPIPE, SIG_IGN);

    std::fprintf(stderr, "[daemon] escuchando en %s (pid=%d)\n",
                 sock_path.c_str(), (int)::getpid());

    JobRegistry reg;
    bool shutdown_flag = false;

    while (!g_stop && !shutdown_flag) {
        struct pollfd pfd{ listen_fd, POLLIN, 0 };
        const int rv = ::poll(&pfd, 1, 100);

        job_runner::reap_finished(reg);
        job_runner::escalate_cancels(reg);

        if (rv <= 0) continue;

        const int cfd = ::accept(listen_fd, nullptr, nullptr);
        if (cfd < 0) { if (errno == EINTR) continue; break; }
	::fcntl(cfd, F_SETFD, FD_CLOEXEC);
        std::string req = ipc::recv_line(cfd);
        const std::string resp = handle_request(reg, req, shutdown_flag);
        ipc::send_all(cfd, resp);
        ::close(cfd);
    }

    ::close(listen_fd);
    ::unlink(sock_path.c_str());
    std::fprintf(stderr, "[daemon] bye\n");
    return 0;
}
