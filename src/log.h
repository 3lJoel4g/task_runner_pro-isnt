#ifndef LOG_H
#define LOG_H

#include <string>

// RF-14: bitácora con timestamp e ID de trabajo.
// Uso:
//   logger::init("jobrunner.log");
//   logger::info(1, "job queued");
//   logger::warn(2, "cancel requested");
//   logger::error(-1, "fork failed");
//
// Si log_file está vacío, la salida va a stderr.
namespace logger {

void init(const std::string& log_file);
void close();

void info (int job_id, const std::string& msg);
void warn (int job_id, const std::string& msg);
void error(int job_id, const std::string& msg);

} // namespace logger

#endif // LOG_H
