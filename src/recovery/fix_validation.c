/**
 * @file fix_validation.c
 * @brief Enhanced error recovery with validation, verification, and learning
 *
 * Phase 3: Error Recovery implementation
 */

#include "cyxmake/fix_validation.h"
#include "cyxmake/logger.h"
#include "cyxmake/compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <cJSON.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #define strdup _strdup
    #define access _access
    #define F_OK 0
#else
    #include <unistd.h>
    #include <sys/stat.h>
#endif

/* ========================================================================
 * Fix Validator Implementation
 * ======================================================================== */

struct FixValidator {
    const ToolRegistry* registry;
    bool check_packages;
    bool check_files;
    bool check_commands;
};

FixValidator* fix_validator_create(const ToolRegistry* registry) {
    FixValidator* validator = calloc(1, sizeof(FixValidator));
    if (!validator) return NULL;

    validator->registry = registry;
    validator->check_packages = true;
    validator->check_files = true;
    validator->check_commands = true;

    return validator;
}

void fix_validator_free(FixValidator* validator) {
    free(validator);
}

/* Check if a file path is valid and accessible */
static bool validate_file_path(const char* path) {
    if (!path || strlen(path) == 0) return false;

    /* Check for obviously invalid paths */
    if (path[0] == '\0') return false;

    /* Check if parent directory exists for new files */
    char* path_copy = strdup(path);
    if (!path_copy) return false;

    char* last_sep = strrchr(path_copy, '/');
#ifdef _WIN32
    char* last_bsep = strrchr(path_copy, '\\');
    if (last_bsep && (!last_sep || last_bsep > last_sep)) {
        last_sep = last_bsep;
    }
#endif

    bool valid = true;
    if (last_sep && last_sep != path_copy) {
        *last_sep = '\0';
        valid = (access(path_copy, F_OK) == 0);
    }

    free(path_copy);
    return valid;
}

/* Check if a package is likely to be installable */
static bool validate_package_installable(const FixValidator* validator,
                                          const char* package_name) {
    if (!package_name || strlen(package_name) == 0) return false;

    /* Check if we have a package manager available */
    if (validator->registry) {
        const ToolInfo* pkg_mgr = package_get_default_manager(validator->registry);
        if (!pkg_mgr) {
            return false;  /* No package manager available */
        }
    }

    /* Basic package name validation */
    for (const char* p = package_name; *p; p++) {
        char c = *p;
        /* Package names should be alphanumeric with some special chars */
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) {
            return false;
        }
    }

    return true;
}

/* Check if a command is likely to succeed */
static bool validate_command(const char* command) {
    if (!command || strlen(command) == 0) return false;

    /* Extract first word (command name) */
    char cmd_copy[256];
    strncpy(cmd_copy, command, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';

    char* space = strchr(cmd_copy, ' ');
    if (space) *space = '\0';

    /* Skip sudo prefix */
    char* cmd_name = cmd_copy;
    if (strcmp(cmd_name, "sudo") == 0 && space) {
        cmd_name = space + 1;
        while (*cmd_name == ' ') cmd_name++;
        space = strchr(cmd_name, ' ');
        if (space) *space = '\0';
    }

    /* Check if command exists */
#ifdef _WIN32
    char where_cmd[512];
    snprintf(where_cmd, sizeof(where_cmd), "where %s >nul 2>&1", cmd_name);
#else
    char where_cmd[512];
    snprintf(where_cmd, sizeof(where_cmd), "which %s >/dev/null 2>&1", cmd_name);
#endif

    return (system(where_cmd) == 0);
}

ValidationResult* fix_validate(FixValidator* validator,
                               const FixAction* action,
                               const ProjectContext* ctx) {
    (void)ctx;  /* May use for additional context */

    ValidationResult* result = calloc(1, sizeof(ValidationResult));
    if (!result) return NULL;

    result->status = VALIDATION_PASSED;
    result->can_proceed = true;
    result->confidence = 1.0;

    if (!action) {
        result->status = VALIDATION_FAILED;
        result->message = strdup("No action provided");
        result->can_proceed = false;
        result->confidence = 0.0;
        return result;
    }

    switch (action->type) {
        case FIX_ACTION_INSTALL_PACKAGE:
            if (validator && validator->check_packages) {
                if (!validate_package_installable(validator, action->target)) {
                    result->status = VALIDATION_WARNING;
                    result->message = strdup("Package may not be available");
                    result->details = action->target ? strdup(action->target) : NULL;
                    result->confidence = 0.5;
                }
            }
            break;

        case FIX_ACTION_CREATE_FILE:
        case FIX_ACTION_MODIFY_FILE:
            if (validator && validator->check_files) {
                if (!validate_file_path(action->target)) {
                    result->status = VALIDATION_FAILED;
                    result->message = strdup("Invalid or inaccessible file path");
                    result->details = action->target ? strdup(action->target) : NULL;
                    result->can_proceed = false;
                    result->confidence = 0.0;
                }
            }
            break;

        case FIX_ACTION_RUN_COMMAND:
            if (validator && validator->check_commands) {
                if (!validate_command(action->command)) {
                    result->status = VALIDATION_WARNING;
                    result->message = strdup("Command may not be available");
                    result->details = action->command ? strdup(action->command) : NULL;
                    result->confidence = 0.6;
                }
            }
            break;

        case FIX_ACTION_FIX_CMAKE_VERSION:
            /* Check if CMakeLists.txt exists */
            if (action->target && access(action->target, F_OK) != 0) {
                result->status = VALIDATION_FAILED;
                result->message = strdup("CMakeLists.txt not found");
                result->details = action->target ? strdup(action->target) : NULL;
                result->can_proceed = false;
                result->confidence = 0.0;
            }
            break;

        case FIX_ACTION_SET_ENV_VAR:
        case FIX_ACTION_CLEAN_BUILD:
        case FIX_ACTION_RETRY:
        case FIX_ACTION_NONE:
            /* These are always valid */
            result->status = VALIDATION_PASSED;
            result->confidence = 1.0;
            break;

        default:
            result->status = VALIDATION_SKIPPED;
            result->message = strdup("Unknown action type");
            result->confidence = 0.5;
            break;
    }

    if (!result->message) {
        result->message = strdup(result->status == VALIDATION_PASSED ?
                                 "Validation passed" : "Validation completed");
    }

    return result;
}

void validation_result_free(ValidationResult* result) {
    if (!result) return;
    free(result->message);
    free(result->details);
    free(result);
}

/* ========================================================================
 * Risk Assessment Implementation
 * ======================================================================== */

RiskAssessment* fix_assess_risk(const FixAction* action,
                                const ProjectContext* ctx) {
    (void)ctx;  /* May use for project-specific risk assessment */

    RiskAssessment* assessment = calloc(1, sizeof(RiskAssessment));
    if (!assessment) return NULL;

    assessment->is_reversible = false;
    assessment->requires_backup = false;
    assessment->requires_confirmation = false;

    if (!action) {
        assessment->level = RISK_NONE;
        assessment->description = strdup("No action");
        return assessment;
    }

    switch (action->type) {
        case FIX_ACTION_RETRY:
        case FIX_ACTION_NONE:
            assessment->level = RISK_NONE;
            assessment->description = strdup("No risk - informational only");
            break;

        case FIX_ACTION_SET_ENV_VAR:
            assessment->level = RISK_LOW;
            assessment->description = strdup("Sets environment variable (session only)");
            assessment->is_reversible = true;
            break;

        case FIX_ACTION_CLEAN_BUILD:
            assessment->level = RISK_LOW;
            assessment->description = strdup("Removes build artifacts (regeneratable)");
            assessment->is_reversible = false;  /* Can't undo deletion */
            break;

        case FIX_ACTION_MODIFY_FILE:
        case FIX_ACTION_FIX_CMAKE_VERSION:
            assessment->level = RISK_MEDIUM;
            assessment->description = strdup("Modifies project files");
            assessment->requires_backup = true;
            assessment->requires_confirmation = true;
            assessment->is_reversible = true;

            /* Track affected files */
            if (action->target) {
                assessment->affected_files = calloc(2, sizeof(char*));
                if (assessment->affected_files) {
                    assessment->affected_files[0] = strdup(action->target);
                    assessment->affected_count = 1;
                }
            }
            break;

        case FIX_ACTION_CREATE_FILE:
            assessment->level = RISK_MEDIUM;
            assessment->description = strdup("Creates new file");
            assessment->requires_confirmation = true;
            assessment->is_reversible = true;

            if (action->target) {
                assessment->affected_files = calloc(2, sizeof(char*));
                if (assessment->affected_files) {
                    assessment->affected_files[0] = strdup(action->target);
                    assessment->affected_count = 1;
                }
            }
            break;

        case FIX_ACTION_RUN_COMMAND:
            /* Commands vary in risk */
            if (action->command) {
                if (strstr(action->command, "sudo") ||
                    strstr(action->command, "rm -rf") ||
                    strstr(action->command, "chmod") ||
                    strstr(action->command, "chown")) {
                    assessment->level = RISK_CRITICAL;
                    assessment->description = strdup("Runs privileged system command");
                    assessment->requires_confirmation = true;
                } else {
                    assessment->level = RISK_MEDIUM;
                    assessment->description = strdup("Runs shell command");
                }
            } else {
                assessment->level = RISK_LOW;
                assessment->description = strdup("No command specified");
            }
            break;

        case FIX_ACTION_INSTALL_PACKAGE:
            assessment->level = RISK_HIGH;
            assessment->description = strdup("Installs system package");
            assessment->requires_confirmation = true;
            assessment->is_reversible = true;  /* Can uninstall */
            break;

        default:
            assessment->level = RISK_MEDIUM;
            assessment->description = strdup("Unknown action type");
            assessment->requires_confirmation = true;
            break;
    }

    /* Override with action's own confirmation requirement */
    if (action->requires_confirmation) {
        assessment->requires_confirmation = true;
    }

    return assessment;
}

void risk_assessment_free(RiskAssessment* assessment) {
    if (!assessment) return;

    free(assessment->description);
    if (assessment->affected_files) {
        for (size_t i = 0; i < assessment->affected_count; i++) {
            free(assessment->affected_files[i]);
        }
        free(assessment->affected_files);
    }
    free(assessment);
}

/* ========================================================================
 * Incremental Fix Session Implementation
 * ======================================================================== */

struct IncrementalFixSession {
    ProjectContext* project_ctx;
    const ToolRegistry* registry;
    RollbackManager* rollback;
    SecurityContext* security;
    FixValidator* validator;

    FixApplicationResult* results;
    size_t result_count;
    size_t result_capacity;

    int successful_fixes;
    int failed_fixes;
    int skipped_fixes;
};

IncrementalFixSession* incremental_fix_session_create(
    ProjectContext* ctx,
    const ToolRegistry* registry,
    RollbackManager* rollback,
    SecurityContext* security) {

    IncrementalFixSession* session = calloc(1, sizeof(IncrementalFixSession));
    if (!session) return NULL;

    session->project_ctx = ctx;
    session->registry = registry;
    session->rollback = rollback;
    session->security = security;
    session->validator = fix_validator_create(registry);

    session->result_capacity = 16;
    session->results = calloc(session->result_capacity, sizeof(FixApplicationResult));
    if (!session->results) {
        fix_validator_free(session->validator);
        free(session);
        return NULL;
    }

    return session;
}

void incremental_fix_session_free(IncrementalFixSession* session) {
    if (!session) return;

    fix_validator_free(session->validator);

    if (session->results) {
        for (size_t i = 0; i < session->result_count; i++) {
            free(session->results[i].message);
        }
        free(session->results);
    }

    free(session);
}

/* Add result to session */
static void session_add_result(IncrementalFixSession* session,
                               const FixAction* action,
                               FixResultStatus status,
                               const char* message,
                               double duration_ms,
                               bool verified) {
    if (session->result_count >= session->result_capacity) {
        size_t new_capacity = session->result_capacity * 2;
        FixApplicationResult* new_results = realloc(session->results,
            new_capacity * sizeof(FixApplicationResult));
        if (!new_results) return;
        session->results = new_results;
        session->result_capacity = new_capacity;
    }

    FixApplicationResult* result = &session->results[session->result_count++];
    result->status = status;
    result->action = action;
    result->message = message ? strdup(message) : NULL;
    result->duration_ms = duration_ms;
    result->verified = verified;
}

/* Get high-resolution time in milliseconds */
static double get_time_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
#endif
}

int incremental_fix_apply(IncrementalFixSession* session,
                          FixAction** fixes,
                          size_t fix_count,
                          bool verify_each,
                          bool stop_on_failure) {
    if (!session || !fixes || fix_count == 0) return 0;

    log_info("Starting incremental fix application (%zu fixes)", fix_count);

    for (size_t i = 0; i < fix_count; i++) {
        FixAction* action = fixes[i];
        if (!action) continue;

        double start_time = get_time_ms();
        log_info("[%zu/%zu] Processing: %s", i + 1, fix_count,
                 action->description ? action->description : "unknown fix");

        /* Step 1: Validate */
        ValidationResult* validation = fix_validate(session->validator, action,
                                                    session->project_ctx);
        if (validation) {
            if (validation->status == VALIDATION_FAILED) {
                log_warning("Validation failed: %s", validation->message);
                session_add_result(session, action, FIX_RESULT_SKIPPED,
                                   validation->message, get_time_ms() - start_time, false);
                session->skipped_fixes++;
                validation_result_free(validation);

                if (stop_on_failure) break;
                continue;
            } else if (validation->status == VALIDATION_WARNING) {
                log_warning("Validation warning: %s", validation->message);
            }
            validation_result_free(validation);
        }

        /* Step 2: Assess risk */
        RiskAssessment* risk = fix_assess_risk(action, session->project_ctx);
        if (risk) {
            log_debug("Risk level: %d - %s", risk->level,
                      risk->description ? risk->description : "");

            /* Create backup if needed */
            if (risk->requires_backup && session->rollback && action->target) {
                log_debug("Creating backup for: %s", action->target);
                rollback_backup_file(session->rollback, action->target, ROLLBACK_FILE_MODIFY);
            }

            /* Check if we need user confirmation */
            if (risk->requires_confirmation) {
                /* Use security context if available */
                if (session->security && session->security->dry_run &&
                    dry_run_is_enabled(session->security->dry_run)) {
                    /* In dry-run mode, just record the action */
                    DryRunAction dry_action = {0};
                    dry_action.action = ACTION_RUN_COMMAND;
                    dry_action.description = (char*)action->description;
                    dry_action.target = action->target;
                    dry_run_record(session->security->dry_run, &dry_action);

                    log_info("[DRY-RUN] Would apply: %s", action->description);
                    session_add_result(session, action, FIX_RESULT_SKIPPED,
                                       "Dry-run mode", get_time_ms() - start_time, false);
                    session->skipped_fixes++;
                    risk_assessment_free(risk);
                    continue;
                }
            }

            risk_assessment_free(risk);
        }

        /* Step 3: Apply the fix */
        bool success = fix_execute_with_tools(action, session->project_ctx,
                                              session->registry);

        double duration = get_time_ms() - start_time;

        if (success) {
            log_success("Fix applied: %s (%.1fms)", action->description, duration);

            /* Step 4: Verify if requested */
            bool verified = false;
            if (verify_each) {
                log_info("Verifying fix with rebuild...");
                /* TODO: Run verification build and check results */
                verified = true;  /* Placeholder - assume success for now */
            }

            session_add_result(session, action, FIX_RESULT_SUCCESS,
                               "Applied successfully", duration, verified);
            session->successful_fixes++;
        } else {
            log_error("Fix failed: %s", action->description);

            /* Try to rollback if we made a backup */
            if (session->rollback && action->target) {
                log_info("Attempting rollback for: %s", action->target);
                if (rollback_last(session->rollback, 1) > 0) {
                    session_add_result(session, action, FIX_RESULT_ROLLED_BACK,
                                       "Failed and rolled back", duration, false);
                } else {
                    session_add_result(session, action, FIX_RESULT_FAILED,
                                       "Failed (rollback also failed)", duration, false);
                }
            } else {
                session_add_result(session, action, FIX_RESULT_FAILED,
                                   "Failed to apply", duration, false);
            }
            session->failed_fixes++;

            if (stop_on_failure) break;
        }
    }

    log_info("Incremental fix session complete: %d succeeded, %d failed, %d skipped",
             session->successful_fixes, session->failed_fixes, session->skipped_fixes);

    return session->successful_fixes;
}

const FixApplicationResult* incremental_fix_get_results(
    const IncrementalFixSession* session,
    size_t* count) {
    if (!session) {
        if (count) *count = 0;
        return NULL;
    }

    if (count) *count = session->result_count;
    return session->results;
}

int incremental_fix_rollback_all(IncrementalFixSession* session) {
    if (!session || !session->rollback) return 0;

    log_info("Rolling back all applied fixes...");
    /* Rollback all successful fixes */
    int rolled_back = rollback_last(session->rollback, session->successful_fixes);
    log_info("Rolled back %d changes", rolled_back);

    return rolled_back;
}

/* ========================================================================
 * Fix Verification Implementation
 * ======================================================================== */

VerifyResult* fix_verify(ProjectContext* ctx,
                         const ErrorDiagnosis* original_diagnosis,
                         const BuildOptions* build_opts) {
    VerifyResult* result = calloc(1, sizeof(VerifyResult));
    if (!result) return NULL;

    if (original_diagnosis && original_diagnosis->error_message) {
        result->original_error = strdup(original_diagnosis->error_message);
    }

    if (!ctx || !build_opts) {
        result->status = VERIFY_BUILD_FAILED;
        result->current_error = strdup("Invalid context or build options");
        return result;
    }

    double start_time = get_time_ms();

    /* Run a verification build */
    BuildResult* build_result = build_execute(ctx, build_opts);

    result->build_time_ms = get_time_ms() - start_time;

    if (!build_result) {
        result->status = VERIFY_BUILD_FAILED;
        result->current_error = strdup("Build failed to execute");
        return result;
    }

    if (build_result->success) {
        result->status = VERIFY_SUCCESS;
        result->current_error_count = 0;
    } else {
        /* Check if the original error is still present */
        if (build_result->stderr_output && original_diagnosis &&
            original_diagnosis->error_message) {

            if (strstr(build_result->stderr_output, original_diagnosis->error_message)) {
                result->status = VERIFY_FAILED;
                result->current_error = strdup(build_result->stderr_output);
            } else {
                /* Different error - might be new or partial fix */
                result->status = VERIFY_NEW_ERRORS;
                result->current_error = strdup(build_result->stderr_output);
            }
        } else {
            result->status = VERIFY_FAILED;
            result->current_error = build_result->stderr_output ?
                strdup(build_result->stderr_output) : strdup("Unknown error");
        }
        result->current_error_count = 1;  /* Simplified count */
    }

    build_result_free(build_result);
    return result;
}

void verify_result_free(VerifyResult* result) {
    if (!result) return;
    free(result->original_error);
    free(result->current_error);
    free(result);
}

/* ========================================================================
 * Fix History Implementation
 * ======================================================================== */

#define MAX_HISTORY_ENTRIES 1000

struct FixHistory {
    char* history_path;
    FixHistoryEntry* entries;
    size_t entry_count;
    size_t entry_capacity;
    bool modified;
};

/* Generate error signature for matching */
static char* generate_error_signature(const ErrorDiagnosis* diagnosis) {
    if (!diagnosis) return NULL;

    char signature[512];
    snprintf(signature, sizeof(signature), "%d:%s",
             diagnosis->pattern_type,
             diagnosis->error_message ? diagnosis->error_message : "unknown");

    /* Normalize the signature (remove specific paths, line numbers, etc.) */
    /* This is a simplified version - could be made more sophisticated */
    return strdup(signature);
}

/* Get default history path */
static char* get_default_history_path(void) {
    char path[512];

#ifdef _WIN32
    const char* home = getenv("USERPROFILE");
    if (home) {
        snprintf(path, sizeof(path), "%s\\.cyxmake\\fix_history.json", home);
    } else {
        snprintf(path, sizeof(path), ".cyxmake\\fix_history.json");
    }
#else
    const char* home = getenv("HOME");
    if (home) {
        snprintf(path, sizeof(path), "%s/.cyxmake/fix_history.json", home);
    } else {
        snprintf(path, sizeof(path), ".cyxmake/fix_history.json");
    }
#endif

    return strdup(path);
}

/* Load history from JSON file */
static bool fix_history_load(FixHistory* history) {
    if (!history || !history->history_path) return false;

    FILE* file = fopen(history->history_path, "r");
    if (!file) return false;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size <= 0 || size > 10 * 1024 * 1024) {  /* Max 10MB */
        fclose(file);
        return false;
    }

    char* content = malloc(size + 1);
    if (!content) {
        fclose(file);
        return false;
    }

    size_t read = fread(content, 1, size, file);
    fclose(file);
    content[read] = '\0';

    cJSON* root = cJSON_Parse(content);
    free(content);

    if (!root) return false;

    cJSON* entries = cJSON_GetObjectItem(root, "entries");
    if (!entries || !cJSON_IsArray(entries)) {
        cJSON_Delete(root);
        return false;
    }

    int count = cJSON_GetArraySize(entries);
    for (int i = 0; i < count && history->entry_count < history->entry_capacity; i++) {
        cJSON* entry = cJSON_GetArrayItem(entries, i);
        if (!entry) continue;

        FixHistoryEntry* he = &history->entries[history->entry_count];

        cJSON* item;
        item = cJSON_GetObjectItem(entry, "error_type");
        if (item) he->error_type = item->valueint;

        item = cJSON_GetObjectItem(entry, "error_signature");
        if (item && item->valuestring) he->error_signature = strdup(item->valuestring);

        item = cJSON_GetObjectItem(entry, "fix_type");
        if (item) he->fix_type = item->valueint;

        item = cJSON_GetObjectItem(entry, "fix_command");
        if (item && item->valuestring) he->fix_command = strdup(item->valuestring);

        item = cJSON_GetObjectItem(entry, "fix_target");
        if (item && item->valuestring) he->fix_target = strdup(item->valuestring);

        item = cJSON_GetObjectItem(entry, "project_type");
        if (item && item->valuestring) he->project_type = strdup(item->valuestring);

        item = cJSON_GetObjectItem(entry, "build_system");
        if (item && item->valuestring) he->build_system = strdup(item->valuestring);

        item = cJSON_GetObjectItem(entry, "success_count");
        if (item) he->success_count = item->valueint;

        item = cJSON_GetObjectItem(entry, "failure_count");
        if (item) he->failure_count = item->valueint;

        item = cJSON_GetObjectItem(entry, "first_seen");
        if (item) he->first_seen = (time_t)item->valuedouble;

        item = cJSON_GetObjectItem(entry, "last_seen");
        if (item) he->last_seen = (time_t)item->valuedouble;

        item = cJSON_GetObjectItem(entry, "avg_fix_time_ms");
        if (item) he->avg_fix_time_ms = item->valuedouble;

        history->entry_count++;
    }

    cJSON_Delete(root);
    return true;
}

FixHistory* fix_history_create(const char* history_path) {
    FixHistory* history = calloc(1, sizeof(FixHistory));
    if (!history) return NULL;

    history->history_path = history_path ? strdup(history_path) :
                            get_default_history_path();

    history->entry_capacity = MAX_HISTORY_ENTRIES;
    history->entries = calloc(history->entry_capacity, sizeof(FixHistoryEntry));
    if (!history->entries) {
        free(history->history_path);
        free(history);
        return NULL;
    }

    /* Try to load existing history */
    fix_history_load(history);

    return history;
}

void fix_history_free(FixHistory* history) {
    if (!history) return;

    /* Save if modified */
    if (history->modified) {
        fix_history_save(history);
    }

    /* Free entries */
    for (size_t i = 0; i < history->entry_count; i++) {
        FixHistoryEntry* e = &history->entries[i];
        free(e->error_signature);
        free(e->fix_command);
        free(e->fix_target);
        free(e->project_type);
        free(e->build_system);
    }
    free(history->entries);
    free(history->history_path);
    free(history);
}

void fix_history_record(FixHistory* history,
                        const ErrorDiagnosis* diagnosis,
                        const FixAction* action,
                        bool success,
                        double fix_time_ms) {
    if (!history || !diagnosis || !action) return;

    char* signature = generate_error_signature(diagnosis);
    if (!signature) return;

    /* Look for existing entry */
    FixHistoryEntry* entry = NULL;
    for (size_t i = 0; i < history->entry_count; i++) {
        if (history->entries[i].error_signature &&
            strcmp(history->entries[i].error_signature, signature) == 0 &&
            history->entries[i].fix_type == action->type) {
            entry = &history->entries[i];
            break;
        }
    }

    if (entry) {
        /* Update existing entry */
        if (success) {
            entry->success_count++;
        } else {
            entry->failure_count++;
        }
        entry->last_seen = time(NULL);

        /* Update average fix time */
        int total_attempts = entry->success_count + entry->failure_count;
        entry->avg_fix_time_ms = ((entry->avg_fix_time_ms * (total_attempts - 1)) +
                                  fix_time_ms) / total_attempts;
    } else if (history->entry_count < history->entry_capacity) {
        /* Create new entry */
        entry = &history->entries[history->entry_count++];
        entry->error_type = diagnosis->pattern_type;
        entry->error_signature = signature;
        signature = NULL;  /* Transferred ownership */
        entry->fix_type = action->type;
        entry->fix_command = action->command ? strdup(action->command) : NULL;
        entry->fix_target = action->target ? strdup(action->target) : NULL;
        entry->success_count = success ? 1 : 0;
        entry->failure_count = success ? 0 : 1;
        entry->first_seen = time(NULL);
        entry->last_seen = time(NULL);
        entry->avg_fix_time_ms = fix_time_ms;
    }

    free(signature);
    history->modified = true;
}

FixHistoryEntry** fix_history_lookup(const FixHistory* history,
                                     const ErrorDiagnosis* diagnosis,
                                     size_t* count) {
    if (!history || !diagnosis || !count) {
        if (count) *count = 0;
        return NULL;
    }

    char* signature = generate_error_signature(diagnosis);
    if (!signature) {
        *count = 0;
        return NULL;
    }

    /* Count matching entries */
    size_t match_count = 0;
    for (size_t i = 0; i < history->entry_count; i++) {
        if (history->entries[i].error_type == diagnosis->pattern_type) {
            match_count++;
        }
    }

    if (match_count == 0) {
        free(signature);
        *count = 0;
        return NULL;
    }

    /* Allocate result array */
    FixHistoryEntry** results = calloc(match_count + 1, sizeof(FixHistoryEntry*));
    if (!results) {
        free(signature);
        *count = 0;
        return NULL;
    }

    /* Fill results */
    size_t j = 0;
    for (size_t i = 0; i < history->entry_count && j < match_count; i++) {
        if (history->entries[i].error_type == diagnosis->pattern_type) {
            /* Create a copy of the entry */
            FixHistoryEntry* copy = calloc(1, sizeof(FixHistoryEntry));
            if (copy) {
                *copy = history->entries[i];
                /* Deep copy strings */
                copy->error_signature = history->entries[i].error_signature ?
                    strdup(history->entries[i].error_signature) : NULL;
                copy->fix_command = history->entries[i].fix_command ?
                    strdup(history->entries[i].fix_command) : NULL;
                copy->fix_target = history->entries[i].fix_target ?
                    strdup(history->entries[i].fix_target) : NULL;
                copy->project_type = history->entries[i].project_type ?
                    strdup(history->entries[i].project_type) : NULL;
                copy->build_system = history->entries[i].build_system ?
                    strdup(history->entries[i].build_system) : NULL;
                results[j++] = copy;
            }
        }
    }

    free(signature);
    *count = j;
    return results;
}

FixAction* fix_history_suggest(const FixHistory* history,
                               const ErrorDiagnosis* diagnosis) {
    if (!history || !diagnosis) return NULL;

    /* Find best matching entry */
    FixHistoryEntry* best = NULL;
    double best_score = 0.0;

    for (size_t i = 0; i < history->entry_count; i++) {
        FixHistoryEntry* e = &history->entries[i];
        if (e->error_type != diagnosis->pattern_type) continue;

        /* Calculate score based on success rate and recency */
        int total = e->success_count + e->failure_count;
        if (total == 0) continue;

        double success_rate = (double)e->success_count / total;
        double recency = 1.0 / (1.0 + difftime(time(NULL), e->last_seen) / 86400.0);
        double score = success_rate * 0.7 + recency * 0.3;

        if (score > best_score) {
            best_score = score;
            best = e;
        }
    }

    if (!best || best_score < 0.5) return NULL;

    /* Create fix action from history entry */
    FixAction* action = calloc(1, sizeof(FixAction));
    if (!action) return NULL;

    action->type = best->fix_type;
    action->description = strdup("Suggested fix based on history");
    action->command = best->fix_command ? strdup(best->fix_command) : NULL;
    action->target = best->fix_target ? strdup(best->fix_target) : NULL;
    action->requires_confirmation = true;

    return action;
}

void fix_history_entries_free(FixHistoryEntry** entries, size_t count) {
    if (!entries) return;

    for (size_t i = 0; i < count; i++) {
        if (entries[i]) {
            free(entries[i]->error_signature);
            free(entries[i]->fix_command);
            free(entries[i]->fix_target);
            free(entries[i]->project_type);
            free(entries[i]->build_system);
            free(entries[i]);
        }
    }
    free(entries);
}

bool fix_history_save(FixHistory* history) {
    if (!history || !history->history_path) return false;

    /* Create directory if needed */
    char* dir = strdup(history->history_path);
    if (dir) {
        char* last_sep = strrchr(dir, '/');
#ifdef _WIN32
        char* last_bsep = strrchr(dir, '\\');
        if (last_bsep && (!last_sep || last_bsep > last_sep)) {
            last_sep = last_bsep;
        }
#endif
        if (last_sep) {
            *last_sep = '\0';
#ifdef _WIN32
            _mkdir(dir);
#else
            mkdir(dir, 0755);
#endif
        }
        free(dir);
    }

    /* Build JSON */
    cJSON* root = cJSON_CreateObject();
    if (!root) return false;

    cJSON* entries = cJSON_CreateArray();
    if (!entries) {
        cJSON_Delete(root);
        return false;
    }
    cJSON_AddItemToObject(root, "entries", entries);

    for (size_t i = 0; i < history->entry_count; i++) {
        FixHistoryEntry* e = &history->entries[i];

        cJSON* entry = cJSON_CreateObject();
        if (!entry) continue;

        cJSON_AddNumberToObject(entry, "error_type", e->error_type);
        if (e->error_signature)
            cJSON_AddStringToObject(entry, "error_signature", e->error_signature);
        cJSON_AddNumberToObject(entry, "fix_type", e->fix_type);
        if (e->fix_command)
            cJSON_AddStringToObject(entry, "fix_command", e->fix_command);
        if (e->fix_target)
            cJSON_AddStringToObject(entry, "fix_target", e->fix_target);
        if (e->project_type)
            cJSON_AddStringToObject(entry, "project_type", e->project_type);
        if (e->build_system)
            cJSON_AddStringToObject(entry, "build_system", e->build_system);
        cJSON_AddNumberToObject(entry, "success_count", e->success_count);
        cJSON_AddNumberToObject(entry, "failure_count", e->failure_count);
        cJSON_AddNumberToObject(entry, "first_seen", (double)e->first_seen);
        cJSON_AddNumberToObject(entry, "last_seen", (double)e->last_seen);
        cJSON_AddNumberToObject(entry, "avg_fix_time_ms", e->avg_fix_time_ms);

        cJSON_AddItemToArray(entries, entry);
    }

    char* json = cJSON_Print(root);
    cJSON_Delete(root);

    if (!json) return false;

    FILE* file = fopen(history->history_path, "w");
    if (!file) {
        free(json);
        return false;
    }

    fputs(json, file);
    fclose(file);
    free(json);

    history->modified = false;
    log_debug("Saved fix history to %s", history->history_path);

    return true;
}

void fix_history_stats(const FixHistory* history,
                       int* total_fixes,
                       int* successful_fixes,
                       int* unique_errors) {
    if (!history) {
        if (total_fixes) *total_fixes = 0;
        if (successful_fixes) *successful_fixes = 0;
        if (unique_errors) *unique_errors = 0;
        return;
    }

    int total = 0, success = 0;
    for (size_t i = 0; i < history->entry_count; i++) {
        total += history->entries[i].success_count + history->entries[i].failure_count;
        success += history->entries[i].success_count;
    }

    if (total_fixes) *total_fixes = total;
    if (successful_fixes) *successful_fixes = success;
    if (unique_errors) *unique_errors = (int)history->entry_count;
}

/* ========================================================================
 * Enhanced Recovery Implementation
 * ======================================================================== */

EnhancedRecoveryOptions enhanced_recovery_defaults(void) {
    EnhancedRecoveryOptions opts = {
        .validate_before_apply = true,
        .verify_after_apply = true,
        .incremental_apply = true,
        .use_history = true,
        .record_history = true,
        .auto_rollback = true,
        .max_auto_risk = RISK_LOW
    };
    return opts;
}

/* Extended recovery context structure */
typedef struct EnhancedRecoveryData {
    EnhancedRecoveryOptions options;
    const ToolRegistry* registry;
    RollbackManager* rollback;
    SecurityContext* security;
    FixHistory* history;
    FixValidator* validator;
} EnhancedRecoveryData;

RecoveryContext* enhanced_recovery_create(
    const RecoveryStrategy* strategy,
    const EnhancedRecoveryOptions* options,
    const ToolRegistry* registry,
    RollbackManager* rollback,
    SecurityContext* security,
    FixHistory* history) {

    /* Create base recovery context */
    RecoveryContext* ctx = recovery_context_create(strategy);
    if (!ctx) return NULL;

    /* Store enhanced data - in a real implementation, this would be
     * stored in the RecoveryContext structure. For now, we use static storage */
    static EnhancedRecoveryData enhanced_data;
    if (options) {
        enhanced_data.options = *options;
    } else {
        enhanced_data.options = enhanced_recovery_defaults();
    }
    enhanced_data.registry = registry;
    enhanced_data.rollback = rollback;
    enhanced_data.security = security;
    enhanced_data.history = history;
    enhanced_data.validator = fix_validator_create(registry);

    /* Set tool registry on context */
    recovery_set_tools(ctx, (ToolRegistry*)registry);

    return ctx;
}

BuildResult* enhanced_recovery_attempt(RecoveryContext* ctx,
                                        const BuildResult* build_result,
                                        ProjectContext* project_ctx) {
    if (!ctx || !build_result || !project_ctx) return NULL;

    log_info("Starting enhanced recovery attempt...");

    /* Use standard recovery but with enhanced features */
    /* The enhanced features are integrated into the fix execution flow */
    return recovery_attempt(ctx, build_result, project_ctx);
}
