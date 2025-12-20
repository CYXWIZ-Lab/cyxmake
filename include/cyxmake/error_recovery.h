/**
 * @file error_recovery.h
 * @brief Automated error recovery system
 */

#ifndef CYXMAKE_ERROR_RECOVERY_H
#define CYXMAKE_ERROR_RECOVERY_H

#include "build_executor.h"
#include "project_context.h"
#include "llm_interface.h"
#include "tool_executor.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Error pattern types
 */
typedef enum {
    ERROR_PATTERN_MISSING_FILE,
    ERROR_PATTERN_MISSING_LIBRARY,
    ERROR_PATTERN_MISSING_HEADER,
    ERROR_PATTERN_PERMISSION_DENIED,
    ERROR_PATTERN_DISK_FULL,
    ERROR_PATTERN_SYNTAX_ERROR,
    ERROR_PATTERN_UNDEFINED_REFERENCE,
    ERROR_PATTERN_VERSION_MISMATCH,
    ERROR_PATTERN_CMAKE_VERSION,          /* CMake minimum version compatibility */
    ERROR_PATTERN_CMAKE_PACKAGE,          /* CMake find_package() failure */
    ERROR_PATTERN_NETWORK_ERROR,
    ERROR_PATTERN_TIMEOUT,
    ERROR_PATTERN_UNKNOWN
} ErrorPatternType;

/**
 * Fix action types
 */
typedef enum {
    FIX_ACTION_INSTALL_PACKAGE,
    FIX_ACTION_CREATE_FILE,
    FIX_ACTION_MODIFY_FILE,
    FIX_ACTION_SET_ENV_VAR,
    FIX_ACTION_RUN_COMMAND,
    FIX_ACTION_CLEAN_BUILD,
    FIX_ACTION_FIX_CMAKE_VERSION,         /* Fix cmake_minimum_required version */
    FIX_ACTION_RETRY,
    FIX_ACTION_NONE
} FixActionType;

/**
 * Error pattern definition
 */
typedef struct {
    ErrorPatternType type;
    const char* name;
    const char** patterns;      /* Array of regex patterns to match */
    size_t pattern_count;
    const char* description;
    int priority;                /* Higher priority patterns are checked first */
} ErrorPattern;

/**
 * Fix action definition
 */
typedef struct {
    FixActionType type;
    const char* description;
    char* command;               /* Command to execute */
    char* target;                /* Target file/package/variable */
    char* value;                 /* Value to set/content to write */
    bool requires_confirmation;  /* Ask user before applying */
} FixAction;

/**
 * Error diagnosis result
 */
typedef struct {
    ErrorPatternType pattern_type;
    char* error_message;         /* Original error message */
    char* diagnosis;             /* Human-readable diagnosis */
    FixAction** suggested_fixes; /* Array of suggested fixes */
    size_t fix_count;
    double confidence;           /* Confidence in diagnosis (0.0-1.0) */
} ErrorDiagnosis;

/**
 * Recovery strategy
 */
typedef struct {
    int max_retries;             /* Maximum retry attempts */
    int retry_delay_ms;          /* Initial retry delay in milliseconds */
    float backoff_multiplier;    /* Exponential backoff multiplier */
    int max_delay_ms;            /* Maximum delay between retries */
    bool use_ai_analysis;        /* Use LLM for complex errors */
    bool auto_apply_fixes;       /* Automatically apply fixes without confirmation */
} RecoveryStrategy;

/**
 * Recovery context
 */
typedef struct RecoveryContext RecoveryContext;

/* ========================================================================
 * Pattern Database
 * ======================================================================== */

/**
 * Initialize error pattern database
 * @return True if initialized successfully
 */
bool error_patterns_init(void);

/**
 * Shutdown pattern database
 */
void error_patterns_shutdown(void);

/**
 * Register a custom error pattern
 * @param pattern Error pattern to register
 * @return True if registered successfully
 */
bool error_patterns_register(const ErrorPattern* pattern);

/**
 * Match error output against patterns
 * @param error_output Error text to analyze
 * @return Matched pattern type, or ERROR_PATTERN_UNKNOWN
 */
ErrorPatternType error_patterns_match(const char* error_output);

/**
 * Get pattern by type
 * @param type Pattern type
 * @return Pattern definition, or NULL if not found
 */
const ErrorPattern* error_patterns_get(ErrorPatternType type);

/* ========================================================================
 * Error Diagnosis
 * ======================================================================== */

/**
 * Diagnose build error
 * @param build_result Build result with error output
 * @param ctx Project context
 * @return Diagnosis result (caller must free with error_diagnosis_free)
 */
ErrorDiagnosis* error_diagnose(const BuildResult* build_result,
                               const ProjectContext* ctx);

/**
 * Free error diagnosis
 * @param diagnosis Diagnosis to free
 */
void error_diagnosis_free(ErrorDiagnosis* diagnosis);

/* ========================================================================
 * Solution Generation
 * ======================================================================== */

/**
 * Generate fix actions for error pattern
 * @param pattern_type Type of error pattern
 * @param error_details Specific error details (e.g., missing library name)
 * @param ctx Project context
 * @return Array of fix actions (caller must free with fix_actions_free)
 */
FixAction** solution_generate(ErrorPatternType pattern_type,
                              const char* error_details,
                              const ProjectContext* ctx,
                              size_t* fix_count);

/**
 * Free fix actions array
 * @param actions Array of fix actions
 * @param count Number of actions
 */
void fix_actions_free(FixAction** actions, size_t count);

/* ========================================================================
 * Fix Execution
 * ======================================================================== */

/**
 * Execute a fix action
 * @param action Fix action to execute
 * @param ctx Project context
 * @return True if fix was applied successfully
 */
bool fix_execute(const FixAction* action, const ProjectContext* ctx);

/**
 * Execute a fix action with tool registry support
 *
 * This variant uses the tool registry for smart package installation,
 * automatically selecting the best package manager for the platform.
 *
 * @param action Fix action to execute
 * @param ctx Project context
 * @param registry Tool registry (optional - falls back to system() if NULL)
 * @return True if fix was applied successfully
 */
bool fix_execute_with_tools(const FixAction* action,
                            const ProjectContext* ctx,
                            const ToolRegistry* registry);

/* Forward declaration for PermissionContext */
#ifndef CYXMAKE_PERMISSION_CONTEXT_FWD
#define CYXMAKE_PERMISSION_CONTEXT_FWD
typedef struct PermissionContext PermissionContext;
#endif

/**
 * Execute fix action with REPL permission system
 *
 * Uses the permission context from the REPL session instead of
 * the simple ask_confirmation() approach. This integrates error
 * recovery with the REPL's interactive approval workflow.
 *
 * @param action Fix action to execute
 * @param ctx Project context
 * @param registry Tool registry (optional)
 * @param permissions Permission context from REPL (required)
 * @return True if fix was applied successfully
 */
bool fix_execute_with_permission(const FixAction* action,
                                  const ProjectContext* ctx,
                                  const ToolRegistry* registry,
                                  PermissionContext* permissions);

/**
 * Execute all fix actions in sequence
 * @param actions Array of fix actions
 * @param count Number of actions
 * @param ctx Project context
 * @return Number of successfully applied fixes
 */
int fix_execute_all(FixAction** actions, size_t count, const ProjectContext* ctx);

/**
 * Execute all fix actions with tool registry support
 * @param actions Array of fix actions
 * @param count Number of actions
 * @param ctx Project context
 * @param registry Tool registry (optional - falls back to system() if NULL)
 * @return Number of successfully applied fixes
 */
int fix_execute_all_with_tools(FixAction** actions, size_t count,
                                const ProjectContext* ctx,
                                const ToolRegistry* registry);

/* ========================================================================
 * LLM-Enhanced Diagnosis
 * ======================================================================== */

/**
 * Diagnose build error with optional LLM analysis
 * 
 * This function first uses local pattern matching. If confidence is low
 * (< 0.6) and an LLM context is provided, it will consult the LLM for
 * deeper analysis.
 *
 * @param build_result Build result with error output
 * @param ctx Project context
 * @param llm_ctx LLM context (optional - NULL for local-only analysis)
 * @return Diagnosis result (caller must free with error_diagnosis_free)
 */
ErrorDiagnosis* error_diagnose_with_llm(const BuildResult* build_result,
                                         const ProjectContext* ctx,
                                         LLMContext* llm_ctx);

/**
 * Get LLM-suggested fix for an error
 * 
 * @param error_output Raw error output text
 * @param ctx Project context
 * @param llm_ctx LLM context (required)
 * @return AI-generated fix suggestion (caller must free), or NULL
 */
char* error_get_llm_suggestion(const char* error_output,
                                const ProjectContext* ctx,
                                LLMContext* llm_ctx);

/**
 * Set LLM context for recovery operations
 *
 * @param recovery_ctx Recovery context
 * @param llm_ctx LLM context to use for AI analysis
 */
void recovery_set_llm(RecoveryContext* recovery_ctx, LLMContext* llm_ctx);
/**
 * Set tool registry for recovery operations
 *
 * The tool registry enables smart package installation using
 * the system's available package managers.
 *
 * @param recovery_ctx Recovery context
 * @param registry Tool registry with discovered package managers
 */
void recovery_set_tools(RecoveryContext* recovery_ctx, ToolRegistry* registry);



/* ========================================================================
 * Recovery Context
 * ======================================================================== */

/**
 * Create recovery context
 * @param strategy Recovery strategy (NULL for defaults)
 * @return Recovery context (caller must free with recovery_context_free)
 */
RecoveryContext* recovery_context_create(const RecoveryStrategy* strategy);

/**
 * Free recovery context
 * @param ctx Recovery context to free
 */
void recovery_context_free(RecoveryContext* ctx);

/**
 * Attempt to recover from build failure
 * @param ctx Recovery context
 * @param build_result Failed build result
 * @param project_ctx Project context
 * @return Recovery result (caller must free)
 */
BuildResult* recovery_attempt(RecoveryContext* ctx,
                              const BuildResult* build_result,
                              ProjectContext* project_ctx);

/**
 * Get recovery statistics
 * @param ctx Recovery context
 * @param total_attempts Output: total recovery attempts
 * @param successful_recoveries Output: successful recoveries
 */
void recovery_get_stats(const RecoveryContext* ctx,
                        int* total_attempts,
                        int* successful_recoveries);

/* ========================================================================
 * Retry Logic
 * ======================================================================== */

/**
 * Execute build with retry logic
 * @param project_ctx Project context
 * @param build_opts Build options
 * @param strategy Recovery strategy
 * @return Final build result (caller must free)
 */
BuildResult* build_with_retry(ProjectContext* project_ctx,
                              const BuildOptions* build_opts,
                              const RecoveryStrategy* strategy);

/**
 * Calculate next retry delay with exponential backoff
 * @param attempt Current attempt number (0-based)
 * @param base_delay_ms Base delay in milliseconds
 * @param multiplier Backoff multiplier
 * @param max_delay_ms Maximum delay
 * @return Delay in milliseconds
 */
int calculate_backoff_delay(int attempt, int base_delay_ms,
                            float multiplier, int max_delay_ms);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_ERROR_RECOVERY_H */