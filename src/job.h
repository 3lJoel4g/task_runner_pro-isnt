#ifndef JOB_H
#define JOB_H

#include <string>
#include <vector>

// RF-06: Estados obligatorios
enum class JobState {
    QUEUED,
    RUNNING,
    SUCCEEDED,
    FAILED,
    CANCELED
};

struct Job {
    int id;
    JobState state;
    std::string command;
    std::vector<std::string> args;
    pid_t pid;
    int exit_code;
};

#endif
