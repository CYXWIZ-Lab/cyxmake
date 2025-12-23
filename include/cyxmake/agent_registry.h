/**
 * @file agent_registry.h
 * @brief Agent Registry - Named agents with lifecycle management
 *
 * Provides a unified registry for managing multiple agent instances:
 * - Named agents with configurable settings
 * - Lifecycle state machine (created -> running -> completed)
 * - Thread-safe registration and lookup
 * - Parent/child relationships for spawned agents
 */

#ifndef CYXMAKE_AGENT_REGISTRY_H
#define CYXMAKE_AGENT_REGISTRY_H

#include "cyxmake/threading.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct SmartAgent SmartAgent;
typedef struct AutonomousAgent AutonomousAgent;
typedef struct AIBuildAgent AIBuildAgent;
typedef struct AIProvider AIProvider;
typedef struct ToolRegistry ToolRegistry;
typedef struct ProjectContext ProjectContext;
typedef struct AgentTask AgentTask;

/* ============================================================================
 * Agent Types and States
 * ============================================================================ */

/**
 * Types of agents that can be registered
 */
typedef enum {
    AGENT_TYPE_SMART,           /* SmartAgent - reasoning and planning */
    AGENT_TYPE_AUTONOMOUS,      /* AutonomousAgent - tool-using AI */
    AGENT_TYPE_BUILD,           /* AIBuildAgent - build orchestration */
    AGENT_TYPE_COORDINATOR,     /* Special: coordinates other agents */
    AGENT_TYPE_CUSTOM           /* User-defined agent type */
} AgentType;

/**
 * Agent lifecycle states
 */
typedef enum {
    AGENT_STATE_CREATED,        /* Agent created but not initialized */
    AGENT_STATE_INITIALIZING,   /* Agent is starting up */
    AGENT_STATE_IDLE,           /* Agent ready to accept tasks */
    AGENT_STATE_RUNNING,        /* Agent executing a task */
    AGENT_STATE_PAUSED,         /* Agent paused mid-task */
    AGENT_STATE_COMPLETING,     /* Agent finishing up */
    AGENT_STATE_COMPLETED,      /* Agent finished successfully */
    AGENT_STATE_TERMINATED,     /* Agent forcibly stopped */
    AGENT_STATE_ERROR           /* Agent encountered fatal error */
} AgentState;

/**
 * Agent capability flags
 */
typedef enum {
    AGENT_CAP_NONE          = 0,
    AGENT_CAP_BUILD         = (1 << 0),   /* Can build projects */
    AGENT_CAP_FIX_ERRORS    = (1 << 1),   /* Can diagnose/fix errors */
    AGENT_CAP_READ_FILES    = (1 << 2),   /* Can read files */
    AGENT_CAP_WRITE_FILES   = (1 << 3),   /* Can modify files */
    AGENT_CAP_EXECUTE       = (1 << 4),   /* Can execute commands */
    AGENT_CAP_INSTALL_DEPS  = (1 << 5),   /* Can install dependencies */
    AGENT_CAP_ANALYZE       = (1 << 6),   /* Can analyze code */
    AGENT_CAP_REASON        = (1 << 7),   /* Can perform reasoning */
    AGENT_CAP_SPAWN         = (1 << 8),   /* Can spawn child agents */
    AGENT_CAP_ALL           = 0xFFFF
} AgentCapability;

/* ============================================================================
 * Agent Instance Configuration
 * ============================================================================ */

/**
 * Configuration for creating an agent instance
 */
typedef struct {
    /* General settings */
    int timeout_sec;            /* Task timeout (0 = no timeout) */
    bool verbose;               /* Enable verbose output */
    bool auto_start;            /* Start immediately after creation */
    int max_retries;            /* Max retries on failure */

    /* Capability restrictions */
    unsigned int capabilities;  /* Bitmask of AgentCapability */
    bool read_only;             /* Prevent file modifications */

    /* AI settings */
    float temperature;          /* LLM temperature (0.0-1.0) */
    int max_tokens;             /* Max tokens per response */
    int max_iterations;         /* Max reasoning iterations */

    /* Testing/debugging */
    bool mock_mode;             /* Run in mock mode (no AI required) */

    /* Custom description */
    const char* description;    /* User-provided description */
    const char* focus;          /* Task focus area */
} AgentInstanceConfig;

/**
 * Create a default agent configuration
 */
AgentInstanceConfig agent_config_defaults(void);

/* ============================================================================
 * Agent Instance
 * ============================================================================ */

/**
 * A single agent instance in the registry
 */
typedef struct AgentInstance {
    /* Identity */
    char* id;                   /* Unique UUID */
    char* name;                 /* User-assigned name */
    AgentType type;
    AgentState state;
    char* description;          /* What this agent does */

    /* Underlying implementation (union for efficiency) */
    union {
        SmartAgent* smart;
        AutonomousAgent* autonomous;
        AIBuildAgent* build;
        void* custom;
    } impl;

    /* Thread context for async execution */
    ThreadHandle thread;
    MutexHandle state_mutex;
    bool thread_active;

    /* Task tracking */
    AgentTask* current_task;
    char* last_result;          /* JSON result from last task */
    char* last_error;           /* Last error message */

    /* Lifecycle timestamps */
    time_t created_at;
    time_t started_at;
    time_t completed_at;

    /* Statistics */
    int tasks_completed;
    int tasks_failed;
    double total_runtime_sec;

    /* Configuration */
    AgentInstanceConfig config;
    unsigned int capabilities;

    /* Parent/child relationships for spawned agents */
    struct AgentInstance* parent;
    struct AgentInstance** children;
    int child_count;
    int child_capacity;

    /* Registry back-reference */
    struct AgentRegistry* registry;
} AgentInstance;

/* ============================================================================
 * Agent Registry
 * ============================================================================ */

/**
 * Registry for managing multiple agent instances
 */
typedef struct AgentRegistry {
    AgentInstance** agents;
    size_t agent_count;
    size_t agent_capacity;

    MutexHandle registry_mutex;     /* Thread-safe access */

    /* Shared resources for all agents */
    AIProvider* default_ai;
    ToolRegistry* tools;
    ThreadPool* thread_pool;

    /* Shared memory pool (all agents access this) */
    char* shared_memory_path;       /* Path to .cyxmake/agent_memory.json */

    /* Configuration */
    int max_concurrent;             /* Max concurrent agents */
    int default_timeout;            /* Default task timeout */
} AgentRegistry;

/* ============================================================================
 * Registry Lifecycle
 * ============================================================================ */

/**
 * Create an agent registry
 *
 * @param ai Default AI provider for agents
 * @param tools Tool registry for agents
 * @param thread_pool Thread pool for async execution (can be NULL)
 * @return New registry or NULL on failure
 */
AgentRegistry* agent_registry_create(AIProvider* ai, ToolRegistry* tools,
                                     ThreadPool* thread_pool);

/**
 * Destroy the registry and all agents
 *
 * @param registry Registry to destroy
 */
void agent_registry_free(AgentRegistry* registry);

/**
 * Set the shared memory path
 *
 * @param registry The registry
 * @param path Path to shared memory file
 */
void agent_registry_set_memory_path(AgentRegistry* registry, const char* path);

/* ============================================================================
 * Agent Management
 * ============================================================================ */

/**
 * Create and register a new agent
 *
 * @param registry The registry
 * @param name Unique name for the agent
 * @param type Type of agent to create
 * @param config Configuration (NULL for defaults)
 * @return New agent instance or NULL on failure
 */
AgentInstance* agent_registry_create_agent(AgentRegistry* registry,
                                           const char* name,
                                           AgentType type,
                                           const AgentInstanceConfig* config);

/**
 * Remove an agent from the registry
 *
 * @param registry The registry
 * @param name_or_id Agent name or ID
 * @return true if removed, false if not found
 */
bool agent_registry_remove(AgentRegistry* registry, const char* name_or_id);

/**
 * Get an agent by name or ID
 *
 * @param registry The registry
 * @param name_or_id Agent name or ID
 * @return Agent instance or NULL if not found
 */
AgentInstance* agent_registry_get(AgentRegistry* registry, const char* name_or_id);

/**
 * Get all agents in the registry
 *
 * @param registry The registry
 * @param count Output: number of agents
 * @return Array of agent pointers (do not free)
 */
AgentInstance** agent_registry_list(AgentRegistry* registry, int* count);

/**
 * Get agents by type
 *
 * @param registry The registry
 * @param type Agent type to filter by
 * @param count Output: number of matching agents
 * @return Array of agent pointers (caller must free array, not contents)
 */
AgentInstance** agent_registry_get_by_type(AgentRegistry* registry,
                                           AgentType type, int* count);

/**
 * Get agents by state
 *
 * @param registry The registry
 * @param state Agent state to filter by
 * @param count Output: number of matching agents
 * @return Array of agent pointers (caller must free array, not contents)
 */
AgentInstance** agent_registry_get_by_state(AgentRegistry* registry,
                                            AgentState state, int* count);

/**
 * Get agents with specific capability
 *
 * @param registry The registry
 * @param capability Required capability
 * @param count Output: number of matching agents
 * @return Array of agent pointers (caller must free array, not contents)
 */
AgentInstance** agent_registry_get_by_capability(AgentRegistry* registry,
                                                  AgentCapability capability,
                                                  int* count);

/**
 * Count agents in a specific state
 *
 * @param registry The registry
 * @param state State to count
 * @return Number of agents in that state
 */
int agent_registry_count_state(AgentRegistry* registry, AgentState state);

/* ============================================================================
 * Agent Lifecycle Control
 * ============================================================================ */

/**
 * Start an agent (transitions to RUNNING)
 *
 * @param agent Agent to start
 * @return true on success
 */
bool agent_start(AgentInstance* agent);

/**
 * Pause an agent mid-task
 *
 * @param agent Agent to pause
 * @return true on success
 */
bool agent_pause(AgentInstance* agent);

/**
 * Resume a paused agent
 *
 * @param agent Agent to resume
 * @return true on success
 */
bool agent_resume(AgentInstance* agent);

/**
 * Terminate an agent (forcibly stop)
 *
 * @param agent Agent to terminate
 * @return true on success
 */
bool agent_terminate(AgentInstance* agent);

/**
 * Wait for an agent to complete
 *
 * @param agent Agent to wait on
 * @param timeout_ms Timeout in milliseconds (0 = infinite)
 * @return true if agent completed, false if timeout
 */
bool agent_wait(AgentInstance* agent, int timeout_ms);

/**
 * Check if agent is in a terminal state
 *
 * @param agent Agent to check
 * @return true if COMPLETED, TERMINATED, or ERROR
 */
bool agent_is_finished(AgentInstance* agent);

/**
 * Get the current state of an agent (thread-safe)
 *
 * @param agent Agent to query
 * @return Current state
 */
AgentState agent_get_state(AgentInstance* agent);

/**
 * Set agent state (thread-safe, internal use)
 *
 * @param agent Agent to modify
 * @param state New state
 */
void agent_set_state(AgentInstance* agent, AgentState state);

/* ============================================================================
 * Agent Task Assignment
 * ============================================================================ */

/**
 * Assign a task to an agent
 *
 * @param agent Agent to assign to
 * @param task Task to assign (takes ownership)
 * @return true on success
 */
bool agent_assign_task(AgentInstance* agent, AgentTask* task);

/**
 * Run a task synchronously (blocking)
 *
 * @param agent Agent to run
 * @param task_description Natural language task
 * @return Result string (caller must free) or NULL on error
 */
char* agent_run_sync(AgentInstance* agent, const char* task_description);

/**
 * Run a task asynchronously (non-blocking)
 *
 * @param agent Agent to run
 * @param task_description Natural language task
 * @return true if task started
 */
bool agent_run_async(AgentInstance* agent, const char* task_description);

/**
 * Get the result of the last task
 *
 * @param agent Agent to query
 * @return Result string (do not free) or NULL if no result
 */
const char* agent_get_result(AgentInstance* agent);

/**
 * Get the last error message
 *
 * @param agent Agent to query
 * @return Error string (do not free) or NULL if no error
 */
const char* agent_get_error(AgentInstance* agent);

/* ============================================================================
 * Agent Spawning (Parent/Child)
 * ============================================================================ */

/**
 * Spawn a child agent from a parent
 *
 * @param parent Parent agent
 * @param name Name for child agent
 * @param type Type of child agent
 * @param config Child configuration (NULL for inherited)
 * @return Child agent or NULL on failure
 */
AgentInstance* agent_spawn_child(AgentInstance* parent, const char* name,
                                 AgentType type, const AgentInstanceConfig* config);

/**
 * Get all children of an agent
 *
 * @param parent Parent agent
 * @param count Output: number of children
 * @return Array of child pointers (do not free)
 */
AgentInstance** agent_get_children(AgentInstance* parent, int* count);

/**
 * Wait for all children to complete
 *
 * @param parent Parent agent
 * @param timeout_ms Timeout in milliseconds (0 = infinite)
 * @return true if all children completed, false if timeout
 */
bool agent_wait_children(AgentInstance* parent, int timeout_ms);

/**
 * Terminate all children of an agent
 *
 * @param parent Parent agent
 */
void agent_terminate_children(AgentInstance* parent);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Get string representation of agent type
 */
const char* agent_type_to_string(AgentType type);

/**
 * Get string representation of agent state
 */
const char* agent_state_to_string(AgentState state);

/**
 * Parse agent type from string
 *
 * @param str Type string ("smart", "build", "auto", etc.)
 * @param type Output: parsed type
 * @return true if valid type
 */
bool agent_type_from_string(const char* str, AgentType* type);

/**
 * Get default capabilities for an agent type
 *
 * @param type Agent type
 * @return Capability bitmask
 */
unsigned int agent_default_capabilities(AgentType type);

/**
 * Generate a unique agent ID (UUID-like)
 *
 * @return New ID string (caller must free)
 */
char* agent_generate_id(void);

/**
 * Free an agent instance (internal use - removes from registry too)
 */
void agent_instance_free(AgentInstance* agent);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_AGENT_REGISTRY_H */
