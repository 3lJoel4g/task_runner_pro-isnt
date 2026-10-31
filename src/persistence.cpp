#include "persistence.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

bool ensure_dir(const std::string& data_dir) {
    if (::mkdir(data_dir.c_str(), 0755) == 0) return true;
    if (errno == EEXIST) return true;
    return false;
}

std::string extract_quoted(const std::string& line) {
    auto first = line.find('"');
    if (first == std::string::npos) return "";
    auto second = line.find('"', first + 1);
    if (second == std::string::npos) return "";
    auto third = line.find('"', second + 1);
    if (third == std::string::npos) return "";
    auto fourth = line.find('"', third + 1);
    if (fourth == std::string::npos) return "";
    return line.substr(third + 1, fourth - third - 1);
}

int extract_int(const std::string& line, const std::string& key) {
    auto pos = line.find("\"" + key + "\"");
    if (pos == std::string::npos) return -1;
    auto colon = line.find(':', pos);
    if (colon == std::string::npos) return -1;
    std::string rest = line.substr(colon + 1);
    size_t i = 0;
    while (i < rest.size() && (rest[i] == ' ' || rest[i] == '\t')) ++i;
    bool neg = false;
    if (i < rest.size() && rest[i] == '-') { neg = true; ++i; }
    int val = 0;
    bool has_digit = false;
    while (i < rest.size() && rest[i] >= '0' && rest[i] <= '9') {
        val = val * 10 + (rest[i] - '0'); ++i; has_digit = true;
    }
    if (!has_digit) return -1;
    return neg ? -val : val;
}

std::string extract_state(const std::string& line) {
    auto pos = line.find("\"state\"");
    if (pos == std::string::npos) return "";
    auto first_quote = line.find('"', line.find(':', pos) + 1);
    if (first_quote == std::string::npos) return "";
    auto second_quote = line.find('"', first_quote + 1);
    if (second_quote == std::string::npos) return "";
    return line.substr(first_quote + 1, second_quote - first_quote - 1);
}

JobState parse_state(const std::string& s) {
    if (s == "QUEUED")    return JobState::QUEUED;
    if (s == "RUNNING")   return JobState::RUNNING;
    if (s == "SUCCEEDED") return JobState::SUCCEEDED;
    if (s == "FAILED")    return JobState::FAILED;
    return JobState::CANCELED;
}

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        if (c == '\n') { out += "\\n"; continue; }
        if (c == '\t') { out += "\\t"; continue; }
        out.push_back(c);
    }
    return out;
}

} // namespace

bool save_state(const JobRegistry& reg, const std::string& data_dir) {
    if (!ensure_dir(data_dir)) {
        std::fprintf(stderr, "[persist] no se pudo crear %s\n", data_dir.c_str());
        return false;
    }

    const std::string final_path = data_dir + "/jobs.json";
    const std::string tmp_path   = final_path + ".tmp";

    // Abrimos con open() directamente, NO con ofstream.
    // Esto evita problemas con el cierre del stream.
    int fd = ::open(tmp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        std::fprintf(stderr, "[persist] open(%s) falló: %s\n",
                     tmp_path.c_str(), std::strerror(errno));
        return false;
    }

    // Serializar JSON en un buffer de memoria.
    std::string buf;
    buf += "[\n";
    bool first = true;
    for (const auto& kv : reg.jobs()) {
        const Job& j = kv.second;
        if (!first) buf += ",\n";
        first = false;
        buf += "  {\n";
        buf += "    \"id\": "         + std::to_string(j.id) + ",\n";
        buf += "    \"command\": \""  + json_escape(j.command) + "\",\n";
        buf += "    \"state\": \""    + std::string(job_state_to_string(j.state)) + "\",\n";
        buf += "    \"exit_code\": "  + std::to_string(j.exit_code) + ",\n";
        buf += "    \"term_signal\": "+ std::to_string(j.term_signal) + ",\n";
        buf += "    \"note\": \""     + json_escape(j.note) + "\"\n";
        buf += "  }";
    }
    buf += "\n]\n";

    // Escribir todo el buffer.
    std::size_t written = 0;
    while (written < buf.size()) {
        const ssize_t n = ::write(fd, buf.data() + written, buf.size() - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            std::fprintf(stderr, "[persist] write falló: %s\n",
                         std::strerror(errno));
            ::close(fd);
            ::unlink(tmp_path.c_str());
            return false;
        }
        written += static_cast<std::size_t>(n);
    }

    // fsync + close del temporal.
    ::fsync(fd);
    ::close(fd);

    // Rename atómico.
    if (::rename(tmp_path.c_str(), final_path.c_str()) != 0) {
        std::fprintf(stderr, "[persist] rename(%s -> %s) falló: %s\n",
                     tmp_path.c_str(), final_path.c_str(),
                     std::strerror(errno));
        ::unlink(tmp_path.c_str());
        return false;
    }

    // fsync del directorio.
    int dfd = ::open(data_dir.c_str(), O_RDONLY | O_DIRECTORY);
    if (dfd >= 0) { ::fsync(dfd); ::close(dfd); }

    return true;
}

bool load_state(JobRegistry& reg, const std::string& data_dir) {
    const std::string path = data_dir + "/jobs.json";
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string line;
    Job current{};
    bool in_job = false;
    bool had_job = false;

    while (std::getline(f, line)) {
        if (line.find('{') != std::string::npos) {
            current = Job{};
            in_job = true;
            continue;
        }
        if (line.find('}') != std::string::npos) {
            if (in_job && current.id > 0) {
                reg.add(current);
                had_job = true;
                if (current.state == JobState::QUEUED) {
                    reg.enqueue(current.id);
                }
            }
            in_job = false;
            continue;
        }
        if (!in_job) continue;

        if (line.find("\"id\"") != std::string::npos) {
            current.id = extract_int(line, "id");
        } else if (line.find("\"command\"") != std::string::npos) {
            current.command = extract_quoted(line);
        } else if (line.find("\"state\"") != std::string::npos) {
            current.state = parse_state(extract_state(line));
        } else if (line.find("\"exit_code\"") != std::string::npos) {
            current.exit_code = extract_int(line, "exit_code");
        } else if (line.find("\"term_signal\"") != std::string::npos) {
            current.term_signal = extract_int(line, "term_signal");
        } else if (line.find("\"note\"") != std::string::npos) {
            current.note = extract_quoted(line);
        }
    }

    if (had_job) apply_recovery_policy(reg);
    return had_job;
}

void apply_recovery_policy(JobRegistry& reg) {
    for (Job* j : reg.by_state(JobState::RUNNING)) {
        j->state       = JobState::FAILED;
        j->exit_code   = -1;
        j->term_signal = 0;
        j->note        = "interrupted by restart";
    }
}
