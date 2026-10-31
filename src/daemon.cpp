#include "daemon.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include "config.h"
#include "ipc.h"
#include "job.h"
#include "job_registry.h"
#include "job_runner.h"
#include "persistence.h"
#include "log.h"

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
            std::string tok = s.substr(start, i - start);
            while (!tok.empty() && (tok.back() == '\n' || tok.back() == '\r'))
                tok.pop_back();
            out.push_back(tok);
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
    if (!j.note.empty()) o << " note=" << j.note;
    return o.str();
}

bool try_spawn(JobRegistry& reg, Job& job) {
    (void)reg;
    job.started_at_ns = now_ns();
    if (!job_runner::spawn(job)) {
        job.state = JobState::FAILED;
        job.note  = "spawn failed";
        return false;
    }
    return true;
}

std::string handle_request(JobRegistry& reg,
                           std::string line,
                           bool& shutdown_flag,
                           const std::string& data_dir) {
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.pop_back();

    const auto parts = split_tabs(line);
    if (parts.empty()) return "ERR empty\n";
    const std::string& verb = parts[0];

    if (verb == "PING") return "OK pong\n";

    if (verb == "SUBMIT") {
        if (parts.size() < 2 || parts[1].empty())
            return "ERR submit requires a command\n";

        const bool can_spawn = reg.can_spawn();
        const bool queue_ok  = reg.queue_has_space();

        if (!can_spawn && !queue_ok) {
            std::ostringstream o;
            o << "ERR queue full (max_queue_size=" << reg.max_queue_size()
              << ", running=" << reg.count_running()
              << ", queued="  << reg.count_queued() << ")\n";
            return o.str();
        }

        Job job{};
        job.id             = reg.next_id();
        job.command        = parts[1];
        for (std::size_t i = 2; i < parts.size(); ++i)
            job.args.push_back(parts[i]);
        job.received_at_ns = now_ns();

        if (can_spawn) {
            try_spawn(reg, job);
            logger::info(job.id, "submitted (RUNNING) cmd=" + job.command);
            std::ostringstream o;
            o << "OK id=" << job.id << " pid=" << job.pid
              << " state=" << job_state_to_string(job.state) << "\n";
            reg.add(std::move(job));
            save_state(reg, data_dir);
            return o.str();
        } else {
            job.state = JobState::QUEUED;
            const int id = job.id;
            reg.add(std::move(job));
            reg.enqueue(id);
            logger::info(id, "submitted (QUEUED) cmd=" + job.command);
            save_state(reg, data_dir);
            std::ostringstream o;
            o << "OK id=" << id << " state=QUEUED\n";
            return o.str();
        }
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

        if (j->state == JobState::QUEUED) {
            j->state = JobState::CANCELED;
            j->note  = "canceled while queued";
            logger::warn(j->id, "cancel while QUEUED");
            save_state(reg, data_dir);
            return "OK cancel requested (QUEUED)\n";
        }
        if (j->state != JobState::RUNNING)
            return "ERR job already terminal\n";
        if (!job_runner::request_cancel(*j)) return "ERR kill failed\n";
        logger::warn(j->id, "cancel while RUNNING (SIGTERM)");
        return "OK cancel requested (RUNNING)\n";
    }

    if (verb == "HEALTH") {
        std::ostringstream o;
        o << "OK\n"
          << "running=" << reg.count_running() << "\n"
          << "queued="  << reg.count_queued()  << "\n"
          << "max_workers="    << reg.max_workers()    << "\n"
          << "max_queue_size=" << reg.max_queue_size() << "\n";
        return o.str();
    }

    if (verb == "SHUTDOWN") {
        shutdown_flag = true;
        logger::info(-1, "shutdown requested");
        return "OK shutting down\n";
    }

    return "ERR unknown verb\n";
}

} // namespace

int run_daemon() {
    const Config cfg = load_config("config.json");
    logger::init(cfg.log_file);
    logger::info(-1, "daemon starting");
    std::fprintf(stderr,
        "[daemon] config: max_workers=%d, max_queue_size=%d, socket=%s\n",
        cfg.max_workers, cfg.max_queue_size, cfg.socket_path.c_str());

    const int listen_fd = ipc::listen_unix(cfg.socket_path);
    if (listen_fd < 0) {
        std::fprintf(stderr, "[daemon] no se pudo escuchar en %s: %s\n",
                     cfg.socket_path.c_str(), std::strerror(errno));
        return 2;
    }
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);
    std::signal(SIGPIPE, SIG_IGN);

    JobRegistry reg;
    reg.set_limits(cfg.max_workers, cfg.max_queue_size);
    const bool loaded = load_state(reg, cfg.data_dir);
    if (loaded) {
        logger::info(-1, "state recovered from disk");
        for (Job* j : reg.by_state(JobState::FAILED)) {
            if (j->note == "interrupted by restart") {
                logger::warn(j->id, "recovered: RUNNING -> FAILED (interrupted by restart)");
            }
        }
    }

    std::fprintf(stderr, "[daemon] escuchando en %s (pid=%d)\n",
                 cfg.socket_path.c_str(), (int)::getpid());

    bool shutdown_flag = false;

    while (!g_stop && !shutdown_flag) {
        job_runner::reap_finished(reg);
        job_runner::escalate_cancels(reg);
        save_state(reg, cfg.data_dir);

        while (reg.can_spawn()) {
            const int next_id = reg.dequeue_next();
            if (next_id < 0) break;
            Job* j = reg.find(next_id);
            if (!j) continue;
            try_spawn(reg, *j);
        }

        struct pollfd pfd{ listen_fd, POLLIN, 0 };
        const int rv = ::poll(&pfd, 1, 100);
        if (rv <= 0) continue;

        const int cfd = ::accept(listen_fd, nullptr, nullptr);
        if (cfd < 0) { if (errno == EINTR) continue; break; }
        ::fcntl(cfd, F_SETFD, FD_CLOEXEC);

        std::string req = ipc::recv_line(cfd);
        const std::string resp = handle_request(reg, req, shutdown_flag, cfg.data_dir);
        ipc::send_all(cfd, resp);
        ::close(cfd);
    }

    for (Job* j : reg.by_state(JobState::RUNNING))
        job_runner::request_cancel(*j);
    for (int i = 0; i < 30; ++i) {
        job_runner::reap_finished(reg);
        job_runner::escalate_cancels(reg);
        if (reg.count_running() == 0) break;
        ::usleep(100 * 1000);
    }
    save_state(reg, cfg.data_dir);

    logger::info(-1, "daemon stopping");
    logger::close();
    ::close(listen_fd);
    ::unlink(cfg.socket_path.c_str());
    std::fprintf(stderr, "[daemon] bye\n");
    return 0;
}
