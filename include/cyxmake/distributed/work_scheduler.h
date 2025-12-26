/**
 * @file work_scheduler.h
 * @brief Work distribution and scheduling for distributed builds
 *
 * Implements job distribution strategies, load balancing, and job lifecycle
 * management for distributed compilation across multiple workers.
 */

#ifndef CYXMAKE_DISTRIBUTED_WORK_SCHEDULER_H
#define CYXMAKE_DISTRIBUTED_WORK_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "cyxmake/distributed/protocol.h"
#include "cyxmake/distributed/worker_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Distribution Strategy
 * ============================================================ */

typedef enum {
    DIST_STRATEGY_COMPILE_UNITS,  /* Distribute individual .c/.cpp files (distcc-style) */
    DIST_STRATEGY_TARGETS,        /* Distribute independent build targets */
    DIST_STRATEGY_WHOLE_PROJECT,  /* Send entire project to single worker */
    DIST_STRATEGY_HYBRID          /* Combine strategies based on project structure */
} DistributionStrategy;

/* ============================================================
 * Load Balancing Algorithm
 * ============================================================ */

typedef enum {
    LB_ROUND_ROBIN,               /* Simple round-robin */
    LB_LEAST_LOADED,              /* Prefer workers with fewer active jobs */
    LB_LEAST_LATENCY,             /* Prefer workers with lowest network latency */
    LB_WEIGHTED,                  /* Weight by CPU cores and job capacity */
    LB_RANDOM                     /* Random selection (for testing) */
} LoadBalancingAlgorithm;

/* ============================================================
 * Job State
 * ============================================================ */

typedef enum {
    JOB_STATE_PENDING = 0,        /* Waiting in queue */
    JOB_STATE_ASSIGNED,           /* Assigned to worker */
    JOB_STATE_RUNNING,            /* Execution in progress */
    JOB_STATE_COMPLETED,          /* Successfully completed */
    JOB_STATE_FAILED,             /* Failed with error */
    JOB_STATE_CANCELLED,          /* Cancelled by user */
    JOB_STATE_TIMEOUT,            /* Exceeded timeout */
    JOB_STATE_RETRY               /* Pending retry */
} JobState;

/* ============================================================
 * Job Priority
 * ============================================================ */

typedef enum {
    JOB_PRIORITY_LOW = 0,
    JOB_PRIORITY_NORMAL = 50,
    JOB_PRIORITY_HIGH = 100,
    JOB_PRIORITY_CRITICAL = 200
} JobPriority;

/* ============================================================
 * Scheduled Job
 * ============================================================ */

typedef struct ScheduledJob {
    /* Identity */
    char* job_id;                 /* Unique job ID */
    char* build_id;               /* Parent build ID */
    int sequence;                 /* Sequence number within build */

    /* Job specification */
    DistributedJob* spec;         /* Job specification */
    int priority;                 /* Priority (higher = more important) */

    /* State */
    JobState state;               /* Current state */
    char* assigned_worker_id;     /* Worker handling this job */
    time_t queued_at;             /* When queued */
    time_t assigned_at;           /* When assigned to worker */
    time_t started_at;            /* When execution started */
    time_t completed_at;          /* When completed */

    /* Retry handling */
    int retry_count;              /* Current retry count */
    int max_retries;              /* Maximum retries allowed */
    char* last_error;             /* Last error message */

    /* Result */
    DistributedJobResult* result; /* Job result (when complete) */

    /* Timeout */
    int timeout_sec;              /* Job timeout */
    time_t deadline;              /* Absolute deadline */

    /* Dependencies */
    char** depends_on;            /* Job IDs this depends on */
    int depends_count;

    /* Callbacks */
    void (*on_complete)(struct ScheduledJob*, void*);
    void (*on_failed)(struct ScheduledJob*, void*);
    void* callback_data;

    /* Internal */
    struct ScheduledJob* next;    /* Queue linkage */
} ScheduledJob;

/* ============================================================
 * Build Session
 * ============================================================ */

typedef enum {
    BUILD_STATE_PENDING,          /* Build not yet started */
    BUILD_STATE_DECOMPOSING,      /* Breaking into jobs */
    BUILD_STATE_RUNNING,          /* Jobs being executed */
    BUILD_STATE_COMPLETING,       /* Aggregating results */
    BUILD_STATE_COMPLETED,        /* All jobs done successfully */
    BUILD_STATE_FAILED,           /* Build failed */
    BUILD_STATE_CANCELLED         /* Build cancelled */
} BuildState;

typedef struct BuildSession {
    char* build_id;               /* Unique build ID */
    char* project_name;           /* Project being built */
    DistributionStrategy strategy; /* Distribution strategy used */

    BuildState state;             /* Current build state */
    time_t started_at;            /* Build start time */
    time_t completed_at;          /* Build completion time */

    /* Job tracking */
    int total_jobs;               /* Total jobs in this build */
    int pending_jobs;             /* Jobs waiting */
    int running_jobs;             /* Jobs in progress */
    int completed_jobs;           /* Successfully completed */
    int failed_jobs;              /* Failed jobs */

    /* Progress */
    double progress_percent;      /* Overall progress (0-100) */
    char* current_phase;          /* Current build phase */

    /* Results */
    bool success;                 /* Overall success */
    char** output_artifacts;      /* Produced artifacts */
    int artifact_count;
    char* error_summary;          /* Error summary if failed */

    struct BuildSession* next;    /* Session list */
} BuildSession;

/* ============================================================
 * Scheduler Configuration
 * ============================================================ */

typedef struct {
    DistributionStrategy default_strategy;
    LoadBalancingAlgorithm lb_algorithm;

    /* Job settings */
    int default_job_timeout_sec;  /* Default job timeout (default: 600) */
    int max_retries;              /* Maximum retries (default: 2) */
    int retry_delay_sec;          /* Delay between retries (default: 5) */

    /* Queue settings */
    int max_pending_jobs;         /* Maximum pending jobs (default: 10000) */
    int max_concurrent_builds;    /* Maximum concurrent builds (default: 10) */

    /* Optimization */
    bool enable_job_coalescing;   /* Combine small jobs */
    bool enable_speculative;      /* Run speculative jobs on idle workers */
    int min_job_size_bytes;       /* Minimum job size to distribute */
} SchedulerConfig;

/* ============================================================
 * Work Scheduler
 * ============================================================ */

typedef struct WorkScheduler WorkScheduler;

/* ============================================================
 * Scheduler Callbacks
 * ============================================================ */

typedef void (*OnJobAssignedCallback)(
    WorkScheduler* scheduler,
    ScheduledJob* job,
    RemoteWorker* worker,
    void* user_data
);

typedef void (*OnJobCompletedCallback)(
    WorkScheduler* scheduler,
    ScheduledJob* job,
    DistributedJobResult* result,
    void* user_data
);

typedef void (*OnJobFailedCallback)(
    WorkScheduler* scheduler,
    ScheduledJob* job,
    const char* error,
    void* user_data
);

typedef void (*OnBuildCompletedCallback)(
    WorkScheduler* scheduler,
    BuildSession* session,
    void* user_data
);

typedef struct {
    OnJobAssignedCallback on_job_assigned;
    OnJobCompletedCallback on_job_completed;
    OnJobFailedCallback on_job_failed;
    OnBuildCompletedCallback on_build_completed;
    void* user_data;
} SchedulerCallbacks;

/* ============================================================
 * Scheduler API
 * ============================================================ */

/**
 * Create a work scheduler
 */
WorkScheduler* scheduler_create(const SchedulerConfig* config,
                                 WorkerRegistry* worker_registry);

/**
 * Free scheduler resources
 */
void scheduler_free(WorkScheduler* scheduler);

/**
 * Set scheduler callbacks
 */
void scheduler_set_callbacks(WorkScheduler* scheduler,
                              const SchedulerCallbacks* callbacks);

/**
 * Start the scheduler (begins processing jobs)
 */
bool scheduler_start(WorkScheduler* scheduler);

/**
 * Stop the scheduler (graceful shutdown)
 */
void scheduler_stop(WorkScheduler* scheduler);

/**
 * Check if scheduler is running
 */
bool scheduler_is_running(WorkScheduler* scheduler);

/* ============================================================
 * Build Management
 * ============================================================ */

/**
 * Create a new build session
 * @param scheduler The scheduler
 * @param project_name Project name
 * @param strategy Distribution strategy to use
 * @return Build session or NULL on error
 */
BuildSession* scheduler_create_build(WorkScheduler* scheduler,
                                      const char* project_name,
                                      DistributionStrategy strategy);

/**
 * Submit a job to a build
 * @param scheduler The scheduler
 * @param build_id Build session ID
 * @param job Job specification
 * @param priority Job priority
 * @return Scheduled job or NULL on error
 */
ScheduledJob* scheduler_submit_job(WorkScheduler* scheduler,
                                    const char* build_id,
                                    DistributedJob* job,
                                    int priority);

/**
 * Start executing a build
 */
bool scheduler_start_build(WorkScheduler* scheduler, const char* build_id);

/**
 * Cancel a build
 */
bool scheduler_cancel_build(WorkScheduler* scheduler,
                             const char* build_id,
                             const char* reason);

/**
 * Get build session by ID
 */
BuildSession* scheduler_get_build(WorkScheduler* scheduler,
                                   const char* build_id);

/**
 * Get build progress
 */
double scheduler_get_build_progress(WorkScheduler* scheduler,
                                     const char* build_id);

/* ============================================================
 * Job Management
 * ============================================================ */

/**
 * Get job by ID
 */
ScheduledJob* scheduler_get_job(WorkScheduler* scheduler,
                                 const char* job_id);

/**
 * Cancel a specific job
 */
bool scheduler_cancel_job(WorkScheduler* scheduler,
                           const char* job_id,
                           const char* reason);

/**
 * Report job result (called when worker reports completion)
 */
void scheduler_report_job_result(WorkScheduler* scheduler,
                                  const char* job_id,
                                  DistributedJobResult* result);

/**
 * Report job failure
 */
void scheduler_report_job_failure(WorkScheduler* scheduler,
                                   const char* job_id,
                                   const char* error);

/**
 * Handle worker disconnect (reschedule its jobs)
 */
void scheduler_handle_worker_disconnect(WorkScheduler* scheduler,
                                          const char* worker_id);

/* ============================================================
 * Queue Operations
 * ============================================================ */

/**
 * Get pending job count
 */
int scheduler_get_pending_count(WorkScheduler* scheduler);

/**
 * Get running job count
 */
int scheduler_get_running_count(WorkScheduler* scheduler);

/**
 * Process queue (assign jobs to workers)
 * Call periodically or when workers become available
 */
int scheduler_process_queue(WorkScheduler* scheduler);

/**
 * Check for timed-out jobs
 * Call periodically
 */
int scheduler_check_timeouts(WorkScheduler* scheduler);

/* ============================================================
 * Strategy Helpers
 * ============================================================ */

/**
 * Decompose a compilation into distributed jobs
 * @param source_files List of source files
 * @param count Number of source files
 * @param compiler Compiler to use
 * @param flags Compilation flags
 * @param out_jobs Output array for created jobs
 * @param max_jobs Maximum jobs to create
 * @return Number of jobs created
 */
int scheduler_decompose_compile(const char** source_files,
                                 int count,
                                 const char* compiler,
                                 const char** flags,
                                 DistributedJob** out_jobs,
                                 int max_jobs);

/**
 * Suggest optimal distribution strategy
 * @param source_count Number of source files
 * @param has_cmake Whether project uses CMake
 * @param target_count Number of build targets
 * @return Recommended strategy
 */
DistributionStrategy scheduler_suggest_strategy(int source_count,
                                                  bool has_cmake,
                                                  int target_count);

/* ============================================================
 * Statistics
 * ============================================================ */

typedef struct {
    /* Job stats */
    int total_jobs_submitted;
    int total_jobs_completed;
    int total_jobs_failed;
    int total_retries;

    /* Build stats */
    int total_builds;
    int successful_builds;
    int failed_builds;

    /* Timing */
    double avg_job_wait_time_sec;
    double avg_job_run_time_sec;
    double avg_build_time_sec;

    /* Worker utilization */
    double avg_worker_utilization;
    int peak_concurrent_jobs;
} SchedulerStats;

/**
 * Get scheduler statistics
 */
SchedulerStats scheduler_get_stats(WorkScheduler* scheduler);

/**
 * Reset statistics
 */
void scheduler_reset_stats(WorkScheduler* scheduler);

/* ============================================================
 * Utility Functions
 * ============================================================ */

/**
 * Get job state name
 */
const char* job_state_name(JobState state);

/**
 * Get build state name
 */
const char* build_state_name(BuildState state);

/**
 * Get distribution strategy name
 */
const char* distribution_strategy_name(DistributionStrategy strategy);

/**
 * Get load balancing algorithm name
 */
const char* lb_algorithm_name(LoadBalancingAlgorithm algo);

/**
 * Create default scheduler configuration
 */
SchedulerConfig scheduler_config_default(void);

/**
 * Free scheduled job
 */
void scheduled_job_free(ScheduledJob* job);

/**
 * Free build session
 */
void build_session_free(BuildSession* session);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_DISTRIBUTED_WORK_SCHEDULER_H */
