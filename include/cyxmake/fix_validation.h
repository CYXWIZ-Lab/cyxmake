/**
 * @file fix_validation.h
 * @brief Enhanced error recovery with validation, verification, and learning
 *
 * Phase 3: Error Recovery enhancements
 * - Validate fixes before applying
 * - User confirmation for risky fixes
 * - Incremental fix application
 * - Fix verification (rebuild after fix)
 * - Learn from successful fixes
 */

#ifndef CYXMAKE_FIX_VALIDATION_H
#define CYXMAKE_FIX_VALIDATION_H

#include "error_recovery.h"
#include "security.h"
#include "permission.h"
#include "tool_executor.h"
#include "build_executor.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Fix Validation
 * ======================================================================== */

/**
 * Validation result for a fix action
 */
typedef enum {
    VALIDATION_PASSED,           /* Fix is likely to succeed */
    VALIDATION_WARNING,          /* Fix may have issues but can proceed */
    VALIDATION_FAILED,           /* Fix will definitely fail */
    VALIDATION_SKIPPED           /* Validation not applicable */
} ValidationStatus;

/**
 * Detailed validation result
 */
typedef struct {
    ValidationStatus status;
    char* message;               /* Human-readable validation message */
    char* details;               /* Technical details */
    bool can_proceed;            /* Whether to allow proceeding despite issues */
    double confidence;           /* Confidence in validation result (0.0-1.0) */
} ValidationResult;

/**
 * Fix validator context
 */
typedef struct FixValidator FixValidator;

/**
 * Create fix validator
 * @param registry Tool registry for checking tool availability
 * @return Validator instance (caller must free with fix_validator_free)
 */
FixValidator* fix_validator_create(const ToolRegistry* registry);

/**
 * Free fix validator
 * @param validator Validator to free
 */
void fix_validator_free(FixValidator* validator);

/**
 * Validate a fix action before applying
 * @param validator Validator instance
 * @param action Fix action to validate
 * @param ctx Project context
 * @return Validation result (caller must free with validation_result_free)
 */
ValidationResult* fix_validate(FixValidator* validator,
                               const FixAction* action,
                               const ProjectContext* ctx);

/**
 * Free validation result
 * @param result Result to free
 */
void validation_result_free(ValidationResult* result);

/* ========================================================================
 * Risk Assessment
 * ======================================================================== */

/**
 * Risk level for a fix action
 */
typedef enum {
    RISK_NONE,                   /* No risk (e.g., retry) */
    RISK_LOW,                    /* Low risk (e.g., set env var) */
    RISK_MEDIUM,                 /* Medium risk (e.g., modify project files) */
    RISK_HIGH,                   /* High risk (e.g., install packages) */
    RISK_CRITICAL                /* Critical risk (e.g., system modifications) */
} RiskLevel;

/**
 * Risk assessment for a fix action
 */
typedef struct {
    RiskLevel level;
    char* description;           /* Risk description */
    bool requires_confirmation;  /* Must confirm with user */
    bool requires_backup;        /* Should backup before applying */
    bool is_reversible;          /* Can be rolled back */
    char** affected_files;       /* Files that will be modified */
    size_t affected_count;
} RiskAssessment;

/**
 * Assess risk of a fix action
 * @param action Fix action to assess
 * @param ctx Project context
 * @return Risk assessment (caller must free with risk_assessment_free)
 */
RiskAssessment* fix_assess_risk(const FixAction* action,
                                const ProjectContext* ctx);

/**
 * Free risk assessment
 * @param assessment Assessment to free
 */
void risk_assessment_free(RiskAssessment* assessment);

/* ========================================================================
 * Incremental Fix Application
 * ======================================================================== */

/**
 * Fix application result
 */
typedef enum {
    FIX_RESULT_SUCCESS,          /* Fix applied successfully */
    FIX_RESULT_FAILED,           /* Fix failed to apply */
    FIX_RESULT_SKIPPED,          /* Fix skipped (user declined or validation failed) */
    FIX_RESULT_ROLLED_BACK       /* Fix was rolled back after failure */
} FixResultStatus;

/**
 * Result of applying a single fix
 */
typedef struct {
    FixResultStatus status;
    const FixAction* action;     /* The fix that was applied */
    char* message;               /* Result message */
    double duration_ms;          /* Time taken to apply */
    bool verified;               /* Whether fix was verified with rebuild */
} FixApplicationResult;

/**
 * Incremental fix session
 */
typedef struct IncrementalFixSession IncrementalFixSession;

/**
 * Create incremental fix session
 * @param ctx Project context
 * @param registry Tool registry
 * @param rollback Rollback manager for undo support
 * @param security Security context (optional)
 * @return Session instance (caller must free)
 */
IncrementalFixSession* incremental_fix_session_create(
    ProjectContext* ctx,
    const ToolRegistry* registry,
    RollbackManager* rollback,
    SecurityContext* security);

/**
 * Free incremental fix session
 * @param session Session to free
 */
void incremental_fix_session_free(IncrementalFixSession* session);

/**
 * Apply fixes incrementally with validation and verification
 *
 * For each fix:
 * 1. Validate the fix
 * 2. Assess risk and get user confirmation if needed
 * 3. Create backup if required
 * 4. Apply the fix
 * 5. Verify with rebuild (optional)
 * 6. Record result
 *
 * @param session Fix session
 * @param fixes Array of fixes to apply
 * @param fix_count Number of fixes
 * @param verify_each Whether to rebuild after each fix
 * @param stop_on_failure Whether to stop after first failure
 * @return Number of successfully applied fixes
 */
int incremental_fix_apply(IncrementalFixSession* session,
                          FixAction** fixes,
                          size_t fix_count,
                          bool verify_each,
                          bool stop_on_failure);

/**
 * Get results from incremental fix session
 * @param session Fix session
 * @param count Output: number of results
 * @return Array of results (owned by session, do not free)
 */
const FixApplicationResult* incremental_fix_get_results(
    const IncrementalFixSession* session,
    size_t* count);

/**
 * Rollback all applied fixes in session
 * @param session Fix session
 * @return Number of fixes rolled back
 */
int incremental_fix_rollback_all(IncrementalFixSession* session);

/* ========================================================================
 * Fix Verification
 * ======================================================================== */

/**
 * Verification result
 */
typedef enum {
    VERIFY_SUCCESS,              /* Error is fixed */
    VERIFY_PARTIAL,              /* Some errors fixed, others remain */
    VERIFY_FAILED,               /* Error still present */
    VERIFY_NEW_ERRORS,           /* Fix introduced new errors */
    VERIFY_BUILD_FAILED          /* Build failed to run */
} VerifyStatus;

/**
 * Detailed verification result
 */
typedef struct {
    VerifyStatus status;
    char* original_error;        /* Original error message */
    char* current_error;         /* Current error (if any) */
    int original_error_count;    /* Errors before fix */
    int current_error_count;     /* Errors after fix */
    double build_time_ms;        /* Time to run verification build */
} VerifyResult;

/**
 * Verify a fix by rebuilding and checking for the original error
 * @param ctx Project context
 * @param original_diagnosis Original error diagnosis
 * @param build_opts Build options for verification build
 * @return Verification result (caller must free with verify_result_free)
 */
VerifyResult* fix_verify(ProjectContext* ctx,
                         const ErrorDiagnosis* original_diagnosis,
                         const BuildOptions* build_opts);

/**
 * Free verification result
 * @param result Result to free
 */
void verify_result_free(VerifyResult* result);

/* ========================================================================
 * Fix Learning System
 * ======================================================================== */

/**
 * Record of a successful fix
 */
typedef struct {
    ErrorPatternType error_type; /* Type of error that was fixed */
    char* error_signature;       /* Unique signature of the error */
    FixActionType fix_type;      /* Type of fix that worked */
    char* fix_command;           /* Command or action that fixed it */
    char* fix_target;            /* Target of the fix */
    char* project_type;          /* Type of project (C, C++, etc.) */
    char* build_system;          /* Build system (CMake, Make, etc.) */
    int success_count;           /* Number of times this fix succeeded */
    int failure_count;           /* Number of times this fix failed */
    time_t first_seen;           /* When first encountered */
    time_t last_seen;            /* When last encountered */
    double avg_fix_time_ms;      /* Average time to apply fix */
} FixHistoryEntry;

/**
 * Fix history database
 */
typedef struct FixHistory FixHistory;

/**
 * Create/open fix history database
 * @param history_path Path to history file (NULL for default ~/.cyxmake/fix_history.json)
 * @return History instance (caller must free with fix_history_free)
 */
FixHistory* fix_history_create(const char* history_path);

/**
 * Free fix history
 * @param history History to free
 */
void fix_history_free(FixHistory* history);

/**
 * Record a fix attempt
 * @param history History database
 * @param diagnosis Error diagnosis
 * @param action Fix action attempted
 * @param success Whether the fix succeeded
 * @param fix_time_ms Time taken to apply fix
 */
void fix_history_record(FixHistory* history,
                        const ErrorDiagnosis* diagnosis,
                        const FixAction* action,
                        bool success,
                        double fix_time_ms);

/**
 * Look up fixes that have worked for similar errors
 * @param history History database
 * @param diagnosis Current error diagnosis
 * @param count Output: number of entries found
 * @return Array of history entries (caller must free with fix_history_entries_free)
 */
FixHistoryEntry** fix_history_lookup(const FixHistory* history,
                                     const ErrorDiagnosis* diagnosis,
                                     size_t* count);

/**
 * Get suggested fix based on history
 * @param history History database
 * @param diagnosis Current error diagnosis
 * @return Best fix suggestion (caller must free), or NULL if no history
 */
FixAction* fix_history_suggest(const FixHistory* history,
                               const ErrorDiagnosis* diagnosis);

/**
 * Free history entries array
 * @param entries Entries to free
 * @param count Number of entries
 */
void fix_history_entries_free(FixHistoryEntry** entries, size_t count);

/**
 * Save history to disk
 * @param history History to save
 * @return true if saved successfully
 */
bool fix_history_save(FixHistory* history);

/**
 * Get statistics from fix history
 * @param history History database
 * @param total_fixes Output: total fix attempts
 * @param successful_fixes Output: successful fixes
 * @param unique_errors Output: unique error types seen
 */
void fix_history_stats(const FixHistory* history,
                       int* total_fixes,
                       int* successful_fixes,
                       int* unique_errors);

/* ========================================================================
 * Enhanced Recovery Context
 * ======================================================================== */

/**
 * Enhanced recovery options
 */
typedef struct {
    bool validate_before_apply;  /* Validate fixes before applying */
    bool verify_after_apply;     /* Rebuild to verify fix worked */
    bool incremental_apply;      /* Apply fixes one at a time */
    bool use_history;            /* Use fix history for suggestions */
    bool record_history;         /* Record fix attempts to history */
    bool auto_rollback;          /* Rollback on verification failure */
    RiskLevel max_auto_risk;     /* Maximum risk level for auto-apply */
} EnhancedRecoveryOptions;

/**
 * Create default enhanced recovery options
 * @return Default options
 */
EnhancedRecoveryOptions enhanced_recovery_defaults(void);

/**
 * Create enhanced recovery context
 * @param strategy Base recovery strategy
 * @param options Enhanced options
 * @param registry Tool registry
 * @param rollback Rollback manager
 * @param security Security context
 * @param history Fix history
 * @return Recovery context (caller must free)
 */
RecoveryContext* enhanced_recovery_create(
    const RecoveryStrategy* strategy,
    const EnhancedRecoveryOptions* options,
    const ToolRegistry* registry,
    RollbackManager* rollback,
    SecurityContext* security,
    FixHistory* history);

/**
 * Attempt recovery with validation and verification
 * @param ctx Recovery context
 * @param build_result Failed build result
 * @param project_ctx Project context
 * @return New build result after recovery attempts
 */
BuildResult* enhanced_recovery_attempt(RecoveryContext* ctx,
                                        const BuildResult* build_result,
                                        ProjectContext* project_ctx);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_FIX_VALIDATION_H */
