#include "log.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>

#include <unistd.h>

namespace logger {

namespace {
std::FILE*   g_file = nullptr;
std::mutex   g_mu;

const char* level_name(int lvl) {
    switch (lvl) {
        case 0: return "INFO";
        case 1: return "WARN";
        default: return "ERROR";
    }
}

void write_line(int lvl, int job_id, const std::string& msg) {
    // Timestamp con milisegundos.
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm_buf;
    localtime_r(&ts.tv_sec, &tm_buf);

    char stamp[64];
    std::snprintf(stamp, sizeof(stamp),
                  "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
                  tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                  tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                  ts.tv_nsec / 1'000'000L);

    // job=N si es válido, si no "job=-".
    char job_tag[32];
    if (job_id > 0)
        std::snprintf(job_tag, sizeof(job_tag), "job=%d", job_id);
    else
        std::snprintf(job_tag, sizeof(job_tag), "job=-");

    // Escribir línea.
    std::lock_guard<std::mutex> lock(g_mu);
    std::FILE* out = g_file ? g_file : stderr;
    std::fprintf(out, "[%s] [%s] [%s] %s\n",
                 stamp, level_name(lvl), job_tag, msg.c_str());
    std::fflush(out);
}

} // namespace

void init(const std::string& log_file) {
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_file) { std::fclose(g_file); g_file = nullptr; }
    if (log_file.empty()) return;   // salida a stderr
    g_file = std::fopen(log_file.c_str(), "a");
    if (!g_file) {
        std::fprintf(stderr, "[logger] no se pudo abrir %s: %s\n",
                     log_file.c_str(), std::strerror(errno));
    }
}

void close() {
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_file) { std::fclose(g_file); g_file = nullptr; }
}

void info (int job_id, const std::string& msg) { write_line(0, job_id, msg); }
void warn (int job_id, const std::string& msg) { write_line(1, job_id, msg); }
void error(int job_id, const std::string& msg) { write_line(2, job_id, msg); }

} // namespace logger
