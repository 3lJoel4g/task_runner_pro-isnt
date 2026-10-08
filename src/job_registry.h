#ifndef JOB_REGISTRY_H
#define JOB_REGISTRY_H

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "job.h"

class JobRegistry {
public:
    int  next_id();
    void add(Job job);
    Job* find(int id);
    const Job* find(int id) const;
    std::vector<Job*> all();
    std::vector<Job*> by_state(JobState s);
    std::size_t size() const { return jobs_.size(); }

private:
    std::unordered_map<int, Job> jobs_;
    int next_id_ = 0;
};

#endif // JOB_REGISTRY_H
