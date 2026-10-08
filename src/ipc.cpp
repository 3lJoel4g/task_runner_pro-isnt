#include "ipc.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace ipc {

std::string default_socket_path() {
    const char* env = std::getenv("JOBRUNNER_SOCKET");
    return env ? std::string(env) : std::string("/tmp/jobrunner.sock");
}

int listen_unix(const std::string& path) {
    ::unlink(path.c_str());
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    ::fcntl(fd, F_SETFD, FD_CLOEXEC);        // <-- nueva línea
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (::bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { ::close(fd); return -1; }
    if (::listen(fd, 16) < 0) { ::close(fd); return -1; }
    return fd;
}

int connect_unix(const std::string& path) {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { ::close(fd); return -1; }
    return fd;
}

bool send_all(int fd, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n = ::write(fd, data.data() + sent, data.size() - sent);
        if (n < 0) { if (errno == EINTR) continue; return false; }
        sent += (std::size_t)n;
    }
    return true;
}

std::string recv_all(int fd) {
    std::string out;
    char buf[4096];
    for (;;) {
        const ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n > 0) { out.append(buf, (std::size_t)n); continue; }
        if (n == 0) break;
        if (errno == EINTR) continue;
        break;
    }
    return out;
}

std::string recv_line(int fd) {
    std::string out;
    char c;
    for (;;) {
        const ssize_t n = ::read(fd, &c, 1);
        if (n == 1) {
            out.push_back(c);
            if (c == '\n') break;
            continue;
        }
        if (n == 0) break;                       // conexión cerrada sin '\n'
        if (errno == EINTR) continue;
        break;
    }
    return out;
}

} // namespace ipc
