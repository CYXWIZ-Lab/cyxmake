/**
 * @file worker_registry.h
 * @brief Worker registration and discovery for distributed builds
 *
 * Manages remote worker registration, capability tracking, health monitoring,
 * and worker selection for job distribution.
 */

#ifndef CYXMAKE_DISTRIBUTED_WORKER_REGISTRY_H
#define CYXMAKE_DISTRIBUTED_WORKER_REGISTRY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "cyxmake/distributed/protocol.h"
#include "cyxmake/distributed/network_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* WorkerState and WorkerCapability are defined in protocol.h */

/* ============================================================
 * Worker Tool Information
 * ============================================================ */

typedef struct WorkerTool {
    char* name;                   /* Tool name (e.g., "gcc", "cmake") */
    char* path;                   /* Full path to tool */
    char* version;                /* Version string */
    struct WorkerTool* next;      /* Linked list */
} WorkerTool;

/* ============================================================
 * Remote Worker
 * ============================================================ */

typedef struct RemoteWorker {
    /* Identity */
    char* id;                     /* Unique worker ID (UUID) */
    char* name;                   /* User-assigned name */
    char* hostname;               /* Hostname or IP */
    uint16_t port;                /* Worker port */

    /* State */
    WorkerState state;            /* Current worker state */
    time_t connected_at;          /* Connection timestamp */
    time_t last_heartbeat;        /* Last heartbeat received */
    int missed_heartbeats;        /* Consecutive missed heartbeats */

    /* Capabilities */
    uint32_t capabilities;        /* Capability bitmask */
    WorkerSystemInfo system_info; /* System information */
    WorkerTool* tools;            /* Available tools (linked list) */

    /* Job tracking */
    int active_jobs;              /* Current number of active jobs */
    int max_jobs;                 /* Maximum concurrent jobs */
    int total_jobs_completed;     /* Total jobs completed */
    int total_jobs_failed;        /* Total jobs failed */
    double avg_job_duration_sec;  /* Average job duration */

    /* Performance metrics */
    double health_score;          /* Overall health score (0.0 - 1.0) */
    double cpu_usage;             /* Last reported CPU usage (0.0 - 1.0) */
    double memory_usage;          /* Last reported memory usage (0.0 - 1.0) */
    double network_latency_ms;    /* Network latency to this worker */

    /* Network connection */
    NetworkConnection* connection; /* Active network connection */

    /* Registry internal */
    struct RemoteWorker* next;    /* Linked list for registry */
} RemoteWorker;

/* ============================================================
 * Worker Registry
 * ============================================================ */

typedef struct WorkerRegistry WorkerRegistry;

/* ============================================================
 * Worker Selection Criteria
 * ============================================================ */

typedef struct {
    uint32_t required_capabilities;  /* Must have these capabilities */
    uint32_t preferred_capabilities; /* Nice to have */
    const char* target_arch;         /* Target architecture (optional) */
    const char* target_os;           /* Target OS (optional) */
    int min_available_slots;         /* Minimum job slots available */
    bool prefer_local;               /* Prefer workers on same network */
    bool prefer_idle;                /* Prefer workers with low load */
} WorkerSelectionCriteria;

/* ============================================================
 * Worker Event Callbacks
 * ============================================================ */

typedef void (*OnWorkerRegisteredCallback)(
    WorkerRegistry* registry,
    RemoteWorker* worker,
    void* user_data
);

typedef void (*OnWorkerUnregisteredCallback)(
    WorkerRegistry* registry,
    const char* worker_id,
    const char* reason,
    void* user_data
);

typedef void (*OnWorkerStateChangedCallback)(
    WorkerRegistry* registry,
    RemoteWorker* worker,
    WorkerState old_state,
    WorkerState new_state,
    void* user_data
);

typedef void (*OnWorkerHealthChangedCallback)(
    WorkerRegistry* registry,
    RemoteWorker* worker,
    double old_score,
    double new_score,
    void* user_data
);

typedef struct {
    OnWorkerRegisteredCallback on_registered;
    OnWorkerUnregisteredCallback on_unregistered;
    OnWorkerStateChangedCallback on_state_changed;
    OnWorkerHealthChangedCallback on_health_changed;
    void* user_data;
} WorkerRegistryCallbacks;

/* ============================================================
 * Registry Configuration
 * ============================================================ */

typedef struct {
    int heartbeat_interval_sec;      /* Expected heartbeat interval (default: 30) */
    int heartbeat_timeout_sec;       /* Heartbeat timeout (default: 90) */
    int max_missed_heartbeats;       /* Max missed before marking offline (default: 3) */
    int max_workers;                 /* Maximum registered workers (default: 256) */
    bool auto_remove_offline;        /* Auto-remove offline workers (default: false) */
    int offline_removal_delay_sec;   /* Delay before removal (default: 300) */
} WorkerRegistryConfig;

/* ============================================================
 * Registry API
 * ============================================================ */

/**
 * Create a worker registry
 */
WorkerRegistry* worker_registry_create(const WorkerRegistryConfig* config);

/**
 * Free registry and all workers
 */
void worker_registry_free(WorkerRegistry* registry);

/**
 * Set registry callbacks
 */
void worker_registry_set_callbacks(WorkerRegistry* registry,
                                    const WorkerRegistryCallbacks* callbacks);

/**
 * Register a new worker
 * @param registry The registry
 * @param worker_info Worker information from HELLO message
 * @param connection Network connection for this worker
 * @return Registered worker or NULL on failure
 */
RemoteWorker* worker_registry_register(WorkerRegistry* registry,
                                        const WorkerSystemInfo* worker_info,
                                        NetworkConnection* connection);

/**
 * Unregister a worker
 * @param registry The registry
 * @param worker_id Worker ID to unregister
 * @param reason Reason for unregistration
 */
void worker_registry_unregister(WorkerRegistry* registry,
                                 const char* worker_id,
                                 const char* reason);

/**
 * Find worker by ID
 */
RemoteWorker* worker_registry_find_by_id(WorkerRegistry* registry,
                                          const char* worker_id);

/**
 * Find worker by connection
 */
RemoteWorker* worker_registry_find_by_connection(WorkerRegistry* registry,
                                                   NetworkConnection* connection);

/**
 * Find worker by name
 */
RemoteWorker* worker_registry_find_by_name(WorkerRegistry* registry,
                                            const char* name);

/**
 * Get number of registered workers
 */
int worker_registry_get_count(WorkerRegistry* registry);

/**
 * Get number of online workers
 */
int worker_registry_get_online_count(WorkerRegistry* registry);

/**
 * Get number of available job slots across all workers
 */
int worker_registry_get_available_slots(WorkerRegistry* registry);

/**
 * Iterate over all workers
 * @param registry The registry
 * @param callback Called for each worker
 * @param user_data Passed to callback
 */
void worker_registry_foreach(WorkerRegistry* registry,
                              void (*callback)(RemoteWorker* worker, void* user_data),
                              void* user_data);

/* ============================================================
 * Worker Selection
 * ============================================================ */

/**
 * Select best worker for a job based on criteria
 * @param registry The registry
 * @param criteria Selection criteria
 * @return Best matching worker or NULL if none available
 */
RemoteWorker* worker_registry_select_worker(WorkerRegistry* registry,
                                              const WorkerSelectionCriteria* criteria);

/**
 * Select multiple workers for parallel job distribution
 * @param registry The registry
 * @param criteria Selection criteria
 * @param max_workers Maximum workers to select
 * @param out_workers Array to store selected workers
 * @return Number of workers selected
 */
int worker_registry_select_workers(WorkerRegistry* registry,
                                    const WorkerSelectionCriteria* criteria,
                                    int max_workers,
                                    RemoteWorker** out_workers);

/* ============================================================
 * Worker State Management
 * ============================================================ */

/**
 * Update worker state
 */
void worker_registry_set_state(WorkerRegistry* registry,
                                RemoteWorker* worker,
                                WorkerState new_state);

/**
 * Record heartbeat from worker
 */
void worker_registry_heartbeat(WorkerRegistry* registry,
                                RemoteWorker* worker,
                                const WorkerSystemInfo* updated_info);

/**
 * Update worker job count
 */
void worker_registry_update_job_count(WorkerRegistry* registry,
                                       RemoteWorker* worker,
                                       int delta);

/**
 * Record job completion (updates stats)
 */
void worker_registry_record_job_complete(WorkerRegistry* registry,
                                          RemoteWorker* worker,
                                          bool success,
                                          double duration_sec);

/**
 * Check for stale workers (missed heartbeats)
 * Call periodically (e.g., every 10 seconds)
 */
void worker_registry_check_heartbeats(WorkerRegistry* registry);

/* ============================================================
 * Worker Health
 * ============================================================ */

/**
 * Calculate worker health score
 * Considers: uptime, success rate, latency, load, missed heartbeats
 */
double worker_registry_calculate_health(RemoteWorker* worker);

/**
 * Update worker health score
 */
void worker_registry_update_health(WorkerRegistry* registry,
                                    RemoteWorker* worker);

/* ============================================================
 * Tool Management
 * ============================================================ */

/**
 * Add tool to worker
 */
void worker_add_tool(RemoteWorker* worker,
                     const char* name,
                     const char* path,
                     const char* version);

/**
 * Find tool on worker
 */
WorkerTool* worker_find_tool(RemoteWorker* worker, const char* name);

/**
 * Check if worker has required tool with minimum version
 */
bool worker_has_tool(RemoteWorker* worker,
                     const char* name,
                     const char* min_version);

/**
 * Free worker tool list
 */
void worker_tools_free(WorkerTool* tools);

/* ============================================================
 * Utility Functions
 * ============================================================ */

/**
 * Get worker state name
 */
const char* worker_state_name(WorkerState state);

/**
 * Get capability name
 */
const char* worker_capability_name(WorkerCapability cap);

/**
 * Parse capabilities from string list
 */
uint32_t worker_capabilities_parse(const char** capability_names, int count);

/**
 * Create default registry configuration
 */
WorkerRegistryConfig worker_registry_config_default(void);

/**
 * Create a new remote worker structure (internal use)
 */
RemoteWorker* remote_worker_create(const char* id, const char* name);

/**
 * Free a remote worker structure
 */
void remote_worker_free(RemoteWorker* worker);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_DISTRIBUTED_WORKER_REGISTRY_H */
