// JobRunner - núcleo local (Avance 1)
//
// Un solo proceso, un solo hilo, un solo bucle de eventos:
//   - lee comandos del usuario por stdin (submit/status/list/cancel/output/quit)
//   - lanza cada trabajo como PROCESO HIJO (fork + execvp)
//   - captura stdout/stderr de cada hijo con pipes no bloqueantes
//   - detecta cuándo termina cada hijo (waitpid con WNOHANG)
//   - cancela con señales: SIGTERM primero, SIGKILL si no obedece
//
// Referencias: ADR-0001 (fork+execvp+pipes), ADR-0002 (duplicados),
//              RF-01..RF-06, RF-11, RNF-09.

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "job.h"

// ---------------------------------------------------------------------------
// Estado global del servicio
// ---------------------------------------------------------------------------
static const double KILL_GRACE_SECONDS = 3.0;      // SIGTERM -> espera -> SIGKILL
static const size_t MAX_CAPTURE = 1024 * 1024;     // tope de salida guardada por stream
static const size_t MAX_LINE = 64 * 1024;          // tope de una línea de comando

static int g_max_running = 4;                      // máximo de trabajos simultáneos
static int g_next_id = 1;
static std::map<int, Job> g_jobs;                  // ordenado por ID => orden FIFO
static volatile sig_atomic_t g_stop = 0;           // lo activa SIGINT/SIGTERM del servidor

static void on_signal(int) { g_stop = 1; }         // solo levanta una bandera (seguro)

static double now_seconds() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------
static std::string cmdline(const Job& j) {
    std::string s = j.command;
    for (const auto& a : j.args) s += " " + a;
    return s;
}

static bool parse_id(const std::string& s, int& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    long v = strtol(s.c_str(), &end, 10);
    if (errno != 0 || *end != '\0' || v < 1 || v > 1000000000L) return false;
    out = static_cast<int>(v);
    return true;
}

static int count_state(JobState st) {
    int n = 0;
    for (const auto& kv : g_jobs) if (kv.second.state == st) n++;
    return n;
}

static int active_count() { return count_state(JobState::QUEUED) + count_state(JobState::RUNNING); }

// Texto del código de salida: "0", "2", "143 (señal 15: Terminated)" o "-"
static std::string exit_text(const Job& j) {
    if (!is_terminal(j.state) || j.exit_code < 0) return "-";
    std::string s = std::to_string(j.exit_code);
    if (j.term_signal != 0) {
        s += " (señal " + std::to_string(j.term_signal) + ": " + strsignal(j.term_signal) + ")";
    }
    return s;
}

// ---------------------------------------------------------------------------
// Pipes: lectura no bloqueante
// ---------------------------------------------------------------------------
static void set_nonblock(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl != -1) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

// Lee TODO lo disponible ahora mismo sin bloquear. Si el hijo cerró su extremo
// (read devuelve 0 = EOF) cierra el descriptor y lo marca como -1.
static void drain_fd(int& fd, std::string& buf) {
    if (fd < 0) return;
    char tmp[4096];
    for (;;) {
        ssize_t n = read(fd, tmp, sizeof(tmp));
        if (n > 0) {
            if (buf.size() < MAX_CAPTURE) {
                size_t room = MAX_CAPTURE - buf.size();
                buf.append(tmp, std::min(static_cast<size_t>(n), room));
            }
            // si el tope se llenó, seguimos LEYENDO (y descartando) para que
            // el hijo nunca se bloquee escribiendo en un pipe lleno
        } else if (n == 0) {
            close(fd);
            fd = -1;
            return;
        } else if (errno == EINTR) {
            continue;
        } else if (errno == EAGAIN) {
            return;                       // no hay más datos por ahora
        } else {
            close(fd);
            fd = -1;
            return;
        }
    }
}

static void close_job_fds(Job& j) {
    if (j.out_fd >= 0) { close(j.out_fd); j.out_fd = -1; }
    if (j.err_fd >= 0) { close(j.err_fd); j.err_fd = -1; }
}

// ---------------------------------------------------------------------------
// Ciclo de vida de un trabajo
// ---------------------------------------------------------------------------
static void child_write(const char* s) {      // write() es seguro tras fork()
    ssize_t r = write(STDERR_FILENO, s, strlen(s));
    (void)r;
}

static void fail_to_start(Job& job, const std::string& why) {
    job.state = JobState::FAILED;
    job.exit_code = 127;
    job.err += "jobrunner: " + why + "\n";
}

static void start_job(Job& job) {
    // O_CLOEXEC: ningún hijo hereda por accidente los pipes de OTROS trabajos
    int out_pipe[2], err_pipe[2];
    if (pipe2(out_pipe, O_CLOEXEC) == -1) {
        fail_to_start(job, std::string("pipe() falló: ") + strerror(errno));
        return;
    }
    if (pipe2(err_pipe, O_CLOEXEC) == -1) {
        std::string why = std::string("pipe() falló: ") + strerror(errno);
        close(out_pipe[0]); close(out_pipe[1]);
        fail_to_start(job, why);
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::string why = std::string("fork() falló: ") + strerror(errno);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        fail_to_start(job, why);
        return;
    }

    if (pid == 0) {
        // ===== PROCESO HIJO =====
        setpgid(0, 0);                                   // grupo propio: así cancelamos también a sus nietos

        int devnull = open("/dev/null", O_RDONLY);       // stdin del trabajo = vacío
        if (devnull >= 0 && devnull != STDIN_FILENO) {   // (no debe robarle el teclado al servidor)
            dup2(devnull, STDIN_FILENO);
            close(devnull);
        }
        dup2(out_pipe[1], STDOUT_FILENO);                // stdout -> pipe
        dup2(err_pipe[1], STDERR_FILENO);                // stderr -> otro pipe
        // los originales se cierran solos al hacer exec (O_CLOEXEC);
        // las copias en fd 1 y 2 NO tienen CLOEXEC, así que sobreviven.

        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(job.command.c_str()));
        for (const auto& a : job.args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());

        // Si llegamos aquí, execvp FALLÓ (comando inexistente, sin permiso...)
        int e = errno;
        child_write("jobrunner: no se pudo ejecutar '");
        child_write(job.command.c_str());
        child_write("': ");
        child_write(strerror(e));
        child_write("\n");
        _exit(127);   // convención de shells: 127 = "command not found"
    }

    // ===== PROCESO PADRE =====
    setpgid(pid, pid);                       // (también aquí, para evitar carrera con el hijo)
    close(out_pipe[1]);                      // el padre solo LEE; si dejara abierto el extremo de
    close(err_pipe[1]);                      // escritura nunca recibiría EOF
    set_nonblock(out_pipe[0]);
    set_nonblock(err_pipe[0]);
    job.pid = pid;
    job.out_fd = out_pipe[0];
    job.err_fd = err_pipe[0];
    job.state = JobState::RUNNING;
}

// Se llama cuando waitpid() ya confirmó que el hijo terminó.
static void finish_job(Job& job, int status) {
    drain_fd(job.out_fd, job.out);           // recoger lo último que haya escrito
    drain_fd(job.err_fd, job.err);
    close_job_fds(job);

    if (WIFEXITED(status)) {
        job.exit_code = WEXITSTATUS(status);
        job.term_signal = 0;
        job.state = (job.exit_code == 0) ? JobState::SUCCEEDED : JobState::FAILED;
    } else if (WIFSIGNALED(status)) {
        job.term_signal = WTERMSIG(status);
        job.exit_code = 128 + job.term_signal;      // misma convención que bash ($?)
        job.state = job.cancel_requested ? JobState::CANCELED : JobState::FAILED;
    } else {
        job.exit_code = -1;
        job.state = JobState::FAILED;
    }
}

static void reap_children() {
    for (auto& kv : g_jobs) {
        Job& j = kv.second;
        if (j.state != JobState::RUNNING) continue;
        int status = 0;
        pid_t r = waitpid(j.pid, &status, WNOHANG);   // WNOHANG: "no esperes, solo pregunta"
        if (r == j.pid) {
            finish_job(j, status);
        } else if (r < 0 && errno != EINTR) {
            close_job_fds(j);
            j.state = JobState::FAILED;
            j.err += std::string("jobrunner: waitpid falló: ") + strerror(errno) + "\n";
        }
    }
}

static void send_signal(const Job& j, int sig) {
    if (kill(-j.pid, sig) == -1) kill(j.pid, sig);    // -pid = a todo el grupo de procesos
}

static void escalate_cancels() {
    double t = now_seconds();
    for (auto& kv : g_jobs) {
        Job& j = kv.second;
        if (j.state == JobState::RUNNING && j.cancel_requested && !j.kill_sent &&
            t - j.cancel_time >= KILL_GRACE_SECONDS) {
            send_signal(j, SIGKILL);                  // SIGKILL no se puede ignorar ni atrapar
            j.kill_sent = true;
        }
    }
}

static void schedule() {
    for (auto& kv : g_jobs) {
        if (count_state(JobState::RUNNING) >= g_max_running) break;
        if (kv.second.state == JobState::QUEUED) start_job(kv.second);
    }
}

static void shutdown_all() {
    for (auto& kv : g_jobs) {
        Job& j = kv.second;
        if (j.state == JobState::QUEUED) {
            j.state = JobState::CANCELED;
        } else if (j.state == JobState::RUNNING) {
            j.cancel_requested = true;
            send_signal(j, SIGTERM);
        }
    }
    double deadline = now_seconds() + 2.0;
    while (count_state(JobState::RUNNING) > 0 && now_seconds() < deadline) {
        reap_children();
        usleep(50000);
    }
    for (auto& kv : g_jobs) {
        if (kv.second.state == JobState::RUNNING) send_signal(kv.second, SIGKILL);
    }
    deadline = now_seconds() + 1.0;
    while (count_state(JobState::RUNNING) > 0 && now_seconds() < deadline) {
        reap_children();
        usleep(20000);
    }
}

// ---------------------------------------------------------------------------
// Comandos del usuario
// ---------------------------------------------------------------------------
static void cmd_submit(const std::vector<std::string>& t) {
    if (t.size() < 2) {
        std::cout << "ERROR: uso: submit <comando> [args...]\n";
        return;
    }
    std::string command = t[1];
    std::vector<std::string> args(t.begin() + 2, t.end());

    // ADR-0002: si ya hay un trabajo ACTIVO idéntico, devolver su ID
    for (const auto& kv : g_jobs) {
        const Job& j = kv.second;
        if ((j.state == JobState::QUEUED || j.state == JobState::RUNNING) &&
            j.command == command && j.args == args) {
            std::cout << "OK id=" << j.id << " (duplicado: ya existe un trabajo activo idéntico)\n";
            return;
        }
    }

    Job job;
    job.id = g_next_id++;
    job.command = command;
    job.args = args;
    job.state = JobState::QUEUED;
    g_jobs[job.id] = job;
    std::cout << "OK id=" << job.id << "\n";
    schedule();
}

static void print_status(const Job& j) {
    std::cout << "id=" << j.id
              << " estado=" << state_name(j.state)
              << " pid=" << (j.pid > 0 ? std::to_string(j.pid) : "-")
              << " exit=" << exit_text(j)
              << " cmd=" << cmdline(j) << "\n";
}

static void cmd_status(const std::vector<std::string>& t) {
    int id;
    if (t.size() != 2 || !parse_id(t[1], id)) { std::cout << "ERROR: uso: status <id>\n"; return; }
    auto it = g_jobs.find(id);
    if (it == g_jobs.end()) { std::cout << "ERROR: no existe el trabajo " << id << "\n"; return; }
    print_status(it->second);
}

static void cmd_list() {
    if (g_jobs.empty()) { std::cout << "(sin trabajos)\n"; return; }
    std::cout << std::left << std::setw(5) << "ID" << std::setw(11) << "ESTADO"
              << std::setw(8) << "PID" << std::setw(28) << "EXIT" << "COMANDO\n";
    for (const auto& kv : g_jobs) {
        const Job& j = kv.second;
        std::cout << std::left << std::setw(5) << j.id << std::setw(11) << state_name(j.state)
                  << std::setw(8) << (j.pid > 0 ? std::to_string(j.pid) : "-")
                  << std::setw(28) << exit_text(j) << cmdline(j) << "\n";
    }
}

static void cmd_cancel(const std::vector<std::string>& t) {
    int id;
    if (t.size() != 2 || !parse_id(t[1], id)) { std::cout << "ERROR: uso: cancel <id>\n"; return; }
    auto it = g_jobs.find(id);
    if (it == g_jobs.end()) { std::cout << "ERROR: no existe el trabajo " << id << "\n"; return; }
    Job& j = it->second;

    if (is_terminal(j.state)) {
        std::cout << "ERROR: el trabajo " << id << " ya terminó (" << state_name(j.state) << ")\n";
    } else if (j.state == JobState::QUEUED) {
        j.state = JobState::CANCELED;                  // nunca llegó a tener proceso
        std::cout << "OK trabajo " << id << " cancelado (estaba en cola)\n";
    } else if (j.cancel_requested) {
        std::cout << "OK la cancelación del trabajo " << id << " ya estaba solicitada\n";
    } else {
        j.cancel_requested = true;
        j.cancel_time = now_seconds();
        send_signal(j, SIGTERM);                       // petición "amable"; SIGKILL llega luego si hace falta
        std::cout << "OK cancelación solicitada para el trabajo " << id << " (SIGTERM enviado)\n";
    }
}

static void cmd_output(const std::vector<std::string>& t) {
    int id;
    if (t.size() != 2 || !parse_id(t[1], id)) { std::cout << "ERROR: uso: output <id>\n"; return; }
    auto it = g_jobs.find(id);
    if (it == g_jobs.end()) { std::cout << "ERROR: no existe el trabajo " << id << "\n"; return; }
    const Job& j = it->second;
    std::cout << "--- stdout (" << j.out.size() << " bytes) ---\n" << j.out;
    if (!j.out.empty() && j.out.back() != '\n') std::cout << "\n";
    std::cout << "--- stderr (" << j.err.size() << " bytes) ---\n" << j.err;
    if (!j.err.empty() && j.err.back() != '\n') std::cout << "\n";
}

static void cmd_help() {
    std::cout << "Comandos:\n"
                 "  submit <cmd> [args...]   enviar un trabajo (devuelve su ID)\n"
                 "  status <id>              estado, pid y código de salida\n"
                 "  list                     listar todos los trabajos\n"
                 "  cancel <id>              solicitar cancelación (SIGTERM, luego SIGKILL)\n"
                 "  output <id>              ver stdout/stderr capturados\n"
                 "  help                     esta ayuda\n"
                 "  quit                     terminar trabajos activos y salir\n";
}

// Divide una línea en palabras. Acepta "comillas dobles" y 'comillas simples'
// para agrupar (ej.: submit sh -c "echo hola; sleep 1"). Sin escapes.
static bool tokenize(const std::string& line, std::vector<std::string>& out) {
    std::string cur;
    bool in_word = false;
    char quote = 0;
    for (char ch : line) {
        if (quote) {
            if (ch == quote) quote = 0; else cur += ch;
        } else if (ch == '"' || ch == '\'') {
            quote = ch;
            in_word = true;
        } else if (ch == ' ' || ch == '\t' || ch == '\r') {
            if (in_word) { out.push_back(cur); cur.clear(); in_word = false; }
        } else {
            cur += ch;
            in_word = true;
        }
    }
    if (quote) return false;
    if (in_word) out.push_back(cur);
    return true;
}

// Devuelve false si hay que terminar el servicio.
static bool handle_line(const std::string& line) {
    std::vector<std::string> t;
    if (!tokenize(line, t)) {
        std::cout << "ERROR: comillas sin cerrar\n";
        return true;
    }
    if (t.empty()) return true;

    const std::string& c = t[0];
    if (c == "submit")       cmd_submit(t);
    else if (c == "status")  cmd_status(t);
    else if (c == "list")    cmd_list();
    else if (c == "cancel")  cmd_cancel(t);
    else if (c == "output")  cmd_output(t);
    else if (c == "help")    cmd_help();
    else if (c == "quit" || c == "exit") return false;
    else std::cout << "ERROR: comando desconocido '" << c << "' (escribe help)\n";
    return true;
}

// ---------------------------------------------------------------------------
// main: el bucle de eventos
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc > 1) {
        int n;
        if (!parse_id(argv[1], n) || n > 64) {
            std::cerr << "uso: " << argv[0] << " [max_trabajos_simultaneos (1-64)]\n";
            return 2;
        }
        g_max_running = n;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);        // Ctrl+C al servidor => apagado ordenado
    sigaction(SIGTERM, &sa, nullptr);

    std::cout << std::unitbuf;
    std::cout << "=== JobRunner (max simultáneos: " << g_max_running << ") — escribe help ===\n";

    const bool tty = isatty(STDIN_FILENO);
    bool stdin_open = true;
    bool quitting = false;
    std::string inbuf;

    while (!g_stop && !quitting) {
        if (!stdin_open && active_count() == 0) break;   // sin entrada y sin trabajo: terminar

        // 1) Dormir hasta que pase algo (entrada, salida de un hijo) o pasen 100 ms.
        std::vector<pollfd> fds;
        if (stdin_open) { pollfd p; p.fd = STDIN_FILENO; p.events = POLLIN; p.revents = 0; fds.push_back(p); }
        for (auto& kv : g_jobs) {
            Job& j = kv.second;
            if (j.state != JobState::RUNNING) continue;
            if (j.out_fd >= 0) { pollfd p; p.fd = j.out_fd; p.events = POLLIN; p.revents = 0; fds.push_back(p); }
            if (j.err_fd >= 0) { pollfd p; p.fd = j.err_fd; p.events = POLLIN; p.revents = 0; fds.push_back(p); }
        }
        if (tty && stdin_open) { std::cout << "jobrunner> "; }
        int n = poll(fds.data(), fds.size(), 100);
        if (n < 0) {
            if (errno == EINTR) continue;           // nos interrumpió una señal; revisar g_stop
            perror("poll");
            break;
        }

        // 2) Entrada del usuario
        if (stdin_open && n > 0 && (fds[0].revents & (POLLIN | POLLHUP | POLLERR))) {
            char tmp[1024];
            ssize_t r = read(STDIN_FILENO, tmp, sizeof(tmp));
            if (r > 0) {
                inbuf.append(tmp, static_cast<size_t>(r));
                size_t pos;
                while ((pos = inbuf.find('\n')) != std::string::npos) {
                    std::string line = inbuf.substr(0, pos);
                    inbuf.erase(0, pos + 1);
                    if (!handle_line(line)) { quitting = true; break; }
                }
                if (inbuf.size() > MAX_LINE) {
                    std::cout << "ERROR: línea demasiado larga, descartada\n";
                    inbuf.clear();
                }
            } else if (r == 0) {
                stdin_open = false;                 // EOF: no más comandos
                if (!inbuf.empty()) { handle_line(inbuf); inbuf.clear(); }
            } else if (errno != EINTR && errno != EAGAIN) {
                stdin_open = false;
            }
        }

        // 3) Salida de los hijos, fin de hijos, escalado de cancelaciones, cola
        for (auto& kv : g_jobs) {
            if (kv.second.state == JobState::RUNNING) {
                drain_fd(kv.second.out_fd, kv.second.out);
                drain_fd(kv.second.err_fd, kv.second.err);
            }
        }
        reap_children();
        escalate_cancels();
        schedule();
    }

    shutdown_all();
    std::cout << "\n=== JobRunner terminado ===\n";
    return 0;
}
