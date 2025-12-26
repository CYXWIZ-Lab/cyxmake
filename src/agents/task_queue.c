/**
 * @file task_queue.c
 * @brief Task Queue implementation with priority heap
 */

#include "cyxmake/task_queue.h"
#include "cyxmake/agent_registry.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef CYXMAKE_WINDOWS
    #include <windows.h>
#else
    #include <uuid/uuid.h>
#endif

/* ============================================================================
 * UUID Generation for Tasks
 * ============================================================================ */

char* task_generate_id(void) {
    char* id = (char*)malloc(48);
    if (!id) return NULL;

#ifdef CYXMAKE_WINDOWS
    /* Simple ID using timestamp + random */
    snprintf(id, 48, "task-%08lx-%04x-%04x",
             (unsigned long)time(NULL),
             (unsigned int)(rand() & 0xFFFF),
             (unsigned int)(rand() & 0xFFFF));
#else
    uuid_t uuid;
    uuid_generate(uuid);
    strcpy(id, "task-");
    uuid_unparse_lower(uuid, id + 5);
#endif

    return id;
}

/* ============================================================================
 * String Conversions
 * ============================================================================ */

const char* task_state_to_string(TaskState state) {
    switch (state) {
        case TASK_STATE_PENDING:       return "pending";
        case TASK_STATE_ASSIGNED:      return "assigned";
        case TASK_STATE_RUNNING:       return "running";
        case TASK_STATE_WAITING_CHILD: return "waiting";
        case TASK_STATE_COMPLETED:     return "completed";
        case TASK_STATE_FAILED:        return "failed";
        case TASK_STATE_CANCELLED:     return "cancelled";
        case TASK_STATE_TIMEOUT:       return "timeout";
        default:                       return "unknown";
    }
}

const char* task_type_to_string(TaskType type) {
    switch (type) {
        case TASK_TYPE_BUILD:    return "build";
        case TASK_TYPE_FIX:      return "fix";
        case TASK_TYPE_ANALYZE:  return "analyze";
        case TASK_TYPE_INSTALL:  return "install";
        case TASK_TYPE_EXECUTE:  return "execute";
        case TASK_TYPE_MODIFY:   return "modify";
        case TASK_TYPE_QUERY:    return "query";
        case TASK_TYPE_GENERAL:  return "general";
        default:                 return "unknown";
    }
}

const char* task_priority_to_string(TaskPriority priority) {
    switch (priority) {
        case TASK_PRIORITY_LOW:      return "low";
        case TASK_PRIORITY_NORMAL:   return "normal";
        case TASK_PRIORITY_HIGH:     return "high";
        case TASK_PRIORITY_CRITICAL: return "critical";
        default:                     return "unknown";
    }
}

/* ============================================================================
 * Task Lifecycle
 * ============================================================================ */

AgentTask* task_create(const char* description, TaskType type,
                       TaskPriority priority) {
    if (!description) {
        log_error("Task description is required");
        return NULL;
    }

    AgentTask* task = (AgentTask*)calloc(1, sizeof(AgentTask));
    if (!task) {
        log_error("Failed to allocate task");
        return NULL;
    }

    task->id = task_generate_id();
    if (!task->id) {
        free(task);
        return NULL;
    }

    task->description = strdup(description);
    task->type = type;
    task->priority = priority;
    task->state = TASK_STATE_PENDING;
    task->created_at = time(NULL);
    task->timeout_sec = 0;
    task->progress_percent = 0;
    task->heap_index = -1;

    return task;
}

void task_free(AgentTask* task) {
    if (!task) return;

    free(task->id);
    free(task->description);
    free(task->assigned_agent_id);
    free(task->preferred_agent);
    free(task->project_path);
    free(task->input_json);
    free(task->context_json);
    free(task->result_json);
    free(task->error_message);
    free(task->progress_message);

    /* Free dependency array */
    if (task->depends_on) {
        for (int i = 0; i < task->dependency_count; i++) {
            free(task->depends_on[i]);
        }
        free(task->depends_on);
    }

    free(task);
}

void task_set_input(AgentTask* task, const char* json) {
    if (!task) return;
    free(task->input_json);
    task->input_json = json ? strdup(json) : NULL;
}

void task_set_context(AgentTask* task, const char* project_path,
                      ProjectContext* ctx) {
    if (!task) return;
    free(task->project_path);
    task->project_path = project_path ? strdup(project_path) : NULL;
    task->project_ctx = ctx; /* Shared, not copied */
}

bool task_add_dependency(AgentTask* task, const char* dependency_id) {
    if (!task || !dependency_id) return false;

    /* Grow array */
    int new_count = task->dependency_count + 1;
    char** new_deps = (char**)realloc(task->depends_on,
                                      new_count * sizeof(char*));
    if (!new_deps) {
        log_error("Failed to add dependency");
        return false;
    }

    new_deps[task->dependency_count] = strdup(dependency_id);
    task->depends_on = new_deps;
    task->dependency_count = new_count;
    task->dependencies_met = false;

    return true;
}

void task_set_callback(AgentTask* task, AgentTaskCallback callback, void* user_data) {
    if (!task) return;
    task->on_complete = callback;
    task->callback_data = user_data;
}

void task_set_timeout(AgentTask* task, int timeout_sec) {
    if (!task) return;
    task->timeout_sec = timeout_sec;
}

void task_complete(AgentTask* task, const char* result_json) {
    if (!task) return;

    task->state = TASK_STATE_COMPLETED;
    task->completed_at = time(NULL);
    task->exit_code = 0;
    task->progress_percent = 100;

    free(task->result_json);
    task->result_json = result_json ? strdup(result_json) : NULL;

    /* Call completion callback */
    if (task->on_complete) {
        task->on_complete(task, task->callback_data);
    }
}

void task_fail(AgentTask* task, const char* error_message, int exit_code) {
    if (!task) return;

    task->state = TASK_STATE_FAILED;
    task->completed_at = time(NULL);
    task->exit_code = exit_code;

    free(task->error_message);
    task->error_message = error_message ? strdup(error_message) : NULL;

    /* Call error callback */
    if (task->on_error) {
        task->on_error(task, task->callback_data);
    }
}

void task_update_progress(AgentTask* task, int percent, const char* message) {
    if (!task) return;

    task->progress_percent = (percent < 0) ? 0 : (percent > 100) ? 100 : percent;
    free(task->progress_message);
    task->progress_message = message ? strdup(message) : NULL;

    /* Call progress callback */
    if (task->on_progress) {
        task->on_progress(task, task->callback_data);
    }
}

double task_elapsed_time(AgentTask* task) {
    if (!task || task->started_at == 0) return -1.0;

    time_t end = task->completed_at ? task->completed_at : time(NULL);
    return difftime(end, task->started_at);
}

bool task_has_timed_out(AgentTask* task) {
    if (!task || task->timeout_sec <= 0 || task->started_at == 0) {
        return false;
    }

    double elapsed = task_elapsed_time(task);
    return elapsed >= (double)task->timeout_sec;
}

/* ============================================================================
 * Priority Heap Operations
 * ============================================================================ */

static void heap_swap(TaskQueue* queue, size_t i, size_t j) {
    AgentTask* temp = queue->heap[i];
    queue->heap[i] = queue->heap[j];
    queue->heap[j] = temp;

    queue->heap[i]->heap_index = (int)i;
    queue->heap[j]->heap_index = (int)j;
}

static bool heap_compare(AgentTask* a, AgentTask* b) {
    /* Higher priority comes first */
    if (a->priority != b->priority) {
        return a->priority > b->priority;
    }
    /* Same priority: earlier creation time comes first */
    return a->created_at < b->created_at;
}

static void heap_bubble_up(TaskQueue* queue, size_t index) {
    while (index > 0) {
        size_t parent = (index - 1) / 2;
        if (heap_compare(queue->heap[index], queue->heap[parent])) {
            heap_swap(queue, index, parent);
            index = parent;
        } else {
            break;
        }
    }
}

static void heap_bubble_down(TaskQueue* queue, size_t index) {
    while (true) {
        size_t left = 2 * index + 1;
        size_t right = 2 * index + 2;
        size_t largest = index;

        if (left < queue->count &&
            heap_compare(queue->heap[left], queue->heap[largest])) {
            largest = left;
        }

        if (right < queue->count &&
            heap_compare(queue->heap[right], queue->heap[largest])) {
            largest = right;
        }

        if (largest != index) {
            heap_swap(queue, index, largest);
            index = largest;
        } else {
            break;
        }
    }
}

static void heap_remove_at(TaskQueue* queue, size_t index) {
    if (index >= queue->count) return;

    /* Move last element to this position */
    queue->count--;
    if (index < queue->count) {
        queue->heap[index] = queue->heap[queue->count];
        queue->heap[index]->heap_index = (int)index;

        /* Re-heapify */
        if (index > 0 && heap_compare(queue->heap[index],
                                      queue->heap[(index - 1) / 2])) {
            heap_bubble_up(queue, index);
        } else {
            heap_bubble_down(queue, index);
        }
    }
}

/* ============================================================================
 * Task Queue Operations
 * ============================================================================ */

TaskQueue* task_queue_create(void) {
    TaskQueue* queue = (TaskQueue*)calloc(1, sizeof(TaskQueue));
    if (!queue) {
        log_error("Failed to allocate task queue");
        return NULL;
    }

    queue->capacity = 32;
    queue->heap = (AgentTask**)calloc(queue->capacity, sizeof(AgentTask*));
    if (!queue->heap) {
        log_error("Failed to allocate task heap");
        free(queue);
        return NULL;
    }

    if (!mutex_init(&queue->mutex)) {
        log_error("Failed to initialize queue mutex");
        free(queue->heap);
        free(queue);
        return NULL;
    }

    if (!condition_init(&queue->not_empty)) {
        log_error("Failed to initialize queue condition");
        mutex_destroy(&queue->mutex);
        free(queue->heap);
        free(queue);
        return NULL;
    }

    queue->count = 0;
    queue->shutdown = false;

    log_debug("Task queue created");
    return queue;
}

void task_queue_free(TaskQueue* queue) {
    if (!queue) return;

    /* Signal shutdown and clear */
    task_queue_shutdown(queue);
    task_queue_clear(queue);

    condition_destroy(&queue->not_empty);
    mutex_destroy(&queue->mutex);
    free(queue->heap);
    free(queue);

    log_debug("Task queue destroyed");
}

bool task_queue_push(TaskQueue* queue, AgentTask* task) {
    if (!queue || !task) return false;

    mutex_lock(&queue->mutex);

    if (queue->shutdown) {
        mutex_unlock(&queue->mutex);
        return false;
    }

    /* Grow heap if needed */
    if (queue->count >= queue->capacity) {
        size_t new_cap = queue->capacity * 2;
        AgentTask** new_heap = (AgentTask**)realloc(
            queue->heap, new_cap * sizeof(AgentTask*));
        if (!new_heap) {
            log_error("Failed to grow task queue");
            mutex_unlock(&queue->mutex);
            return false;
        }
        queue->heap = new_heap;
        queue->capacity = new_cap;
    }

    /* Add to heap */
    task->heap_index = (int)queue->count;
    queue->heap[queue->count++] = task;
    heap_bubble_up(queue, queue->count - 1);

    /* Signal waiting consumers */
    condition_signal(&queue->not_empty);

    mutex_unlock(&queue->mutex);

    log_debug("Task '%s' pushed to queue (priority: %s)",
              task->id, task_priority_to_string(task->priority));
    return true;
}

AgentTask* task_queue_pop(TaskQueue* queue) {
    if (!queue) return NULL;

    mutex_lock(&queue->mutex);

    while (queue->count == 0 && !queue->shutdown) {
        condition_wait(&queue->not_empty, &queue->mutex);
    }

    if (queue->shutdown && queue->count == 0) {
        mutex_unlock(&queue->mutex);
        return NULL;
    }

    /* Extract highest priority task */
    AgentTask* task = queue->heap[0];
    heap_remove_at(queue, 0);

    mutex_unlock(&queue->mutex);

    log_debug("Task '%s' popped from queue", task->id);
    return task;
}

AgentTask* task_queue_pop_timeout(TaskQueue* queue, int timeout_ms) {
    if (!queue) return NULL;

    mutex_lock(&queue->mutex);

    if (queue->count == 0 && !queue->shutdown) {
        if (!condition_timedwait(&queue->not_empty, &queue->mutex, timeout_ms)) {
            mutex_unlock(&queue->mutex);
            return NULL; /* Timeout */
        }
    }

    if (queue->shutdown || queue->count == 0) {
        mutex_unlock(&queue->mutex);
        return NULL;
    }

    AgentTask* task = queue->heap[0];
    heap_remove_at(queue, 0);

    mutex_unlock(&queue->mutex);
    return task;
}

AgentTask* task_queue_try_pop(TaskQueue* queue) {
    if (!queue) return NULL;

    mutex_lock(&queue->mutex);

    if (queue->count == 0) {
        mutex_unlock(&queue->mutex);
        return NULL;
    }

    AgentTask* task = queue->heap[0];
    heap_remove_at(queue, 0);

    mutex_unlock(&queue->mutex);
    return task;
}

AgentTask* task_queue_pop_for_agent(TaskQueue* queue, AgentInstance* agent) {
    if (!queue || !agent) return NULL;

    mutex_lock(&queue->mutex);

    /* Find highest priority task matching agent capabilities */
    AgentTask* best_task = NULL;
    size_t best_index = 0;

    for (size_t i = 0; i < queue->count; i++) {
        AgentTask* task = queue->heap[i];

        /* Check capability match */
        if (task->required_capabilities &&
            !(agent->capabilities & task->required_capabilities)) {
            continue;
        }

        /* Check preferred agent */
        if (task->preferred_agent &&
            strcmp(task->preferred_agent, agent->name) != 0) {
            continue;
        }

        /* Check dependencies */
        if (!task->dependencies_met && task->dependency_count > 0) {
            continue;
        }

        /* Take first matching (heap guarantees priority order) */
        if (!best_task) {
            best_task = task;
            best_index = i;
            break;
        }
    }

    if (best_task) {
        heap_remove_at(queue, best_index);
    }

    mutex_unlock(&queue->mutex);
    return best_task;
}

AgentTask* task_queue_peek(TaskQueue* queue) {
    if (!queue) return NULL;

    mutex_lock(&queue->mutex);
    AgentTask* task = (queue->count > 0) ? queue->heap[0] : NULL;
    mutex_unlock(&queue->mutex);

    return task;
}

AgentTask* task_queue_get(TaskQueue* queue, const char* task_id) {
    if (!queue || !task_id) return NULL;

    mutex_lock(&queue->mutex);

    AgentTask* found = NULL;
    for (size_t i = 0; i < queue->count; i++) {
        if (strcmp(queue->heap[i]->id, task_id) == 0) {
            found = queue->heap[i];
            break;
        }
    }

    mutex_unlock(&queue->mutex);
    return found;
}

AgentTask* task_queue_remove(TaskQueue* queue, const char* task_id) {
    if (!queue || !task_id) return NULL;

    mutex_lock(&queue->mutex);

    AgentTask* found = NULL;
    for (size_t i = 0; i < queue->count; i++) {
        if (strcmp(queue->heap[i]->id, task_id) == 0) {
            found = queue->heap[i];
            heap_remove_at(queue, i);
            break;
        }
    }

    mutex_unlock(&queue->mutex);
    return found;
}

bool task_queue_cancel(TaskQueue* queue, const char* task_id) {
    AgentTask* task = task_queue_remove(queue, task_id);
    if (task) {
        task->state = TASK_STATE_CANCELLED;
        task->completed_at = time(NULL);
        log_info("Task '%s' cancelled", task_id);
        task_free(task);
        return true;
    }
    return false;
}

size_t task_queue_count(TaskQueue* queue) {
    if (!queue) return 0;

    mutex_lock(&queue->mutex);
    size_t count = queue->count;
    mutex_unlock(&queue->mutex);

    return count;
}

bool task_queue_is_empty(TaskQueue* queue) {
    return task_queue_count(queue) == 0;
}

void task_queue_shutdown(TaskQueue* queue) {
    if (!queue) return;

    mutex_lock(&queue->mutex);
    queue->shutdown = true;
    condition_broadcast(&queue->not_empty);
    mutex_unlock(&queue->mutex);
}

void task_queue_clear(TaskQueue* queue) {
    if (!queue) return;

    mutex_lock(&queue->mutex);

    for (size_t i = 0; i < queue->count; i++) {
        task_free(queue->heap[i]);
    }
    queue->count = 0;

    mutex_unlock(&queue->mutex);
}

/* ============================================================================
 * Dependency Management
 * ============================================================================ */

bool task_dependencies_met(TaskQueue* queue, AgentTask* task) {
    if (!queue || !task) return true;
    if (task->dependency_count == 0) return true;

    mutex_lock(&queue->mutex);

    /* Check if all dependencies are completed (not in queue) */
    for (int i = 0; i < task->dependency_count; i++) {
        bool found = false;
        for (size_t j = 0; j < queue->count; j++) {
            if (strcmp(queue->heap[j]->id, task->depends_on[i]) == 0) {
                /* Dependency still in queue = not completed */
                found = true;
                break;
            }
        }
        if (found) {
            mutex_unlock(&queue->mutex);
            return false;
        }
    }

    mutex_unlock(&queue->mutex);
    return true;
}

void task_queue_update_dependencies(TaskQueue* queue,
                                    const char* completed_task_id) {
    if (!queue || !completed_task_id) return;

    mutex_lock(&queue->mutex);

    /* Update dependency status for all tasks */
    for (size_t i = 0; i < queue->count; i++) {
        AgentTask* task = queue->heap[i];
        if (task->dependency_count > 0 && !task->dependencies_met) {
            /* Check if this was the last dependency */
            bool all_met = true;
            for (int d = 0; d < task->dependency_count; d++) {
                /* Check if dependency is still in queue */
                for (size_t j = 0; j < queue->count; j++) {
                    if (strcmp(queue->heap[j]->id, task->depends_on[d]) == 0) {
                        all_met = false;
                        break;
                    }
                }
                if (!all_met) break;
            }
            task->dependencies_met = all_met;
        }
    }

    mutex_unlock(&queue->mutex);
}

AgentTask** task_queue_get_blocked_by(TaskQueue* queue, const char* task_id,
                                      int* count) {
    if (!queue || !task_id || !count) {
        if (count) *count = 0;
        return NULL;
    }

    mutex_lock(&queue->mutex);

    /* Count blocked tasks */
    int blocked_count = 0;
    for (size_t i = 0; i < queue->count; i++) {
        AgentTask* task = queue->heap[i];
        for (int d = 0; d < task->dependency_count; d++) {
            if (strcmp(task->depends_on[d], task_id) == 0) {
                blocked_count++;
                break;
            }
        }
    }

    if (blocked_count == 0) {
        *count = 0;
        mutex_unlock(&queue->mutex);
        return NULL;
    }

    /* Collect blocked tasks */
    AgentTask** blocked = (AgentTask**)malloc(blocked_count * sizeof(AgentTask*));
    if (!blocked) {
        *count = 0;
        mutex_unlock(&queue->mutex);
        return NULL;
    }

    int idx = 0;
    for (size_t i = 0; i < queue->count; i++) {
        AgentTask* task = queue->heap[i];
        for (int d = 0; d < task->dependency_count; d++) {
            if (strcmp(task->depends_on[d], task_id) == 0) {
                blocked[idx++] = task;
                break;
            }
        }
    }

    *count = blocked_count;
    mutex_unlock(&queue->mutex);
    return blocked;
}
