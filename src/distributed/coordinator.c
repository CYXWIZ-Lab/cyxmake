/**
 * @file coordinator.c
 * @brief Distributed build coordinator implementation
 *
 * Central coordinator service that manages workers, schedules jobs,
 * and orchestrates distributed builds.
 */

#include "cyxmake/distributed/distributed.h"
#include "cyxmake/logger.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
#include "cyxmake/threading.h"
#endif

/* ============================================================
 * Constants
 * ============================================================ */

#define DEFAULT_PORT 9876
#define DEFAULT_BIND_ADDRESS "0.0.0.0"
#define DEFAULT_MAX_WORKERS 256
#define DEFAULT_MAX_BUILDS 10
#define DEFAULT_MAX_PENDING 10000
#define DEFAULT_HEARTBEAT_SEC 30
#define DEFAULT_JOB_TIMEOUT_SEC 600
#define DEFAULT_CONN_TIMEOUT_SEC 10

/* ============================================================
 * Coordinator Structure
 * ============================================================ */

struct Coordinator {
    DistributedCoordinatorConfig config;

    /* Components */
    NetworkServer* server;
    WorkerRegistry* registry;
    WorkScheduler* scheduler;
    AuthContext* auth;
    ArtifactCache* cache;

    /* State */
    volatile bool running;
    time_t started_at;

    /* Callbacks */
    CoordinatorCallbacks callbacks;

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    MutexHandle mutex;
    ThreadHandle heartbeat_thread;
#endif
};

/* ============================================================
 * Configuration
 * ============================================================ */

DistributedCoordinatorConfig distributed_coordinator_config_default(void) {
    DistributedCoordinatorConfig config = {
        .port = DEFAULT_PORT,
        .bind_address = NULL,
        .enable_tls = false,
        .cert_path = NULL,
        .key_path = NULL,
        .auth_method = AUTH_METHOD_TOKEN,
        .auth_token = NULL,
        .default_strategy = DIST_STRATEGY_COMPILE_UNITS,
        .lb_algorithm = LB_LEAST_LOADED,
        .max_workers = DEFAULT_MAX_WORKERS,
        .max_concurrent_builds = DEFAULT_MAX_BUILDS,
        .max_pending_jobs = DEFAULT_MAX_PENDING,
        .heartbeat_interval_sec = DEFAULT_HEARTBEAT_SEC,
        .job_timeout_sec = DEFAULT_JOB_TIMEOUT_SEC,
        .connection_timeout_sec = DEFAULT_CONN_TIMEOUT_SEC,
        .enable_cache = true,
        .cache_dir = NULL,
        .cache_max_size = 10ULL * 1024 * 1024 * 1024,  /* 10GB */
        .log_file = NULL,
        .log_level = 0
    };
    return config;
}

void distributed_coordinator_config_free(DistributedCoordinatorConfig* config) {
    if (!config) return;
    free(config->bind_address);
    free(config->cert_path);
    free(config->key_path);
    free(config->auth_token);
    free(config->cache_dir);
    free(config->log_file);
}

/* ============================================================
 * Network Callbacks
 * ============================================================ */

static void on_client_connect(NetworkConnection* conn, void* user_data) {
    Coordinator* coord = (Coordinator*)user_data;
    if (!coord) return;

    const char* addr = network_connection_get_remote_addr(conn);
    log_info("Worker connecting from %s", addr ? addr : "unknown");

    /* Worker will send HELLO message to register */
}

static void on_client_disconnect(NetworkConnection* conn,
                                  const char* reason,
                                  void* user_data) {
    Coordinator* coord = (Coordinator*)user_data;
    if (!coord) return;

    /* Find and unregister worker */
    RemoteWorker* worker = worker_registry_find_by_connection(
        coord->registry, conn);

    if (worker) {
        const char* worker_id = worker->id;

        /* Reschedule worker's jobs */
        scheduler_handle_worker_disconnect(coord->scheduler, worker_id);

        /* Callback */
        if (coord->callbacks.on_worker_disconnected) {
            coord->callbacks.on_worker_disconnected(coord, worker_id,
                                                     coord->callbacks.user_data);
        }

        /* Unregister */
        worker_registry_unregister(coord->registry, worker_id, reason);
    }

    log_info("Worker disconnected: %s", reason ? reason : "unknown");
}

static void on_client_message(NetworkConnection* conn,
                               ProtocolMessage* msg,
                               void* user_data) {
    Coordinator* coord = (Coordinator*)user_data;
    if (!coord || !msg) return;

    switch (msg->type) {
        case PROTO_MSG_HELLO: {
            /* Worker registration */
            log_debug("Received HELLO from worker");

            /* Validate auth if enabled */
            if (coord->config.auth_method != AUTH_METHOD_NONE &&
                coord->config.auth_token) {
                /* Extract token from payload and validate */
                /* TODO: Parse payload JSON for token */
            }

            /* Parse worker info from payload */
            WorkerSystemInfo info = {0};
            /* TODO: Parse JSON payload for system info */

            RemoteWorker* worker = worker_registry_register(
                coord->registry, &info, conn);

            if (worker) {
                /* Send WELCOME response */
                ProtocolMessage* welcome = protocol_message_create(PROTO_MSG_WELCOME);
                if (welcome) {
                    network_server_send(coord->server, conn, welcome);
                    protocol_message_free(welcome);
                }

                /* Callback */
                if (coord->callbacks.on_worker_connected) {
                    coord->callbacks.on_worker_connected(coord, worker,
                                                          coord->callbacks.user_data);
                }

                log_info("Worker registered: %s", worker->id);
            } else {
                /* Send error */
                ProtocolMessage* error = protocol_message_create(PROTO_MSG_ERROR);
                if (error) {
                    error->payload_json = strdup("Registration failed");
                    network_server_send(coord->server, conn, error);
                    protocol_message_free(error);
                }
            }
            break;
        }

        case PROTO_MSG_HEARTBEAT: {
            /* Worker heartbeat */
            RemoteWorker* worker = worker_registry_find_by_connection(
                coord->registry, conn);
            if (worker) {
                worker_registry_heartbeat(coord->registry, worker, NULL);
            }
            break;
        }

        case PROTO_MSG_STATUS_UPDATE: {
            /* Worker status update */
            RemoteWorker* worker = worker_registry_find_by_connection(
                coord->registry, conn);
            if (worker) {
                /* TODO: Parse status from payload */
                worker_registry_update_health(coord->registry, worker);
            }
            break;
        }

        case PROTO_MSG_JOB_PROGRESS: {
            /* Job progress update */
            /* TODO: Update job progress */
            break;
        }

        case PROTO_MSG_JOB_COMPLETE: {
            /* Job completed */
            DistributedJobResult* result = NULL;
            /* TODO: Parse result from payload */

            if (msg->correlation_id) {
                scheduler_report_job_result(coord->scheduler,
                                            msg->correlation_id, result);
            }
            break;
        }

        case PROTO_MSG_JOB_FAILED: {
            /* Job failed */
            if (msg->correlation_id) {
                scheduler_report_job_failure(coord->scheduler,
                                              msg->correlation_id,
                                              msg->payload_json);
            }
            break;
        }

        case PROTO_MSG_ARTIFACT_PUSH: {
            /* Worker pushing artifact to cache */
            /* TODO: Handle artifact storage */
            break;
        }

        case PROTO_MSG_ARTIFACT_REQUEST: {
            /* Worker requesting artifact from cache */
            /* TODO: Handle artifact retrieval */
            break;
        }

        default:
            log_warning("Unknown message type: %d", msg->type);
            break;
    }
}

static void on_client_error(NetworkConnection* conn,
                             const char* error,
                             void* user_data) {
    Coordinator* coord = (Coordinator*)user_data;
    if (!coord) return;

    log_error("Connection error: %s", error ? error : "unknown");

    if (coord->callbacks.on_error) {
        coord->callbacks.on_error(coord, error, coord->callbacks.user_data);
    }
}

/* ============================================================
 * Scheduler Callbacks
 * ============================================================ */

static void on_job_assigned(WorkScheduler* scheduler,
                             ScheduledJob* job,
                             RemoteWorker* worker,
                             void* user_data) {
    Coordinator* coord = (Coordinator*)user_data;
    if (!coord || !job || !worker) return;

    (void)scheduler;

    /* Send job to worker */
    ProtocolMessage* msg = protocol_message_create(PROTO_MSG_JOB_REQUEST);
    if (msg) {
        msg->correlation_id = strdup(job->job_id);

        /* Serialize job spec */
        if (job->spec) {
            msg->payload_json = distributed_job_to_json(job->spec);
            if (msg->payload_json) {
                msg->payload_size = strlen(msg->payload_json);
            }
        }

        if (worker->connection) {
            network_server_send(coord->server, worker->connection, msg);
        }

        protocol_message_free(msg);
    }

    /* Callback */
    if (coord->callbacks.on_job_assigned) {
        coord->callbacks.on_job_assigned(coord, job, worker,
                                          coord->callbacks.user_data);
    }

    log_debug("Job %s assigned to worker %s", job->job_id, worker->id);
}

static void on_build_completed(WorkScheduler* scheduler,
                                BuildSession* session,
                                void* user_data) {
    Coordinator* coord = (Coordinator*)user_data;
    if (!coord || !session) return;

    (void)scheduler;

    log_info("Build completed: %s (success: %s)",
             session->build_id, session->success ? "yes" : "no");

    if (coord->callbacks.on_build_completed) {
        coord->callbacks.on_build_completed(coord, session,
                                             coord->callbacks.user_data);
    }
}

/* ============================================================
 * Heartbeat Thread
 * ============================================================ */

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
static THREAD_RETURN_TYPE THREAD_CALL_CONVENTION heartbeat_thread(void* arg) {
    Coordinator* coord = (Coordinator*)arg;

    while (coord->running) {
        /* Check worker heartbeats */
        worker_registry_check_heartbeats(coord->registry);

        /* Check job timeouts */
        scheduler_check_timeouts(coord->scheduler);

        /* Process job queue */
        scheduler_process_queue(coord->scheduler);

        /* Sleep for heartbeat interval */
#ifdef _WIN32
        Sleep(coord->config.heartbeat_interval_sec * 1000);
#else
        sleep(coord->config.heartbeat_interval_sec);
#endif
    }

    return (THREAD_RETURN_TYPE)0;
}
#endif

/* ============================================================
 * Coordinator API Implementation
 * ============================================================ */

Coordinator* distributed_coordinator_create(const DistributedCoordinatorConfig* config) {
    Coordinator* coord = calloc(1, sizeof(Coordinator));
    if (!coord) {
        log_error("Failed to allocate coordinator");
        return NULL;
    }

    /* Copy config */
    if (config) {
        coord->config = *config;
        if (config->bind_address) {
            coord->config.bind_address = strdup(config->bind_address);
        }
        if (config->cert_path) {
            coord->config.cert_path = strdup(config->cert_path);
        }
        if (config->key_path) {
            coord->config.key_path = strdup(config->key_path);
        }
        if (config->auth_token) {
            coord->config.auth_token = strdup(config->auth_token);
        }
        if (config->cache_dir) {
            coord->config.cache_dir = strdup(config->cache_dir);
        }
        if (config->log_file) {
            coord->config.log_file = strdup(config->log_file);
        }
    } else {
        coord->config = distributed_coordinator_config_default();
    }

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    coord->mutex = mutex_create();
    if (!coord->mutex) {
        log_error("Failed to create coordinator mutex");
        distributed_coordinator_free(coord);
        return NULL;
    }
#endif

    /* Create network server */
    NetworkConfig net_config = {0};
    net_config.bind_address = coord->config.bind_address ?
                              coord->config.bind_address : strdup(DEFAULT_BIND_ADDRESS);
    net_config.port = coord->config.port;
    net_config.use_tls = coord->config.enable_tls;
    net_config.cert_path = coord->config.cert_path;
    net_config.key_path = coord->config.key_path;
    net_config.max_connections = coord->config.max_workers;
    net_config.connection_timeout_sec = coord->config.connection_timeout_sec;

    coord->server = network_server_create(&net_config);
    if (!coord->server) {
        log_error("Failed to create network server");
        distributed_coordinator_free(coord);
        return NULL;
    }

    /* Set server callbacks */
    NetworkServerCallbacks server_cbs = {
        .on_connect = on_client_connect,
        .on_disconnect = on_client_disconnect,
        .on_message = on_client_message,
        .on_error = on_client_error,
        .user_data = coord
    };
    network_server_set_callbacks(coord->server, &server_cbs);

    /* Create worker registry */
    WorkerRegistryConfig reg_config = worker_registry_config_default();
    reg_config.max_workers = coord->config.max_workers;
    reg_config.heartbeat_interval_sec = coord->config.heartbeat_interval_sec;

    coord->registry = worker_registry_create(&reg_config);
    if (!coord->registry) {
        log_error("Failed to create worker registry");
        distributed_coordinator_free(coord);
        return NULL;
    }

    /* Create work scheduler */
    SchedulerConfig sched_config = scheduler_config_default();
    sched_config.default_strategy = coord->config.default_strategy;
    sched_config.lb_algorithm = coord->config.lb_algorithm;
    sched_config.max_concurrent_builds = coord->config.max_concurrent_builds;
    sched_config.max_pending_jobs = coord->config.max_pending_jobs;
    sched_config.default_job_timeout_sec = coord->config.job_timeout_sec;

    coord->scheduler = scheduler_create(&sched_config, coord->registry);
    if (!coord->scheduler) {
        log_error("Failed to create work scheduler");
        distributed_coordinator_free(coord);
        return NULL;
    }

    /* Set scheduler callbacks */
    SchedulerCallbacks sched_cbs = {
        .on_job_assigned = on_job_assigned,
        .on_build_completed = on_build_completed,
        .user_data = coord
    };
    scheduler_set_callbacks(coord->scheduler, &sched_cbs);

    /* Create auth context */
    AuthConfig auth_config = auth_config_default();
    auth_config.method = coord->config.auth_method;

    coord->auth = auth_context_create(&auth_config);
    if (!coord->auth) {
        log_warning("Failed to create auth context, continuing without auth");
    }

    /* Generate default worker token if none provided */
    if (coord->auth && !coord->config.auth_token) {
        AuthToken* token = auth_token_generate(coord->auth,
                                                AUTH_TOKEN_TYPE_WORKER,
                                                "default-worker",
                                                -1);  /* Never expires */
        if (token) {
            coord->config.auth_token = strdup(token->token_value);
            log_info("Generated worker token: %s", token->token_value);
        }
    }

    /* Create artifact cache */
    if (coord->config.enable_cache) {
        ArtifactCacheConfig cache_config = artifact_cache_config_default();
        cache_config.cache_dir = coord->config.cache_dir;
        cache_config.max_size_bytes = coord->config.cache_max_size;

        coord->cache = artifact_cache_create(&cache_config);
        if (coord->cache) {
            artifact_cache_init(coord->cache);
        } else {
            log_warning("Failed to create artifact cache");
        }
    }

    log_info("Coordinator created (port: %d, max workers: %d)",
             coord->config.port, coord->config.max_workers);

    return coord;
}

void distributed_coordinator_free(Coordinator* coord) {
    if (!coord) return;

    coordinator_stop(coord);

    if (coord->cache) {
        artifact_cache_free(coord->cache);
    }
    if (coord->auth) {
        auth_context_free(coord->auth);
    }
    if (coord->scheduler) {
        scheduler_free(coord->scheduler);
    }
    if (coord->registry) {
        worker_registry_free(coord->registry);
    }
    if (coord->server) {
        network_server_free(coord->server);
    }

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    if (coord->mutex) {
        mutex_destroy(coord->mutex);
    }
#endif

    distributed_coordinator_config_free(&coord->config);
    free(coord);

    log_debug("Coordinator freed");
}

void coordinator_set_callbacks(Coordinator* coord,
                                const CoordinatorCallbacks* callbacks) {
    if (!coord || !callbacks) return;
    coord->callbacks = *callbacks;
}

bool coordinator_start(Coordinator* coord) {
    if (!coord) return false;

    if (coord->running) {
        log_warning("Coordinator already running");
        return true;
    }

    /* Start network server */
    if (!network_server_start(coord->server)) {
        log_error("Failed to start network server");
        return false;
    }

    /* Start scheduler */
    if (!scheduler_start(coord->scheduler)) {
        log_error("Failed to start scheduler");
        network_server_stop(coord->server);
        return false;
    }

    coord->running = true;
    coord->started_at = time(NULL);

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    /* Start heartbeat/maintenance thread */
    coord->heartbeat_thread = thread_create(heartbeat_thread, coord);
    if (!coord->heartbeat_thread) {
        log_warning("Failed to start heartbeat thread");
    }
#endif

    log_info("Coordinator started on port %d", coord->config.port);
    return true;
}

void coordinator_stop(Coordinator* coord) {
    if (!coord || !coord->running) return;

    coord->running = false;

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    if (coord->heartbeat_thread) {
        thread_join(coord->heartbeat_thread);
        coord->heartbeat_thread = NULL;
    }
#endif

    if (coord->scheduler) {
        scheduler_stop(coord->scheduler);
    }

    if (coord->server) {
        network_server_stop(coord->server);
    }

    log_info("Coordinator stopped");
}

bool coordinator_is_running(Coordinator* coord) {
    return coord && coord->running;
}

CoordinatorStatus coordinator_get_status(Coordinator* coord) {
    CoordinatorStatus status = {0};

    if (!coord) return status;

    status.running = coord->running;
    status.started_at = coord->started_at;

    if (coord->running) {
        status.uptime_sec = time(NULL) - coord->started_at;
    }

    if (coord->server) {
        status.connected_workers = network_server_get_connection_count(coord->server);
    }

    if (coord->registry) {
        status.online_workers = worker_registry_get_online_count(coord->registry);
    }

    if (coord->scheduler) {
        SchedulerStats stats = scheduler_get_stats(coord->scheduler);
        status.pending_jobs = scheduler_get_pending_count(coord->scheduler);
        status.running_jobs = scheduler_get_running_count(coord->scheduler);
        status.active_builds = stats.total_builds - stats.successful_builds - stats.failed_builds;
    }

    if (coord->cache) {
        status.cache_size = artifact_cache_get_size(coord->cache);
        status.cache_hit_rate = artifact_cache_get_hit_rate(coord->cache);
    }

    return status;
}

WorkerRegistry* coordinator_get_registry(Coordinator* coord) {
    return coord ? coord->registry : NULL;
}

WorkScheduler* coordinator_get_scheduler(Coordinator* coord) {
    return coord ? coord->scheduler : NULL;
}

ArtifactCache* coordinator_get_cache(Coordinator* coord) {
    return coord ? coord->cache : NULL;
}

char* coordinator_generate_worker_token(Coordinator* coord,
                                         const char* worker_name,
                                         int ttl_sec) {
    if (!coord) return NULL;

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    if (!coord->auth) {
        log_error("Coordinator has no auth context");
        return NULL;
    }

    AuthToken* token = auth_token_generate(coord->auth,
                                            AUTH_TOKEN_WORKER,
                                            worker_name,
                                            ttl_sec);
    if (!token) {
        log_error("Failed to generate worker token");
        return NULL;
    }

    char* token_str = token->token ? CYXMAKE_STRDUP(token->token) : NULL;
    auth_token_free(token);
    return token_str;
#else
    (void)worker_name;
    (void)ttl_sec;
    /* Generate a simple random token for stub mode */
    return auth_generate_random_token(32);
#endif
}

/* ============================================================
 * Build Submission
 * ============================================================ */

BuildSession* coordinator_submit_build(Coordinator* coord,
                                        const char* project_path,
                                        const DistributedBuildOptions* options) {
    if (!coord || !project_path) return NULL;

    DistributionStrategy strategy = options ?
        options->strategy : coord->config.default_strategy;

    BuildSession* session = scheduler_create_build(coord->scheduler,
                                                    project_path, strategy);
    if (!session) return NULL;

    /* TODO: Analyze project and decompose into jobs */
    /* For now, create a single whole-project job */

    log_info("Build submitted: %s (project: %s)", session->build_id, project_path);

    if (coord->callbacks.on_build_started) {
        coord->callbacks.on_build_started(coord, session,
                                           coord->callbacks.user_data);
    }

    return session;
}

bool coordinator_wait_build(Coordinator* coord,
                             const char* build_id,
                             int timeout_sec) {
    if (!coord || !build_id) return false;

    time_t start = time(NULL);

    while (1) {
        BuildSession* build = scheduler_get_build(coord->scheduler, build_id);
        if (!build) return false;

        if (build->state == BUILD_STATE_COMPLETED ||
            build->state == BUILD_STATE_FAILED ||
            build->state == BUILD_STATE_CANCELLED) {
            return true;
        }

        if (timeout_sec >= 0) {
            if ((time(NULL) - start) >= timeout_sec) {
                return false;  /* Timeout */
            }
        }

        /* Sleep a bit */
#ifdef _WIN32
        Sleep(100);
#else
        usleep(100000);
#endif
    }
}

bool coordinator_cancel_build(Coordinator* coord, const char* build_id) {
    if (!coord || !build_id) return false;
    return scheduler_cancel_build(coord->scheduler, build_id, "User cancelled");
}

DistributedBuildResult* coordinator_get_build_result(Coordinator* coord,
                                                       const char* build_id) {
    if (!coord || !build_id) return NULL;

    BuildSession* build = scheduler_get_build(coord->scheduler, build_id);
    if (!build) return NULL;

    DistributedBuildResult* result = calloc(1, sizeof(DistributedBuildResult));
    if (!result) return NULL;

    result->success = build->success;
    result->duration_sec = difftime(build->completed_at, build->started_at);
    result->jobs_completed = build->completed_jobs;
    result->jobs_failed = build->failed_jobs;

    if (build->error_summary) {
        result->error_message = strdup(build->error_summary);
    }

    /* Copy artifacts */
    if (build->output_artifacts && build->artifact_count > 0) {
        result->artifacts = malloc(sizeof(char*) * build->artifact_count);
        if (result->artifacts) {
            result->artifact_count = build->artifact_count;
            for (int i = 0; i < build->artifact_count; i++) {
                result->artifacts[i] = strdup(build->output_artifacts[i]);
            }
        }
    }

    return result;
}

void distributed_build_result_free(DistributedBuildResult* result) {
    if (!result) return;

    free(result->error_message);

    if (result->artifacts) {
        for (int i = 0; i < result->artifact_count; i++) {
            free(result->artifacts[i]);
        }
        free(result->artifacts);
    }

    free(result);
}

/* ============================================================
 * Utility Functions
 * ============================================================ */

bool distributed_is_available(void) {
#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    return network_is_available();
#else
    return false;
#endif
}

const char* distributed_get_version(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d",
             CYXMAKE_DISTRIBUTED_VERSION_MAJOR,
             CYXMAKE_DISTRIBUTED_VERSION_MINOR,
             CYXMAKE_DISTRIBUTED_VERSION_PATCH);
    return version;
}

DistributedBuildOptions distributed_build_options_default(void) {
    DistributedBuildOptions options = {
        .strategy = DIST_STRATEGY_COMPILE_UNITS,
        .max_parallel_jobs = 0,
        .job_timeout_sec = 600,
        .use_cache = true,
        .verbose = false,
        .target_arch = NULL,
        .target_os = NULL,
        .cross_compile = false,
        .cross_target = NULL
    };
    return options;
}

void distributed_build_options_free(DistributedBuildOptions* options) {
    if (!options) return;
    free(options->target_arch);
    free(options->target_os);
    free(options->cross_target);
}

/* ============================================================
 * Worker Client Stubs (to be implemented in worker_daemon.c)
 * ============================================================ */

WorkerClientConfig worker_client_config_default(void) {
    WorkerClientConfig config = {
        .name = NULL,
        .coordinator_url = NULL,
        .auth_token = NULL,
        .max_jobs = 4,
        .auto_detect_tools = true,
        .auto_reconnect = true,
        .reconnect_delay_sec = 5,
        .max_reconnect_attempts = 10,
        .enable_sandbox = false,
        .sandbox_dir = NULL
    };
    return config;
}

void worker_client_config_free(WorkerClientConfig* config) {
    if (!config) return;
    free(config->name);
    free(config->coordinator_url);
    free(config->auth_token);
    free(config->sandbox_dir);
}

/* Worker client implementation will be in worker_daemon.c */
