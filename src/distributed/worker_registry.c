/**
 * @file worker_registry.c
 * @brief Worker registration and discovery implementation
 *
 * Manages remote worker lifecycle, capability tracking, health monitoring,
 * and intelligent worker selection for job distribution.
 */

#include "cyxmake/distributed/worker_registry.h"
#include "cyxmake/logger.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
#include "cyxmake/threading.h"
#endif

/* Portable popcount for MSVC compatibility */
#ifdef _MSC_VER
static inline int popcount32(uint32_t x) {
    x = x - ((x >> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = (x + (x >> 4)) & 0x0F0F0F0F;
    x = x + (x >> 8);
    x = x + (x >> 16);
    return x & 0x3F;
}
#define POPCOUNT32(x) popcount32(x)
#else
#define POPCOUNT32(x) __builtin_popcount(x)
#endif

/* ============================================================
 * Constants
 * ============================================================ */

#define DEFAULT_HEARTBEAT_INTERVAL_SEC 30
#define DEFAULT_HEARTBEAT_TIMEOUT_SEC 90
#define DEFAULT_MAX_MISSED_HEARTBEATS 3
#define DEFAULT_MAX_WORKERS 256
#define DEFAULT_OFFLINE_REMOVAL_DELAY_SEC 300

/* Health score weights */
#define HEALTH_WEIGHT_SUCCESS_RATE 0.3
#define HEALTH_WEIGHT_LATENCY 0.2
#define HEALTH_WEIGHT_LOAD 0.2
#define HEALTH_WEIGHT_HEARTBEAT 0.2
#define HEALTH_WEIGHT_UPTIME 0.1

/* ============================================================
 * Registry Structure
 * ============================================================ */

struct WorkerRegistry {
    WorkerRegistryConfig config;
    RemoteWorker* workers;        /* Linked list head */
    int worker_count;

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    MutexHandle mutex;            /* Thread safety */
#endif

    WorkerRegistryCallbacks callbacks;
};

/* ============================================================
 * Helper Functions
 * ============================================================ */

static char* generate_worker_id(void) {
    /* Simple UUID-like ID generation */
    char* id = malloc(37);
    if (!id) return NULL;

    static unsigned int counter = 0;
    time_t now = time(NULL);

    snprintf(id, 37, "worker-%08x-%04x-%04x",
             (unsigned int)now,
             (unsigned int)(counter++ & 0xFFFF),
             (unsigned int)(rand() & 0xFFFF));

    return id;
}

static void registry_lock(WorkerRegistry* registry) {
#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    mutex_lock(&registry->mutex);
#else
    (void)registry;
#endif
}

static void registry_unlock(WorkerRegistry* registry) {
#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    mutex_unlock(&registry->mutex);
#else
    (void)registry;
#endif
}

/* ============================================================
 * Configuration
 * ============================================================ */

WorkerRegistryConfig worker_registry_config_default(void) {
    WorkerRegistryConfig config = {
        .heartbeat_interval_sec = DEFAULT_HEARTBEAT_INTERVAL_SEC,
        .heartbeat_timeout_sec = DEFAULT_HEARTBEAT_TIMEOUT_SEC,
        .max_missed_heartbeats = DEFAULT_MAX_MISSED_HEARTBEATS,
        .max_workers = DEFAULT_MAX_WORKERS,
        .auto_remove_offline = false,
        .offline_removal_delay_sec = DEFAULT_OFFLINE_REMOVAL_DELAY_SEC
    };
    return config;
}

/* ============================================================
 * Remote Worker Management
 * ============================================================ */

RemoteWorker* remote_worker_create(const char* id, const char* name) {
    RemoteWorker* worker = calloc(1, sizeof(RemoteWorker));
    if (!worker) {
        log_error("Failed to allocate remote worker");
        return NULL;
    }

    worker->id = id ? strdup(id) : generate_worker_id();
    worker->name = name ? strdup(name) : NULL;
    worker->state = WORKER_STATE_OFFLINE;
    worker->health_score = 1.0;  /* Start healthy */
    worker->max_jobs = 4;        /* Default concurrency */

    if (!worker->id) {
        free(worker);
        return NULL;
    }

    return worker;
}

void remote_worker_free(RemoteWorker* worker) {
    if (!worker) return;

    free(worker->id);
    free(worker->name);
    free(worker->hostname);
    worker_tools_free(worker->tools);

    /* Don't free connection - managed by network layer */
    worker->connection = NULL;

    free(worker);
}

/* ============================================================
 * Tool Management
 * ============================================================ */

void worker_add_tool(RemoteWorker* worker,
                     const char* name,
                     const char* path,
                     const char* version) {
    if (!worker || !name) return;

    WorkerTool* tool = calloc(1, sizeof(WorkerTool));
    if (!tool) return;

    tool->name = strdup(name);
    tool->path = path ? strdup(path) : NULL;
    tool->version = version ? strdup(version) : NULL;

    /* Add to front of list */
    tool->next = worker->tools;
    worker->tools = tool;
}

WorkerTool* worker_find_tool(RemoteWorker* worker, const char* name) {
    if (!worker || !name) return NULL;

    for (WorkerTool* tool = worker->tools; tool; tool = tool->next) {
        if (strcmp(tool->name, name) == 0) {
            return tool;
        }
    }
    return NULL;
}

bool worker_has_tool(RemoteWorker* worker,
                     const char* name,
                     const char* min_version) {
    WorkerTool* tool = worker_find_tool(worker, name);
    if (!tool) return false;

    if (!min_version) return true;

    /* Simple version comparison (assumes semver-like) */
    if (!tool->version) return false;

    /* TODO: Proper version comparison */
    return strcmp(tool->version, min_version) >= 0;
}

void worker_tools_free(WorkerTool* tools) {
    while (tools) {
        WorkerTool* next = tools->next;
        free(tools->name);
        free(tools->path);
        free(tools->version);
        free(tools);
        tools = next;
    }
}

/* ============================================================
 * Registry API Implementation
 * ============================================================ */

WorkerRegistry* worker_registry_create(const WorkerRegistryConfig* config) {
    WorkerRegistry* registry = calloc(1, sizeof(WorkerRegistry));
    if (!registry) {
        log_error("Failed to allocate worker registry");
        return NULL;
    }

    if (config) {
        registry->config = *config;
    } else {
        registry->config = worker_registry_config_default();
    }

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    if (!mutex_init(&registry->mutex)) {
        log_error("Failed to create registry mutex");
        free(registry);
        return NULL;
    }
#endif

    log_debug("Worker registry created (max workers: %d)",
              registry->config.max_workers);

    return registry;
}

void worker_registry_free(WorkerRegistry* registry) {
    if (!registry) return;

    /* Free all workers */
    RemoteWorker* worker = registry->workers;
    while (worker) {
        RemoteWorker* next = worker->next;
        remote_worker_free(worker);
        worker = next;
    }

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    mutex_destroy(&registry->mutex);
#endif

    free(registry);
    log_debug("Worker registry freed");
}

void worker_registry_set_callbacks(WorkerRegistry* registry,
                                    const WorkerRegistryCallbacks* callbacks) {
    if (!registry || !callbacks) return;
    registry->callbacks = *callbacks;
}

RemoteWorker* worker_registry_register(WorkerRegistry* registry,
                                        const WorkerSystemInfo* worker_info,
                                        NetworkConnection* connection) {
    if (!registry) return NULL;

    registry_lock(registry);

    /* Check capacity */
    if (registry->worker_count >= registry->config.max_workers) {
        log_warning("Worker registry full (max: %d)", registry->config.max_workers);
        registry_unlock(registry);
        return NULL;
    }

    /* Create worker - name will be set later from HELLO message */
    RemoteWorker* worker = remote_worker_create(NULL, NULL);
    if (!worker) {
        registry_unlock(registry);
        return NULL;
    }

    /* Set connection */
    worker->connection = connection;
    worker->state = WORKER_STATE_ONLINE;
    worker->connected_at = time(NULL);
    worker->last_heartbeat = worker->connected_at;

    /* Copy system info */
    if (worker_info) {
        /* Copy basic fields */
        worker->system_info.cpu_cores = worker_info->cpu_cores;
        worker->system_info.cpu_threads = worker_info->cpu_threads;
        worker->system_info.memory_mb = worker_info->memory_mb;
        worker->system_info.disk_free_mb = worker_info->disk_free_mb;

        /* Deep copy strings */
        if (worker_info->arch) {
            worker->system_info.arch = strdup(worker_info->arch);
        }
        if (worker_info->os) {
            worker->system_info.os = strdup(worker_info->os);
            /* Also use as hostname if not set */
            if (!worker->hostname) {
                worker->hostname = strdup(worker_info->os);
            }
        }
        if (worker_info->os_version) {
            worker->system_info.os_version = strdup(worker_info->os_version);
        }

        /* Set max jobs based on CPU cores */
        if (worker_info->cpu_cores > 0) {
            worker->max_jobs = worker_info->cpu_cores;
        }
    }

    /* Add to list */
    worker->next = registry->workers;
    registry->workers = worker;
    registry->worker_count++;

    log_info("Worker registered: %s (%s)", worker->id,
             worker->hostname ? worker->hostname : "unknown");

    registry_unlock(registry);

    /* Callback */
    if (registry->callbacks.on_registered) {
        registry->callbacks.on_registered(registry, worker,
                                           registry->callbacks.user_data);
    }

    return worker;
}

void worker_registry_unregister(WorkerRegistry* registry,
                                 const char* worker_id,
                                 const char* reason) {
    if (!registry || !worker_id) return;

    registry_lock(registry);

    RemoteWorker* prev = NULL;
    RemoteWorker* worker = registry->workers;

    while (worker) {
        if (strcmp(worker->id, worker_id) == 0) {
            /* Remove from list */
            if (prev) {
                prev->next = worker->next;
            } else {
                registry->workers = worker->next;
            }
            registry->worker_count--;

            log_info("Worker unregistered: %s (%s)",
                     worker_id, reason ? reason : "no reason");

            registry_unlock(registry);

            /* Callback before freeing */
            if (registry->callbacks.on_unregistered) {
                registry->callbacks.on_unregistered(registry, worker_id, reason,
                                                     registry->callbacks.user_data);
            }

            remote_worker_free(worker);
            return;
        }
        prev = worker;
        worker = worker->next;
    }

    registry_unlock(registry);
    log_warning("Worker not found for unregistration: %s", worker_id);
}

RemoteWorker* worker_registry_find_by_id(WorkerRegistry* registry,
                                          const char* worker_id) {
    if (!registry || !worker_id) return NULL;

    registry_lock(registry);

    for (RemoteWorker* worker = registry->workers; worker; worker = worker->next) {
        if (strcmp(worker->id, worker_id) == 0) {
            registry_unlock(registry);
            return worker;
        }
    }

    registry_unlock(registry);
    return NULL;
}

RemoteWorker* worker_registry_find_by_connection(WorkerRegistry* registry,
                                                   NetworkConnection* connection) {
    if (!registry || !connection) return NULL;

    registry_lock(registry);

    for (RemoteWorker* worker = registry->workers; worker; worker = worker->next) {
        if (worker->connection == connection) {
            registry_unlock(registry);
            return worker;
        }
    }

    registry_unlock(registry);
    return NULL;
}

RemoteWorker* worker_registry_find_by_name(WorkerRegistry* registry,
                                            const char* name) {
    if (!registry || !name) return NULL;

    registry_lock(registry);

    for (RemoteWorker* worker = registry->workers; worker; worker = worker->next) {
        if (worker->name && strcmp(worker->name, name) == 0) {
            registry_unlock(registry);
            return worker;
        }
    }

    registry_unlock(registry);
    return NULL;
}

int worker_registry_get_count(WorkerRegistry* registry) {
    return registry ? registry->worker_count : 0;
}

int worker_registry_get_online_count(WorkerRegistry* registry) {
    if (!registry) return 0;

    registry_lock(registry);

    int count = 0;
    for (RemoteWorker* worker = registry->workers; worker; worker = worker->next) {
        if (worker->state == WORKER_STATE_ONLINE ||
            worker->state == WORKER_STATE_BUSY) {
            count++;
        }
    }

    registry_unlock(registry);
    return count;
}

int worker_registry_get_available_slots(WorkerRegistry* registry) {
    if (!registry) return 0;

    registry_lock(registry);

    int slots = 0;
    for (RemoteWorker* worker = registry->workers; worker; worker = worker->next) {
        if (worker->state == WORKER_STATE_ONLINE ||
            worker->state == WORKER_STATE_BUSY) {
            int available = worker->max_jobs - worker->active_jobs;
            if (available > 0) {
                slots += available;
            }
        }
    }

    registry_unlock(registry);
    return slots;
}

void worker_registry_foreach(WorkerRegistry* registry,
                              void (*callback)(RemoteWorker* worker, void* user_data),
                              void* user_data) {
    if (!registry || !callback) return;

    registry_lock(registry);

    for (RemoteWorker* worker = registry->workers; worker; worker = worker->next) {
        callback(worker, user_data);
    }

    registry_unlock(registry);
}

/* ============================================================
 * Worker Selection
 * ============================================================ */

static double score_worker(RemoteWorker* worker,
                           const WorkerSelectionCriteria* criteria) {
    double score = worker->health_score;

    /* Check required capabilities */
    if (criteria->required_capabilities) {
        if ((worker->capabilities & criteria->required_capabilities) !=
            criteria->required_capabilities) {
            return -1.0;  /* Doesn't meet requirements */
        }
    }

    /* Check available slots */
    int available = worker->max_jobs - worker->active_jobs;
    if (criteria->min_available_slots > 0 &&
        available < criteria->min_available_slots) {
        return -1.0;  /* Not enough slots */
    }

    /* Bonus for preferred capabilities */
    if (criteria->preferred_capabilities) {
        uint32_t matched = worker->capabilities & criteria->preferred_capabilities;
        int matched_count = POPCOUNT32(matched);
        int preferred_count = POPCOUNT32(criteria->preferred_capabilities);
        if (preferred_count > 0) {
            score += 0.2 * ((double)matched_count / preferred_count);
        }
    }

    /* Bonus for low load */
    if (criteria->prefer_idle) {
        double load = (double)worker->active_jobs / worker->max_jobs;
        score += 0.3 * (1.0 - load);
    }

    /* Check target architecture */
    if (criteria->target_arch && worker->system_info.arch) {
        if (strcmp(criteria->target_arch, worker->system_info.arch) == 0) {
            score += 0.2;  /* Native arch bonus */
        }
    }

    /* Check target OS */
    if (criteria->target_os && worker->system_info.os) {
        if (strcmp(criteria->target_os, worker->system_info.os) == 0) {
            score += 0.1;  /* Native OS bonus */
        }
    }

    return score;
}

RemoteWorker* worker_registry_select_worker(WorkerRegistry* registry,
                                              const WorkerSelectionCriteria* criteria) {
    if (!registry) return NULL;

    registry_lock(registry);

    RemoteWorker* best = NULL;
    double best_score = -1.0;

    for (RemoteWorker* worker = registry->workers; worker; worker = worker->next) {
        /* Only consider online or busy workers */
        if (worker->state != WORKER_STATE_ONLINE &&
            worker->state != WORKER_STATE_BUSY) {
            continue;
        }

        /* Check if worker has available slots */
        if (worker->active_jobs >= worker->max_jobs) {
            continue;
        }

        double score = score_worker(worker, criteria);
        if (score > best_score) {
            best_score = score;
            best = worker;
        }
    }

    registry_unlock(registry);

    if (best) {
        log_debug("Selected worker %s (score: %.2f)", best->id, best_score);
    }

    return best;
}

int worker_registry_select_workers(WorkerRegistry* registry,
                                    const WorkerSelectionCriteria* criteria,
                                    int max_workers,
                                    RemoteWorker** out_workers) {
    if (!registry || !out_workers || max_workers <= 0) return 0;

    registry_lock(registry);

    /* Score all workers */
    typedef struct {
        RemoteWorker* worker;
        double score;
    } ScoredWorker;

    ScoredWorker* scored = malloc(sizeof(ScoredWorker) * registry->worker_count);
    if (!scored) {
        registry_unlock(registry);
        return 0;
    }

    int scored_count = 0;
    for (RemoteWorker* worker = registry->workers; worker; worker = worker->next) {
        if (worker->state != WORKER_STATE_ONLINE &&
            worker->state != WORKER_STATE_BUSY) {
            continue;
        }

        if (worker->active_jobs >= worker->max_jobs) {
            continue;
        }

        double score = score_worker(worker, criteria);
        if (score >= 0) {
            scored[scored_count].worker = worker;
            scored[scored_count].score = score;
            scored_count++;
        }
    }

    /* Sort by score (simple bubble sort for small lists) */
    for (int i = 0; i < scored_count - 1; i++) {
        for (int j = 0; j < scored_count - i - 1; j++) {
            if (scored[j].score < scored[j + 1].score) {
                ScoredWorker tmp = scored[j];
                scored[j] = scored[j + 1];
                scored[j + 1] = tmp;
            }
        }
    }

    /* Select top workers */
    int selected = 0;
    for (int i = 0; i < scored_count && selected < max_workers; i++) {
        out_workers[selected++] = scored[i].worker;
    }

    free(scored);
    registry_unlock(registry);

    return selected;
}

/* ============================================================
 * Worker State Management
 * ============================================================ */

void worker_registry_set_state(WorkerRegistry* registry,
                                RemoteWorker* worker,
                                WorkerState new_state) {
    if (!registry || !worker) return;

    WorkerState old_state = worker->state;
    if (old_state == new_state) return;

    worker->state = new_state;

    log_debug("Worker %s state changed: %s -> %s",
              worker->id,
              worker_state_name(old_state),
              worker_state_name(new_state));

    if (registry->callbacks.on_state_changed) {
        registry->callbacks.on_state_changed(registry, worker,
                                              old_state, new_state,
                                              registry->callbacks.user_data);
    }
}

void worker_registry_heartbeat(WorkerRegistry* registry,
                                RemoteWorker* worker,
                                const WorkerSystemInfo* updated_info) {
    if (!registry || !worker) return;

    worker->last_heartbeat = time(NULL);
    worker->missed_heartbeats = 0;

    /* Update system info if provided */
    if (updated_info) {
        /* Update system info fields */
        worker->system_info.memory_mb = updated_info->memory_mb;
        worker->system_info.disk_free_mb = updated_info->disk_free_mb;
        /* Note: cpu_usage and memory_usage should be updated via STATUS_UPDATE messages */
    }

    /* Recalculate health */
    worker_registry_update_health(registry, worker);
}

void worker_registry_update_job_count(WorkerRegistry* registry,
                                       RemoteWorker* worker,
                                       int delta) {
    if (!registry || !worker) return;

    worker->active_jobs += delta;
    if (worker->active_jobs < 0) worker->active_jobs = 0;

    /* Update state based on load */
    if (worker->active_jobs >= worker->max_jobs) {
        if (worker->state == WORKER_STATE_ONLINE) {
            worker_registry_set_state(registry, worker, WORKER_STATE_BUSY);
        }
    } else {
        if (worker->state == WORKER_STATE_BUSY) {
            worker_registry_set_state(registry, worker, WORKER_STATE_ONLINE);
        }
    }
}

void worker_registry_record_job_complete(WorkerRegistry* registry,
                                          RemoteWorker* worker,
                                          bool success,
                                          double duration_sec) {
    if (!registry || !worker) return;

    if (success) {
        worker->total_jobs_completed++;
    } else {
        worker->total_jobs_failed++;
    }

    /* Update average duration (exponential moving average) */
    if (worker->avg_job_duration_sec == 0) {
        worker->avg_job_duration_sec = duration_sec;
    } else {
        worker->avg_job_duration_sec =
            0.9 * worker->avg_job_duration_sec + 0.1 * duration_sec;
    }

    /* Recalculate health */
    worker_registry_update_health(registry, worker);
}

void worker_registry_check_heartbeats(WorkerRegistry* registry) {
    if (!registry) return;

    registry_lock(registry);

    time_t now = time(NULL);
    RemoteWorker* worker = registry->workers;

    while (worker) {
        RemoteWorker* next = worker->next;  /* Save next in case of removal */

        if (worker->state == WORKER_STATE_ONLINE ||
            worker->state == WORKER_STATE_BUSY) {

            int elapsed = (int)(now - worker->last_heartbeat);

            if (elapsed > registry->config.heartbeat_timeout_sec) {
                worker->missed_heartbeats++;

                log_warning("Worker %s missed heartbeat (%d/%d)",
                            worker->id, worker->missed_heartbeats,
                            registry->config.max_missed_heartbeats);

                if (worker->missed_heartbeats >= registry->config.max_missed_heartbeats) {
                    log_warning("Worker %s marked offline (missed %d heartbeats)",
                                worker->id, worker->missed_heartbeats);
                    worker_registry_set_state(registry, worker, WORKER_STATE_OFFLINE);
                }
            }
        }

        worker = next;
    }

    registry_unlock(registry);
}

/* ============================================================
 * Worker Health
 * ============================================================ */

double worker_registry_calculate_health(RemoteWorker* worker) {
    if (!worker) return 0.0;

    double health = 0.0;

    /* Success rate */
    int total = worker->total_jobs_completed + worker->total_jobs_failed;
    if (total > 0) {
        health += HEALTH_WEIGHT_SUCCESS_RATE *
                  ((double)worker->total_jobs_completed / total);
    } else {
        health += HEALTH_WEIGHT_SUCCESS_RATE;  /* No jobs = assume good */
    }

    /* Latency (lower is better, normalize to 0-1) */
    if (worker->network_latency_ms > 0) {
        double latency_score = 1.0 - fmin(worker->network_latency_ms / 1000.0, 1.0);
        health += HEALTH_WEIGHT_LATENCY * latency_score;
    } else {
        health += HEALTH_WEIGHT_LATENCY;
    }

    /* Load (lower is better) */
    double load = (worker->cpu_usage + worker->memory_usage) / 2.0;
    health += HEALTH_WEIGHT_LOAD * (1.0 - load);

    /* Heartbeat (no missed = good) */
    double heartbeat_score = 1.0 - fmin(worker->missed_heartbeats / 3.0, 1.0);
    health += HEALTH_WEIGHT_HEARTBEAT * heartbeat_score;

    /* Uptime (longer is better, cap at 24 hours for max score) */
    if (worker->connected_at > 0) {
        time_t uptime = time(NULL) - worker->connected_at;
        double uptime_hours = (double)uptime / 3600.0;
        double uptime_score = fmin(uptime_hours / 24.0, 1.0);
        health += HEALTH_WEIGHT_UPTIME * uptime_score;
    } else {
        health += HEALTH_WEIGHT_UPTIME;
    }

    return fmin(fmax(health, 0.0), 1.0);  /* Clamp to 0-1 */
}

void worker_registry_update_health(WorkerRegistry* registry,
                                    RemoteWorker* worker) {
    if (!registry || !worker) return;

    double old_score = worker->health_score;
    double new_score = worker_registry_calculate_health(worker);
    worker->health_score = new_score;

    /* Notify if significant change (>5%) */
    if (fabs(new_score - old_score) > 0.05) {
        if (registry->callbacks.on_health_changed) {
            registry->callbacks.on_health_changed(registry, worker,
                                                   old_score, new_score,
                                                   registry->callbacks.user_data);
        }
    }
}

/* ============================================================
 * Utility Functions
 * ============================================================ */

const char* worker_state_name(WorkerState state) {
    switch (state) {
        case WORKER_STATE_OFFLINE: return "OFFLINE";
        case WORKER_STATE_CONNECTING: return "CONNECTING";
        case WORKER_STATE_ONLINE: return "ONLINE";
        case WORKER_STATE_BUSY: return "BUSY";
        case WORKER_STATE_DRAINING: return "DRAINING";
        case WORKER_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* worker_capability_name(WorkerCapability cap) {
    switch (cap) {
        case WORKER_CAP_NONE: return "NONE";
        case WORKER_CAP_COMPILE_C: return "COMPILE_C";
        case WORKER_CAP_COMPILE_CPP: return "COMPILE_CPP";
        case WORKER_CAP_COMPILE_RUST: return "COMPILE_RUST";
        case WORKER_CAP_COMPILE_GO: return "COMPILE_GO";
        case WORKER_CAP_CMAKE: return "CMAKE";
        case WORKER_CAP_MAKE: return "MAKE";
        case WORKER_CAP_NINJA: return "NINJA";
        case WORKER_CAP_MSBUILD: return "MSBUILD";
        case WORKER_CAP_CROSS_ARM: return "CROSS_ARM";
        case WORKER_CAP_CROSS_ARM64: return "CROSS_ARM64";
        case WORKER_CAP_CROSS_X86: return "CROSS_X86";
        case WORKER_CAP_CROSS_X64: return "CROSS_X64";
        case WORKER_CAP_CROSS_WASM: return "CROSS_WASM";
        case WORKER_CAP_GPU_CUDA: return "GPU_CUDA";
        case WORKER_CAP_GPU_OPENCL: return "GPU_OPENCL";
        case WORKER_CAP_GPU_VULKAN: return "GPU_VULKAN";
        case WORKER_CAP_GPU_METAL: return "GPU_METAL";
        case WORKER_CAP_SANDBOX: return "SANDBOX";
        case WORKER_CAP_DOCKER: return "DOCKER";
        case WORKER_CAP_HIGH_MEMORY: return "HIGH_MEMORY";
        case WORKER_CAP_SSD_STORAGE: return "SSD_STORAGE";
        default: return "UNKNOWN";
    }
}

uint32_t worker_capabilities_parse(const char** capability_names, int count) {
    uint32_t caps = 0;

    for (int i = 0; i < count; i++) {
        const char* name = capability_names[i];
        if (!name) continue;

        if (strcmp(name, "COMPILE_C") == 0) caps |= WORKER_CAP_COMPILE_C;
        else if (strcmp(name, "COMPILE_CPP") == 0) caps |= WORKER_CAP_COMPILE_CPP;
        else if (strcmp(name, "COMPILE_RUST") == 0) caps |= WORKER_CAP_COMPILE_RUST;
        else if (strcmp(name, "COMPILE_GO") == 0) caps |= WORKER_CAP_COMPILE_GO;
        else if (strcmp(name, "CMAKE") == 0) caps |= WORKER_CAP_CMAKE;
        else if (strcmp(name, "MAKE") == 0) caps |= WORKER_CAP_MAKE;
        else if (strcmp(name, "NINJA") == 0) caps |= WORKER_CAP_NINJA;
        else if (strcmp(name, "MSBUILD") == 0) caps |= WORKER_CAP_MSBUILD;
        else if (strcmp(name, "CROSS_ARM") == 0) caps |= WORKER_CAP_CROSS_ARM;
        else if (strcmp(name, "CROSS_ARM64") == 0) caps |= WORKER_CAP_CROSS_ARM64;
        else if (strcmp(name, "CROSS_X86") == 0) caps |= WORKER_CAP_CROSS_X86;
        else if (strcmp(name, "CROSS_X64") == 0) caps |= WORKER_CAP_CROSS_X64;
        else if (strcmp(name, "CROSS_WASM") == 0) caps |= WORKER_CAP_CROSS_WASM;
        else if (strcmp(name, "GPU_CUDA") == 0) caps |= WORKER_CAP_GPU_CUDA;
        else if (strcmp(name, "GPU_OPENCL") == 0) caps |= WORKER_CAP_GPU_OPENCL;
        else if (strcmp(name, "GPU_VULKAN") == 0) caps |= WORKER_CAP_GPU_VULKAN;
        else if (strcmp(name, "GPU_METAL") == 0) caps |= WORKER_CAP_GPU_METAL;
        else if (strcmp(name, "SANDBOX") == 0) caps |= WORKER_CAP_SANDBOX;
        else if (strcmp(name, "DOCKER") == 0) caps |= WORKER_CAP_DOCKER;
        else if (strcmp(name, "HIGH_MEMORY") == 0) caps |= WORKER_CAP_HIGH_MEMORY;
        else if (strcmp(name, "SSD_STORAGE") == 0) caps |= WORKER_CAP_SSD_STORAGE;
    }

    return caps;
}
