/**
 * @file permission.h
 * @brief Permission system for REPL actions
 *
 * Provides a permission model where dangerous operations
 * require user approval before execution.
 */

#ifndef CYXMAKE_PERMISSION_H
#define CYXMAKE_PERMISSION_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Permission levels for actions
 */
typedef enum {
    PERM_SAFE,          /* Execute immediately, no prompt */
    PERM_ASK,           /* Show prompt, wait for Y/N */
    PERM_DANGEROUS,     /* Show warning, require explicit confirmation */
    PERM_BLOCKED        /* Never allow (system files, etc.) */
} PermissionLevel;

/**
 * Action types that may require permission
 */
typedef enum {
    ACTION_READ_FILE,       /* Safe - no permission needed */
    ACTION_BUILD,           /* Safe */
    ACTION_ANALYZE,         /* Safe */
    ACTION_STATUS,          /* Safe */
    ACTION_CLEAN,           /* Ask - deletes build dirs */

    ACTION_CREATE_FILE,     /* Ask permission */
    ACTION_MODIFY_FILE,     /* Ask permission */
    ACTION_DELETE_FILE,     /* Ask permission */
    ACTION_INSTALL_PKG,     /* Ask permission */
    ACTION_RUN_COMMAND,     /* Ask permission */

    ACTION_DELETE_DIR,      /* Dangerous - explicit confirm */
    ACTION_SYSTEM_MODIFY    /* Dangerous */
} ActionType;

/**
 * Permission response from user
 */
typedef enum {
    PERM_RESPONSE_YES,      /* Allow this action */
    PERM_RESPONSE_NO,       /* Deny this action */
    PERM_RESPONSE_ALWAYS,   /* Always allow this action type */
    PERM_RESPONSE_NEVER,    /* Never allow this action type */
    PERM_RESPONSE_VIEW      /* View more details */
} PermissionResponse;

/**
 * Permission request structure
 */
typedef struct {
    ActionType action;
    const char* description;    /* Human-readable action description */
    const char* target;         /* File/package/command target */
    const char* reason;         /* Why this action is being requested */
    const char* details;        /* Additional details (file content, etc.) */
} PermissionRequest;

/**
 * Permission context for session
 */
struct PermissionContext {
    /* Auto-approve settings (per-session) */
    bool auto_approve_read;
    bool auto_approve_build;
    bool auto_approve_clean;
    bool auto_approve_create;
    bool auto_approve_modify;
    bool auto_approve_delete;
    bool auto_approve_install;
    bool auto_approve_command;

    /* Blocked paths (never allow) */
    const char** blocked_paths;
    int blocked_count;

    /* Use colors in prompts */
    bool colors_enabled;

    /* Audit log callback */
    void (*audit_callback)(const PermissionRequest* req, PermissionResponse resp);
};

typedef struct PermissionContext PermissionContext;

/**
 * Create default permission context
 * @return New permission context with safe defaults
 */
PermissionContext* permission_context_create(void);

/**
 * Free permission context
 * @param ctx Context to free
 */
void permission_context_free(PermissionContext* ctx);

/**
 * Get permission level for an action type
 * @param action Action type
 * @return Permission level
 */
PermissionLevel permission_get_level(ActionType action);

/**
 * Check if action requires permission
 * @param ctx Permission context
 * @param action Action type
 * @return true if permission needed, false if auto-approved
 */
bool permission_needs_prompt(PermissionContext* ctx, ActionType action);

/**
 * Request permission from user
 * @param ctx Permission context
 * @param request Permission request details
 * @return User's response
 */
PermissionResponse permission_request(PermissionContext* ctx, const PermissionRequest* request);

/**
 * Quick permission check with prompt
 * @param ctx Permission context
 * @param action Action type
 * @param target Target file/package/etc
 * @param reason Why the action is needed
 * @return true if allowed, false if denied
 */
bool permission_check(PermissionContext* ctx, ActionType action,
                      const char* target, const char* reason);

/**
 * Check if a path is blocked
 * @param ctx Permission context
 * @param path Path to check
 * @return true if blocked, false if allowed
 */
bool permission_is_blocked(PermissionContext* ctx, const char* path);

/**
 * Add a blocked path
 * @param ctx Permission context
 * @param path Path to block
 */
void permission_block_path(PermissionContext* ctx, const char* path);

/**
 * Get action type name for display
 * @param action Action type
 * @return Human-readable name
 */
const char* permission_action_name(ActionType action);

/**
 * Update auto-approve setting for action
 * @param ctx Permission context
 * @param action Action type
 * @param auto_approve Whether to auto-approve
 */
void permission_set_auto_approve(PermissionContext* ctx, ActionType action, bool auto_approve);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_PERMISSION_H */
