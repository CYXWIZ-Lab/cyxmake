/**
 * @file action_planner.h
 * @brief Multi-step action planning with approval and rollback
 *
 * Provides structured execution of complex AI-driven tasks:
 * - Plan generation from AI responses
 * - Step-by-step preview and approval
 * - Sequential execution with status tracking
 * - Rollback on failure
 */

#ifndef CYXMAKE_ACTION_PLANNER_H
#define CYXMAKE_ACTION_PLANNER_H

#include "prompt_templates.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct ReplSession ReplSession;

/**
 * Step execution status
 */
typedef enum {
    STEP_PENDING,       /* Not yet executed */
    STEP_IN_PROGRESS,   /* Currently executing */
    STEP_COMPLETED,     /* Successfully completed */
    STEP_FAILED,        /* Failed during execution */
    STEP_SKIPPED,       /* Skipped by user or due to dependency failure */
    STEP_ROLLED_BACK    /* Was completed but rolled back */
} StepStatus;

/**
 * Rollback action type
 */
typedef enum {
    ROLLBACK_NONE,          /* No rollback possible */
    ROLLBACK_DELETE_FILE,   /* Delete a created file */
    ROLLBACK_RESTORE_FILE,  /* Restore original file content */
    ROLLBACK_DELETE_DIR,    /* Delete a created directory */
    ROLLBACK_UNINSTALL,     /* Uninstall a package */
    ROLLBACK_CUSTOM         /* Custom rollback command */
} RollbackType;

/**
 * Rollback information for a step
 */
typedef struct {
    RollbackType type;
    char* target;           /* File/package/etc to rollback */
    char* original_content; /* Original file content for restore */
    char* custom_command;   /* Custom rollback command */
} RollbackInfo;

/**
 * A single step in an action plan
 */
typedef struct ActionStep {
    int step_number;        /* 1-indexed step number */
    AIActionType action;    /* Type of action */
    char* description;      /* Human-readable description */
    char* target;           /* Target file/package/etc */
    char* content;          /* Content for file creation, command, etc */
    char* reason;           /* Why this step is needed */

    /* Execution state */
    StepStatus status;
    char* error_message;    /* Error if failed */
    time_t started_at;
    time_t completed_at;

    /* Rollback support */
    RollbackInfo rollback;
    bool can_rollback;

    /* Linked list */
    struct ActionStep* next;
    struct ActionStep* prev;
} ActionStep;

/**
 * Approval mode for plan execution
 */
typedef enum {
    APPROVAL_NONE,          /* No approval needed (internal use) */
    APPROVAL_ALL,           /* Approve entire plan at once */
    APPROVAL_STEP_BY_STEP,  /* Approve each step individually */
    APPROVAL_DENIED         /* User denied execution */
} ApprovalMode;

/**
 * Plan execution state
 */
typedef enum {
    PLAN_CREATED,       /* Plan created, not yet approved */
    PLAN_APPROVED,      /* Approved, ready to execute */
    PLAN_EXECUTING,     /* Currently executing */
    PLAN_COMPLETED,     /* All steps completed successfully */
    PLAN_FAILED,        /* One or more steps failed */
    PLAN_ABORTED,       /* Aborted by user */
    PLAN_ROLLED_BACK    /* Rolled back after failure/abort */
} PlanState;

/**
 * An action plan containing multiple steps
 */
typedef struct ActionPlan {
    char* title;            /* Brief title for the plan */
    char* description;      /* Detailed description */
    char* user_request;     /* Original user request */

    /* Steps */
    ActionStep* steps;      /* Head of step list */
    ActionStep* last_step;  /* Tail for O(1) append */
    int step_count;
    int completed_count;
    int failed_count;

    /* State */
    PlanState state;
    ApprovalMode approval_mode;

    /* Timing */
    time_t created_at;
    time_t started_at;
    time_t completed_at;

    /* Error info */
    char* error_message;    /* Overall error if failed */
} ActionPlan;

/**
 * Plan execution options
 */
typedef struct {
    bool stop_on_failure;       /* Stop executing if a step fails (default: true) */
    bool auto_rollback;         /* Automatically rollback on failure (default: false) */
    bool verbose;               /* Show detailed execution info */
    bool dry_run;               /* Show what would happen without executing */
    int step_delay_ms;          /* Delay between steps (for visibility, default: 0) */
} PlanExecOptions;

/* ============================================================================
 * Plan Creation
 * ============================================================================ */

/**
 * Create an empty action plan
 * @param title Plan title
 * @param user_request Original user request that triggered this plan
 * @return New plan or NULL on error
 */
ActionPlan* plan_create(const char* title, const char* user_request);

/**
 * Create action plan from AI agent response
 * @param response AI agent response with actions
 * @param user_request Original user request
 * @return New plan or NULL if no actions
 */
ActionPlan* plan_from_ai_response(AIAgentResponse* response, const char* user_request);

/**
 * Add a step to a plan
 * @param plan Plan to add to
 * @param action Action type
 * @param description Human-readable description
 * @param target Target file/package/etc
 * @param content Content for the action
 * @param reason Why this step is needed
 * @return The new step or NULL on error
 */
ActionStep* plan_add_step(ActionPlan* plan,
                          AIActionType action,
                          const char* description,
                          const char* target,
                          const char* content,
                          const char* reason);

/**
 * Free an action plan
 * @param plan Plan to free
 */
void plan_free(ActionPlan* plan);

/**
 * Free an action step
 * @param step Step to free
 */
void step_free(ActionStep* step);

/* ============================================================================
 * Plan Display
 * ============================================================================ */

/**
 * Display plan summary to console
 * @param plan Plan to display
 * @param colors_enabled Use colored output
 */
void plan_display(ActionPlan* plan, bool colors_enabled);

/**
 * Display single step details
 * @param step Step to display
 * @param colors_enabled Use colored output
 */
void step_display(ActionStep* step, bool colors_enabled);

/**
 * Display plan execution progress
 * @param plan Plan being executed
 * @param colors_enabled Use colored output
 */
void plan_display_progress(ActionPlan* plan, bool colors_enabled);

/* ============================================================================
 * Plan Approval
 * ============================================================================ */

/**
 * Request user approval for a plan
 * @param plan Plan to approve
 * @param session REPL session for I/O
 * @return Approval mode (APPROVAL_ALL, APPROVAL_STEP_BY_STEP, or APPROVAL_DENIED)
 */
ApprovalMode plan_request_approval(ActionPlan* plan, ReplSession* session);

/**
 * Request approval for a single step (step-by-step mode)
 * @param step Step to approve
 * @param session REPL session for I/O
 * @return true if approved, false if denied or skipped
 */
bool step_request_approval(ActionStep* step, ReplSession* session);

/* ============================================================================
 * Plan Execution
 * ============================================================================ */

/**
 * Default execution options
 * @return Default options
 */
PlanExecOptions plan_exec_options_default(void);

/**
 * Execute an action plan
 * @param plan Plan to execute
 * @param session REPL session for I/O and context
 * @param options Execution options (NULL for defaults)
 * @return true if all steps succeeded, false otherwise
 */
bool plan_execute(ActionPlan* plan, ReplSession* session, const PlanExecOptions* options);

/**
 * Execute a single step
 * @param step Step to execute
 * @param session REPL session
 * @return true if successful
 */
bool step_execute(ActionStep* step, ReplSession* session);

/* ============================================================================
 * Rollback
 * ============================================================================ */

/**
 * Set rollback info for a step
 * @param step Step to configure
 * @param type Rollback type
 * @param target Rollback target
 * @param original_content Original content for restore (optional)
 */
void step_set_rollback(ActionStep* step,
                       RollbackType type,
                       const char* target,
                       const char* original_content);

/**
 * Rollback completed steps in a plan
 * @param plan Plan to rollback
 * @param session REPL session
 * @return Number of steps rolled back
 */
int plan_rollback(ActionPlan* plan, ReplSession* session);

/**
 * Rollback a single step
 * @param step Step to rollback
 * @param session REPL session
 * @return true if rollback successful
 */
bool step_rollback(ActionStep* step, ReplSession* session);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Get status name string
 * @param status Step status
 * @return Static string
 */
const char* step_status_name(StepStatus status);

/**
 * Get plan state name string
 * @param state Plan state
 * @return Static string
 */
const char* plan_state_name(PlanState state);

/**
 * Check if plan has any pending steps
 * @param plan Plan to check
 * @return true if has pending steps
 */
bool plan_has_pending_steps(ActionPlan* plan);

/**
 * Get next pending step
 * @param plan Plan to search
 * @return Next pending step or NULL
 */
ActionStep* plan_get_next_pending(ActionPlan* plan);

/**
 * Get step by number
 * @param plan Plan to search
 * @param step_number 1-indexed step number
 * @return Step or NULL if not found
 */
ActionStep* plan_get_step(ActionPlan* plan, int step_number);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_ACTION_PLANNER_H */
