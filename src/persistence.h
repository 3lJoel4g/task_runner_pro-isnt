#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <string>
#include "job_registry.h"

// RF-12: guarda el estado actual en <data_dir>/jobs.json de forma atómica.
// Devuelve true si escribió correctamente.
bool save_state(const JobRegistry& reg, const std::string& data_dir);

// RF-13: carga el estado desde <data_dir>/jobs.json.
// Al cargar, aplica la política de recuperación de ADR-0005:
//   - RUNNING -> FAILED con note="interrupted by restart"
//   - QUEUED  -> se mantiene en cola (se reencola)
//   - terminales (SUCCEEDED/FAILED/CANCELED) -> se preservan
// Devuelve true si el archivo existía y se cargó.
bool load_state(JobRegistry& reg, const std::string& data_dir);

// RF-13: helper que aplica la política de recuperación sobre un registro
// ya cargado. Útil cuando load_state() ignora el archivo por corrupción.
void apply_recovery_policy(JobRegistry& reg);

#endif // PERSISTENCE_H
