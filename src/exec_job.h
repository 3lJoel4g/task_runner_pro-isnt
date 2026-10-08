#ifndef EXEC_JOB_H
#define EXEC_JOB_H

#include "job.h"

// RF-04, RF-07, RF-11:
// Ejecuta un trabajo: fork + execvp, captura stdout/stderr por separado
// con multiplexación poll() (sin deadlock), registra marcas de tiempo y
// actualiza state/exit_code/term_signal.
//
// Devuelve true si el trabajo se llegó a lanzar (aunque el comando
// termine con error). Devuelve false ante fallo de setup interno
// (pipe/fork/dup2). En ambos casos el Job queda con state y timestamps
// coherentes.
bool exec_job(Job& job);

#endif // EXEC_JOB_H
