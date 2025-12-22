/**
 * @file agent_coordinator.h
 * @brief Agent Coordinator - Task distribution and conflict resolution
 *
 * Provides:
 * - Task distribution to agents
 * - Conflict detection and user-prompted resolution
 * - Result aggregation from parallel agents
 * - Agent orchestration
 */

#ifndef CYXMAKE_AGENT_COORDINATOR_H
#define CYXMAKE_AGENT_COORDINATOR_H

#include "cyxmake/agent_registry.h"
#include "cyxmake/agent_comm.h"
#include "cyxmake/task_queue.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Conflict Types
 * ============================================================================ */

/**
 * Types of conflicts that can occur between agents
 */
typedef enum {
    CONFLICT_NONE,               /* No conflict */
    CONFLICT_RESOURCE,           /* Both want same file/resource */
    CONFLICT_DECISION,           /* Different approaches to same problem */
    CONFLICT_DEPENDENCY,         /* Circular or blocking dependency */
    CONFLICT_TIMEOUT             /* Agent not responding */
} ConflictType;

/**
 * Resolution strategies for conflicts
 */
typedef enum {
    RESOLUTION_USER_PROMPT,      /* Ask user to choose (default) */
    RESOLUTION_PRIORITY,         /* Higher priority agent wins */
    RESOLUTION_FIRST_COME,       /* First requester wins */
    RESOLUTION_CANCEL_BOTH,      /* Cancel both conflicting operations */
    RESOLUTION_MERGE             /* Try to merge both operations */
} ResolutionStrategy;

/**
 * Result of conflict resolution
 */
typedef enum {
    RESOLUTION_RESULT_AGENT1,    /* Agent 1 wins */
    RESOLUTION_RESULT_AGENT2,    /* Agent 2 wins */
    RESOLUTION_RESULT_BOTH,      /* Both proceed (merge) */
    RESOLUTION_RESULT_NEITHER,   /* Cancel both */
    RESOLUTION_RESULT_ERROR      /* Resolution failed */
} ResolutionResult;

/* ============================================================================
 * Conflict Structure
 * ============================================================================ */

/**
 * Describes a conflict between agents
 */
typedef struct {
    ConflictType type;

    /* Conflicting agents */
    char* agent1_id;
    char* agent1_name;
    char* agent2_id;
    char* agent2_name;

    /* What they're conflicting over */
    char* resource_id;           /* File path, resource name, etc. */
    char* resource_type;         /* "file", "package", "config", etc. */

    /* Their positions */
    char* agent1_action;         /* What agent1 wants to do */
    char* agent2_action;         /* What agent2 wants to do */

    /* Resolution */
    ResolutionResult resolution;
    char* resolution_reason;
    time_t detected_at;
    time_t resolved_at;
} AgentConflict;

/* ============================================================================
 * Aggregated Result
 * ============================================================================ */

/**
 * Aggregated results from multiple agents
 */
typedef struct {
    bool all_succeeded;          /* All agents completed successfully */
    int success_count;
    int failure_count;
    int timeout_count;

    char* combined_output;       /* Merged output from all agents */
    char** individual_outputs;   /* Per-agent outputs */
    char** agent_names;          /* Corresponding agent names */
    int output_count;

    char* first_error;           /* First error encountered */
    double total_duration_sec;
} AggregatedResult;

/* ============================================================================
 * Coordinator Configuration
 * ============================================================================ */

/**
 * Configuration for the agent coordinator
 */
typedef struct {
    ResolutionStrategy default_resolution;
    int max_concurrent_agents;
    int task_timeout_sec;
    bool verbose;

    /* User interaction callback for conflict resolution */
    int (*prompt_user)(const AgentConflict* conflict,
                       const char* message,
                       const char** options,
                       int option_count);
} CoordinatorConfig;

/**
 * Default coordinator configuration
 */
CoordinatorConfig coordinator_config_defaults(void);

/* ============================================================================
 * Agent Coordinator
 * ============================================================================ */

/**
 * The agent coordinator
 */
typedef struct AgentCoordinator {
    AgentRegistry* registry;
    MessageBus* message_bus;
    SharedState* shared_state;
    TaskQueue* task_queue;

    CoordinatorConfig config;

    /* Active conflicts */
    AgentConflict** conflicts;
    int conflict_count;
    int conflict_capacity;

    /* Thread safety */
    MutexHandle mutex;

    /* Resource tracking for conflict detection */
    char** locked_resources;
    char** resource_owners;
    int resource_count;
    int resource_capacity;
} AgentCoordinator;

/* ============================================================================
 * Coordinator Lifecycle
 * ============================================================================ */

/**
 * Create an agent coordinator
 *
 * @param registry Agent registry to coordinate
 * @param bus Message bus for communication
 * @param state Shared state for context
 * @param config Coordinator configuration (NULL for defaults)
 * @return New coordinator or NULL on failure
 */
AgentCoordinator* coordinator_create(AgentRegistry* registry,
                                     MessageBus* bus,
                                     SharedState* state,
                                     const CoordinatorConfig* config);

/**
 * Destroy the coordinator
 */
void coordinator_free(AgentCoordinator* coord);

/**
 * Set the task queue for the coordinator
 */
void coordinator_set_task_queue(AgentCoordinator* coord, TaskQueue* queue);

/* ============================================================================
 * Task Distribution
 * ============================================================================ */

/**
 * Distribution strategies
 */
typedef enum {
    DISTRIBUTION_ROUND_ROBIN,    /* Distribute evenly */
    DISTRIBUTION_LOAD_BALANCED,  /* Send to least busy */
    DISTRIBUTION_CAPABILITY,     /* Match task to agent capabilities */
    DISTRIBUTION_AFFINITY        /* Same agent for related tasks */
} DistributionStrategy;

/**
 * Set the distribution strategy
 */
void coordinator_set_distribution(AgentCoordinator* coord,
                                  DistributionStrategy strategy);

/**
 * Assign a task to the best available agent
 *
 * @param coord The coordinator
 * @param task Task to assign
 * @return Assigned agent or NULL if no suitable agent
 */
AgentInstance* coordinator_assign_task(AgentCoordinator* coord, AgentTask* task);

/**
 * Assign a task to a specific agent by name
 *
 * @param coord The coordinator
 * @param task Task to assign
 * @param agent_name Target agent name
 * @return true on success
 */
bool coordinator_assign_to(AgentCoordinator* coord, AgentTask* task,
                           const char* agent_name);

/**
 * Spawn worker agents for a complex task
 *
 * @param coord The coordinator
 * @param parent_task Task to subdivide
 * @param worker_count Number of workers to spawn
 * @return true on success
 */
bool coordinator_spawn_workers(AgentCoordinator* coord,
                               AgentTask* parent_task,
                               int worker_count);

/**
 * Wait for all assigned tasks to complete
 *
 * @param coord The coordinator
 * @param timeout_ms Maximum wait time (0 = infinite)
 * @return true if all completed, false on timeout
 */
bool coordinator_wait_all(AgentCoordinator* coord, int timeout_ms);

/* ============================================================================
 * Conflict Detection and Resolution
 * ============================================================================ */

/**
 * Request access to a resource
 *
 * @param coord The coordinator
 * @param agent_id Requesting agent
 * @param resource_id Resource to access
 * @param action What the agent wants to do
 * @return true if access granted, false if conflict
 */
bool coordinator_request_resource(AgentCoordinator* coord,
                                  const char* agent_id,
                                  const char* resource_id,
                                  const char* action);

/**
 * Release a resource
 *
 * @param coord The coordinator
 * @param agent_id Releasing agent
 * @param resource_id Resource to release
 */
void coordinator_release_resource(AgentCoordinator* coord,
                                  const char* agent_id,
                                  const char* resource_id);

/**
 * Detect conflicts between active agents
 *
 * @param coord The coordinator
 * @return Detected conflict or NULL if none
 */
AgentConflict* coordinator_detect_conflict(AgentCoordinator* coord);

/**
 * Resolve a conflict by prompting the user
 *
 * @param coord The coordinator
 * @param conflict Conflict to resolve
 * @return Resolution result
 */
ResolutionResult coordinator_resolve_conflict(AgentCoordinator* coord,
                                              AgentConflict* conflict);

/**
 * Free a conflict structure
 */
void conflict_free(AgentConflict* conflict);

/**
 * Get string representation of conflict type
 */
const char* conflict_type_to_string(ConflictType type);

/**
 * Get string representation of resolution result
 */
const char* resolution_result_to_string(ResolutionResult result);

/* ============================================================================
 * Result Aggregation
 * ============================================================================ */

/**
 * Aggregate results from multiple agents
 *
 * @param coord The coordinator
 * @param agents Array of agents to aggregate from
 * @param count Number of agents
 * @return Aggregated result (caller must free)
 */
AggregatedResult* coordinator_aggregate_results(AgentCoordinator* coord,
                                                AgentInstance** agents,
                                                int count);

/**
 * Free an aggregated result
 */
void aggregated_result_free(AggregatedResult* result);

/* ============================================================================
 * Coordinator Commands (for REPL integration)
 * ============================================================================ */

/**
 * List all active agents with their status
 *
 * @param coord The coordinator
 * @return Formatted status string (caller must free)
 */
char* coordinator_status_report(AgentCoordinator* coord);

/**
 * Get conflicts as formatted string
 *
 * @param coord The coordinator
 * @return Formatted conflict report (caller must free)
 */
char* coordinator_conflict_report(AgentCoordinator* coord);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_AGENT_COORDINATOR_H */
