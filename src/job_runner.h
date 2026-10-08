#ifndef JOB_RUNNER_H
#define JOB_RUNNER_H

#include "job.h"
#include "job_registry.h"

namespace job_runner {

bool spawn(Job& job);
int  reap_finished(JobRegistry& reg);
bool request_cancel(Job& job);
void escalate_cancels(JobRegistry& reg);

} // namespace job_runner

#endif // JOB_RUNNER_H
