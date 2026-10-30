#ifndef JOB_REGISTRY_H
#define JOB_REGISTRY_H

#include <cstddef>
#include <deque>
#include <unordered_map>
#include <vector>

#include "job.h"

class JobRegistry {
public:
    // --- Configuración de límites (RF-05, RF-25) ---
    void set_limits(int max_workers, int max_queue_size);
    int  max_workers()      const { return max_workers_; }
    int  max_queue_size()   const { return max_queue_size_; }

    // --- Ciclo de vida ---
    int  next_id();
    void add(Job job);
    Job* find(int id);
    const Job* find(int id) const;
    std::vector<Job*> all();
    std::vector<Job*> by_state(JobState s);

    // --- Cola FIFO (RF-03) ---
    // Encola un trabajo (pasa a QUEUED). Devuelve false si la cola está llena.
    bool enqueue(int id);
    // Desencola el siguiente trabajo pendiente (FIFO) que esté QUEUED.
    // Devuelve -1 si no hay ninguno.
    int  dequeue_next();
    // Número de trabajos actualmente en RUNNING.
    int  count_running() const;
    // Número de trabajos actualmente en QUEUED.
    int  count_queued() const;
    // ¿Podemos lanzar un trabajo nuevo? (running < max_workers)
    bool can_spawn() const { return count_running() < max_workers_; }
    // ¿Hay espacio en la cola?
    bool queue_has_space() const { return count_queued() < max_queue_size_; }

    // Exponer el mapa completo para persistencia (solo lectura).
    const std::unordered_map<int, Job>& jobs() const { return jobs_; }

private:
    std::unordered_map<int, Job> jobs_;
    std::deque<int>              fifo_;      // IDs en orden de llegada
    int next_id_       = 0;
    int max_workers_   = 3;
    int max_queue_size_ = 100;
};

#endif // JOB_REGISTRY_H
