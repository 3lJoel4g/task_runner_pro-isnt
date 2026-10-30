#ifndef CONFIG_H
#define CONFIG_H

#include <string>

struct Config {
    int         max_workers     = 3;              // RF-05: concurrencia máxima
    int         max_queue_size  = 100;            // RF-25: capacidad de cola
    std::string data_dir        = "./data";       // RF-12: persistencia
    std::string log_file        = "jobrunner.log";// RF-14: bitácora
    std::string socket_path     = "/tmp/jobrunner.sock"; // RF-16
};

// Carga la configuración desde `config.json`. Si el archivo no existe,
// crea uno con valores por defecto y lo devuelve.
Config load_config(const std::string& path = "config.json");

#endif // CONFIG_H
