/**
 * @file agent_registry.c
 * @brief Agent Registry implementation
 *
 * Provides named agent management with lifecycle control and threading.
 */

#include "cyxmake/agent_registry.h"
#include "cyxmake/agent_comm.h"
#include "cyxmake/task_queue.h"
#include "cyxmake/smart_agent.h"
#include "cyxmake/autonomous_agent.h"
#include "cyxmake/ai_build_agent.h"
#include "cyxmake/ai_provider.h"
#include "cyxmake/tool_executor.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef CYXMAKE_WINDOWS
    #include <windows.h>
    #include <rpc.h>
    #pragma comment(lib, "rpcrt4.lib")
#else
    #include <uuid/uuid.h>
#endif

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

AgentInstanceConfig agent_config_defaults(void) {
    AgentInstanceConfig config = {
        .timeout_sec = 300,
        .verbose = false,
        .auto_start = false,
        .max_retries = 3,
        .capabilities = AGENT_CAP_ALL,
        .read_only = false,
        .temperature = 0.7f,
        .max_tokens = 4096,
        .max_iterations = 20,
        .mock_mode = false,
        .description = NULL,
        .focus = NULL
    };
    return config;
}

/* ============================================================================
 * UUID Generation
 * ============================================================================ */

char* agent_generate_id(void) {
    char* id = (char*)malloc(40);
    if (!id) return NULL;

#ifdef CYXMAKE_WINDOWS
    UUID uuid;
    if (UuidCreate(&uuid) == RPC_S_OK) {
        unsigned char* str = NULL;
        if (UuidToStringA(&uuid, &str) == RPC_S_OK) {
            strncpy(id, (char*)str, 39);
            id[39] = '\0';
            RpcStringFreeA(&str);
            return id;
        }
    }
    /* Fallback: use timestamp + random */
    snprintf(id, 40, "agent-%08x-%04x",
             (unsigned int)time(NULL), (unsigned int)(rand() & 0xFFFF));
#else
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, id);
#endif

    return id;
}

/* ============================================================================
 * Type/State String Conversion
 * ============================================================================ */

const char* agent_type_to_string(AgentType type) {
    switch (type) {
        case AGENT_TYPE_SMART:       return "smart";
        case AGENT_TYPE_AUTONOMOUS:  return "autonomous";
        case AGENT_TYPE_BUILD:       return "build";
        case AGENT_TYPE_COORDINATOR: return "coordinator";
        case AGENT_TYPE_CUSTOM:      return "custom";
        default:                     return "unknown";
    }
}

const char* agent_state_to_string(AgentState state) {
    switch (state) {
        case AGENT_STATE_CREATED:      return "created";
        case AGENT_STATE_INITIALIZING: return "initializing";
        case AGENT_STATE_IDLE:         return "idle";
        case AGENT_STATE_RUNNING:      return "running";
        case AGENT_STATE_PAUSED:       return "paused";
        case AGENT_STATE_COMPLETING:   return "completing";
        case AGENT_STATE_COMPLETED:    return "completed";
        case AGENT_STATE_TERMINATED:   return "terminated";
        case AGENT_STATE_ERROR:        return "error";
        default:                       return "unknown";
    }
}

bool agent_type_from_string(const char* str, AgentType* type) {
    if (!str || !type) return false;

    if (strcmp(str, "smart") == 0) {
        *type = AGENT_TYPE_SMART;
    } else if (strcmp(str, "autonomous") == 0 || strcmp(str, "auto") == 0) {
        *type = AGENT_TYPE_AUTONOMOUS;
    } else if (strcmp(str, "build") == 0) {
        *type = AGENT_TYPE_BUILD;
    } else if (strcmp(str, "coordinator") == 0 || strcmp(str, "coord") == 0) {
        *type = AGENT_TYPE_COORDINATOR;
    } else if (strcmp(str, "custom") == 0) {
        *type = AGENT_TYPE_CUSTOM;
    } else {
        return false;
    }
    return true;
}

unsigned int agent_default_capabilities(AgentType type) {
    switch (type) {
        case AGENT_TYPE_SMART:
            return AGENT_CAP_REASON | AGENT_CAP_ANALYZE | AGENT_CAP_FIX_ERRORS;

        case AGENT_TYPE_AUTONOMOUS:
            return AGENT_CAP_READ_FILES | AGENT_CAP_WRITE_FILES |
                   AGENT_CAP_EXECUTE | AGENT_CAP_ANALYZE | AGENT_CAP_REASON;

        case AGENT_TYPE_BUILD:
            return AGENT_CAP_BUILD | AGENT_CAP_FIX_ERRORS | AGENT_CAP_INSTALL_DEPS |
                   AGENT_CAP_EXECUTE | AGENT_CAP_WRITE_FILES;

        case AGENT_TYPE_COORDINATOR:
            return AGENT_CAP_SPAWN | AGENT_CAP_REASON | AGENT_CAP_ANALYZE;

        default:
            return AGENT_CAP_NONE;
    }
}

/* ============================================================================
 * Registry Lifecycle
 * ============================================================================ */

AgentRegistry* agent_registry_create(AIProvider* ai, ToolRegistry* tools,
                                     ThreadPool* thread_pool) {
    AgentRegistry* registry = (AgentRegistry*)calloc(1, sizeof(AgentRegistry));
    if (!registry) {
        log_error("Failed to allocate agent registry");
        return NULL;
    }

    registry->agent_capacity = 16;
    registry->agents = (AgentInstance**)calloc(registry->agent_capacity,
                                               sizeof(AgentInstance*));
    if (!registry->agents) {
        log_error("Failed to allocate agent array");
        free(registry);
        return NULL;
    }

    if (!mutex_init(&registry->registry_mutex)) {
        log_error("Failed to initialize registry mutex");
        free(registry->agents);
        free(registry);
        return NULL;
    }

    registry->agent_count = 0;
    registry->default_ai = ai;
    registry->tools = tools;
    registry->thread_pool = thread_pool;
    registry->max_concurrent = 4;
    registry->default_timeout = 300;
    registry->shared_memory_path = NULL;

    log_debug("Agent registry created");
    return registry;
}

void agent_registry_free(AgentRegistry* registry) {
    if (!registry) return;

    log_debug("Freeing agent registry with %zu agents...", registry->agent_count);

    /* Lock and copy agent pointers to a local array */
    mutex_lock(&registry->registry_mutex);

    size_t count = registry->agent_count;
    registry->agent_count = 0;

    /* Copy agent pointers to local array before freeing to avoid
     * potential issues with registry access during agent cleanup */
    AgentInstance** agents_to_free = NULL;
    if (count > 0) {
        agents_to_free = (AgentInstance**)malloc(count * sizeof(AgentInstance*));
        if (agents_to_free) {
            for (size_t i = 0; i < count; i++) {
                agents_to_free[i] = registry->agents[i];
                registry->agents[i] = NULL;
            }
        }
    }

    mutex_unlock(&registry->registry_mutex);

    /* Now free agents outside the lock */
    if (agents_to_free) {
        for (size_t i = 0; i < count; i++) {
            AgentInstance* agent = agents_to_free[i];
            if (agent) {
                log_debug("Freeing agent '%s'...", agent->name ? agent->name : "(null)");
                agent_instance_free(agent);
            }
        }
        free(agents_to_free);
    }

    /* Final cleanup */
    mutex_destroy(&registry->registry_mutex);
    free(registry->agents);
    free(registry->shared_memory_path);
    free(registry);

    log_debug("Agent registry destroyed");
}

void agent_registry_set_memory_path(AgentRegistry* registry, const char* path) {
    if (!registry) return;

    mutex_lock(&registry->registry_mutex);
    free(registry->shared_memory_path);
    registry->shared_memory_path = path ? strdup(path) : NULL;
    mutex_unlock(&registry->registry_mutex);
}

void agent_registry_set_shared_state(AgentRegistry* registry, SharedState* state) {
    if (!registry) return;

    mutex_lock(&registry->registry_mutex);
    registry->shared_state = state;
    mutex_unlock(&registry->registry_mutex);

    log_debug("Shared state set for agent registry");
}

/* ============================================================================
 * Agent Instance Creation
 * ============================================================================ */

static bool init_agent_impl(AgentInstance* agent, AIProvider* ai,
                            ToolRegistry* tools) {
    switch (agent->type) {
        case AGENT_TYPE_SMART:
            agent->impl.smart = smart_agent_create(ai, tools);
            if (!agent->impl.smart) {
                log_error("Failed to create SmartAgent");
                return false;
            }
            agent->impl.smart->verbose = agent->config.verbose;
            break;

        case AGENT_TYPE_AUTONOMOUS: {
            AgentConfig config = agent_config_default();
            config.max_iterations = agent->config.max_iterations;
            config.max_tokens = agent->config.max_tokens;
            config.temperature = agent->config.temperature;
            config.verbose = agent->config.verbose;

            agent->impl.autonomous = agent_create(ai, &config);
            if (!agent->impl.autonomous) {
                log_error("Failed to create AutonomousAgent");
                return false;
            }
            agent_register_builtin_tools(agent->impl.autonomous);
            break;
        }

        case AGENT_TYPE_BUILD: {
            AIBuildAgentConfig config = ai_build_agent_config_default();
            config.verbose = agent->config.verbose;
            config.temperature = agent->config.temperature;

            agent->impl.build = ai_build_agent_create(ai, tools, &config);
            if (!agent->impl.build) {
                log_error("Failed to create AIBuildAgent");
                return false;
            }
            break;
        }

        case AGENT_TYPE_COORDINATOR:
            /* Coordinator doesn't have a dedicated impl, uses registry */
            agent->impl.custom = NULL;
            break;

        case AGENT_TYPE_CUSTOM:
            agent->impl.custom = NULL;
            break;

        default:
            log_error("Unknown agent type: %d", agent->type);
            return false;
    }

    return true;
}

AgentInstance* agent_registry_create_agent(AgentRegistry* registry,
                                           const char* name,
                                           AgentType type,
                                           const AgentInstanceConfig* config) {
    if (!registry || !name) {
        log_error("Invalid parameters for agent creation");
        return NULL;
    }

    mutex_lock(&registry->registry_mutex);

    /* Check for duplicate name */
    for (size_t i = 0; i < registry->agent_count; i++) {
        if (registry->agents[i] && strcmp(registry->agents[i]->name, name) == 0) {
            log_error("Agent with name '%s' already exists", name);
            mutex_unlock(&registry->registry_mutex);
            return NULL;
        }
    }

    /* Check concurrent limit */
    int running = agent_registry_count_state(registry, AGENT_STATE_RUNNING);
    if (running >= registry->max_concurrent) {
        log_warning("Maximum concurrent agents (%d) reached", registry->max_concurrent);
        /* Still allow creation, just won't start immediately */
    }

    /* Allocate agent */
    AgentInstance* agent = (AgentInstance*)calloc(1, sizeof(AgentInstance));
    if (!agent) {
        log_error("Failed to allocate agent instance");
        mutex_unlock(&registry->registry_mutex);
        return NULL;
    }

    /* Initialize identity */
    agent->id = agent_generate_id();
    agent->name = strdup(name);
    agent->type = type;
    agent->state = AGENT_STATE_CREATED;

    /* Apply configuration */
    if (config) {
        agent->config = *config;
        if (config->description) {
            agent->description = strdup(config->description);
        }
    } else {
        agent->config = agent_config_defaults();
    }

    /* Set capabilities */
    agent->capabilities = config ? config->capabilities
                                 : agent_default_capabilities(type);

    /* Initialize mutex */
    if (!mutex_init(&agent->state_mutex)) {
        log_error("Failed to initialize agent mutex");
        free(agent->id);
        free(agent->name);
        free(agent);
        mutex_unlock(&registry->registry_mutex);
        return NULL;
    }

    /* Initialize underlying agent implementation */
    if (!init_agent_impl(agent, registry->default_ai, registry->tools)) {
        mutex_destroy(&agent->state_mutex);
        free(agent->id);
        free(agent->name);
        free(agent);
        mutex_unlock(&registry->registry_mutex);
        return NULL;
    }

    /* Set timestamps */
    agent->created_at = time(NULL);
    agent->thread_active = false;

    /* Initialize child tracking */
    agent->parent = NULL;
    agent->children = NULL;
    agent->child_count = 0;
    agent->child_capacity = 0;

    /* Set registry back-reference */
    agent->registry = registry;

    /* Grow array if needed */
    if (registry->agent_count >= registry->agent_capacity) {
        size_t new_cap = registry->agent_capacity * 2;
        AgentInstance** new_agents = (AgentInstance**)realloc(
            registry->agents, new_cap * sizeof(AgentInstance*));
        if (!new_agents) {
            log_error("Failed to grow agent array");
            agent_instance_free(agent);
            mutex_unlock(&registry->registry_mutex);
            return NULL;
        }
        registry->agents = new_agents;
        registry->agent_capacity = new_cap;
    }

    /* Add to registry */
    registry->agents[registry->agent_count++] = agent;

    /* Transition to IDLE state */
    agent->state = AGENT_STATE_IDLE;

    mutex_unlock(&registry->registry_mutex);

    log_info("Created agent '%s' (type: %s, id: %s)",
             name, agent_type_to_string(type), agent->id);

    /* Auto-start if configured */
    if (agent->config.auto_start) {
        agent_start(agent);
    }

    return agent;
}

bool agent_registry_remove(AgentRegistry* registry, const char* name_or_id) {
    if (!registry || !name_or_id) return false;

    mutex_lock(&registry->registry_mutex);

    for (size_t i = 0; i < registry->agent_count; i++) {
        AgentInstance* agent = registry->agents[i];
        if (agent && (strcmp(agent->name, name_or_id) == 0 ||
                      strcmp(agent->id, name_or_id) == 0)) {

            /* Terminate if running */
            AgentState state = agent_get_state(agent);
            if (state == AGENT_STATE_RUNNING || state == AGENT_STATE_PAUSED) {
                agent_terminate(agent);
            }

            /* Remove from array */
            agent_instance_free(agent);
            registry->agents[i] = registry->agents[--registry->agent_count];
            registry->agents[registry->agent_count] = NULL;

            mutex_unlock(&registry->registry_mutex);
            log_info("Removed agent '%s'", name_or_id);
            return true;
        }
    }

    mutex_unlock(&registry->registry_mutex);
    log_warning("Agent '%s' not found", name_or_id);
    return false;
}

AgentInstance* agent_registry_get(AgentRegistry* registry, const char* name_or_id) {
    if (!registry || !name_or_id) return NULL;

    mutex_lock(&registry->registry_mutex);

    for (size_t i = 0; i < registry->agent_count; i++) {
        AgentInstance* agent = registry->agents[i];
        if (agent && (strcmp(agent->name, name_or_id) == 0 ||
                      strcmp(agent->id, name_or_id) == 0)) {
            mutex_unlock(&registry->registry_mutex);
            return agent;
        }
    }

    mutex_unlock(&registry->registry_mutex);
    return NULL;
}

AgentInstance** agent_registry_list(AgentRegistry* registry, int* count) {
    if (!registry || !count) {
        if (count) *count = 0;
        return NULL;
    }

    mutex_lock(&registry->registry_mutex);
    *count = (int)registry->agent_count;
    mutex_unlock(&registry->registry_mutex);

    return registry->agents;
}

AgentInstance** agent_registry_get_by_type(AgentRegistry* registry,
                                           AgentType type, int* count) {
    if (!registry || !count) {
        if (count) *count = 0;
        return NULL;
    }

    mutex_lock(&registry->registry_mutex);

    /* Count matching agents */
    int matching = 0;
    for (size_t i = 0; i < registry->agent_count; i++) {
        if (registry->agents[i] && registry->agents[i]->type == type) {
            matching++;
        }
    }

    if (matching == 0) {
        *count = 0;
        mutex_unlock(&registry->registry_mutex);
        return NULL;
    }

    /* Allocate result array */
    AgentInstance** result = (AgentInstance**)malloc(matching * sizeof(AgentInstance*));
    if (!result) {
        *count = 0;
        mutex_unlock(&registry->registry_mutex);
        return NULL;
    }

    /* Fill result */
    int idx = 0;
    for (size_t i = 0; i < registry->agent_count; i++) {
        if (registry->agents[i] && registry->agents[i]->type == type) {
            result[idx++] = registry->agents[i];
        }
    }

    *count = matching;
    mutex_unlock(&registry->registry_mutex);
    return result;
}

AgentInstance** agent_registry_get_by_state(AgentRegistry* registry,
                                            AgentState state, int* count) {
    if (!registry || !count) {
        if (count) *count = 0;
        return NULL;
    }

    mutex_lock(&registry->registry_mutex);

    /* Count matching agents */
    int matching = 0;
    for (size_t i = 0; i < registry->agent_count; i++) {
        if (registry->agents[i] && agent_get_state(registry->agents[i]) == state) {
            matching++;
        }
    }

    if (matching == 0) {
        *count = 0;
        mutex_unlock(&registry->registry_mutex);
        return NULL;
    }

    /* Allocate and fill result */
    AgentInstance** result = (AgentInstance**)malloc(matching * sizeof(AgentInstance*));
    if (!result) {
        *count = 0;
        mutex_unlock(&registry->registry_mutex);
        return NULL;
    }

    int idx = 0;
    for (size_t i = 0; i < registry->agent_count; i++) {
        if (registry->agents[i] && agent_get_state(registry->agents[i]) == state) {
            result[idx++] = registry->agents[i];
        }
    }

    *count = matching;
    mutex_unlock(&registry->registry_mutex);
    return result;
}

AgentInstance** agent_registry_get_by_capability(AgentRegistry* registry,
                                                  AgentCapability capability,
                                                  int* count) {
    if (!registry || !count) {
        if (count) *count = 0;
        return NULL;
    }

    mutex_lock(&registry->registry_mutex);

    /* Count matching agents */
    int matching = 0;
    for (size_t i = 0; i < registry->agent_count; i++) {
        if (registry->agents[i] &&
            (registry->agents[i]->capabilities & capability)) {
            matching++;
        }
    }

    if (matching == 0) {
        *count = 0;
        mutex_unlock(&registry->registry_mutex);
        return NULL;
    }

    /* Allocate and fill result */
    AgentInstance** result = (AgentInstance**)malloc(matching * sizeof(AgentInstance*));
    if (!result) {
        *count = 0;
        mutex_unlock(&registry->registry_mutex);
        return NULL;
    }

    int idx = 0;
    for (size_t i = 0; i < registry->agent_count; i++) {
        if (registry->agents[i] &&
            (registry->agents[i]->capabilities & capability)) {
            result[idx++] = registry->agents[i];
        }
    }

    *count = matching;
    mutex_unlock(&registry->registry_mutex);
    return result;
}

int agent_registry_count_state(AgentRegistry* registry, AgentState state) {
    if (!registry) return 0;

    /* Note: Caller should hold registry_mutex if needed */
    int count = 0;
    for (size_t i = 0; i < registry->agent_count; i++) {
        if (registry->agents[i] && agent_get_state(registry->agents[i]) == state) {
            count++;
        }
    }
    return count;
}

/* ============================================================================
 * Agent Lifecycle Control
 * ============================================================================ */

AgentState agent_get_state(AgentInstance* agent) {
    if (!agent) return AGENT_STATE_ERROR;

    mutex_lock(&agent->state_mutex);
    AgentState state = agent->state;
    mutex_unlock(&agent->state_mutex);

    return state;
}

void agent_set_state(AgentInstance* agent, AgentState state) {
    if (!agent) return;

    mutex_lock(&agent->state_mutex);
    agent->state = state;
    mutex_unlock(&agent->state_mutex);
}

bool agent_start(AgentInstance* agent) {
    if (!agent) return false;

    AgentState current = agent_get_state(agent);
    if (current != AGENT_STATE_IDLE && current != AGENT_STATE_CREATED) {
        log_warning("Cannot start agent '%s' from state '%s'",
                   agent->name, agent_state_to_string(current));
        return false;
    }

    agent_set_state(agent, AGENT_STATE_INITIALIZING);
    agent->started_at = time(NULL);
    agent_set_state(agent, AGENT_STATE_IDLE);

    log_debug("Agent '%s' started", agent->name);
    return true;
}

bool agent_pause(AgentInstance* agent) {
    if (!agent) return false;

    AgentState current = agent_get_state(agent);
    if (current != AGENT_STATE_RUNNING) {
        log_warning("Cannot pause agent '%s' - not running", agent->name);
        return false;
    }

    agent_set_state(agent, AGENT_STATE_PAUSED);
    log_debug("Agent '%s' paused", agent->name);
    return true;
}

bool agent_resume(AgentInstance* agent) {
    if (!agent) return false;

    AgentState current = agent_get_state(agent);
    if (current != AGENT_STATE_PAUSED) {
        log_warning("Cannot resume agent '%s' - not paused", agent->name);
        return false;
    }

    agent_set_state(agent, AGENT_STATE_RUNNING);
    log_debug("Agent '%s' resumed", agent->name);
    return true;
}

bool agent_terminate(AgentInstance* agent) {
    if (!agent) return false;

    AgentState current = agent_get_state(agent);
    if (current == AGENT_STATE_TERMINATED || current == AGENT_STATE_COMPLETED) {
        return true; /* Already done */
    }

    /* Terminate children first */
    agent_terminate_children(agent);

    agent_set_state(agent, AGENT_STATE_TERMINATED);
    agent->completed_at = time(NULL);

    /* TODO: If thread is active, signal it to stop */
    if (agent->thread_active) {
        /* Wait briefly for thread to notice termination */
        thread_sleep(100);
    }

    log_info("Agent '%s' terminated", agent->name);
    return true;
}

bool agent_wait(AgentInstance* agent, int timeout_ms) {
    if (!agent) return false;

    int elapsed = 0;
    int interval = 50; /* Check every 50ms */

    /* Wait for current task to complete (thread becomes inactive)
     * or for agent to reach a terminal state */
    while (1) {
        /* Check terminal states first */
        if (agent_is_finished(agent)) {
            return true;
        }

        /* Check if agent is running - if not running and not finished,
         * there's no task to wait for */
        AgentState state = agent_get_state(agent);
        if (state == AGENT_STATE_IDLE) {
            /* Agent is idle - check if there's an active thread */
            mutex_lock(&agent->state_mutex);
            bool active = agent->thread_active;
            mutex_unlock(&agent->state_mutex);

            if (!active) {
                /* No active task, agent is idle - we're done waiting */
                return true;
            }
        }

        thread_sleep(interval);
        elapsed += interval;

        if (timeout_ms > 0 && elapsed >= timeout_ms) {
            return false; /* Timeout */
        }
    }

    return true;
}

bool agent_is_finished(AgentInstance* agent) {
    if (!agent) return true;

    AgentState state = agent_get_state(agent);
    return (state == AGENT_STATE_COMPLETED ||
            state == AGENT_STATE_TERMINATED ||
            state == AGENT_STATE_ERROR);
}

/* ============================================================================
 * Task Assignment and Execution
 * ============================================================================ */

bool agent_assign_task(AgentInstance* agent, AgentTask* task) {
    if (!agent || !task) return false;

    AgentState current = agent_get_state(agent);
    if (current != AGENT_STATE_IDLE) {
        log_warning("Cannot assign task to agent '%s' - not idle (state: %s)",
                   agent->name, agent_state_to_string(current));
        return false;
    }

    mutex_lock(&agent->state_mutex);

    /* Assign task */
    agent->current_task = task;
    task->assigned_agent_id = strdup(agent->id);
    task->state = TASK_STATE_ASSIGNED;

    mutex_unlock(&agent->state_mutex);

    log_debug("Task '%s' assigned to agent '%s'", task->id, agent->name);
    return true;
}

char* agent_run_sync(AgentInstance* agent, const char* task_description) {
    if (!agent || !task_description) return NULL;

    AgentState current = agent_get_state(agent);
    if (current != AGENT_STATE_IDLE) {
        log_warning("Agent '%s' is not idle", agent->name);
        return NULL;
    }

    agent_set_state(agent, AGENT_STATE_RUNNING);
    char* result = NULL;

    /* Check for mock mode */
    if (agent->config.mock_mode) {
        log_info("[MOCK] Agent '%s' simulating task: %s", agent->name, task_description);

        /* Generate a mock response */
        char mock_response[512];
        snprintf(mock_response, sizeof(mock_response),
                "[MOCK RESULT] Agent '%s' (type: %s) completed task.\n"
                "Task: %s\n"
                "Status: Success (simulated)\n"
                "Note: Running in mock mode - no AI backend required.",
                agent->name,
                agent_type_to_string(agent->type),
                task_description);

        result = strdup(mock_response);
        log_success("[MOCK] Task completed successfully (simulated)");
    } else {
        /* Execute based on agent type */
        switch (agent->type) {
            case AGENT_TYPE_SMART:
                if (agent->impl.smart) {
                    SmartResult* sr = smart_agent_execute(agent->impl.smart,
                                                          task_description);
                    if (sr) {
                        result = sr->output ? strdup(sr->output) : NULL;
                        smart_result_free(sr);
                    }
                }
                break;

            case AGENT_TYPE_AUTONOMOUS:
                if (agent->impl.autonomous) {
                    result = agent_run(agent->impl.autonomous, task_description);
                }
                break;

            case AGENT_TYPE_BUILD:
                /* Build agent needs a project path, not a task description */
                log_warning("Build agent requires project path, not task description");
                break;

            default:
                log_warning("Agent type '%s' does not support sync execution",
                           agent_type_to_string(agent->type));
                break;
        }
    }

    /* Update state and statistics */
    agent_set_state(agent, AGENT_STATE_IDLE);
    if (result) {
        agent->tasks_completed++;
        free(agent->last_result);
        agent->last_result = strdup(result);
    } else {
        agent->tasks_failed++;
    }

    return result;
}

/* Context for async task execution */
typedef struct {
    AgentInstance* agent;
    char* task_description;
} AsyncTaskContext;

/* Helper to update shared state for an agent */
static void update_agent_shared_state(AgentInstance* agent, const char* key_suffix,
                                      const char* value) {
    if (!agent || !agent->registry || !agent->registry->shared_state) return;
    if (!agent->name || !key_suffix || !value) return;

    char key[256];
    snprintf(key, sizeof(key), "%s.%s", agent->name, key_suffix);
    shared_state_set(agent->registry->shared_state, key, value);
}

/* Thread function for async execution */
static void async_task_runner(void* arg) {
    AsyncTaskContext* ctx = (AsyncTaskContext*)arg;
    if (!ctx || !ctx->agent || !ctx->task_description) {
        free(ctx);
        return;
    }

    AgentInstance* agent = ctx->agent;
    char* task_desc = ctx->task_description;

    log_debug("Async task started for agent '%s'", agent->name);

    /* Mark thread as active */
    mutex_lock(&agent->state_mutex);
    agent->thread_active = true;
    mutex_unlock(&agent->state_mutex);

    /* Execute the task synchronously in this thread */
    agent_set_state(agent, AGENT_STATE_RUNNING);

    /* Auto-update shared state: task started */
    update_agent_shared_state(agent, "status", "running");
    update_agent_shared_state(agent, "task", task_desc);
    char* result = NULL;

    /* Check for mock mode */
    if (agent->config.mock_mode) {
        log_info("[MOCK] Agent '%s' simulating task: %s", agent->name, task_desc);

        /* Generate a mock response */
        char mock_response[512];
        snprintf(mock_response, sizeof(mock_response),
                "[MOCK RESULT] Agent '%s' (type: %s) completed task.\n"
                "Task: %s\n"
                "Status: Success (simulated)\n"
                "Note: Running in mock mode - no AI backend required.",
                agent->name,
                agent_type_to_string(agent->type),
                task_desc);

        result = strdup(mock_response);
        log_success("[MOCK] Task completed successfully (simulated)");
    } else {
        /* Execute based on agent type */
        switch (agent->type) {
            case AGENT_TYPE_SMART:
                if (agent->impl.smart) {
                    SmartResult* sr = smart_agent_execute(agent->impl.smart, task_desc);
                    if (sr) {
                        result = sr->output ? strdup(sr->output) : NULL;
                        smart_result_free(sr);
                    }
                }
                break;

            case AGENT_TYPE_AUTONOMOUS:
                if (agent->impl.autonomous) {
                    result = agent_run(agent->impl.autonomous, task_desc);
                }
                break;

            case AGENT_TYPE_BUILD:
                log_warning("Build agent requires project path, not task description");
                break;

            default:
                log_warning("Agent type '%s' does not support async execution",
                           agent_type_to_string(agent->type));
                break;
        }
    }

    /* Update state and statistics */
    mutex_lock(&agent->state_mutex);
    agent->state = AGENT_STATE_IDLE;
    agent->thread_active = false;
    if (result) {
        agent->tasks_completed++;
        free(agent->last_result);
        agent->last_result = strdup(result);
    } else {
        agent->tasks_failed++;
    }
    mutex_unlock(&agent->state_mutex);

    /* Auto-update shared state: task completed */
    if (result) {
        update_agent_shared_state(agent, "status", "completed");
        update_agent_shared_state(agent, "result", result);
        free(result);
    } else {
        update_agent_shared_state(agent, "status", "failed");
        update_agent_shared_state(agent, "result", "Task execution failed");
    }

    log_debug("Async task completed for agent '%s'", agent->name);

    /* Cleanup context */
    free(task_desc);
    free(ctx);
}

bool agent_run_async(AgentInstance* agent, const char* task_description) {
    if (!agent || !task_description) return false;

    /* Check if agent has a registry with thread pool */
    if (!agent->registry || !agent->registry->thread_pool) {
        log_warning("No thread pool available, falling back to sync execution");
        char* result = agent_run_sync(agent, task_description);
        free(result);
        return result != NULL;
    }

    /* Check agent state */
    AgentState current = agent_get_state(agent);
    if (current != AGENT_STATE_IDLE) {
        log_warning("Agent '%s' is not idle (state: %s)",
                   agent->name, agent_state_to_string(current));
        return false;
    }

    /* Check if thread is already active */
    mutex_lock(&agent->state_mutex);
    if (agent->thread_active) {
        mutex_unlock(&agent->state_mutex);
        log_warning("Agent '%s' already has an active task", agent->name);
        return false;
    }
    mutex_unlock(&agent->state_mutex);

    /* Create async context */
    AsyncTaskContext* ctx = (AsyncTaskContext*)malloc(sizeof(AsyncTaskContext));
    if (!ctx) {
        log_error("Failed to allocate async task context");
        return false;
    }

    ctx->agent = agent;
    ctx->task_description = strdup(task_description);
    if (!ctx->task_description) {
        free(ctx);
        log_error("Failed to allocate task description");
        return false;
    }

    /* Submit to thread pool */
    if (!thread_pool_submit(agent->registry->thread_pool, async_task_runner, ctx)) {
        free(ctx->task_description);
        free(ctx);
        log_error("Failed to submit task to thread pool");
        return false;
    }

    log_debug("Async task submitted for agent '%s': %s", agent->name, task_description);
    return true;
}

const char* agent_get_result(AgentInstance* agent) {
    return agent ? agent->last_result : NULL;
}

const char* agent_get_error_msg(AgentInstance* agent) {
    return agent ? agent->last_error : NULL;
}

/* ============================================================================
 * Agent Spawning (Parent/Child)
 * ============================================================================ */

AgentInstance* agent_spawn_child(AgentInstance* parent, const char* name,
                                 AgentType type, const AgentInstanceConfig* config) {
    if (!parent || !name || !parent->registry) {
        return NULL;
    }

    /* Check if parent has spawn capability */
    if (!(parent->capabilities & AGENT_CAP_SPAWN)) {
        log_warning("Agent '%s' does not have spawn capability", parent->name);
        return NULL;
    }

    /* Create child with inherited config if none provided */
    AgentInstanceConfig child_config;
    if (config) {
        child_config = *config;
    } else {
        child_config = parent->config;
        child_config.description = NULL; /* Don't inherit description */
    }

    AgentInstance* child = agent_registry_create_agent(
        parent->registry, name, type, &child_config);

    if (!child) {
        return NULL;
    }

    /* Set parent-child relationship */
    child->parent = parent;

    /* Add to parent's children list */
    mutex_lock(&parent->state_mutex);

    if (parent->child_count >= parent->child_capacity) {
        int new_cap = parent->child_capacity ? parent->child_capacity * 2 : 4;
        AgentInstance** new_children = (AgentInstance**)realloc(
            parent->children, new_cap * sizeof(AgentInstance*));
        if (!new_children) {
            mutex_unlock(&parent->state_mutex);
            agent_registry_remove(parent->registry, child->id);
            return NULL;
        }
        parent->children = new_children;
        parent->child_capacity = new_cap;
    }

    parent->children[parent->child_count++] = child;

    mutex_unlock(&parent->state_mutex);

    log_info("Agent '%s' spawned child '%s' (type: %s)",
             parent->name, name, agent_type_to_string(type));

    return child;
}

AgentInstance** agent_get_children(AgentInstance* parent, int* count) {
    if (!parent || !count) {
        if (count) *count = 0;
        return NULL;
    }

    mutex_lock(&parent->state_mutex);
    *count = parent->child_count;
    AgentInstance** children = parent->children;
    mutex_unlock(&parent->state_mutex);

    return children;
}

bool agent_wait_children(AgentInstance* parent, int timeout_ms) {
    if (!parent) return true;

    int count;
    AgentInstance** children = agent_get_children(parent, &count);
    if (!children || count == 0) return true;

    int elapsed = 0;
    int interval = 50;

    while (elapsed < timeout_ms || timeout_ms == 0) {
        bool all_done = true;

        for (int i = 0; i < count; i++) {
            if (!agent_is_finished(children[i])) {
                all_done = false;
                break;
            }
        }

        if (all_done) return true;

        thread_sleep(interval);
        elapsed += interval;
    }

    return false;
}

void agent_terminate_children(AgentInstance* parent) {
    if (!parent) return;

    int count;
    AgentInstance** children = agent_get_children(parent, &count);
    if (!children) return;

    for (int i = 0; i < count; i++) {
        agent_terminate(children[i]);
    }
}

/* ============================================================================
 * Instance Cleanup
 * ============================================================================ */

void agent_instance_free(AgentInstance* agent) {
    if (!agent) return;

    log_debug("Freeing agent instance type=%d...", agent->type);

    /* Free underlying implementation */
    switch (agent->type) {
        case AGENT_TYPE_SMART:
            if (agent->impl.smart) {
                log_debug("Freeing SmartAgent impl...");
                smart_agent_free(agent->impl.smart);
                agent->impl.smart = NULL;
            }
            break;
        case AGENT_TYPE_AUTONOMOUS:
            if (agent->impl.autonomous) {
                log_debug("Freeing AutonomousAgent impl...");
                agent_free(agent->impl.autonomous);
                agent->impl.autonomous = NULL;
            }
            break;
        case AGENT_TYPE_BUILD:
            if (agent->impl.build) {
                log_debug("Freeing AIBuildAgent impl...");
                ai_build_agent_free(agent->impl.build);
                agent->impl.build = NULL;
            }
            break;
        default:
            break;
    }

    /* Free task if present */
    if (agent->current_task) {
        log_debug("Freeing current task...");
        task_free(agent->current_task);
        agent->current_task = NULL;
    }

    /* Free children array (not the children themselves - registry owns them) */
    free(agent->children);
    agent->children = NULL;

    /* Free strings */
    free(agent->id);
    free(agent->name);
    free(agent->description);
    free(agent->last_result);
    free(agent->last_error);

    log_debug("Destroying agent mutex...");
    mutex_destroy(&agent->state_mutex);

    log_debug("Agent instance freed");
    free(agent);
}
