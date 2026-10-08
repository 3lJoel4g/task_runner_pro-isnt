#ifndef IPC_H
#define IPC_H
#include <string>
namespace ipc {
int listen_unix(const std::string& path);
int connect_unix(const std::string& path);
bool send_all(int fd, const std::string& data);
std::string recv_all(int fd);       // hasta EOF (cliente -> daemon ha cerrado)
std::string recv_line(int fd);      // hasta '\n' (petición de una línea)
std::string default_socket_path();
}
#endif
