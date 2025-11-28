/**
 * @file ai_build_agent.h
 * @brief AI-First Autonomous Build Agent
 *
 * This module implements an AI-driven build system where the LLM is the
 * primary decision maker. Instead of using AI as a fallback, it:
 *
 * 1. Analyzes the project and creates a build plan
 * 2. Executes the plan step-by-step
 * 3. When errors occur, AI understands and fixes them autonomously
 * 4. Retries until success or max attempts reached
 */

#ifndef CYXMAKE_AI_BUILD_AGENT_H
#define CYXMAKE_AI_BUILD_AGENT_H

#include "cyxmake/ai_provider.h"
#include "cyxmake/project_context.h"
#include "cyxmake/tool_executor.h"
#include "cyxmake/build_executor.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Build Step Types - What the AI can do
 * ======================================================================== */

typedef enum {
    BUILD_STEP_CONFIGURE,      /* Run cmake/configure */
    BUILD_STEP_BUILD,          /* Run the actual build */
    BUILD_STEP_INSTALL_DEP,    /* Install a dependency */
    BUILD_STEP_CREATE_DIR,     /* Create a directory */
    BUILD_STEP_RUN_COMMAND,    /* Run an arbitrary command */
    BUILD_STEP_MODIFY_FILE,    /* Modify a file (CMakeLists.txt, etc) */
    BUILD_STEP_SET_ENV,        /* Set environment variable */
    BUILD_STEP_CLEAN,          /* Clean build artifacts */
    BUILD_STEP_DONE,           /* Build complete */
    BUILD_STEP_FAILED          /* Unrecoverable failure */
} BuildStepType;

/* A single step in the AI build plan */
typedef struct AIBuildStep {
    BuildStepType type;
    char* description;         /* Human-readable description */
    char* command;             /* Command to execute (if applicable) */
    char* target;              /* Target file/package/directory */
    char* content;             /* Content for file modifications */
    char* reason;              /* Why this step is needed */
    bool executed;             /* Has this step been executed? */
    bool success;              /* Did this step succeed? */
    char* error_output;        /* Error output if failed */
    struct AIBuildStep* next;  /* Next step in plan */
} AIBuildStep;

/* The complete build plan */
typedef struct {
    AIBuildStep* steps;        /* Linked list of steps */
    int step_count;            /* Number of steps */
    int current_step;          /* Index of current step */
    char* project_path;        /* Path to project */
    char* summary;             /* AI-generated summary of the plan */
} AIBuildPlan;

/* ========================================================================
 * AI Build Agent
 * ======================================================================== */

typedef struct AIBuildAgent AIBuildAgent;

/* Agent configuration */
typedef struct {
    int max_attempts;          /* Maximum build attempts (default: 5) */
    int max_fix_attempts;      /* Max fixes per error (default: 3) */
    bool verbose;              /* Show detailed output */
    bool auto_install_deps;    /* Automatically install dependencies */
    bool allow_file_mods;      /* Allow AI to modify files */
    bool allow_commands;       /* Allow AI to run arbitrary commands */
    float temperature;         /* LLM temperature (default: 0.2) */
} AIBuildAgentConfig;

/* Create default agent configuration */
AIBuildAgentConfig ai_build_agent_config_default(void);

/* Create the AI build agent */
AIBuildAgent* ai_build_agent_create(AIProvider* ai,
                                     ToolRegistry* tools,
                                     const AIBuildAgentConfig* config);

/* Destroy the AI build agent */
void ai_build_agent_free(AIBuildAgent* agent);

/* ========================================================================
 * Main API - Autonomous Build
 * ======================================================================== */

/**
 * Autonomously build a project.
 *
 * The AI agent will:
 * 1. Analyze the project structure
 * 2. Create a build plan
 * 3. Execute the plan step-by-step
 * 4. When errors occur, analyze and fix them
 * 5. Retry until success or max attempts
 *
 * @param agent The AI build agent
 * @param project_path Path to the project
 * @return Build result (caller must free)
 */
BuildResult* ai_build_agent_build(AIBuildAgent* agent,
                                   const char* project_path);

/**
 * Get AI to analyze a project and create a build plan.
 *
 * @param agent The AI build agent
 * @param ctx Project context
 * @return Build plan (caller must free)
 */
AIBuildPlan* ai_build_agent_plan(AIBuildAgent* agent,
                                  const ProjectContext* ctx);

/**
 * Execute a single build step.
 *
 * @param agent The AI build agent
 * @param step The step to execute
 * @param ctx Project context
 * @return true if step succeeded
 */
bool ai_build_agent_execute_step(AIBuildAgent* agent,
                                  AIBuildStep* step,
                                  const ProjectContext* ctx);

/**
 * Analyze a build error and generate a fix plan.
 *
 * @param agent The AI build agent
 * @param error_output The error output
 * @param ctx Project context
 * @return Build plan with fix steps (caller must free)
 */
AIBuildPlan* ai_build_agent_analyze_error(AIBuildAgent* agent,
                                           const char* error_output,
                                           const ProjectContext* ctx);

/* ========================================================================
 * Build Plan Management
 * ======================================================================== */

/* Create empty build plan */
AIBuildPlan* ai_build_plan_create(const char* project_path);

/* Add a step to the build plan */
void ai_build_plan_add_step(AIBuildPlan* plan, AIBuildStep* step);

/* Create a new build step */
AIBuildStep* ai_build_step_create(BuildStepType type,
                                   const char* description,
                                   const char* command,
                                   const char* target);

/* Free a build step */
void ai_build_step_free(AIBuildStep* step);

/* Free a build plan */
void ai_build_plan_free(AIBuildPlan* plan);

/* Print build plan to console */
void ai_build_plan_print(const AIBuildPlan* plan);

/* Get step type name */
const char* build_step_type_name(BuildStepType type);

/* ========================================================================
 * Prompt Generation for AI Agent
 * ======================================================================== */

/**
 * Generate prompt for build planning.
 * AI analyzes project and generates build steps as JSON.
 */
char* prompt_ai_build_plan(const ProjectContext* ctx,
                           const char* build_output,
                           const char* previous_errors);

/**
 * Generate prompt for error analysis.
 * AI analyzes error and generates fix steps as JSON.
 */
char* prompt_ai_error_fix(const char* error_output,
                          const ProjectContext* ctx,
                          const char* attempted_fixes);

/**
 * Parse AI response into build plan.
 */
AIBuildPlan* parse_ai_build_plan_response(const char* response,
                                           const char* project_path);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_AI_BUILD_AGENT_H */
