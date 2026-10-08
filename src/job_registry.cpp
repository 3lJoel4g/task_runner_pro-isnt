#include "job_registry.h"

#include <utility>

int JobRegistry::next_id() { return ++next_id_; }

void JobRegistry::add(Job job) { jobs_[job.id] = std::move(job); }

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
