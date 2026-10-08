#include "client.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

#include "ipc.h"

namespace {

int one_shot(const std::string& request) {
    const int fd = ipc::connect_unix(ipc::default_socket_path());
    if (fd < 0) {
        std::cerr << "error: no se pudo conectar al daemon. "
                     "¿Está corriendo './server daemon'?\n";
        return 3;
    }
    ipc::send_all(fd, request);
    const std::string resp = ipc::recv_all(fd);
    ::close(fd);
    std::cout << resp;
    return resp.rfind("OK", 0) == 0 ? 0 : 1;
}

} // namespace

int client_submit(int argc, char** argv) {
    if (argc < 3) { std::cerr << "uso: server submit <cmd> [args...]\n"; return 2; }
    std::string req = "SUBMIT\t" + std::string(argv[2]);
    for (int i = 3; i < argc; ++i) req += "\t" + std::string(argv[i]);
    req += "\n";
    return one_shot(req);
}

int client_status(int argc, char** argv) {
    if (argc < 3) { std::cerr << "uso: server status <id>\n"; return 2; }
    return one_shot("STATUS\t" + std::string(argv[2]) + "\n");
}

int client_list(int argc, char** argv) {
    std::string req = "LIST";
    if (argc >= 3) req += "\t" + std::string(argv[2]);
    req += "\n";
    return one_shot(req);
}

int client_cancel(int argc, char** argv) {
    if (argc < 3) { std::cerr << "uso: server cancel <id>\n"; return 2; }
    return one_shot("CANCEL\t" + std::string(argv[2]) + "\n");
}

int client_shutdown() { return one_shot("SHUTDOWN\n"); }
