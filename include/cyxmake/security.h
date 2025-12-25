/**
 * @file security.h
 * @brief Security module for audit logging, dry-run mode, and rollback support
 *
 * This module provides:
 * - Comprehensive audit logging of all actions
 * - Dry-run mode for testing operations without side effects
 * - File modification rollback support
 * - Security policy enforcement
 */

#ifndef CYXMAKE_SECURITY_H
#define CYXMAKE_SECURITY_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include "permission.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Audit Logging
 * ======================================================================== */

/**
 * Audit log entry severity
 */
typedef enum {
    AUDIT_DEBUG,        /* Debug information */
    AUDIT_INFO,         /* Informational */
    AUDIT_WARNING,      /* Warning - action may have risks */
    AUDIT_ACTION,       /* User action performed */
    AUDIT_DENIED,       /* Action was denied */
    AUDIT_ERROR,        /* Error occurred */
    AUDIT_SECURITY      /* Security-related event */
} AuditSeverity;

/**
 * Audit log entry
 */
typedef struct {
    time_t timestamp;           /* When the event occurred */
    AuditSeverity severity;     /* Event severity */
    ActionType action;          /* Action type (from permission.h) */
    char* description;          /* Human-readable description */
    char* target;               /* Target file/resource */
    char* user;                 /* User who initiated (if known) */
    char* details;              /* Additional details (JSON) */
    bool success;               /* Whether action succeeded */
    int exit_code;              /* Exit code if applicable */
} AuditEntry;

/**
 * Audit log configuration
 */
typedef struct {
    bool enabled;               /* Enable audit logging */
    const char* log_file;       /* Path to audit log file */
    bool log_to_console;        /* Also log to console */
    AuditSeverity min_severity; /* Minimum severity to log */
    bool include_timestamps;    /* Include timestamps in log */
    bool include_user;          /* Include user information */
    int max_entries;            /* Max entries to keep in memory (0 = unlimited) */
    int rotation_size_mb;       /* Log rotation size in MB (0 = no rotation) */
} AuditConfig;

/**
 * Audit logger instance
 */
typedef struct AuditLogger AuditLogger;

/**
 * Create audit logger with configuration
 * @param config Audit configuration
 * @return New audit logger (caller must free)
 */
AuditLogger* audit_logger_create(const AuditConfig* config);

/**
 * Create audit logger with default configuration
 * @return New audit logger (caller must free)
 */
AuditLogger* audit_logger_create_default(void);

/**
 * Free audit logger
 * @param logger Logger to free
 */
void audit_logger_free(AuditLogger* logger);

/**
 * Log an audit entry
 * @param logger Audit logger
 * @param entry Entry to log
 */
void audit_log(AuditLogger* logger, const AuditEntry* entry);

/**
 * Log a simple action
 * @param logger Audit logger
 * @param severity Severity level
 * @param action Action type
 * @param target Target resource
 * @param description Description
 * @param success Whether action succeeded
 */
void audit_log_action(AuditLogger* logger, AuditSeverity severity,
                       ActionType action, const char* target,
                       const char* description, bool success);

/**
 * Log a permission decision
 * @param logger Audit logger
 * @param request Permission request
 * @param response User's response
 */
void audit_log_permission(AuditLogger* logger, const PermissionRequest* request,
                          PermissionResponse response);

/**
 * Log a command execution
 * @param logger Audit logger
 * @param command Command executed
 * @param args Command arguments
 * @param exit_code Exit code
 * @param success Whether command succeeded
 */
void audit_log_command(AuditLogger* logger, const char* command,
                        const char* args, int exit_code, bool success);

/**
 * Log a security event
 * @param logger Audit logger
 * @param event Event description
 * @param details Additional details
 */
void audit_log_security(AuditLogger* logger, const char* event,
                         const char* details);

/**
 * Get recent audit entries
 * @param logger Audit logger
 * @param count Number of entries to retrieve
 * @param out_entries Output array of entries (caller provides)
 * @return Number of entries returned
 */
int audit_get_recent(AuditLogger* logger, int count, AuditEntry** out_entries);

/**
 * Export audit log to file
 * @param logger Audit logger
 * @param filepath Output file path
 * @param format Output format ("json", "csv", "text")
 * @return true on success
 */
bool audit_export(AuditLogger* logger, const char* filepath, const char* format);

/**
 * Free an audit entry
 * @param entry Entry to free
 */
void audit_entry_free(AuditEntry* entry);

/**
 * Get severity name as string
 * @param severity Severity level
 * @return Severity name
 */
const char* audit_severity_name(AuditSeverity severity);

/* ========================================================================
 * Dry-Run Mode
 * ======================================================================== */

/**
 * Dry-run context
 */
typedef struct DryRunContext DryRunContext;

/**
 * Dry-run action record
 */
typedef struct {
    ActionType action;
    char* description;
    char* target;
    char* command;              /* Command that would be executed */
    char* expected_result;      /* Expected result description */
    bool would_succeed;         /* Whether action would likely succeed */
    char* potential_issues;     /* Potential issues identified */
} DryRunAction;

/**
 * Create dry-run context
 * @return New dry-run context (caller must free)
 */
DryRunContext* dry_run_create(void);

/**
 * Free dry-run context
 * @param ctx Context to free
 */
void dry_run_free(DryRunContext* ctx);

/**
 * Check if dry-run mode is enabled
 * @param ctx Dry-run context
 * @return true if dry-run mode is active
 */
bool dry_run_is_enabled(DryRunContext* ctx);

/**
 * Enable/disable dry-run mode
 * @param ctx Dry-run context
 * @param enabled Enable or disable
 */
void dry_run_set_enabled(DryRunContext* ctx, bool enabled);

/**
 * Record a dry-run action (instead of executing)
 * @param ctx Dry-run context
 * @param action Action that would be performed
 */
void dry_run_record(DryRunContext* ctx, const DryRunAction* action);

/**
 * Record a file operation
 * @param ctx Dry-run context
 * @param action Action type
 * @param filepath Target file path
 * @param description What would happen
 */
void dry_run_record_file(DryRunContext* ctx, ActionType action,
                          const char* filepath, const char* description);

/**
 * Record a command execution
 * @param ctx Dry-run context
 * @param command Command that would run
 * @param working_dir Working directory
 */
void dry_run_record_command(DryRunContext* ctx, const char* command,
                             const char* working_dir);

/**
 * Get all recorded dry-run actions
 * @param ctx Dry-run context
 * @param count Output: number of actions
 * @return Array of actions (do not free individual actions)
 */
const DryRunAction** dry_run_get_actions(DryRunContext* ctx, int* count);

/**
 * Print dry-run summary
 * @param ctx Dry-run context
 */
void dry_run_print_summary(DryRunContext* ctx);

/**
 * Clear recorded actions
 * @param ctx Dry-run context
 */
void dry_run_clear(DryRunContext* ctx);

/**
 * Free a dry-run action
 * @param action Action to free
 */
void dry_run_action_free(DryRunAction* action);

/* ========================================================================
 * Rollback Support
 * ======================================================================== */

/**
 * Rollback entry type
 */
typedef enum {
    ROLLBACK_FILE_CREATE,       /* File was created - rollback deletes it */
    ROLLBACK_FILE_MODIFY,       /* File was modified - rollback restores original */
    ROLLBACK_FILE_DELETE,       /* File was deleted - rollback restores it */
    ROLLBACK_DIR_CREATE,        /* Directory was created - rollback deletes it */
    ROLLBACK_DIR_DELETE,        /* Directory was deleted - rollback recreates it */
    ROLLBACK_COMMAND            /* Command was executed - no auto-rollback */
} RollbackType;

/**
 * Rollback entry
 */
typedef struct {
    RollbackType type;
    char* filepath;             /* Target file/directory */
    char* backup_path;          /* Path to backup (for modify/delete) */
    char* original_content;     /* Original content (for small files) */
    size_t original_size;       /* Original file size */
    time_t timestamp;           /* When the action occurred */
    char* description;          /* Human-readable description */
    bool can_rollback;          /* Whether rollback is possible */
} RollbackEntry;

/**
 * Rollback manager
 */
typedef struct RollbackManager RollbackManager;

/**
 * Rollback configuration
 */
typedef struct {
    bool enabled;               /* Enable rollback support */
    const char* backup_dir;     /* Directory for backups */
    int max_entries;            /* Maximum rollback entries (0 = unlimited) */
    size_t max_file_size;       /* Max file size to backup in memory (bytes) */
    bool backup_large_files;    /* Backup large files to disk */
    int retention_hours;        /* How long to keep backups (0 = forever) */
} RollbackConfig;

/**
 * Create rollback manager with configuration
 * @param config Rollback configuration
 * @return New rollback manager (caller must free)
 */
RollbackManager* rollback_create(const RollbackConfig* config);

/**
 * Create rollback manager with default configuration
 * @return New rollback manager (caller must free)
 */
RollbackManager* rollback_create_default(void);

/**
 * Free rollback manager
 * @param mgr Manager to free
 */
void rollback_free(RollbackManager* mgr);

/**
 * Check if rollback is enabled
 * @param mgr Rollback manager
 * @return true if rollback is enabled
 */
bool rollback_is_enabled(RollbackManager* mgr);

/**
 * Backup a file before modification
 * @param mgr Rollback manager
 * @param filepath File to backup
 * @param type Type of modification that will occur
 * @return true on success
 */
bool rollback_backup_file(RollbackManager* mgr, const char* filepath,
                          RollbackType type);

/**
 * Record a file creation (for rollback = delete)
 * @param mgr Rollback manager
 * @param filepath File that was created
 * @return true on success
 */
bool rollback_record_create(RollbackManager* mgr, const char* filepath);

/**
 * Record a directory creation
 * @param mgr Rollback manager
 * @param dirpath Directory that was created
 * @return true on success
 */
bool rollback_record_mkdir(RollbackManager* mgr, const char* dirpath);

/**
 * Rollback a specific entry
 * @param mgr Rollback manager
 * @param index Entry index (0 = most recent)
 * @return true on success
 */
bool rollback_entry(RollbackManager* mgr, int index);

/**
 * Rollback all changes since a timestamp
 * @param mgr Rollback manager
 * @param since Timestamp to rollback to
 * @return Number of entries rolled back
 */
int rollback_since(RollbackManager* mgr, time_t since);

/**
 * Rollback the last N operations
 * @param mgr Rollback manager
 * @param count Number of operations to rollback
 * @return Number of entries rolled back
 */
int rollback_last(RollbackManager* mgr, int count);

/**
 * Get rollback history
 * @param mgr Rollback manager
 * @param count Output: number of entries
 * @return Array of entries (do not free)
 */
const RollbackEntry** rollback_get_history(RollbackManager* mgr, int* count);

/**
 * Print rollback history
 * @param mgr Rollback manager
 */
void rollback_print_history(RollbackManager* mgr);

/**
 * Clear rollback history
 * @param mgr Rollback manager
 */
void rollback_clear(RollbackManager* mgr);

/**
 * Clean up old backups
 * @param mgr Rollback manager
 * @return Number of backups cleaned up
 */
int rollback_cleanup(RollbackManager* mgr);

/* ========================================================================
 * Security Context (Unified)
 * ======================================================================== */

/**
 * Unified security context
 * Combines permission, audit, dry-run, and rollback functionality
 */
typedef struct {
    PermissionContext* permissions;
    AuditLogger* audit;
    DryRunContext* dry_run;
    RollbackManager* rollback;
    bool initialized;
} SecurityContext;

/**
 * Security context configuration
 */
typedef struct {
    bool enable_permissions;
    bool enable_audit;
    bool enable_dry_run;
    bool enable_rollback;
    AuditConfig audit_config;
    RollbackConfig rollback_config;
} SecurityConfig;

/**
 * Get default security configuration
 * @return Default security config
 */
SecurityConfig security_config_default(void);

/**
 * Create security context with configuration
 * @param config Security configuration
 * @return New security context (caller must free)
 */
SecurityContext* security_context_create(const SecurityConfig* config);

/**
 * Create security context with defaults
 * @return New security context (caller must free)
 */
SecurityContext* security_context_create_default(void);

/**
 * Free security context
 * @param ctx Context to free
 */
void security_context_free(SecurityContext* ctx);

/**
 * Check permission with audit logging
 * @param ctx Security context
 * @param action Action type
 * @param target Target resource
 * @param reason Reason for action
 * @return true if allowed
 */
bool security_check_permission(SecurityContext* ctx, ActionType action,
                                const char* target, const char* reason);

/**
 * Execute a file operation with security checks
 * Handles permissions, dry-run, audit, and rollback
 * @param ctx Security context
 * @param action Action type
 * @param filepath Target file
 * @param callback Function to perform the operation
 * @param callback_data Data passed to callback
 * @return true if operation succeeded or was simulated in dry-run
 */
typedef bool (*SecurityFileCallback)(const char* filepath, void* data);
bool security_file_operation(SecurityContext* ctx, ActionType action,
                              const char* filepath, SecurityFileCallback callback,
                              void* callback_data);

/**
 * Execute a command with security checks
 * @param ctx Security context
 * @param command Command to execute
 * @param args Command arguments
 * @param working_dir Working directory
 * @param callback Function to perform execution
 * @param callback_data Data passed to callback
 * @return true if command succeeded or was simulated in dry-run
 */
typedef bool (*SecurityCommandCallback)(const char* command, const char* args,
                                          const char* working_dir, void* data);
bool security_execute_command(SecurityContext* ctx, const char* command,
                               const char* args, const char* working_dir,
                               SecurityCommandCallback callback, void* callback_data);

/**
 * Print security status
 * @param ctx Security context
 */
void security_print_status(SecurityContext* ctx);

/* ========================================================================
 * Sandboxed Command Execution
 * ======================================================================== */

/**
 * Sandbox restriction level
 */
typedef enum {
    SANDBOX_NONE,           /* No sandboxing (full access) */
    SANDBOX_LIGHT,          /* Light restrictions (no system writes) */
    SANDBOX_MEDIUM,         /* Medium restrictions (limited paths) */
    SANDBOX_STRICT          /* Strict restrictions (read-only, no network) */
} SandboxLevel;

/**
 * Sandbox configuration
 */
typedef struct {
    SandboxLevel level;             /* Restriction level */
    bool allow_network;             /* Allow network access */
    bool allow_subprocesses;        /* Allow spawning child processes */
    const char** allowed_read_paths;  /* Paths allowed for reading */
    int allowed_read_count;
    const char** allowed_write_paths; /* Paths allowed for writing */
    int allowed_write_count;
    int max_memory_mb;              /* Max memory in MB (0 = unlimited) */
    int max_cpu_sec;                /* Max CPU time in seconds (0 = unlimited) */
    int max_file_descriptors;       /* Max open file descriptors */
} SandboxConfig;

/**
 * Sandbox execution result
 */
typedef struct {
    bool success;               /* Command succeeded */
    int exit_code;              /* Exit code */
    char* stdout_output;        /* Captured stdout */
    char* stderr_output;        /* Captured stderr */
    bool was_killed;            /* Was killed due to resource limits */
    char* kill_reason;          /* Reason for kill (if applicable) */
    double cpu_time_used;       /* CPU time used in seconds */
    size_t memory_used;         /* Peak memory used in bytes */
} SandboxResult;

/**
 * Get default sandbox configuration for a level
 * @param level Sandbox level
 * @return Default sandbox config
 */
SandboxConfig sandbox_config_default(SandboxLevel level);

/**
 * Execute a command in a sandbox
 * @param command Command to execute
 * @param args Command arguments (NULL-terminated array)
 * @param working_dir Working directory
 * @param config Sandbox configuration
 * @return Sandbox result (caller must free with sandbox_result_free)
 */
SandboxResult* sandbox_execute(const char* command,
                                char* const* args,
                                const char* working_dir,
                                const SandboxConfig* config);

/**
 * Free sandbox result
 * @param result Result to free
 */
void sandbox_result_free(SandboxResult* result);

/**
 * Check if sandboxing is available on this platform
 * @return true if sandboxing is available
 */
bool sandbox_is_available(void);

/**
 * Get sandbox capability message
 * @return Human-readable string describing sandbox capabilities
 */
const char* sandbox_capability_message(void);

/**
 * Get sandbox level name as string
 * @param level Sandbox level
 * @return Level name
 */
const char* sandbox_level_name(SandboxLevel level);

/**
 * Check if a path is allowed in sandbox
 * @param config Sandbox configuration
 * @param path Path to check
 * @param for_write Check for write access (false = read)
 * @return true if path is allowed
 */
bool sandbox_path_allowed(const SandboxConfig* config, const char* path,
                           bool for_write);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_SECURITY_H */
