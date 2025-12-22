/**
 * @file agent_coordinator.c
 * @brief Agent Coordinator implementation
 */

#include "cyxmake/agent_coordinator.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

CoordinatorConfig coordinator_config_defaults(void) {
    CoordinatorConfig config = {
        .default_resolution = RESOLUTION_USER_PROMPT,
        .max_concurrent_agents = 4,
        .task_timeout_sec = 300,
        .verbose = false,
        .prompt_user = NULL
    };
    return config;
}

/* ============================================================================
 * String Conversions
 * ============================================================================ */

const char* conflict_type_to_string(ConflictType type) {
    switch (type) {
        case CONFLICT_NONE:       return "none";
        case CONFLICT_RESOURCE:   return "resource";
        case CONFLICT_DECISION:   return "decision";
        case CONFLICT_DEPENDENCY: return "dependency";
        case CONFLICT_TIMEOUT:    return "timeout";
        default:                  return "unknown";
    }
}

const char* resolution_result_to_string(ResolutionResult result) {
    switch (result) {
        case RESOLUTION_RESULT_AGENT1:  return "agent1_wins";
        case RESOLUTION_RESULT_AGENT2:  return "agent2_wins";
        case RESOLUTION_RESULT_BOTH:    return "both_proceed";
        case RESOLUTION_RESULT_NEITHER: return "both_cancelled";
        case RESOLUTION_RESULT_ERROR:   return "error";
        default:                        return "unknown";
    }
}

/* ============================================================================
 * Coordinator Lifecycle
 * ============================================================================ */

AgentCoordinator* coordinator_create(AgentRegistry* registry,
                                     MessageBus* bus,
                                     SharedState* state,
                                     const CoordinatorConfig* config) {
    if (!registry) {
        log_error("Registry is required for coordinator");
        return NULL;
    }

    AgentCoordinator* coord = (AgentCoordinator*)calloc(1, sizeof(AgentCoordinator));
    if (!coord) {
        log_error("Failed to allocate coordinator");
        return NULL;
    }

    coord->registry = registry;
    coord->message_bus = bus;
    coord->shared_state = state;
    coord->task_queue = NULL;

    if (config) {
        coord->config = *config;
    } else {
        coord->config = coordinator_config_defaults();
    }

    if (!mutex_init(&coord->mutex)) {
        log_error("Failed to initialize coordinator mutex");
        free(coord);
        return NULL;
    }

    /* Initialize conflict tracking */
    coord->conflict_capacity = 8;
    coord->conflicts = (AgentConflict**)calloc(coord->conflict_capacity,
                                               sizeof(AgentConflict*));

    /* Initialize resource tracking */
    coord->resource_capacity = 32;
    coord->locked_resources = (char**)calloc(coord->resource_capacity, sizeof(char*));
    coord->resource_owners = (char**)calloc(coord->resource_capacity, sizeof(char*));

    if (!coord->conflicts || !coord->locked_resources || !coord->resource_owners) {
        log_error("Failed to allocate coordinator arrays");
        mutex_destroy(&coord->mutex);
        free(coord->conflicts);
        free(coord->locked_resources);
        free(coord->resource_owners);
        free(coord);
        return NULL;
    }

    log_debug("Agent coordinator created");
    return coord;
}

void coordinator_free(AgentCoordinator* coord) {
    if (!coord) return;

    mutex_lock(&coord->mutex);

    /* Free conflicts */
    for (int i = 0; i < coord->conflict_count; i++) {
        conflict_free(coord->conflicts[i]);
    }
    free(coord->conflicts);

    /* Free resource tracking */
    for (int i = 0; i < coord->resource_count; i++) {
        free(coord->locked_resources[i]);
        free(coord->resource_owners[i]);
    }
    free(coord->locked_resources);
    free(coord->resource_owners);

    mutex_unlock(&coord->mutex);
    mutex_destroy(&coord->mutex);

    free(coord);
    log_debug("Agent coordinator destroyed");
}

void coordinator_set_task_queue(AgentCoordinator* coord, TaskQueue* queue) {
    if (!coord) return;
    coord->task_queue = queue;
}

/* ============================================================================
 * Task Distribution
 * ============================================================================ */

static AgentInstance* find_best_agent(AgentCoordinator* coord, AgentTask* task) {
    int count;
    AgentInstance** agents = agent_registry_list(coord->registry, &count);
    if (!agents || count == 0) return NULL;

    AgentInstance* best = NULL;
    int best_score = -1;

    for (int i = 0; i < count; i++) {
        AgentInstance* agent = agents[i];

        /* Must be idle */
        if (agent_get_state(agent) != AGENT_STATE_IDLE) {
            continue;
        }

        /* Must have required capabilities */
        if (task->required_capabilities &&
            !(agent->capabilities & task->required_capabilities)) {
            continue;
        }

        /* Check preferred agent */
        if (task->preferred_agent) {
            if (strcmp(agent->name, task->preferred_agent) == 0) {
                return agent; /* Exact match */
            }
        }

        /* Score based on task type match */
        int score = 0;

        switch (task->type) {
            case TASK_TYPE_BUILD:
                if (agent->type == AGENT_TYPE_BUILD) score = 100;
                break;
            case TASK_TYPE_FIX:
            case TASK_TYPE_ANALYZE:
                if (agent->type == AGENT_TYPE_SMART) score = 100;
                else if (agent->type == AGENT_TYPE_BUILD) score = 50;
                break;
            case TASK_TYPE_EXECUTE:
            case TASK_TYPE_MODIFY:
                if (agent->type == AGENT_TYPE_AUTONOMOUS) score = 100;
                break;
            default:
                score = 50; /* Any agent can handle general tasks */
        }

        /* Bonus for fewer completed tasks (load balancing) */
        score -= agent->tasks_completed;

        if (score > best_score) {
            best_score = score;
            best = agent;
        }
    }

    return best;
}

void coordinator_set_distribution(AgentCoordinator* coord,
                                  DistributionStrategy strategy) {
    /* Store for future use - currently using capability-based by default */
    (void)coord;
    (void)strategy;
}

AgentInstance* coordinator_assign_task(AgentCoordinator* coord, AgentTask* task) {
    if (!coord || !task) return NULL;

    mutex_lock(&coord->mutex);

    AgentInstance* agent = find_best_agent(coord, task);
    if (!agent) {
        log_warning("No suitable agent found for task '%s'", task->description);
        mutex_unlock(&coord->mutex);
        return NULL;
    }

    if (!agent_assign_task(agent, task)) {
        mutex_unlock(&coord->mutex);
        return NULL;
    }

    mutex_unlock(&coord->mutex);

    log_info("Task '%s' assigned to agent '%s'", task->id, agent->name);
    return agent;
}

bool coordinator_assign_to(AgentCoordinator* coord, AgentTask* task,
                           const char* agent_name) {
    if (!coord || !task || !agent_name) return false;

    AgentInstance* agent = agent_registry_get(coord->registry, agent_name);
    if (!agent) {
        log_error("Agent '%s' not found", agent_name);
        return false;
    }

    return agent_assign_task(agent, task);
}

bool coordinator_spawn_workers(AgentCoordinator* coord,
                               AgentTask* parent_task,
                               int worker_count) {
    if (!coord || !parent_task || worker_count <= 0) return false;

    log_info("Spawning %d workers for task '%s'", worker_count, parent_task->id);

    for (int i = 0; i < worker_count; i++) {
        char name[64];
        snprintf(name, sizeof(name), "worker_%s_%d", parent_task->id, i + 1);

        AgentInstanceConfig config = agent_config_defaults();
        config.description = parent_task->description;

        AgentInstance* worker = agent_registry_create_agent(
            coord->registry, name, AGENT_TYPE_AUTONOMOUS, &config);

        if (!worker) {
            log_warning("Failed to spawn worker %d", i + 1);
            continue;
        }

        agent_start(worker);
    }

    return true;
}

bool coordinator_wait_all(AgentCoordinator* coord, int timeout_ms) {
    if (!coord) return true;

    int elapsed = 0;
    int interval = 100;

    while (elapsed < timeout_ms || timeout_ms == 0) {
        int running = agent_registry_count_state(coord->registry, AGENT_STATE_RUNNING);
        if (running == 0) {
            return true;
        }

        thread_sleep(interval);
        elapsed += interval;
    }

    return false;
}

/* ============================================================================
 * Resource Management and Conflict Detection
 * ============================================================================ */

static int find_resource(AgentCoordinator* coord, const char* resource_id) {
    for (int i = 0; i < coord->resource_count; i++) {
        if (strcmp(coord->locked_resources[i], resource_id) == 0) {
            return i;
        }
    }
    return -1;
}

bool coordinator_request_resource(AgentCoordinator* coord,
                                  const char* agent_id,
                                  const char* resource_id,
                                  const char* action) {
    if (!coord || !agent_id || !resource_id) return false;

    mutex_lock(&coord->mutex);

    int idx = find_resource(coord, resource_id);

    if (idx >= 0) {
        /* Resource already locked */
        if (strcmp(coord->resource_owners[idx], agent_id) == 0) {
            /* Same agent - OK */
            mutex_unlock(&coord->mutex);
            return true;
        }

        /* Conflict! Create conflict record */
        AgentConflict* conflict = (AgentConflict*)calloc(1, sizeof(AgentConflict));
        if (conflict) {
            conflict->type = CONFLICT_RESOURCE;
            conflict->agent1_id = strdup(coord->resource_owners[idx]);
            conflict->agent2_id = strdup(agent_id);
            conflict->resource_id = strdup(resource_id);
            conflict->agent2_action = action ? strdup(action) : NULL;
            conflict->detected_at = time(NULL);

            /* Get agent names */
            AgentInstance* a1 = agent_registry_get(coord->registry, conflict->agent1_id);
            AgentInstance* a2 = agent_registry_get(coord->registry, conflict->agent2_id);
            if (a1) conflict->agent1_name = strdup(a1->name);
            if (a2) conflict->agent2_name = strdup(a2->name);

            /* Store conflict */
            if (coord->conflict_count >= coord->conflict_capacity) {
                int new_cap = coord->conflict_capacity * 2;
                AgentConflict** new_arr = (AgentConflict**)realloc(
                    coord->conflicts, new_cap * sizeof(AgentConflict*));
                if (new_arr) {
                    coord->conflicts = new_arr;
                    coord->conflict_capacity = new_cap;
                }
            }
            coord->conflicts[coord->conflict_count++] = conflict;

            log_warning("Resource conflict: '%s' and '%s' both want '%s'",
                       conflict->agent1_name, conflict->agent2_name, resource_id);
        }

        mutex_unlock(&coord->mutex);
        return false;
    }

    /* Lock the resource */
    if (coord->resource_count >= coord->resource_capacity) {
        int new_cap = coord->resource_capacity * 2;
        char** new_res = (char**)realloc(coord->locked_resources,
                                         new_cap * sizeof(char*));
        char** new_own = (char**)realloc(coord->resource_owners,
                                         new_cap * sizeof(char*));
        if (new_res && new_own) {
            coord->locked_resources = new_res;
            coord->resource_owners = new_own;
            coord->resource_capacity = new_cap;
        } else {
            mutex_unlock(&coord->mutex);
            return false;
        }
    }

    coord->locked_resources[coord->resource_count] = strdup(resource_id);
    coord->resource_owners[coord->resource_count] = strdup(agent_id);
    coord->resource_count++;

    mutex_unlock(&coord->mutex);
    return true;
}

void coordinator_release_resource(AgentCoordinator* coord,
                                  const char* agent_id,
                                  const char* resource_id) {
    if (!coord || !agent_id || !resource_id) return;

    mutex_lock(&coord->mutex);

    int idx = find_resource(coord, resource_id);
    if (idx >= 0 && strcmp(coord->resource_owners[idx], agent_id) == 0) {
        free(coord->locked_resources[idx]);
        free(coord->resource_owners[idx]);

        /* Move last element to this slot */
        coord->resource_count--;
        if (idx < coord->resource_count) {
            coord->locked_resources[idx] = coord->locked_resources[coord->resource_count];
            coord->resource_owners[idx] = coord->resource_owners[coord->resource_count];
        }
    }

    mutex_unlock(&coord->mutex);
}

AgentConflict* coordinator_detect_conflict(AgentCoordinator* coord) {
    if (!coord) return NULL;

    mutex_lock(&coord->mutex);

    /* Return first unresolved conflict */
    for (int i = 0; i < coord->conflict_count; i++) {
        if (coord->conflicts[i]->resolved_at == 0) {
            AgentConflict* conflict = coord->conflicts[i];
            mutex_unlock(&coord->mutex);
            return conflict;
        }
    }

    mutex_unlock(&coord->mutex);
    return NULL;
}

ResolutionResult coordinator_resolve_conflict(AgentCoordinator* coord,
                                              AgentConflict* conflict) {
    if (!coord || !conflict) return RESOLUTION_RESULT_ERROR;

    char message[512];
    snprintf(message, sizeof(message),
             "Conflict: Agents '%s' and '%s' both want to access '%s'.\n"
             "  '%s': %s\n"
             "  '%s': %s\n"
             "Which should proceed?",
             conflict->agent1_name ? conflict->agent1_name : conflict->agent1_id,
             conflict->agent2_name ? conflict->agent2_name : conflict->agent2_id,
             conflict->resource_id,
             conflict->agent1_name ? conflict->agent1_name : "Agent 1",
             conflict->agent1_action ? conflict->agent1_action : "(unknown action)",
             conflict->agent2_name ? conflict->agent2_name : "Agent 2",
             conflict->agent2_action ? conflict->agent2_action : "(unknown action)");

    ResolutionResult result = RESOLUTION_RESULT_AGENT1;

    if (coord->config.prompt_user) {
        const char* options[] = {
            conflict->agent1_name ? conflict->agent1_name : "Agent 1",
            conflict->agent2_name ? conflict->agent2_name : "Agent 2",
            "Both (sequential)",
            "Cancel both"
        };

        int choice = coord->config.prompt_user(conflict, message, options, 4);

        switch (choice) {
            case 0: result = RESOLUTION_RESULT_AGENT1; break;
            case 1: result = RESOLUTION_RESULT_AGENT2; break;
            case 2: result = RESOLUTION_RESULT_BOTH; break;
            case 3: result = RESOLUTION_RESULT_NEITHER; break;
            default: result = RESOLUTION_RESULT_ERROR; break;
        }
    } else {
        /* No user prompt available - use default strategy */
        log_warning("%s", message);
        log_info("Defaulting to first agent (no user prompt configured)");
        result = RESOLUTION_RESULT_AGENT1;
    }

    conflict->resolution = result;
    conflict->resolved_at = time(NULL);

    /* Apply resolution */
    mutex_lock(&coord->mutex);

    if (result == RESOLUTION_RESULT_AGENT2 || result == RESOLUTION_RESULT_NEITHER) {
        /* Release resource from agent1 */
        int idx = find_resource(coord, conflict->resource_id);
        if (idx >= 0) {
            free(coord->locked_resources[idx]);
            free(coord->resource_owners[idx]);
            coord->resource_count--;
            if (idx < coord->resource_count) {
                coord->locked_resources[idx] = coord->locked_resources[coord->resource_count];
                coord->resource_owners[idx] = coord->resource_owners[coord->resource_count];
            }
        }
    }

    if (result == RESOLUTION_RESULT_AGENT2) {
        /* Grant resource to agent2 */
        if (coord->resource_count < coord->resource_capacity) {
            coord->locked_resources[coord->resource_count] = strdup(conflict->resource_id);
            coord->resource_owners[coord->resource_count] = strdup(conflict->agent2_id);
            coord->resource_count++;
        }
    }

    mutex_unlock(&coord->mutex);

    log_info("Conflict resolved: %s", resolution_result_to_string(result));
    return result;
}

void conflict_free(AgentConflict* conflict) {
    if (!conflict) return;

    free(conflict->agent1_id);
    free(conflict->agent1_name);
    free(conflict->agent2_id);
    free(conflict->agent2_name);
    free(conflict->resource_id);
    free(conflict->resource_type);
    free(conflict->agent1_action);
    free(conflict->agent2_action);
    free(conflict->resolution_reason);
    free(conflict);
}

/* ============================================================================
 * Result Aggregation
 * ============================================================================ */

AggregatedResult* coordinator_aggregate_results(AgentCoordinator* coord,
                                                AgentInstance** agents,
                                                int count) {
    if (!coord || !agents || count <= 0) return NULL;

    AggregatedResult* result = (AggregatedResult*)calloc(1, sizeof(AggregatedResult));
    if (!result) return NULL;

    result->individual_outputs = (char**)calloc(count, sizeof(char*));
    result->agent_names = (char**)calloc(count, sizeof(char*));

    if (!result->individual_outputs || !result->agent_names) {
        free(result->individual_outputs);
        free(result->agent_names);
        free(result);
        return NULL;
    }

    result->output_count = count;
    result->all_succeeded = true;

    size_t combined_len = 0;

    for (int i = 0; i < count; i++) {
        AgentInstance* agent = agents[i];
        result->agent_names[i] = agent->name ? strdup(agent->name) : NULL;

        AgentState state = agent_get_state(agent);

        if (state == AGENT_STATE_COMPLETED) {
            result->success_count++;
            const char* out = agent_get_result(agent);
            if (out) {
                result->individual_outputs[i] = strdup(out);
                combined_len += strlen(out) + 50; /* Extra for formatting */
            }
        } else if (state == AGENT_STATE_ERROR || state == AGENT_STATE_TERMINATED) {
            result->failure_count++;
            result->all_succeeded = false;

            const char* err = agent_get_error(agent);
            if (err && !result->first_error) {
                result->first_error = strdup(err);
            }
        } else {
            result->timeout_count++;
            result->all_succeeded = false;
        }

        result->total_duration_sec += agent->total_runtime_sec;
    }

    /* Build combined output */
    if (combined_len > 0) {
        result->combined_output = (char*)malloc(combined_len);
        if (result->combined_output) {
            result->combined_output[0] = '\0';
            for (int i = 0; i < count; i++) {
                if (result->individual_outputs[i]) {
                    if (result->combined_output[0] != '\0') {
                        strcat(result->combined_output, "\n---\n");
                    }
                    char header[128];
                    snprintf(header, sizeof(header), "[%s]:\n",
                             result->agent_names[i] ? result->agent_names[i] : "Agent");
                    strcat(result->combined_output, header);
                    strcat(result->combined_output, result->individual_outputs[i]);
                }
            }
        }
    }

    return result;
}

void aggregated_result_free(AggregatedResult* result) {
    if (!result) return;

    free(result->combined_output);
    free(result->first_error);

    for (int i = 0; i < result->output_count; i++) {
        free(result->individual_outputs[i]);
        free(result->agent_names[i]);
    }
    free(result->individual_outputs);
    free(result->agent_names);

    free(result);
}

/* ============================================================================
 * Status Reporting
 * ============================================================================ */

char* coordinator_status_report(AgentCoordinator* coord) {
    if (!coord) return NULL;

    int count;
    AgentInstance** agents = agent_registry_list(coord->registry, &count);

    size_t buf_size = 2048;
    char* report = (char*)malloc(buf_size);
    if (!report) return NULL;

    int pos = 0;
    pos += snprintf(report + pos, buf_size - pos,
                    "=== Agent Status ===\n"
                    "Total agents: %d\n\n", count);

    pos += snprintf(report + pos, buf_size - pos,
                    "%-15s %-12s %-10s %-10s\n",
                    "NAME", "TYPE", "STATE", "TASKS");
    pos += snprintf(report + pos, buf_size - pos,
                    "-----------------------------------------------\n");

    for (int i = 0; i < count && pos < (int)buf_size - 100; i++) {
        AgentInstance* agent = agents[i];
        pos += snprintf(report + pos, buf_size - pos,
                        "%-15s %-12s %-10s %d/%d\n",
                        agent->name,
                        agent_type_to_string(agent->type),
                        agent_state_to_string(agent_get_state(agent)),
                        agent->tasks_completed,
                        agent->tasks_completed + agent->tasks_failed);
    }

    /* Add conflict summary */
    mutex_lock(&coord->mutex);
    int unresolved = 0;
    for (int i = 0; i < coord->conflict_count; i++) {
        if (coord->conflicts[i]->resolved_at == 0) unresolved++;
    }
    mutex_unlock(&coord->mutex);

    if (unresolved > 0) {
        pos += snprintf(report + pos, buf_size - pos,
                        "\nUnresolved conflicts: %d\n", unresolved);
    }

    return report;
}

char* coordinator_conflict_report(AgentCoordinator* coord) {
    if (!coord) return NULL;

    mutex_lock(&coord->mutex);

    if (coord->conflict_count == 0) {
        mutex_unlock(&coord->mutex);
        return strdup("No conflicts recorded.");
    }

    size_t buf_size = 4096;
    char* report = (char*)malloc(buf_size);
    if (!report) {
        mutex_unlock(&coord->mutex);
        return NULL;
    }

    int pos = 0;
    pos += snprintf(report + pos, buf_size - pos,
                    "=== Conflict History ===\n\n");

    for (int i = 0; i < coord->conflict_count && pos < (int)buf_size - 200; i++) {
        AgentConflict* c = coord->conflicts[i];
        pos += snprintf(report + pos, buf_size - pos,
                        "Conflict %d: %s\n"
                        "  Agents: '%s' vs '%s'\n"
                        "  Resource: %s\n"
                        "  Status: %s\n\n",
                        i + 1,
                        conflict_type_to_string(c->type),
                        c->agent1_name ? c->agent1_name : c->agent1_id,
                        c->agent2_name ? c->agent2_name : c->agent2_id,
                        c->resource_id,
                        c->resolved_at ? resolution_result_to_string(c->resolution)
                                       : "UNRESOLVED");
    }

    mutex_unlock(&coord->mutex);
    return report;
}
