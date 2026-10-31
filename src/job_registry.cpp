#include "job_registry.h"

#include <utility>

void JobRegistry::set_limits(int max_workers, int max_queue_size) {
    max_workers_   = max_workers   > 0 ? max_workers   : 1;
    max_queue_size_ = max_queue_size > 0 ? max_queue_size : 1;
}

int JobRegistry::next_id() { return ++next_id_; }

void JobRegistry::add(Job job) {
    jobs_[job.id] = std::move(job);
}

Job* JobRegistry::find(int id) {
    auto it = jobs_.find(id);
    return it == jobs_.end() ? nullptr : &it->second;
}

const Job* JobRegistry::find(int id) const {
    auto it = jobs_.find(id);
    return it == jobs_.end() ? nullptr : &it->second;
}

std::vector<Job*> JobRegistry::all() {
    std::vector<Job*> out;
    out.reserve(jobs_.size());
    for (auto& kv : jobs_) out.push_back(&kv.second);
    return out;
}

std::vector<Job*> JobRegistry::by_state(JobState s) {
    std::vector<Job*> out;
    for (auto& kv : jobs_) if (kv.second.state == s) out.push_back(&kv.second);
    return out;
}

bool JobRegistry::enqueue(int id) {
    if (!queue_has_space()) return false;
    Job* j = find(id);
    if (!j) return false;
    j->state = JobState::QUEUED;
    fifo_.push_back(id);
    return true;
}

int JobRegistry::dequeue_next() {
    while (!fifo_.empty()) {
        const int id = fifo_.front();
        fifo_.pop_front();
        Job* j = find(id);
        if (j && j->state == JobState::QUEUED) return id;
    }
    return -1;
}

int JobRegistry::count_running() const {
    int n = 0;
    for (const auto& kv : jobs_)
        if (kv.second.state == JobState::RUNNING) ++n;
    return n;
}

int JobRegistry::count_queued() const {
    int n = 0;
    for (const auto& kv : jobs_)
        if (kv.second.state == JobState::QUEUED) ++n;
    return n;
}
