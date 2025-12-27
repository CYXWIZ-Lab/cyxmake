/**
 * @file work_scheduler.c
 * @brief Work distribution and scheduling implementation
 *
 * Implements job distribution, load balancing, and lifecycle management
 * for distributed builds across multiple workers.
 */

#include "cyxmake/distributed/work_scheduler.h"
#include "cyxmake/logger.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
#include "cyxmake/threading.h"
#endif

/* ============================================================
 * Constants
 * ============================================================ */

#define DEFAULT_JOB_TIMEOUT_SEC 600
#define DEFAULT_MAX_RETRIES 2
#define DEFAULT_RETRY_DELAY_SEC 5
#define DEFAULT_MAX_PENDING_JOBS 10000
#define DEFAULT_MAX_CONCURRENT_BUILDS 10
#define DEFAULT_MIN_JOB_SIZE_BYTES 1024

/* ============================================================
 * Internal Structures
 * ============================================================ */

struct WorkScheduler {
    SchedulerConfig config;
    WorkerRegistry* worker_registry;

    /* Job queues */
    ScheduledJob* pending_head;   /* Priority queue head */
    ScheduledJob* pending_tail;
    int pending_count;

    ScheduledJob* running_head;   /* Running jobs list */
    int running_count;

    /* Build sessions */
    BuildSession* builds;
    int build_count;

    /* State */
    volatile bool running;
    int round_robin_index;        /* For round-robin LB */

    /* Statistics */
    SchedulerStats stats;

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    MutexHandle mutex;
    ThreadHandle scheduler_thread;
#endif

    SchedulerCallbacks callbacks;
};

/* ============================================================
 * Helper Functions
 * ============================================================ */

static void scheduler_lock(WorkScheduler* scheduler) {
#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    mutex_lock(&scheduler->mutex);
#else
    (void)scheduler;
#endif
}

static void scheduler_unlock(WorkScheduler* scheduler) {
#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    mutex_unlock(&scheduler->mutex);
#else
    (void)scheduler;
#endif
}

static char* generate_job_id(void) {
    static unsigned int counter = 0;
    char* id = malloc(48);
    if (!id) return NULL;

    snprintf(id, 48, "job-%08x-%04x",
             (unsigned int)time(NULL),
             (unsigned int)(counter++ & 0xFFFF));

    return id;
}

static char* generate_build_id(void) {
    static unsigned int counter = 0;
    char* id = malloc(48);
    if (!id) return NULL;

    snprintf(id, 48, "build-%08x-%04x",
             (unsigned int)time(NULL),
             (unsigned int)(counter++ & 0xFFFF));

    return id;
}

/* ============================================================
 * Configuration
 * ============================================================ */

SchedulerConfig scheduler_config_default(void) {
    SchedulerConfig config = {
        .default_strategy = DIST_STRATEGY_COMPILE_UNITS,
        .lb_algorithm = LB_LEAST_LOADED,
        .default_job_timeout_sec = DEFAULT_JOB_TIMEOUT_SEC,
        .max_retries = DEFAULT_MAX_RETRIES,
        .retry_delay_sec = DEFAULT_RETRY_DELAY_SEC,
        .max_pending_jobs = DEFAULT_MAX_PENDING_JOBS,
        .max_concurrent_builds = DEFAULT_MAX_CONCURRENT_BUILDS,
        .enable_job_coalescing = false,
        .enable_speculative = false,
        .min_job_size_bytes = DEFAULT_MIN_JOB_SIZE_BYTES
    };
    return config;
}

/* ============================================================
 * Job Management
 * ============================================================ */

void scheduled_job_free(ScheduledJob* job) {
    if (!job) return;

    free(job->job_id);
    free(job->build_id);
    free(job->assigned_worker_id);
    free(job->last_error);

    if (job->depends_on) {
        for (int i = 0; i < job->depends_count; i++) {
            free(job->depends_on[i]);
        }
        free(job->depends_on);
    }

    /* Note: spec and result are owned by caller */

    free(job);
}

void build_session_free(BuildSession* session) {
    if (!session) return;

    free(session->build_id);
    free(session->project_name);
    free(session->current_phase);
    free(session->error_summary);

    if (session->output_artifacts) {
        for (int i = 0; i < session->artifact_count; i++) {
            free(session->output_artifacts[i]);
        }
        free(session->output_artifacts);
    }

    free(session);
}

/* ============================================================
 * Worker Selection
 * ============================================================ */

/* Context for round-robin worker selection callback */
typedef struct {
    int target;
    int current;
    RemoteWorker* selected;
} RoundRobinContext;

static void round_robin_worker_checker(RemoteWorker* w, void* data) {
    RoundRobinContext* ctx = (RoundRobinContext*)data;
    if ((w->state == WORKER_STATE_ONLINE || w->state == WORKER_STATE_BUSY) &&
        w->active_jobs < w->max_jobs) {
        if (ctx->current == ctx->target) {
            ctx->selected = w;
        }
        ctx->current++;
    }
}

static RemoteWorker* select_worker_round_robin(WorkScheduler* scheduler) {
    int online = worker_registry_get_online_count(scheduler->worker_registry);
    if (online == 0) return NULL;

    /* Find the nth online worker */
    RoundRobinContext ctx = {0};
    ctx.target = scheduler->round_robin_index % online;
    ctx.current = 0;
    ctx.selected = NULL;

    scheduler->round_robin_index++;

    worker_registry_foreach(scheduler->worker_registry,
                            round_robin_worker_checker,
                            &ctx);

    return ctx.selected;
}

static RemoteWorker* select_worker_least_loaded(WorkScheduler* scheduler,
                                                  ScheduledJob* job) {
    WorkerSelectionCriteria criteria = {0};

    /* Determine required capabilities from job type */
    if (job->spec) {
        switch (job->spec->type) {
            case JOB_TYPE_COMPILE:
                criteria.required_capabilities = WORKER_CAP_COMPILE_C | WORKER_CAP_COMPILE_CPP;
                break;
            case JOB_TYPE_LINK:
                criteria.required_capabilities = WORKER_CAP_COMPILE_C;
                break;
            case JOB_TYPE_CMAKE_CONFIG:
                criteria.required_capabilities = WORKER_CAP_CMAKE;
                break;
            default:
                break;
        }
    }

    criteria.prefer_idle = true;
    criteria.min_available_slots = 1;

    return worker_registry_select_worker(scheduler->worker_registry, &criteria);
}

static RemoteWorker* select_worker(WorkScheduler* scheduler, ScheduledJob* job) {
    switch (scheduler->config.lb_algorithm) {
        case LB_ROUND_ROBIN:
            return select_worker_round_robin(scheduler);

        case LB_LEAST_LOADED:
        case LB_WEIGHTED:
            return select_worker_least_loaded(scheduler, job);

        case LB_LEAST_LATENCY: {
            /* Select worker with lowest latency */
            WorkerSelectionCriteria criteria = {0};
            criteria.min_available_slots = 1;
            /* TODO: Implement latency-based selection */
            return worker_registry_select_worker(scheduler->worker_registry, &criteria);
        }

        case LB_RANDOM: {
            /* Random selection */
            WorkerSelectionCriteria criteria = {0};
            criteria.min_available_slots = 1;
            return worker_registry_select_worker(scheduler->worker_registry, &criteria);
        }

        default:
            return select_worker_least_loaded(scheduler, job);
    }
}

/* ============================================================
 * Queue Operations
 * ============================================================ */

static void enqueue_job(WorkScheduler* scheduler, ScheduledJob* job) {
    /* Insert by priority (higher priority first) */
    if (!scheduler->pending_head || job->priority > scheduler->pending_head->priority) {
        job->next = scheduler->pending_head;
        scheduler->pending_head = job;
        if (!scheduler->pending_tail) {
            scheduler->pending_tail = job;
        }
    } else {
        ScheduledJob* prev = scheduler->pending_head;
        while (prev->next && prev->next->priority >= job->priority) {
            prev = prev->next;
        }
        job->next = prev->next;
        prev->next = job;
        if (!job->next) {
            scheduler->pending_tail = job;
        }
    }

    scheduler->pending_count++;
}

static ScheduledJob* dequeue_job(WorkScheduler* scheduler) {
    if (!scheduler->pending_head) return NULL;

    ScheduledJob* job = scheduler->pending_head;
    scheduler->pending_head = job->next;
    if (!scheduler->pending_head) {
        scheduler->pending_tail = NULL;
    }
    job->next = NULL;
    scheduler->pending_count--;

    return job;
}

static void add_to_running(WorkScheduler* scheduler, ScheduledJob* job) {
    job->next = scheduler->running_head;
    scheduler->running_head = job;
    scheduler->running_count++;

    if (scheduler->running_count > scheduler->stats.peak_concurrent_jobs) {
        scheduler->stats.peak_concurrent_jobs = scheduler->running_count;
    }
}

static void remove_from_running(WorkScheduler* scheduler, ScheduledJob* job) {
    if (!scheduler->running_head) return;

    if (scheduler->running_head == job) {
        scheduler->running_head = job->next;
        scheduler->running_count--;
        return;
    }

    ScheduledJob* prev = scheduler->running_head;
    while (prev->next && prev->next != job) {
        prev = prev->next;
    }

    if (prev->next == job) {
        prev->next = job->next;
        scheduler->running_count--;
    }
}

/* ============================================================
 * Scheduler API Implementation
 * ============================================================ */

WorkScheduler* scheduler_create(const SchedulerConfig* config,
                                 WorkerRegistry* worker_registry) {
    if (!worker_registry) {
        log_error("Worker registry required for scheduler");
        return NULL;
    }

    WorkScheduler* scheduler = calloc(1, sizeof(WorkScheduler));
    if (!scheduler) {
        log_error("Failed to allocate scheduler");
        return NULL;
    }

    if (config) {
        scheduler->config = *config;
    } else {
        scheduler->config = scheduler_config_default();
    }

    scheduler->worker_registry = worker_registry;

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    if (!mutex_init(&scheduler->mutex)) {
        log_error("Failed to create scheduler mutex");
        free(scheduler);
        return NULL;
    }
#endif

    log_info("Work scheduler created (strategy: %s, LB: %s)",
             distribution_strategy_name(scheduler->config.default_strategy),
             lb_algorithm_name(scheduler->config.lb_algorithm));

    return scheduler;
}

void scheduler_free(WorkScheduler* scheduler) {
    if (!scheduler) return;

    scheduler_stop(scheduler);

    /* Free pending jobs */
    ScheduledJob* job = scheduler->pending_head;
    while (job) {
        ScheduledJob* next = job->next;
        scheduled_job_free(job);
        job = next;
    }

    /* Free running jobs */
    job = scheduler->running_head;
    while (job) {
        ScheduledJob* next = job->next;
        scheduled_job_free(job);
        job = next;
    }

    /* Free build sessions */
    BuildSession* build = scheduler->builds;
    while (build) {
        BuildSession* next = build->next;
        build_session_free(build);
        build = next;
    }

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    mutex_destroy(&scheduler->mutex);
#endif

    free(scheduler);
    log_debug("Work scheduler freed");
}

void scheduler_set_callbacks(WorkScheduler* scheduler,
                              const SchedulerCallbacks* callbacks) {
    if (!scheduler || !callbacks) return;
    scheduler->callbacks = *callbacks;
}

bool scheduler_start(WorkScheduler* scheduler) {
    if (!scheduler) return false;

    scheduler->running = true;
    log_info("Work scheduler started");

    return true;
}

void scheduler_stop(WorkScheduler* scheduler) {
    if (!scheduler) return;

    scheduler->running = false;

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    if (scheduler->scheduler_thread) {
        thread_join(scheduler->scheduler_thread);
        scheduler->scheduler_thread = NULL;
    }
#endif

    log_info("Work scheduler stopped");
}

bool scheduler_is_running(WorkScheduler* scheduler) {
    return scheduler && scheduler->running;
}

/* ============================================================
 * Build Management
 * ============================================================ */

BuildSession* scheduler_create_build(WorkScheduler* scheduler,
                                      const char* project_name,
                                      DistributionStrategy strategy) {
    if (!scheduler) return NULL;

    scheduler_lock(scheduler);

    /* Check concurrent build limit */
    if (scheduler->build_count >= scheduler->config.max_concurrent_builds) {
        log_warning("Maximum concurrent builds reached (%d)",
                    scheduler->config.max_concurrent_builds);
        scheduler_unlock(scheduler);
        return NULL;
    }

    BuildSession* build = calloc(1, sizeof(BuildSession));
    if (!build) {
        scheduler_unlock(scheduler);
        return NULL;
    }

    build->build_id = generate_build_id();
    build->project_name = project_name ? strdup(project_name) : NULL;
    build->strategy = strategy;
    build->state = BUILD_STATE_PENDING;
    build->started_at = time(NULL);

    /* Add to list */
    build->next = scheduler->builds;
    scheduler->builds = build;
    scheduler->build_count++;

    scheduler->stats.total_builds++;

    log_info("Build session created: %s (project: %s, strategy: %s)",
             build->build_id,
             project_name ? project_name : "unknown",
             distribution_strategy_name(strategy));

    scheduler_unlock(scheduler);
    return build;
}

ScheduledJob* scheduler_submit_job(WorkScheduler* scheduler,
                                    const char* build_id,
                                    DistributedJob* job_spec,
                                    int priority) {
    if (!scheduler || !job_spec) return NULL;

    scheduler_lock(scheduler);

    /* Check queue limit */
    if (scheduler->pending_count >= scheduler->config.max_pending_jobs) {
        log_warning("Job queue full (%d)", scheduler->config.max_pending_jobs);
        scheduler_unlock(scheduler);
        return NULL;
    }

    ScheduledJob* job = calloc(1, sizeof(ScheduledJob));
    if (!job) {
        scheduler_unlock(scheduler);
        return NULL;
    }

    job->job_id = generate_job_id();
    job->build_id = build_id ? strdup(build_id) : NULL;
    job->spec = job_spec;
    job->priority = priority > 0 ? priority : JOB_PRIORITY_NORMAL;
    job->state = JOB_STATE_PENDING;
    job->queued_at = time(NULL);
    job->max_retries = scheduler->config.max_retries;
    job->timeout_sec = job_spec->timeout_sec > 0 ?
                       job_spec->timeout_sec : scheduler->config.default_job_timeout_sec;

    /* Enqueue */
    enqueue_job(scheduler, job);
    scheduler->stats.total_jobs_submitted++;

    /* Update build session */
    if (build_id) {
        for (BuildSession* b = scheduler->builds; b; b = b->next) {
            if (strcmp(b->build_id, build_id) == 0) {
                b->total_jobs++;
                b->pending_jobs++;
                break;
            }
        }
    }

    log_debug("Job submitted: %s (priority: %d)", job->job_id, job->priority);

    scheduler_unlock(scheduler);
    return job;
}

bool scheduler_start_build(WorkScheduler* scheduler, const char* build_id) {
    if (!scheduler || !build_id) return false;

    scheduler_lock(scheduler);

    for (BuildSession* build = scheduler->builds; build; build = build->next) {
        if (strcmp(build->build_id, build_id) == 0) {
            build->state = BUILD_STATE_RUNNING;
            log_info("Build started: %s", build_id);
            scheduler_unlock(scheduler);
            return true;
        }
    }

    scheduler_unlock(scheduler);
    log_warning("Build not found: %s", build_id);
    return false;
}

bool scheduler_cancel_build(WorkScheduler* scheduler,
                             const char* build_id,
                             const char* reason) {
    if (!scheduler || !build_id) return false;

    scheduler_lock(scheduler);

    /* Find build */
    BuildSession* build = NULL;
    for (BuildSession* b = scheduler->builds; b; b = b->next) {
        if (strcmp(b->build_id, build_id) == 0) {
            build = b;
            break;
        }
    }

    if (!build) {
        scheduler_unlock(scheduler);
        return false;
    }

    build->state = BUILD_STATE_CANCELLED;

    /* Cancel all pending jobs for this build */
    ScheduledJob* prev = NULL;
    ScheduledJob* job = scheduler->pending_head;
    while (job) {
        ScheduledJob* next = job->next;
        if (job->build_id && strcmp(job->build_id, build_id) == 0) {
            job->state = JOB_STATE_CANCELLED;
            if (prev) {
                prev->next = next;
            } else {
                scheduler->pending_head = next;
            }
            scheduler->pending_count--;
        } else {
            prev = job;
        }
        job = next;
    }

    log_info("Build cancelled: %s (%s)", build_id, reason ? reason : "no reason");

    scheduler_unlock(scheduler);
    return true;
}

BuildSession* scheduler_get_build(WorkScheduler* scheduler,
                                   const char* build_id) {
    if (!scheduler || !build_id) return NULL;

    scheduler_lock(scheduler);

    for (BuildSession* b = scheduler->builds; b; b = b->next) {
        if (strcmp(b->build_id, build_id) == 0) {
            scheduler_unlock(scheduler);
            return b;
        }
    }

    scheduler_unlock(scheduler);
    return NULL;
}

double scheduler_get_build_progress(WorkScheduler* scheduler,
                                     const char* build_id) {
    BuildSession* build = scheduler_get_build(scheduler, build_id);
    if (!build || build->total_jobs == 0) return 0.0;

    return (double)build->completed_jobs / build->total_jobs * 100.0;
}

/* ============================================================
 * Job Management
 * ============================================================ */

ScheduledJob* scheduler_get_job(WorkScheduler* scheduler,
                                 const char* job_id) {
    if (!scheduler || !job_id) return NULL;

    scheduler_lock(scheduler);

    /* Check pending queue */
    for (ScheduledJob* j = scheduler->pending_head; j; j = j->next) {
        if (strcmp(j->job_id, job_id) == 0) {
            scheduler_unlock(scheduler);
            return j;
        }
    }

    /* Check running list */
    for (ScheduledJob* j = scheduler->running_head; j; j = j->next) {
        if (strcmp(j->job_id, job_id) == 0) {
            scheduler_unlock(scheduler);
            return j;
        }
    }

    scheduler_unlock(scheduler);
    return NULL;
}

bool scheduler_cancel_job(WorkScheduler* scheduler,
                           const char* job_id,
                           const char* reason) {
    if (!scheduler || !job_id) return false;

    ScheduledJob* job = scheduler_get_job(scheduler, job_id);
    if (!job) return false;

    scheduler_lock(scheduler);

    job->state = JOB_STATE_CANCELLED;
    free(job->last_error);
    job->last_error = reason ? strdup(reason) : NULL;

    log_info("Job cancelled: %s (%s)", job_id, reason ? reason : "no reason");

    scheduler_unlock(scheduler);
    return true;
}

void scheduler_report_job_result(WorkScheduler* scheduler,
                                  const char* job_id,
                                  DistributedJobResult* result) {
    if (!scheduler || !job_id) return;

    scheduler_lock(scheduler);

    /* Find job in running list */
    ScheduledJob* job = NULL;
    for (ScheduledJob* j = scheduler->running_head; j; j = j->next) {
        if (strcmp(j->job_id, job_id) == 0) {
            job = j;
            break;
        }
    }

    if (!job) {
        scheduler_unlock(scheduler);
        log_warning("Job not found for result: %s", job_id);
        return;
    }

    /* Update job */
    job->completed_at = time(NULL);
    job->result = result;

    if (result->success) {
        job->state = JOB_STATE_COMPLETED;
        scheduler->stats.total_jobs_completed++;

        double wait_time = difftime(job->assigned_at, job->queued_at);
        double run_time = difftime(job->completed_at, job->started_at);

        /* Update averages */
        int total = scheduler->stats.total_jobs_completed;
        scheduler->stats.avg_job_wait_time_sec =
            ((total - 1) * scheduler->stats.avg_job_wait_time_sec + wait_time) / total;
        scheduler->stats.avg_job_run_time_sec =
            ((total - 1) * scheduler->stats.avg_job_run_time_sec + run_time) / total;

        log_debug("Job completed: %s (%.2fs)", job_id, run_time);

        if (scheduler->callbacks.on_job_completed) {
            scheduler->callbacks.on_job_completed(scheduler, job, result,
                                                   scheduler->callbacks.user_data);
        }
    } else {
        job->state = JOB_STATE_FAILED;
        free(job->last_error);
        job->last_error = result->stderr_output ? strdup(result->stderr_output) : NULL;
        scheduler_report_job_failure(scheduler, job_id,
                                      result->stderr_output ? result->stderr_output : "Unknown error");
    }

    /* Remove from running */
    remove_from_running(scheduler, job);

    /* Update worker */
    if (job->assigned_worker_id) {
        RemoteWorker* worker = worker_registry_find_by_id(
            scheduler->worker_registry, job->assigned_worker_id);
        if (worker) {
            worker_registry_update_job_count(scheduler->worker_registry, worker, -1);
            worker_registry_record_job_complete(scheduler->worker_registry, worker,
                                                 result->success, result->duration_sec);
        }
    }

    /* Update build */
    if (job->build_id) {
        for (BuildSession* b = scheduler->builds; b; b = b->next) {
            if (strcmp(b->build_id, job->build_id) == 0) {
                b->running_jobs--;
                if (result->success) {
                    b->completed_jobs++;
                } else {
                    b->failed_jobs++;
                }
                b->progress_percent = (double)b->completed_jobs / b->total_jobs * 100.0;

                /* Check if build complete */
                if (b->completed_jobs + b->failed_jobs >= b->total_jobs) {
                    b->completed_at = time(NULL);
                    b->success = (b->failed_jobs == 0);
                    b->state = b->success ? BUILD_STATE_COMPLETED : BUILD_STATE_FAILED;

                    if (b->success) {
                        scheduler->stats.successful_builds++;
                    } else {
                        scheduler->stats.failed_builds++;
                    }

                    if (scheduler->callbacks.on_build_completed) {
                        scheduler->callbacks.on_build_completed(scheduler, b,
                                                                 scheduler->callbacks.user_data);
                    }
                }
                break;
            }
        }
    }

    scheduler_unlock(scheduler);
}

void scheduler_report_job_failure(WorkScheduler* scheduler,
                                   const char* job_id,
                                   const char* error) {
    if (!scheduler || !job_id) return;

    ScheduledJob* job = scheduler_get_job(scheduler, job_id);
    if (!job) return;

    scheduler_lock(scheduler);

    free(job->last_error);
    job->last_error = error ? strdup(error) : NULL;

    /* Check if can retry */
    if (job->retry_count < job->max_retries) {
        job->retry_count++;
        job->state = JOB_STATE_RETRY;
        scheduler->stats.total_retries++;

        log_info("Job will retry (%d/%d): %s",
                 job->retry_count, job->max_retries, job_id);

        /* Move back to pending queue */
        remove_from_running(scheduler, job);
        enqueue_job(scheduler, job);
    } else {
        job->state = JOB_STATE_FAILED;
        scheduler->stats.total_jobs_failed++;

        log_error("Job failed (max retries): %s - %s", job_id, error ? error : "");

        if (scheduler->callbacks.on_job_failed) {
            scheduler->callbacks.on_job_failed(scheduler, job, error,
                                                scheduler->callbacks.user_data);
        }
    }

    scheduler_unlock(scheduler);
}

void scheduler_handle_worker_disconnect(WorkScheduler* scheduler,
                                          const char* worker_id) {
    if (!scheduler || !worker_id) return;

    scheduler_lock(scheduler);

    /* Find all jobs assigned to this worker and reschedule */
    ScheduledJob* job = scheduler->running_head;
    while (job) {
        ScheduledJob* next = job->next;

        if (job->assigned_worker_id &&
            strcmp(job->assigned_worker_id, worker_id) == 0) {

            log_warning("Rescheduling job %s (worker disconnected)", job->job_id);

            job->state = JOB_STATE_RETRY;
            job->retry_count++;
            free(job->assigned_worker_id);
            job->assigned_worker_id = NULL;

            remove_from_running(scheduler, job);
            enqueue_job(scheduler, job);
        }

        job = next;
    }

    scheduler_unlock(scheduler);
}

/* ============================================================
 * Queue Processing
 * ============================================================ */

int scheduler_get_pending_count(WorkScheduler* scheduler) {
    return scheduler ? scheduler->pending_count : 0;
}

int scheduler_get_running_count(WorkScheduler* scheduler) {
    return scheduler ? scheduler->running_count : 0;
}

int scheduler_process_queue(WorkScheduler* scheduler) {
    if (!scheduler || !scheduler->running) return 0;

    scheduler_lock(scheduler);

    int assigned = 0;

    /* Process pending jobs */
    while (scheduler->pending_head) {
        ScheduledJob* job = scheduler->pending_head;

        /* Check if dependencies satisfied */
        bool deps_ok = true;
        for (int i = 0; i < job->depends_count && deps_ok; i++) {
            ScheduledJob* dep = scheduler_get_job(scheduler, job->depends_on[i]);
            if (dep && dep->state != JOB_STATE_COMPLETED) {
                deps_ok = false;
            }
        }

        if (!deps_ok) {
            break;  /* Wait for dependencies */
        }

        /* Select worker */
        RemoteWorker* worker = select_worker(scheduler, job);
        if (!worker) {
            break;  /* No available workers */
        }

        /* Assign job */
        dequeue_job(scheduler);
        job->state = JOB_STATE_ASSIGNED;
        job->assigned_at = time(NULL);
        job->assigned_worker_id = strdup(worker->id);
        job->deadline = job->assigned_at + job->timeout_sec;

        add_to_running(scheduler, job);
        worker_registry_update_job_count(scheduler->worker_registry, worker, 1);
        assigned++;

        log_debug("Job %s assigned to worker %s", job->job_id, worker->id);

        if (scheduler->callbacks.on_job_assigned) {
            scheduler->callbacks.on_job_assigned(scheduler, job, worker,
                                                  scheduler->callbacks.user_data);
        }
    }

    scheduler_unlock(scheduler);
    return assigned;
}

int scheduler_check_timeouts(WorkScheduler* scheduler) {
    if (!scheduler) return 0;

    scheduler_lock(scheduler);

    time_t now = time(NULL);
    int timed_out = 0;

    ScheduledJob* job = scheduler->running_head;
    while (job) {
        ScheduledJob* next = job->next;

        if (job->deadline > 0 && now > job->deadline) {
            log_warning("Job timed out: %s", job->job_id);

            job->state = JOB_STATE_TIMEOUT;
            scheduler_report_job_failure(scheduler, job->job_id, "Job timed out");
            timed_out++;
        }

        job = next;
    }

    scheduler_unlock(scheduler);
    return timed_out;
}

/* ============================================================
 * Strategy Helpers
 * ============================================================ */

int scheduler_decompose_compile(const char** source_files,
                                 int count,
                                 const char* compiler,
                                 const char** flags,
                                 DistributedJob** out_jobs,
                                 int max_jobs) {
    if (!source_files || count == 0 || !out_jobs) return 0;

    int created = 0;

    for (int i = 0; i < count && created < max_jobs; i++) {
        DistributedJob* job = calloc(1, sizeof(DistributedJob));
        if (!job) break;

        job->type = JOB_TYPE_COMPILE;
        job->source_file = strdup(source_files[i]);
        job->compiler = compiler ? strdup(compiler) : NULL;

        /* Copy flags */
        if (flags) {
            int flag_count = 0;
            while (flags[flag_count]) flag_count++;

            job->compiler_args = malloc(sizeof(char*) * (flag_count + 1));
            if (job->compiler_args) {
                for (int j = 0; j < flag_count; j++) {
                    job->compiler_args[j] = strdup(flags[j]);
                }
                job->compiler_args[flag_count] = NULL;
                job->arg_count = flag_count;
            }
        }

        out_jobs[created++] = job;
    }

    return created;
}

DistributionStrategy scheduler_suggest_strategy(int source_count,
                                                  bool has_cmake,
                                                  int target_count) {
    /* Simple heuristic */
    if (source_count < 5) {
        return DIST_STRATEGY_WHOLE_PROJECT;
    }

    if (has_cmake && target_count > 1) {
        return DIST_STRATEGY_TARGETS;
    }

    if (source_count > 50) {
        return DIST_STRATEGY_COMPILE_UNITS;
    }

    return DIST_STRATEGY_HYBRID;
}

/* ============================================================
 * Statistics
 * ============================================================ */

SchedulerStats scheduler_get_stats(WorkScheduler* scheduler) {
    if (!scheduler) {
        SchedulerStats empty = {0};
        return empty;
    }
    return scheduler->stats;
}

void scheduler_reset_stats(WorkScheduler* scheduler) {
    if (!scheduler) return;
    memset(&scheduler->stats, 0, sizeof(SchedulerStats));
}

/* ============================================================
 * Utility Functions
 * ============================================================ */

const char* job_state_name(JobState state) {
    switch (state) {
        case JOB_STATE_PENDING: return "PENDING";
        case JOB_STATE_ASSIGNED: return "ASSIGNED";
        case JOB_STATE_RUNNING: return "RUNNING";
        case JOB_STATE_COMPLETED: return "COMPLETED";
        case JOB_STATE_FAILED: return "FAILED";
        case JOB_STATE_CANCELLED: return "CANCELLED";
        case JOB_STATE_TIMEOUT: return "TIMEOUT";
        case JOB_STATE_RETRY: return "RETRY";
        default: return "UNKNOWN";
    }
}

const char* build_state_name(BuildState state) {
    switch (state) {
        case BUILD_STATE_PENDING: return "PENDING";
        case BUILD_STATE_DECOMPOSING: return "DECOMPOSING";
        case BUILD_STATE_RUNNING: return "RUNNING";
        case BUILD_STATE_COMPLETING: return "COMPLETING";
        case BUILD_STATE_COMPLETED: return "COMPLETED";
        case BUILD_STATE_FAILED: return "FAILED";
        case BUILD_STATE_CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}

const char* distribution_strategy_name(DistributionStrategy strategy) {
    switch (strategy) {
        case DIST_STRATEGY_COMPILE_UNITS: return "COMPILE_UNITS";
        case DIST_STRATEGY_TARGETS: return "TARGETS";
        case DIST_STRATEGY_WHOLE_PROJECT: return "WHOLE_PROJECT";
        case DIST_STRATEGY_HYBRID: return "HYBRID";
        default: return "UNKNOWN";
    }
}

const char* lb_algorithm_name(LoadBalancingAlgorithm algo) {
    switch (algo) {
        case LB_ROUND_ROBIN: return "ROUND_ROBIN";
        case LB_LEAST_LOADED: return "LEAST_LOADED";
        case LB_LEAST_LATENCY: return "LEAST_LATENCY";
        case LB_WEIGHTED: return "WEIGHTED";
        case LB_RANDOM: return "RANDOM";
        default: return "UNKNOWN";
    }
}
