/**
 * @file distributed.h
 * @brief Main API for CyxMake Distributed Builds
 *
 * High-level API for distributed build coordination. Provides a unified
 * interface for starting coordinators, workers, and distributed builds.
 */

#ifndef CYXMAKE_DISTRIBUTED_H
#define CYXMAKE_DISTRIBUTED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Include all distributed subsystem headers */
#include "cyxmake/distributed/protocol.h"
#include "cyxmake/distributed/network_transport.h"
#include "cyxmake/distributed/worker_registry.h"
#include "cyxmake/distributed/auth.h"
#include "cyxmake/distributed/work_scheduler.h"
#include "cyxmake/distributed/artifact_cache.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Version Information
 * ============================================================ */

#define CYXMAKE_DISTRIBUTED_VERSION_MAJOR 0
#define CYXMAKE_DISTRIBUTED_VERSION_MINOR 1
#define CYXMAKE_DISTRIBUTED_VERSION_PATCH 0

/* ============================================================
 * Coordinator Service
 * ============================================================ */

typedef struct Coordinator Coordinator;

/**
 * Distributed coordinator configuration
 * Note: Named DistributedCoordinatorConfig to avoid conflict with agent_coordinator.h
 */
typedef struct {
    /* Network */
    uint16_t port;                /* Listen port (default: 9876) */
    char* bind_address;           /* Bind address (default: "0.0.0.0") */
    bool enable_tls;              /* Enable TLS */
    char* cert_path;              /* TLS certificate */
    char* key_path;               /* TLS private key */

    /* Authentication */
    AuthMethod auth_method;       /* Authentication method */
    char* auth_token;             /* Pre-shared token for workers */

    /* Scheduling */
    DistributionStrategy default_strategy;
    LoadBalancingAlgorithm lb_algorithm;

    /* Limits */
    int max_workers;              /* Maximum workers */
    int max_concurrent_builds;    /* Maximum concurrent builds */
    int max_pending_jobs;         /* Maximum pending jobs */

    /* Timeouts */
    int heartbeat_interval_sec;   /* Heartbeat interval */
    int job_timeout_sec;          /* Default job timeout */
    int connection_timeout_sec;   /* Connection timeout */

    /* Cache */
    bool enable_cache;            /* Enable artifact cache */
    char* cache_dir;              /* Cache directory */
    size_t cache_max_size;        /* Maximum cache size */

    /* Logging */
    char* log_file;               /* Log file path */
    int log_level;                /* Log verbosity */
} DistributedCoordinatorConfig;

/**
 * Coordinator callbacks
 */
typedef struct {
    void (*on_worker_connected)(Coordinator*, RemoteWorker*, void*);
    void (*on_worker_disconnected)(Coordinator*, const char*, void*);
    void (*on_build_started)(Coordinator*, BuildSession*, void*);
    void (*on_build_completed)(Coordinator*, BuildSession*, void*);
    void (*on_job_assigned)(Coordinator*, ScheduledJob*, RemoteWorker*, void*);
    void (*on_error)(Coordinator*, const char*, void*);
    void* user_data;
} CoordinatorCallbacks;

/**
 * Create coordinator
 * Note: Named distributed_coordinator_* to avoid conflict with agent_coordinator
 */
Coordinator* distributed_coordinator_create(const DistributedCoordinatorConfig* config);

/**
 * Free coordinator
 */
void distributed_coordinator_free(Coordinator* coord);

/**
 * Set coordinator callbacks
 */
void coordinator_set_callbacks(Coordinator* coord,
                                const CoordinatorCallbacks* callbacks);

/**
 * Start coordinator (begins accepting connections)
 */
bool coordinator_start(Coordinator* coord);

/**
 * Stop coordinator (graceful shutdown)
 */
void coordinator_stop(Coordinator* coord);

/**
 * Check if coordinator is running
 */
bool coordinator_is_running(Coordinator* coord);

/**
 * Get coordinator status information
 */
typedef struct {
    bool running;
    int connected_workers;
    int online_workers;
    int active_builds;
    int pending_jobs;
    int running_jobs;
    size_t cache_size;
    double cache_hit_rate;
    time_t started_at;
    time_t uptime_sec;
} CoordinatorStatus;

CoordinatorStatus coordinator_get_status(Coordinator* coord);

/**
 * Get worker registry
 */
WorkerRegistry* coordinator_get_registry(Coordinator* coord);

/**
 * Get work scheduler
 */
WorkScheduler* coordinator_get_scheduler(Coordinator* coord);

/**
 * Get artifact cache
 */
ArtifactCache* coordinator_get_cache(Coordinator* coord);

/**
 * Generate a worker authentication token
 * @param coord Coordinator
 * @param worker_name Name for the worker
 * @param ttl_sec Time to live in seconds (0 = default, -1 = never expires)
 * @return Token string (caller must free) or NULL on error
 */
char* coordinator_generate_worker_token(Coordinator* coord,
                                         const char* worker_name,
                                         int ttl_sec);

/* ============================================================
 * Distributed Build API
 * ============================================================ */

/**
 * Distributed build options
 */
typedef struct {
    DistributionStrategy strategy; /* Distribution strategy */
    int max_parallel_jobs;         /* Max parallel jobs (0 = auto) */
    int job_timeout_sec;           /* Job timeout */
    bool use_cache;                /* Use artifact cache */
    bool verbose;                  /* Verbose output */

    /* Target filtering */
    char* target_arch;             /* Target architecture filter */
    char* target_os;               /* Target OS filter */

    /* Cross-compilation */
    bool cross_compile;            /* Enable cross-compilation */
    char* cross_target;            /* Cross-compile target triple */
} DistributedBuildOptions;

/**
 * Submit a distributed build
 * @param coord Coordinator
 * @param project_path Path to project
 * @param options Build options
 * @return Build session or NULL on error
 */
BuildSession* coordinator_submit_build(Coordinator* coord,
                                        const char* project_path,
                                        const DistributedBuildOptions* options);

/**
 * Wait for build to complete
 * @param coord Coordinator
 * @param build_id Build ID
 * @param timeout_sec Timeout (-1 = wait forever)
 * @return true if completed, false if timeout/error
 */
bool coordinator_wait_build(Coordinator* coord,
                             const char* build_id,
                             int timeout_sec);

/**
 * Cancel a build
 */
bool coordinator_cancel_build(Coordinator* coord, const char* build_id);

/**
 * Get build result
 */
typedef struct {
    bool success;
    int exit_code;
    double duration_sec;
    int jobs_completed;
    int jobs_failed;
    int cache_hits;             /* Number of cache hits during build */
    char* error_message;
    char** artifacts;
    int artifact_count;
} DistributedBuildResult;

DistributedBuildResult* coordinator_get_build_result(Coordinator* coord,
                                                       const char* build_id);

void distributed_build_result_free(DistributedBuildResult* result);

/* ============================================================
 * Worker Client API
 * ============================================================ */

typedef struct WorkerClient WorkerClient;

/**
 * Worker configuration
 */
typedef struct {
    char* name;                   /* Worker name */
    char* coordinator_url;        /* Coordinator URL (ws://host:port) */
    char* auth_token;             /* Authentication token */

    int max_jobs;                 /* Maximum concurrent jobs */
    bool auto_detect_tools;       /* Auto-detect available tools */

    /* Reconnection */
    bool auto_reconnect;          /* Auto-reconnect on disconnect */
    int reconnect_delay_sec;      /* Reconnect delay */
    int max_reconnect_attempts;   /* Max reconnect attempts */

    /* Sandbox */
    bool enable_sandbox;          /* Enable sandboxed execution */
    char* sandbox_dir;            /* Sandbox working directory */
} WorkerClientConfig;

/**
 * Worker callbacks
 */
typedef struct {
    void (*on_connected)(WorkerClient*, void*);
    void (*on_disconnected)(WorkerClient*, const char*, void*);
    void (*on_job_received)(WorkerClient*, DistributedJob*, void*);
    void (*on_error)(WorkerClient*, const char*, void*);
    void* user_data;
} WorkerClientCallbacks;

/**
 * Create worker client
 */
WorkerClient* worker_client_create(const WorkerClientConfig* config);

/**
 * Free worker client
 */
void worker_client_free(WorkerClient* client);

/**
 * Set worker callbacks
 */
void worker_client_set_callbacks(WorkerClient* client,
                                  const WorkerClientCallbacks* callbacks);

/**
 * Connect to coordinator
 */
bool worker_client_connect(WorkerClient* client);

/**
 * Disconnect from coordinator
 */
void worker_client_disconnect(WorkerClient* client);

/**
 * Check if connected
 */
bool worker_client_is_connected(WorkerClient* client);

/**
 * Run worker main loop (blocking)
 */
void worker_client_run(WorkerClient* client);

/**
 * Stop worker
 */
void worker_client_stop(WorkerClient* client);

/**
 * Report job result to coordinator
 */
bool worker_client_report_result(WorkerClient* client,
                                  const char* job_id,
                                  DistributedJobResult* result);

/* ============================================================
 * Utility Functions
 * ============================================================ */

/**
 * Check if distributed builds are available
 */
bool distributed_is_available(void);

/**
 * Get distributed module version string
 */
const char* distributed_get_version(void);

/**
 * Create default distributed coordinator config
 */
DistributedCoordinatorConfig distributed_coordinator_config_default(void);

/**
 * Free distributed coordinator config
 */
void distributed_coordinator_config_free(DistributedCoordinatorConfig* config);

/**
 * Create default worker config
 */
WorkerClientConfig worker_client_config_default(void);

/**
 * Free worker config
 */
void worker_client_config_free(WorkerClientConfig* config);

/**
 * Create default build options
 */
DistributedBuildOptions distributed_build_options_default(void);

/**
 * Free build options
 */
void distributed_build_options_free(DistributedBuildOptions* options);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_DISTRIBUTED_H */
