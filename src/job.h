#ifndef JOB_H
#define JOB_H

#include <sys/types.h>

#include <cstdint>
#include <string>
#include <vector>

enum class JobState {
    QUEUED,
    RUNNING,
    SUCCEEDED,
    FAILED,
    CANCELED
};

const char* job_state_to_string(JobState s);

struct Job {
    int         id      = 0;
    std::string command;
    std::vector<std::string> args;

    JobState    state   = JobState::QUEUED;

    std::int64_t received_at_ns = 0;
    std::int64_t started_at_ns  = 0;
    std::int64_t finished_at_ns = 0;

    pid_t       pid         = -1;
    int         exit_code   = -1;
    int         term_signal =  0;

    int         stdout_fd   = -1;
    int         stderr_fd   = -1;
    std::string stdout_path;
    std::string stderr_path;
    std::string stdout_data;
    std::string stderr_data;
};

#endif // JOB_H
