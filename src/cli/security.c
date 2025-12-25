/**
 * @file security.c
 * @brief Security module implementation
 */

#include "cyxmake/security.h"
#include "cyxmake/logger.h"
#include "cyxmake/compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

/* Maximum entries for arrays */
#define MAX_AUDIT_ENTRIES 1000
#define MAX_DRY_RUN_ACTIONS 500
#define MAX_ROLLBACK_ENTRIES 100
#define MAX_FILE_BACKUP_SIZE (1024 * 1024)  /* 1MB */

/* ========================================================================
 * Audit Logger Implementation
 * ======================================================================== */

struct AuditLogger {
    AuditConfig config;
    AuditEntry** entries;
    int entry_count;
    int entry_capacity;
    FILE* log_file;
};

const char* audit_severity_name(AuditSeverity severity) {
    switch (severity) {
        case AUDIT_DEBUG:    return "DEBUG";
        case AUDIT_INFO:     return "INFO";
        case AUDIT_WARNING:  return "WARNING";
        case AUDIT_ACTION:   return "ACTION";
        case AUDIT_DENIED:   return "DENIED";
        case AUDIT_ERROR:    return "ERROR";
        case AUDIT_SECURITY: return "SECURITY";
        default:             return "UNKNOWN";
    }
}

static AuditEntry* audit_entry_create(void) {
    AuditEntry* entry = calloc(1, sizeof(AuditEntry));
    if (entry) {
        entry->timestamp = time(NULL);
    }
    return entry;
}

void audit_entry_free(AuditEntry* entry) {
    if (!entry) return;
    free(entry->description);
    free(entry->target);
    free(entry->user);
    free(entry->details);
    free(entry);
}

static void audit_write_entry(AuditLogger* logger, const AuditEntry* entry) {
    if (!logger || !entry) return;

    char time_buf[64] = "";
    if (logger->config.include_timestamps) {
        struct tm* tm_info = localtime(&entry->timestamp);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    }

    /* Format log line */
    char log_line[1024];
    snprintf(log_line, sizeof(log_line), "[%s] [%s] %s%s%s%s\n",
             time_buf,
             audit_severity_name(entry->severity),
             permission_action_name(entry->action),
             entry->target ? " -> " : "",
             entry->target ? entry->target : "",
             entry->description ? entry->description : "");

    /* Write to file */
    if (logger->log_file) {
        fputs(log_line, logger->log_file);
        fflush(logger->log_file);
    }

    /* Write to console */
    if (logger->config.log_to_console) {
        switch (entry->severity) {
            case AUDIT_ERROR:
            case AUDIT_SECURITY:
                log_error("[AUDIT] %s", log_line);
                break;
            case AUDIT_WARNING:
            case AUDIT_DENIED:
                log_warning("[AUDIT] %s", log_line);
                break;
            default:
                log_info("[AUDIT] %s", log_line);
                break;
        }
    }
}

AuditLogger* audit_logger_create(const AuditConfig* config) {
    AuditLogger* logger = calloc(1, sizeof(AuditLogger));
    if (!logger) return NULL;

    logger->config = *config;
    logger->entry_capacity = config->max_entries > 0 ? config->max_entries : MAX_AUDIT_ENTRIES;
    logger->entries = calloc(logger->entry_capacity, sizeof(AuditEntry*));

    if (config->enabled && config->log_file) {
        logger->log_file = fopen(config->log_file, "a");
        if (!logger->log_file) {
            log_warning("Could not open audit log file: %s", config->log_file);
        }
    }

    return logger;
}

AuditLogger* audit_logger_create_default(void) {
    AuditConfig config = {
        .enabled = true,
        .log_file = ".cyxmake/audit.log",
        .log_to_console = false,
        .min_severity = AUDIT_ACTION,
        .include_timestamps = true,
        .include_user = true,
        .max_entries = MAX_AUDIT_ENTRIES,
        .rotation_size_mb = 10
    };
    return audit_logger_create(&config);
}

void audit_logger_free(AuditLogger* logger) {
    if (!logger) return;

    if (logger->log_file) {
        fclose(logger->log_file);
    }

    if (logger->entries) {
        for (int i = 0; i < logger->entry_count; i++) {
            audit_entry_free(logger->entries[i]);
        }
        free(logger->entries);
    }

    free(logger);
}

void audit_log(AuditLogger* logger, const AuditEntry* entry) {
    if (!logger || !entry || !logger->config.enabled) return;

    /* Check minimum severity */
    if (entry->severity < logger->config.min_severity) return;

    /* Store entry */
    if (logger->entry_count < logger->entry_capacity) {
        AuditEntry* copy = audit_entry_create();
        if (copy) {
            copy->timestamp = entry->timestamp;
            copy->severity = entry->severity;
            copy->action = entry->action;
            copy->description = entry->description ? strdup(entry->description) : NULL;
            copy->target = entry->target ? strdup(entry->target) : NULL;
            copy->user = entry->user ? strdup(entry->user) : NULL;
            copy->details = entry->details ? strdup(entry->details) : NULL;
            copy->success = entry->success;
            copy->exit_code = entry->exit_code;
            logger->entries[logger->entry_count++] = copy;
        }
    }

    /* Write to log */
    audit_write_entry(logger, entry);
}

void audit_log_action(AuditLogger* logger, AuditSeverity severity,
                       ActionType action, const char* target,
                       const char* description, bool success) {
    AuditEntry entry = {
        .timestamp = time(NULL),
        .severity = severity,
        .action = action,
        .target = (char*)target,
        .description = (char*)description,
        .success = success
    };
    audit_log(logger, &entry);
}

void audit_log_permission(AuditLogger* logger, const PermissionRequest* request,
                          PermissionResponse response) {
    if (!logger || !request) return;

    AuditSeverity severity = (response == PERM_RESPONSE_NO) ? AUDIT_DENIED : AUDIT_ACTION;

    char desc[256];
    snprintf(desc, sizeof(desc), "Permission %s for: %s",
             response == PERM_RESPONSE_YES ? "GRANTED" :
             response == PERM_RESPONSE_NO ? "DENIED" :
             response == PERM_RESPONSE_ALWAYS ? "AUTO-APPROVED" : "VIEWED",
             request->description ? request->description : "unknown");

    audit_log_action(logger, severity, request->action, request->target,
                     desc, response != PERM_RESPONSE_NO);
}

void audit_log_command(AuditLogger* logger, const char* command,
                        const char* args, int exit_code, bool success) {
    char desc[512];
    snprintf(desc, sizeof(desc), "Command: %s %s (exit: %d)",
             command ? command : "", args ? args : "", exit_code);

    audit_log_action(logger, success ? AUDIT_ACTION : AUDIT_ERROR,
                     ACTION_RUN_COMMAND, command, desc, success);
}

void audit_log_security(AuditLogger* logger, const char* event,
                         const char* details) {
    AuditEntry entry = {
        .timestamp = time(NULL),
        .severity = AUDIT_SECURITY,
        .action = ACTION_SYSTEM_MODIFY,
        .description = (char*)event,
        .details = (char*)details,
        .success = true
    };
    audit_log(logger, &entry);
}

int audit_get_recent(AuditLogger* logger, int count, AuditEntry** out_entries) {
    if (!logger || !out_entries || count <= 0) return 0;

    int start = logger->entry_count > count ? logger->entry_count - count : 0;
    int result_count = logger->entry_count - start;

    for (int i = 0; i < result_count; i++) {
        out_entries[i] = logger->entries[start + i];
    }

    return result_count;
}

bool audit_export(AuditLogger* logger, const char* filepath, const char* format) {
    if (!logger || !filepath) return false;

    FILE* f = fopen(filepath, "w");
    if (!f) return false;

    if (strcmp(format, "json") == 0) {
        fprintf(f, "[\n");
        for (int i = 0; i < logger->entry_count; i++) {
            AuditEntry* e = logger->entries[i];
            fprintf(f, "  {\"timestamp\": %ld, \"severity\": \"%s\", \"action\": \"%s\", \"target\": \"%s\", \"success\": %s}%s\n",
                    (long)e->timestamp,
                    audit_severity_name(e->severity),
                    permission_action_name(e->action),
                    e->target ? e->target : "",
                    e->success ? "true" : "false",
                    i < logger->entry_count - 1 ? "," : "");
        }
        fprintf(f, "]\n");
    } else {
        /* Text format */
        for (int i = 0; i < logger->entry_count; i++) {
            AuditEntry* e = logger->entries[i];
            char time_buf[64];
            struct tm* tm_info = localtime(&e->timestamp);
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
            fprintf(f, "[%s] [%s] %s -> %s (%s)\n",
                    time_buf, audit_severity_name(e->severity),
                    permission_action_name(e->action),
                    e->target ? e->target : "",
                    e->success ? "success" : "failed");
        }
    }

    fclose(f);
    return true;
}

/* ========================================================================
 * Dry-Run Mode Implementation
 * ======================================================================== */

struct DryRunContext {
    bool enabled;
    DryRunAction** actions;
    int action_count;
    int action_capacity;
};

DryRunContext* dry_run_create(void) {
    DryRunContext* ctx = calloc(1, sizeof(DryRunContext));
    if (!ctx) return NULL;

    ctx->enabled = false;
    ctx->action_capacity = MAX_DRY_RUN_ACTIONS;
    ctx->actions = calloc(ctx->action_capacity, sizeof(DryRunAction*));

    return ctx;
}

void dry_run_action_free(DryRunAction* action) {
    if (!action) return;
    free(action->description);
    free(action->target);
    free(action->command);
    free(action->expected_result);
    free(action->potential_issues);
    free(action);
}

void dry_run_free(DryRunContext* ctx) {
    if (!ctx) return;

    if (ctx->actions) {
        for (int i = 0; i < ctx->action_count; i++) {
            dry_run_action_free(ctx->actions[i]);
        }
        free(ctx->actions);
    }

    free(ctx);
}

bool dry_run_is_enabled(DryRunContext* ctx) {
    return ctx && ctx->enabled;
}

void dry_run_set_enabled(DryRunContext* ctx, bool enabled) {
    if (ctx) {
        ctx->enabled = enabled;
        if (enabled) {
            log_info("[DRY-RUN] Dry-run mode ENABLED - no changes will be made");
        } else {
            log_info("[DRY-RUN] Dry-run mode DISABLED");
        }
    }
}

void dry_run_record(DryRunContext* ctx, const DryRunAction* action) {
    if (!ctx || !action || ctx->action_count >= ctx->action_capacity) return;

    DryRunAction* copy = calloc(1, sizeof(DryRunAction));
    if (!copy) return;

    copy->action = action->action;
    copy->description = action->description ? strdup(action->description) : NULL;
    copy->target = action->target ? strdup(action->target) : NULL;
    copy->command = action->command ? strdup(action->command) : NULL;
    copy->expected_result = action->expected_result ? strdup(action->expected_result) : NULL;
    copy->would_succeed = action->would_succeed;
    copy->potential_issues = action->potential_issues ? strdup(action->potential_issues) : NULL;

    ctx->actions[ctx->action_count++] = copy;
}

void dry_run_record_file(DryRunContext* ctx, ActionType action,
                          const char* filepath, const char* description) {
    DryRunAction act = {
        .action = action,
        .target = (char*)filepath,
        .description = (char*)description,
        .would_succeed = true
    };
    dry_run_record(ctx, &act);

    log_info("[DRY-RUN] Would %s: %s",
             permission_action_name(action), filepath);
}

void dry_run_record_command(DryRunContext* ctx, const char* command,
                             const char* working_dir) {
    DryRunAction act = {
        .action = ACTION_RUN_COMMAND,
        .command = (char*)command,
        .target = (char*)working_dir,
        .description = (char*)"Execute command",
        .would_succeed = true
    };
    dry_run_record(ctx, &act);

    log_info("[DRY-RUN] Would execute: %s", command);
}

const DryRunAction** dry_run_get_actions(DryRunContext* ctx, int* count) {
    if (!ctx || !count) return NULL;
    *count = ctx->action_count;
    return (const DryRunAction**)ctx->actions;
}

void dry_run_print_summary(DryRunContext* ctx) {
    if (!ctx) return;

    log_info("=== Dry-Run Summary ===");
    log_info("Actions that would be performed: %d", ctx->action_count);
    log_info("");

    for (int i = 0; i < ctx->action_count; i++) {
        DryRunAction* a = ctx->actions[i];
        log_info("  %d. %s: %s",
                 i + 1,
                 permission_action_name(a->action),
                 a->target ? a->target : a->command ? a->command : "");
        if (a->description) {
            log_info("     %s", a->description);
        }
    }

    log_info("");
    log_info("To execute these actions, disable dry-run mode and run again.");
}

void dry_run_clear(DryRunContext* ctx) {
    if (!ctx) return;

    for (int i = 0; i < ctx->action_count; i++) {
        dry_run_action_free(ctx->actions[i]);
        ctx->actions[i] = NULL;
    }
    ctx->action_count = 0;
}

/* ========================================================================
 * Rollback Support Implementation
 * ======================================================================== */

struct RollbackManager {
    RollbackConfig config;
    RollbackEntry** entries;
    int entry_count;
    int entry_capacity;
};

static RollbackEntry* rollback_entry_create(void) {
    RollbackEntry* entry = calloc(1, sizeof(RollbackEntry));
    if (entry) {
        entry->timestamp = time(NULL);
        entry->can_rollback = true;
    }
    return entry;
}

static void rollback_entry_free_internal(RollbackEntry* entry) {
    if (!entry) return;
    free(entry->filepath);
    free(entry->backup_path);
    free(entry->original_content);
    free(entry->description);
    free(entry);
}

RollbackManager* rollback_create(const RollbackConfig* config) {
    RollbackManager* mgr = calloc(1, sizeof(RollbackManager));
    if (!mgr) return NULL;

    mgr->config = *config;
    mgr->entry_capacity = config->max_entries > 0 ? config->max_entries : MAX_ROLLBACK_ENTRIES;
    mgr->entries = calloc(mgr->entry_capacity, sizeof(RollbackEntry*));

    /* Create backup directory if needed */
    if (config->enabled && config->backup_dir) {
        mkdir(config->backup_dir, 0755);
    }

    return mgr;
}

RollbackManager* rollback_create_default(void) {
    RollbackConfig config = {
        .enabled = true,
        .backup_dir = ".cyxmake/backups",
        .max_entries = MAX_ROLLBACK_ENTRIES,
        .max_file_size = MAX_FILE_BACKUP_SIZE,
        .backup_large_files = true,
        .retention_hours = 24
    };
    return rollback_create(&config);
}

void rollback_free(RollbackManager* mgr) {
    if (!mgr) return;

    if (mgr->entries) {
        for (int i = 0; i < mgr->entry_count; i++) {
            rollback_entry_free_internal(mgr->entries[i]);
        }
        free(mgr->entries);
    }

    free(mgr);
}

bool rollback_is_enabled(RollbackManager* mgr) {
    return mgr && mgr->config.enabled;
}

bool rollback_backup_file(RollbackManager* mgr, const char* filepath,
                          RollbackType type) {
    if (!mgr || !filepath || !mgr->config.enabled) return false;
    if (mgr->entry_count >= mgr->entry_capacity) return false;

    RollbackEntry* entry = rollback_entry_create();
    if (!entry) return false;

    entry->type = type;
    entry->filepath = strdup(filepath);

    /* Read file content for backup */
    FILE* f = fopen(filepath, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        entry->original_size = (size_t)size;

        if (size <= (long)mgr->config.max_file_size) {
            /* Store in memory */
            entry->original_content = malloc(size + 1);
            if (entry->original_content) {
                fread(entry->original_content, 1, size, f);
                entry->original_content[size] = '\0';
            }
        } else if (mgr->config.backup_large_files && mgr->config.backup_dir) {
            /* Backup to file */
            char backup_path[512];
            snprintf(backup_path, sizeof(backup_path), "%s/backup_%ld_%d",
                     mgr->config.backup_dir, (long)time(NULL), mgr->entry_count);

            FILE* backup = fopen(backup_path, "wb");
            if (backup) {
                char buffer[4096];
                size_t bytes;
                fseek(f, 0, SEEK_SET);
                while ((bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
                    fwrite(buffer, 1, bytes, backup);
                }
                fclose(backup);
                entry->backup_path = strdup(backup_path);
            }
        }
        fclose(f);
    }

    char desc[256];
    snprintf(desc, sizeof(desc), "Backed up before %s",
             type == ROLLBACK_FILE_MODIFY ? "modification" : "deletion");
    entry->description = strdup(desc);

    mgr->entries[mgr->entry_count++] = entry;
    log_debug("Rollback: backed up %s", filepath);
    return true;
}

bool rollback_record_create(RollbackManager* mgr, const char* filepath) {
    if (!mgr || !filepath || !mgr->config.enabled) return false;
    if (mgr->entry_count >= mgr->entry_capacity) return false;

    RollbackEntry* entry = rollback_entry_create();
    if (!entry) return false;

    entry->type = ROLLBACK_FILE_CREATE;
    entry->filepath = strdup(filepath);
    entry->description = strdup("File created - rollback will delete");

    mgr->entries[mgr->entry_count++] = entry;
    log_debug("Rollback: recorded creation of %s", filepath);
    return true;
}

bool rollback_record_mkdir(RollbackManager* mgr, const char* dirpath) {
    if (!mgr || !dirpath || !mgr->config.enabled) return false;
    if (mgr->entry_count >= mgr->entry_capacity) return false;

    RollbackEntry* entry = rollback_entry_create();
    if (!entry) return false;

    entry->type = ROLLBACK_DIR_CREATE;
    entry->filepath = strdup(dirpath);
    entry->description = strdup("Directory created - rollback will delete");

    mgr->entries[mgr->entry_count++] = entry;
    return true;
}

bool rollback_entry(RollbackManager* mgr, int index) {
    if (!mgr || index < 0 || index >= mgr->entry_count) return false;

    RollbackEntry* entry = mgr->entries[index];
    if (!entry || !entry->can_rollback) return false;

    bool success = false;

    switch (entry->type) {
        case ROLLBACK_FILE_CREATE:
            /* Delete the created file */
            if (remove(entry->filepath) == 0) {
                log_info("Rollback: deleted %s", entry->filepath);
                success = true;
            }
            break;

        case ROLLBACK_FILE_MODIFY:
        case ROLLBACK_FILE_DELETE:
            /* Restore original content */
            if (entry->original_content) {
                FILE* f = fopen(entry->filepath, "wb");
                if (f) {
                    fwrite(entry->original_content, 1, entry->original_size, f);
                    fclose(f);
                    log_info("Rollback: restored %s from memory", entry->filepath);
                    success = true;
                }
            } else if (entry->backup_path) {
                /* Restore from backup file */
                FILE* backup = fopen(entry->backup_path, "rb");
                FILE* target = fopen(entry->filepath, "wb");
                if (backup && target) {
                    char buffer[4096];
                    size_t bytes;
                    while ((bytes = fread(buffer, 1, sizeof(buffer), backup)) > 0) {
                        fwrite(buffer, 1, bytes, target);
                    }
                    success = true;
                    log_info("Rollback: restored %s from backup", entry->filepath);
                }
                if (backup) fclose(backup);
                if (target) fclose(target);
            }
            break;

        case ROLLBACK_DIR_CREATE:
            /* Delete the created directory */
            if (rmdir(entry->filepath) == 0) {
                log_info("Rollback: removed directory %s", entry->filepath);
                success = true;
            }
            break;

        default:
            log_warning("Rollback: cannot rollback this entry type");
            break;
    }

    entry->can_rollback = false;
    return success;
}

int rollback_last(RollbackManager* mgr, int count) {
    if (!mgr || count <= 0) return 0;

    int rolled_back = 0;
    int start = mgr->entry_count > count ? mgr->entry_count - count : 0;

    /* Rollback in reverse order */
    for (int i = mgr->entry_count - 1; i >= start; i--) {
        if (rollback_entry(mgr, i)) {
            rolled_back++;
        }
    }

    return rolled_back;
}

int rollback_since(RollbackManager* mgr, time_t since) {
    if (!mgr) return 0;

    int rolled_back = 0;

    /* Rollback in reverse order */
    for (int i = mgr->entry_count - 1; i >= 0; i--) {
        if (mgr->entries[i]->timestamp >= since) {
            if (rollback_entry(mgr, i)) {
                rolled_back++;
            }
        }
    }

    return rolled_back;
}

const RollbackEntry** rollback_get_history(RollbackManager* mgr, int* count) {
    if (!mgr || !count) return NULL;
    *count = mgr->entry_count;
    return (const RollbackEntry**)mgr->entries;
}

void rollback_print_history(RollbackManager* mgr) {
    if (!mgr) return;

    log_info("=== Rollback History ===");
    log_info("Entries: %d", mgr->entry_count);
    log_info("");

    for (int i = 0; i < mgr->entry_count; i++) {
        RollbackEntry* e = mgr->entries[i];
        char time_buf[64];
        struct tm* tm_info = localtime(&e->timestamp);
        strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);

        const char* type_str =
            e->type == ROLLBACK_FILE_CREATE ? "CREATE" :
            e->type == ROLLBACK_FILE_MODIFY ? "MODIFY" :
            e->type == ROLLBACK_FILE_DELETE ? "DELETE" :
            e->type == ROLLBACK_DIR_CREATE ? "MKDIR" :
            e->type == ROLLBACK_DIR_DELETE ? "RMDIR" : "CMD";

        log_info("  %d. [%s] %s: %s %s",
                 i, time_buf, type_str, e->filepath,
                 e->can_rollback ? "(can rollback)" : "(already rolled back)");
    }
}

void rollback_clear(RollbackManager* mgr) {
    if (!mgr) return;

    for (int i = 0; i < mgr->entry_count; i++) {
        /* Delete backup files */
        if (mgr->entries[i]->backup_path) {
            remove(mgr->entries[i]->backup_path);
        }
        rollback_entry_free_internal(mgr->entries[i]);
        mgr->entries[i] = NULL;
    }
    mgr->entry_count = 0;
}

int rollback_cleanup(RollbackManager* mgr) {
    if (!mgr || mgr->config.retention_hours <= 0) return 0;

    time_t cutoff = time(NULL) - (mgr->config.retention_hours * 3600);
    int cleaned = 0;

    for (int i = 0; i < mgr->entry_count; i++) {
        if (mgr->entries[i]->timestamp < cutoff) {
            if (mgr->entries[i]->backup_path) {
                remove(mgr->entries[i]->backup_path);
            }
            rollback_entry_free_internal(mgr->entries[i]);
            mgr->entries[i] = NULL;
            cleaned++;
        }
    }

    /* Compact array */
    int write_idx = 0;
    for (int i = 0; i < mgr->entry_count; i++) {
        if (mgr->entries[i]) {
            mgr->entries[write_idx++] = mgr->entries[i];
        }
    }
    mgr->entry_count = write_idx;

    return cleaned;
}

/* ========================================================================
 * Unified Security Context
 * ======================================================================== */

SecurityConfig security_config_default(void) {
    SecurityConfig config = {
        .enable_permissions = true,
        .enable_audit = true,
        .enable_dry_run = false,
        .enable_rollback = true,
        .audit_config = {
            .enabled = true,
            .log_file = ".cyxmake/audit.log",
            .log_to_console = false,
            .min_severity = AUDIT_ACTION,
            .include_timestamps = true,
            .include_user = true,
            .max_entries = MAX_AUDIT_ENTRIES,
            .rotation_size_mb = 10
        },
        .rollback_config = {
            .enabled = true,
            .backup_dir = ".cyxmake/backups",
            .max_entries = MAX_ROLLBACK_ENTRIES,
            .max_file_size = MAX_FILE_BACKUP_SIZE,
            .backup_large_files = true,
            .retention_hours = 24
        }
    };
    return config;
}

SecurityContext* security_context_create(const SecurityConfig* config) {
    SecurityContext* ctx = calloc(1, sizeof(SecurityContext));
    if (!ctx) return NULL;

    if (config->enable_permissions) {
        ctx->permissions = permission_context_create();
    }

    if (config->enable_audit) {
        ctx->audit = audit_logger_create(&config->audit_config);
    }

    if (config->enable_dry_run) {
        ctx->dry_run = dry_run_create();
        dry_run_set_enabled(ctx->dry_run, true);
    }

    if (config->enable_rollback) {
        ctx->rollback = rollback_create(&config->rollback_config);
    }

    ctx->initialized = true;
    return ctx;
}

SecurityContext* security_context_create_default(void) {
    SecurityConfig config = security_config_default();
    return security_context_create(&config);
}

void security_context_free(SecurityContext* ctx) {
    if (!ctx) return;

    if (ctx->permissions) permission_context_free(ctx->permissions);
    if (ctx->audit) audit_logger_free(ctx->audit);
    if (ctx->dry_run) dry_run_free(ctx->dry_run);
    if (ctx->rollback) rollback_free(ctx->rollback);

    free(ctx);
}

bool security_check_permission(SecurityContext* ctx, ActionType action,
                                const char* target, const char* reason) {
    if (!ctx) return true;  /* No security context = allow all */

    bool allowed = true;

    if (ctx->permissions) {
        allowed = permission_check(ctx->permissions, action, target, reason);
    }

    /* Log the permission decision */
    if (ctx->audit) {
        PermissionRequest req = {
            .action = action,
            .target = (char*)target,
            .reason = (char*)reason
        };
        audit_log_permission(ctx->audit, &req,
                            allowed ? PERM_RESPONSE_YES : PERM_RESPONSE_NO);
    }

    return allowed;
}

bool security_file_operation(SecurityContext* ctx, ActionType action,
                              const char* filepath, SecurityFileCallback callback,
                              void* callback_data) {
    if (!ctx || !filepath || !callback) return false;

    /* Check permission */
    if (!security_check_permission(ctx, action, filepath, NULL)) {
        return false;
    }

    /* Dry-run mode */
    if (ctx->dry_run && dry_run_is_enabled(ctx->dry_run)) {
        dry_run_record_file(ctx->dry_run, action, filepath, NULL);
        return true;  /* Simulated success */
    }

    /* Backup for rollback if modifying/deleting */
    if (ctx->rollback && rollback_is_enabled(ctx->rollback)) {
        if (action == ACTION_MODIFY_FILE) {
            rollback_backup_file(ctx->rollback, filepath, ROLLBACK_FILE_MODIFY);
        } else if (action == ACTION_DELETE_FILE) {
            rollback_backup_file(ctx->rollback, filepath, ROLLBACK_FILE_DELETE);
        }
    }

    /* Execute the operation */
    bool success = callback(filepath, callback_data);

    /* Record creation for rollback */
    if (success && ctx->rollback && rollback_is_enabled(ctx->rollback)) {
        if (action == ACTION_CREATE_FILE) {
            rollback_record_create(ctx->rollback, filepath);
        }
    }

    /* Audit log */
    if (ctx->audit) {
        audit_log_action(ctx->audit, success ? AUDIT_ACTION : AUDIT_ERROR,
                        action, filepath, NULL, success);
    }

    return success;
}

bool security_execute_command(SecurityContext* ctx, const char* command,
                               const char* args, const char* working_dir,
                               SecurityCommandCallback callback, void* callback_data) {
    if (!ctx || !command) return false;

    /* Check permission */
    if (!security_check_permission(ctx, ACTION_RUN_COMMAND, command, NULL)) {
        return false;
    }

    /* Dry-run mode */
    if (ctx->dry_run && dry_run_is_enabled(ctx->dry_run)) {
        dry_run_record_command(ctx->dry_run, command, working_dir);
        return true;  /* Simulated success */
    }

    /* Execute the command */
    bool success = callback ? callback(command, args, working_dir, callback_data) : false;

    /* Audit log */
    if (ctx->audit) {
        audit_log_command(ctx->audit, command, args, 0, success);
    }

    return success;
}

void security_print_status(SecurityContext* ctx) {
    if (!ctx) {
        log_info("Security context not initialized");
        return;
    }

    log_info("=== Security Status ===");
    log_info("");
    log_info("Permissions: %s", ctx->permissions ? "ENABLED" : "DISABLED");
    log_info("Audit Logging: %s", ctx->audit ? "ENABLED" : "DISABLED");
    log_info("Dry-Run Mode: %s",
             ctx->dry_run && dry_run_is_enabled(ctx->dry_run) ? "ENABLED" : "DISABLED");
    log_info("Rollback Support: %s",
             ctx->rollback && rollback_is_enabled(ctx->rollback) ? "ENABLED" : "DISABLED");
    log_info("Sandbox: %s", sandbox_is_available() ? "AVAILABLE" : "NOT AVAILABLE");

    if (ctx->rollback) {
        int count;
        rollback_get_history(ctx->rollback, &count);
        log_info("  Rollback entries: %d", count);
    }

    log_info("");
}

/* ========================================================================
 * Sandboxed Command Execution
 * ======================================================================== */

const char* sandbox_level_name(SandboxLevel level) {
    switch (level) {
        case SANDBOX_NONE:   return "None (full access)";
        case SANDBOX_LIGHT:  return "Light (no system writes)";
        case SANDBOX_MEDIUM: return "Medium (limited paths)";
        case SANDBOX_STRICT: return "Strict (read-only, no network)";
        default:             return "Unknown";
    }
}

SandboxConfig sandbox_config_default(SandboxLevel level) {
    SandboxConfig config = {
        .level = level,
        .allow_network = true,
        .allow_subprocesses = true,
        .allowed_read_paths = NULL,
        .allowed_read_count = 0,
        .allowed_write_paths = NULL,
        .allowed_write_count = 0,
        .max_memory_mb = 0,
        .max_cpu_sec = 0,
        .max_file_descriptors = 0
    };

    switch (level) {
        case SANDBOX_NONE:
            /* Full access - no restrictions */
            break;

        case SANDBOX_LIGHT:
            config.max_memory_mb = 2048;    /* 2GB */
            config.max_cpu_sec = 300;       /* 5 minutes */
            break;

        case SANDBOX_MEDIUM:
            config.max_memory_mb = 1024;    /* 1GB */
            config.max_cpu_sec = 120;       /* 2 minutes */
            config.max_file_descriptors = 256;
            break;

        case SANDBOX_STRICT:
            config.allow_network = false;
            config.allow_subprocesses = false;
            config.max_memory_mb = 512;     /* 512MB */
            config.max_cpu_sec = 60;        /* 1 minute */
            config.max_file_descriptors = 64;
            break;
    }

    return config;
}

void sandbox_result_free(SandboxResult* result) {
    if (!result) return;
    free(result->stdout_output);
    free(result->stderr_output);
    free(result->kill_reason);
    free(result);
}

bool sandbox_path_allowed(const SandboxConfig* config, const char* path,
                           bool for_write) {
    if (!config || !path) return false;

    /* SANDBOX_NONE allows everything */
    if (config->level == SANDBOX_NONE) return true;

    const char** allowed_paths = for_write ? config->allowed_write_paths :
                                              config->allowed_read_paths;
    int count = for_write ? config->allowed_write_count :
                            config->allowed_read_count;

    /* If no paths specified, check level defaults */
    if (!allowed_paths || count == 0) {
        if (config->level == SANDBOX_STRICT && for_write) {
            return false;  /* Strict mode: no writes allowed */
        }
        return true;  /* Allow by default for reads */
    }

    /* Check if path is in allowed list */
    for (int i = 0; i < count; i++) {
        if (allowed_paths[i]) {
            size_t len = strlen(allowed_paths[i]);
            if (strncmp(path, allowed_paths[i], len) == 0) {
                char next = path[len];
                if (next == '\0' || next == '/' || next == '\\') {
                    return true;
                }
            }
        }
    }

    return false;
}

#ifdef _WIN32

/* Windows implementation using Job Objects */
bool sandbox_is_available(void) {
    return true;  /* Job Objects available on all modern Windows */
}

const char* sandbox_capability_message(void) {
    return "Windows Job Objects: Resource limits, process isolation. "
           "Note: Full filesystem sandboxing requires additional setup.";
}

SandboxResult* sandbox_execute(const char* command,
                                char* const* args,
                                const char* working_dir,
                                const SandboxConfig* config) {
    SandboxResult* result = calloc(1, sizeof(SandboxResult));
    if (!result) return NULL;

    if (!command) {
        result->success = false;
        result->stderr_output = strdup("No command specified");
        return result;
    }

    /* Build command line */
    char cmdline[4096] = "";
    strncpy(cmdline, command, sizeof(cmdline) - 1);

    if (args) {
        for (int i = 0; args[i]; i++) {
            strncat(cmdline, " ", sizeof(cmdline) - strlen(cmdline) - 1);
            strncat(cmdline, args[i], sizeof(cmdline) - strlen(cmdline) - 1);
        }
    }

    /* Create Job Object for resource limits */
    HANDLE hJob = CreateJobObject(NULL, NULL);
    if (hJob && config && config->level != SANDBOX_NONE) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo = {0};
        jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

        if (config->max_memory_mb > 0) {
            jobInfo.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
            jobInfo.ProcessMemoryLimit = (SIZE_T)config->max_memory_mb * 1024 * 1024;
        }

        if (config->max_cpu_sec > 0) {
            jobInfo.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_TIME;
            jobInfo.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart =
                (LONGLONG)config->max_cpu_sec * 10000000LL;  /* 100ns units */
        }

        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation,
                               &jobInfo, sizeof(jobInfo));
    }

    /* Create pipes for output capture */
    HANDLE hStdOutRead, hStdOutWrite;
    HANDLE hStdErrRead, hStdErrWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0);
    CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0);

    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

    /* Create process */
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdErrWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    DWORD flags = CREATE_SUSPENDED;

    BOOL success = CreateProcessA(
        NULL,
        cmdline,
        NULL,
        NULL,
        TRUE,
        flags,
        NULL,
        working_dir,
        &si,
        &pi
    );

    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);

    if (!success) {
        result->success = false;
        result->exit_code = GetLastError();
        result->stderr_output = strdup("Failed to create process");
        CloseHandle(hStdOutRead);
        CloseHandle(hStdErrRead);
        if (hJob) CloseHandle(hJob);
        return result;
    }

    /* Assign to job and resume */
    if (hJob) {
        AssignProcessToJobObject(hJob, pi.hProcess);
    }
    ResumeThread(pi.hThread);

    /* Read output */
    char buffer[4096];
    DWORD bytesRead;
    size_t stdoutSize = 0, stderrSize = 0;
    result->stdout_output = malloc(1);
    result->stderr_output = malloc(1);
    result->stdout_output[0] = '\0';
    result->stderr_output[0] = '\0';

    while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        result->stdout_output = realloc(result->stdout_output, stdoutSize + bytesRead + 1);
        memcpy(result->stdout_output + stdoutSize, buffer, bytesRead + 1);
        stdoutSize += bytesRead;
    }

    while (ReadFile(hStdErrRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        result->stderr_output = realloc(result->stderr_output, stderrSize + bytesRead + 1);
        memcpy(result->stderr_output + stderrSize, buffer, bytesRead + 1);
        stderrSize += bytesRead;
    }

    /* Wait for process */
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    result->exit_code = (int)exitCode;
    result->success = (exitCode == 0);

    /* Check if killed due to limits */
    if (hJob) {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION acctInfo;
        if (QueryInformationJobObject(hJob, JobObjectBasicAccountingInformation,
                                      &acctInfo, sizeof(acctInfo), NULL)) {
            result->cpu_time_used = (double)acctInfo.TotalUserTime.QuadPart / 10000000.0;
        }
    }

    CloseHandle(hStdOutRead);
    CloseHandle(hStdErrRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (hJob) CloseHandle(hJob);

    return result;
}

#else

/* Unix implementation using resource limits */
#include <sys/wait.h>
#include <sys/resource.h>
#include <signal.h>
#include <errno.h>

bool sandbox_is_available(void) {
    return true;  /* Resource limits available on all Unix systems */
}

const char* sandbox_capability_message(void) {
    return "Unix resource limits (rlimit): Memory, CPU, file descriptor limits. "
           "For stronger isolation, consider using containers.";
}

SandboxResult* sandbox_execute(const char* command,
                                char* const* args,
                                const char* working_dir,
                                const SandboxConfig* config) {
    SandboxResult* result = calloc(1, sizeof(SandboxResult));
    if (!result) return NULL;

    if (!command) {
        result->success = false;
        result->stderr_output = strdup("No command specified");
        return result;
    }

    /* Create pipes for output capture */
    int stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdout_pipe) < 0 || pipe(stderr_pipe) < 0) {
        result->success = false;
        result->stderr_output = strdup("Failed to create pipes");
        return result;
    }

    pid_t pid = fork();

    if (pid < 0) {
        result->success = false;
        result->stderr_output = strdup("Failed to fork");
        return result;
    }

    if (pid == 0) {
        /* Child process */

        /* Set up output redirection */
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        /* Change working directory */
        if (working_dir) {
            chdir(working_dir);
        }

        /* Apply resource limits */
        if (config && config->level != SANDBOX_NONE) {
            struct rlimit limit;

            if (config->max_memory_mb > 0) {
                limit.rlim_cur = limit.rlim_max = (rlim_t)config->max_memory_mb * 1024 * 1024;
                setrlimit(RLIMIT_AS, &limit);
            }

            if (config->max_cpu_sec > 0) {
                limit.rlim_cur = limit.rlim_max = (rlim_t)config->max_cpu_sec;
                setrlimit(RLIMIT_CPU, &limit);
            }

            if (config->max_file_descriptors > 0) {
                limit.rlim_cur = limit.rlim_max = (rlim_t)config->max_file_descriptors;
                setrlimit(RLIMIT_NOFILE, &limit);
            }

            if (!config->allow_subprocesses) {
                limit.rlim_cur = limit.rlim_max = 0;
                setrlimit(RLIMIT_NPROC, &limit);
            }
        }

        /* Execute */
        if (args) {
            execvp(command, args);
        } else {
            execlp(command, command, NULL);
        }

        /* If exec fails */
        _exit(127);
    }

    /* Parent process */
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    /* Read output */
    char buffer[4096];
    ssize_t bytesRead;
    size_t stdoutSize = 0, stderrSize = 0;
    result->stdout_output = malloc(1);
    result->stderr_output = malloc(1);
    result->stdout_output[0] = '\0';
    result->stderr_output[0] = '\0';

    while ((bytesRead = read(stdout_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        result->stdout_output = realloc(result->stdout_output, stdoutSize + bytesRead + 1);
        memcpy(result->stdout_output + stdoutSize, buffer, bytesRead + 1);
        stdoutSize += bytesRead;
    }

    while ((bytesRead = read(stderr_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        result->stderr_output = realloc(result->stderr_output, stderrSize + bytesRead + 1);
        memcpy(result->stderr_output + stderrSize, buffer, bytesRead + 1);
        stderrSize += bytesRead;
    }

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    /* Wait for child */
    int status;
    struct rusage usage;
    wait4(pid, &status, 0, &usage);

    if (WIFEXITED(status)) {
        result->exit_code = WEXITSTATUS(status);
        result->success = (result->exit_code == 0);
    } else if (WIFSIGNALED(status)) {
        result->exit_code = 128 + WTERMSIG(status);
        result->success = false;
        result->was_killed = true;

        int sig = WTERMSIG(status);
        if (sig == SIGKILL || sig == SIGXCPU) {
            result->kill_reason = strdup("Killed due to resource limits");
        } else {
            char reason[64];
            snprintf(reason, sizeof(reason), "Killed by signal %d", sig);
            result->kill_reason = strdup(reason);
        }
    }

    /* Record resource usage */
    result->cpu_time_used = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0
                          + usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0;
    result->memory_used = usage.ru_maxrss * 1024;  /* Convert to bytes */

    return result;
}

#endif
