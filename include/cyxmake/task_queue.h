/**
 * @file task_queue.h
 * @brief Task Queue - Priority-based task scheduling for agents
 *
 * Provides:
 * - Priority-based task queue (heap-based)
 * - Task dependencies and ordering
 * - Task lifecycle management
 * - Completion callbacks
 */

#ifndef CYXMAKE_TASK_QUEUE_H
#define CYXMAKE_TASK_QUEUE_H

#include "cyxmake/threading.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct ProjectContext ProjectContext;
typedef struct AgentInstance AgentInstance;
typedef struct AgentTask AgentTask;

/* ============================================================================
 * Task Priority and State
 * ============================================================================ */

/**
 * Task priority levels
 */
typedef enum {
    TASK_PRIORITY_LOW = 0,
    TASK_PRIORITY_NORMAL = 1,
    TASK_PRIORITY_HIGH = 2,
    TASK_PRIORITY_CRITICAL = 3
} TaskPriority;

/**
 * Task lifecycle states
 */
typedef enum {
    TASK_STATE_PENDING,         /* Waiting in queue */
    TASK_STATE_ASSIGNED,        /* Assigned to agent, not started */
    TASK_STATE_RUNNING,         /* Currently executing */
    TASK_STATE_WAITING_CHILD,   /* Waiting on spawned sub-tasks */
    TASK_STATE_COMPLETED,       /* Finished successfully */
    TASK_STATE_FAILED,          /* Finished with error */
    TASK_STATE_CANCELLED,       /* Cancelled before completion */
    TASK_STATE_TIMEOUT          /* Exceeded time limit */
} TaskState;

/**
 * Task type hints for routing to appropriate agent
 */
typedef enum {
    TASK_TYPE_BUILD,            /* Build the project */
    TASK_TYPE_FIX,              /* Fix an error */
    TASK_TYPE_ANALYZE,          /* Analyze code */
    TASK_TYPE_INSTALL,          /* Install dependencies */
    TASK_TYPE_EXECUTE,          /* Execute a command */
    TASK_TYPE_MODIFY,           /* Modify files */
    TASK_TYPE_QUERY,            /* Answer a question */
    TASK_TYPE_GENERAL           /* General task */
} TaskType;

/* ============================================================================
 * Task Structure
 * ============================================================================ */

/**
 * Agent task completion callback function type
 * Note: Named AgentTaskCallback to avoid conflict with threading.h TaskCallback
 */
typedef void (*AgentTaskCallback)(AgentTask* task, void* user_data);

/**
 * A task to be executed by an agent
 */
typedef struct AgentTask {
    /* Identity */
    char* id;                   /* Unique task ID */
    char* description;          /* Natural language task description */
    TaskType type;              /* Type hint for routing */
    TaskPriority priority;
    TaskState state;

    /* Assignment */
    char* assigned_agent_id;    /* Agent ID (NULL if unassigned) */
    char* preferred_agent;      /* Preferred agent name (optional) */
    unsigned int required_capabilities;  /* Required agent capabilities */

    /* Dependencies */
    char** depends_on;          /* Task IDs this depends on */
    int dependency_count;
    bool dependencies_met;      /* All dependencies completed */

    /* Context */
    char* project_path;         /* Path to project (optional) */
    ProjectContext* project_ctx; /* Project context (shared, not owned) */
    char* input_json;           /* JSON input parameters */
    char* context_json;         /* Additional context data */

    /* Result */
    char* result_json;          /* JSON result on completion */
    char* error_message;        /* Error message on failure */
    int exit_code;              /* Exit code (0 = success) */

    /* Timing */
    time_t created_at;
    time_t started_at;
    time_t completed_at;
    int timeout_sec;            /* 0 = no timeout */

    /* Callbacks */
    AgentTaskCallback on_complete;   /* Called when task completes */
    AgentTaskCallback on_error;      /* Called on error */
    AgentTaskCallback on_progress;   /* Called for progress updates */
    void* callback_data;        /* User data for callbacks */

    /* Progress tracking */
    int progress_percent;       /* 0-100 */
    char* progress_message;     /* Current status message */

    /* Queue linkage (for internal use) */
    struct AgentTask* next;     /* Next task in linked list */
    int heap_index;             /* Index in priority heap */
} AgentTask;

/* ============================================================================
 * Task Queue
 * ============================================================================ */

/**
 * Priority queue for agent tasks
 */
typedef struct TaskQueue {
    AgentTask** heap;           /* Binary heap for priority queue */
    size_t count;               /* Number of tasks in queue */
    size_t capacity;            /* Heap capacity */

    MutexHandle mutex;          /* Thread-safe access */
    ConditionHandle not_empty;  /* Signal when task added */

    bool shutdown;              /* Queue is shutting down */
} TaskQueue;

/* ============================================================================
 * Task Lifecycle
 * ============================================================================ */

/**
 * Create a new task
 *
 * @param description Natural language task description
 * @param type Task type hint
 * @param priority Task priority
 * @return New task or NULL on failure
 */
AgentTask* task_create(const char* description, TaskType type,
                       TaskPriority priority);

/**
 * Free a task
 *
 * @param task Task to free
 */
void task_free(AgentTask* task);

/**
 * Set task input parameters
 *
 * @param task Task to modify
 * @param json JSON input string
 */
void task_set_input(AgentTask* task, const char* json);

/**
 * Set task context
 *
 * @param task Task to modify
 * @param project_path Project path (optional)
 * @param ctx Project context (optional)
 */
void task_set_context(AgentTask* task, const char* project_path,
                      ProjectContext* ctx);

/**
 * Add a dependency to a task
 *
 * @param task Task to modify
 * @param dependency_id ID of task this depends on
 * @return true on success
 */
bool task_add_dependency(AgentTask* task, const char* dependency_id);

/**
 * Set task completion callback
 *
 * @param task Task to modify
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void task_set_callback(AgentTask* task, AgentTaskCallback callback, void* user_data);

/**
 * Set task timeout
 *
 * @param task Task to modify
 * @param timeout_sec Timeout in seconds (0 = no timeout)
 */
void task_set_timeout(AgentTask* task, int timeout_sec);

/**
 * Mark task as completed
 *
 * @param task Task to complete
 * @param result_json JSON result (copied)
 */
void task_complete(AgentTask* task, const char* result_json);

/**
 * Mark task as failed
 *
 * @param task Task to fail
 * @param error_message Error message (copied)
 * @param exit_code Exit code
 */
void task_fail(AgentTask* task, const char* error_message, int exit_code);

/**
 * Update task progress
 *
 * @param task Task to update
 * @param percent Progress percentage (0-100)
 * @param message Progress message (copied)
 */
void task_update_progress(AgentTask* task, int percent, const char* message);

/* ============================================================================
 * Task Queue Operations
 * ============================================================================ */

/**
 * Create a task queue
 *
 * @return New queue or NULL on failure
 */
TaskQueue* task_queue_create(void);

/**
 * Free a task queue and all contained tasks
 *
 * @param queue Queue to free
 */
void task_queue_free(TaskQueue* queue);

/**
 * Push a task onto the queue
 *
 * @param queue The queue
 * @param task Task to add (queue takes ownership)
 * @return true on success
 */
bool task_queue_push(TaskQueue* queue, AgentTask* task);

/**
 * Pop the highest priority task (blocking)
 *
 * @param queue The queue
 * @return Next task or NULL if queue is shutting down
 */
AgentTask* task_queue_pop(TaskQueue* queue);

/**
 * Pop with timeout
 *
 * @param queue The queue
 * @param timeout_ms Maximum time to wait
 * @return Next task or NULL if timeout/shutdown
 */
AgentTask* task_queue_pop_timeout(TaskQueue* queue, int timeout_ms);

/**
 * Try to pop without blocking
 *
 * @param queue The queue
 * @return Next task or NULL if queue is empty
 */
AgentTask* task_queue_try_pop(TaskQueue* queue);

/**
 * Pop a task suitable for a specific agent
 *
 * @param queue The queue
 * @param agent Agent to match against
 * @return Matching task or NULL if none available
 */
AgentTask* task_queue_pop_for_agent(TaskQueue* queue, AgentInstance* agent);

/**
 * Peek at the next task without removing
 *
 * @param queue The queue
 * @return Next task or NULL (do not free)
 */
AgentTask* task_queue_peek(TaskQueue* queue);

/**
 * Get task by ID
 *
 * @param queue The queue
 * @param task_id Task ID to find
 * @return Task or NULL (do not free)
 */
AgentTask* task_queue_get(TaskQueue* queue, const char* task_id);

/**
 * Remove a specific task by ID
 *
 * @param queue The queue
 * @param task_id Task ID to remove
 * @return Removed task or NULL if not found (caller must free)
 */
AgentTask* task_queue_remove(TaskQueue* queue, const char* task_id);

/**
 * Cancel a task
 *
 * @param queue The queue
 * @param task_id Task ID to cancel
 * @return true if task was cancelled
 */
bool task_queue_cancel(TaskQueue* queue, const char* task_id);

/**
 * Get number of tasks in queue
 *
 * @param queue The queue
 * @return Number of pending tasks
 */
size_t task_queue_count(TaskQueue* queue);

/**
 * Check if queue is empty
 *
 * @param queue The queue
 * @return true if empty
 */
bool task_queue_is_empty(TaskQueue* queue);

/**
 * Signal queue to shut down
 *
 * @param queue The queue
 */
void task_queue_shutdown(TaskQueue* queue);

/**
 * Clear all tasks from the queue
 *
 * @param queue The queue
 */
void task_queue_clear(TaskQueue* queue);

/* ============================================================================
 * Dependency Management
 * ============================================================================ */

/**
 * Check if a task's dependencies are all completed
 *
 * @param queue The queue
 * @param task Task to check
 * @return true if all dependencies are completed
 */
bool task_dependencies_met(TaskQueue* queue, AgentTask* task);

/**
 * Update dependency status for all pending tasks
 *
 * @param queue The queue
 * @param completed_task_id ID of task that just completed
 */
void task_queue_update_dependencies(TaskQueue* queue,
                                    const char* completed_task_id);

/**
 * Get tasks that are blocked by a specific task
 *
 * @param queue The queue
 * @param task_id Task ID to check
 * @param count Output: number of blocked tasks
 * @return Array of blocked tasks (caller must free array)
 */
AgentTask** task_queue_get_blocked_by(TaskQueue* queue, const char* task_id,
                                      int* count);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Generate a unique task ID
 *
 * @return New ID string (caller must free)
 */
char* task_generate_id(void);

/**
 * Get string representation of task state
 */
const char* task_state_to_string(TaskState state);

/**
 * Get string representation of task type
 */
const char* task_type_to_string(TaskType type);

/**
 * Get string representation of priority
 */
const char* task_priority_to_string(TaskPriority priority);

/**
 * Calculate elapsed time for a task
 *
 * @param task Task to check
 * @return Elapsed seconds (-1 if not started)
 */
double task_elapsed_time(AgentTask* task);

/**
 * Check if a task has timed out
 *
 * @param task Task to check
 * @return true if timeout exceeded
 */
bool task_has_timed_out(AgentTask* task);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_TASK_QUEUE_H */
