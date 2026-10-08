#ifndef CLIENT_H
#define CLIENT_H
int client_submit(int argc, char** argv);
int client_status(int argc, char** argv);
int client_list(int argc, char** argv);
int client_cancel(int argc, char** argv);
int client_shutdown();
#endif
