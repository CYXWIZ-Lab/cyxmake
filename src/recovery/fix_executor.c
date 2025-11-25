/**
 * @file fix_executor.c
 * @brief Execute fix actions to recover from errors
 */

#include "cyxmake/error_recovery.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
    #define setenv(name, value, overwrite) _putenv_s(name, value)
#else
    #include <unistd.h>
#endif

/* Execute a system command */
static bool execute_command(const char* command) {
    if (!command) return false;

    log_info("Executing: %s", command);

    int result = system(command);
    if (result != 0) {
        log_error("Command failed with exit code %d", result);
        return false;
    }

    return true;
}

/* Create a file with content */
static bool create_file(const char* path, const char* content) {
    if (!path) return false;

    log_info("Creating file: %s", path);

    FILE* file = fopen(path, "w");
    if (!file) {
        log_error("Failed to create file: %s", path);
        return false;
    }

    if (content) {
        fprintf(file, "%s", content);
    }

    fclose(file);
    log_success("File created: %s", path);
    return true;
}

/* Append content to file */
static bool append_to_file(const char* path, const char* content) {
    if (!path || !content) return false;

    log_info("Appending to file: %s", path);

    FILE* file = fopen(path, "a");
    if (!file) {
        log_error("Failed to open file for appending: %s", path);
        return false;
    }

    fprintf(file, "\n%s\n", content);
    fclose(file);

    log_success("Content appended to: %s", path);
    return true;
}

/* Check if file exists */
static bool file_exists(const char* path) {
    FILE* file = fopen(path, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

/* Set environment variable */
static bool set_environment_var(const char* name, const char* value) {
    if (!name || !value) return false;

    log_info("Setting environment variable: %s=%s", name, value);

#ifdef _WIN32
    if (setenv(name, value, 1) != 0) {
#else
    if (setenv(name, value, 1) != 0) {
#endif
        log_error("Failed to set environment variable");
        return false;
    }

    log_success("Environment variable set: %s", name);
    return true;
}

/* Install package using platform-specific package manager */
static bool install_package(const char* command, const char* package) {
    if (!command) return false;

    log_info("Installing package: %s", package);

    /* Check for sudo requirement */
    bool needs_sudo = strstr(command, "sudo") != NULL;
    if (needs_sudo) {
        log_warning("This command requires administrator privileges");
    }

    /* Execute installation command */
    bool success = execute_command(command);

    if (success) {
        log_success("Package installed: %s", package);
    } else {
        log_error("Failed to install package: %s", package);
    }

    return success;
}

/* Clean build directory */
static bool clean_build(const ProjectContext* ctx) {
    log_info("Cleaning build directory");

    const char* build_dir = "build";  /* Default build directory */
    (void)ctx;  /* Unused for now */

#ifdef _WIN32
    char command[512];
    snprintf(command, sizeof(command), "if exist %s rd /s /q %s", build_dir, build_dir);
#else
    char command[512];
    snprintf(command, sizeof(command), "rm -rf %s", build_dir);
#endif

    bool success = execute_command(command);

    /* Recreate build directory */
    if (success) {
#ifdef _WIN32
        snprintf(command, sizeof(command), "mkdir %s", build_dir);
#else
        snprintf(command, sizeof(command), "mkdir -p %s", build_dir);
#endif
        success = execute_command(command);
    }

    if (success) {
        log_success("Build directory cleaned");
    }

    return success;
}

/* Ask user for confirmation */
static bool ask_confirmation(const char* action_desc) {
    log_plain("\nThe following action requires confirmation:");
    log_plain("  %s", action_desc);
    log_plain("Do you want to proceed? (y/n): ");

    char response[10];
    if (fgets(response, sizeof(response), stdin) == NULL) {
        return false;
    }

    return (response[0] == 'y' || response[0] == 'Y');
}

/* Execute a single fix action */
bool fix_execute(const FixAction* action, const ProjectContext* ctx) {
    if (!action) return false;

    log_info("Applying fix: %s", action->description);

    /* Check if confirmation is needed */
    if (action->requires_confirmation) {
        if (!ask_confirmation(action->description)) {
            log_warning("Fix skipped by user");
            return false;
        }
    }

    bool success = false;

    switch (action->type) {
        case FIX_ACTION_INSTALL_PACKAGE:
            success = install_package(action->command, action->target);
            break;

        case FIX_ACTION_CREATE_FILE:
            success = create_file(action->target, action->value);
            break;

        case FIX_ACTION_MODIFY_FILE:
            /* Check if file exists to decide between create or append */
            if (file_exists(action->target)) {
                success = append_to_file(action->target, action->value);
            } else {
                success = create_file(action->target, action->value);
            }
            break;

        case FIX_ACTION_SET_ENV_VAR:
            success = set_environment_var(action->target, action->value);
            break;

        case FIX_ACTION_RUN_COMMAND:
            success = execute_command(action->command);
            break;

        case FIX_ACTION_CLEAN_BUILD:
            success = clean_build(ctx);
            break;

        case FIX_ACTION_RETRY:
            /* Retry is handled at a higher level */
            log_info("Retry requested - will attempt rebuild");
            success = true;
            break;

        case FIX_ACTION_NONE:
            log_warning("No automated fix available - manual intervention required");
            success = false;
            break;

        default:
            log_error("Unknown fix action type: %d", action->type);
            success = false;
            break;
    }

    if (success) {
        log_success("Fix applied successfully");
    } else {
        log_error("Failed to apply fix");
    }

    return success;
}

/* Execute all fix actions in sequence */
int fix_execute_all(FixAction** actions, size_t count, const ProjectContext* ctx) {
    if (!actions || count == 0) return 0;

    log_info("Applying %zu fix action(s)", count);

    int successful_fixes = 0;
    bool retry_requested = false;

    for (size_t i = 0; i < count; i++) {
        if (!actions[i]) continue;

        /* Check for retry action */
        if (actions[i]->type == FIX_ACTION_RETRY) {
            retry_requested = true;
        }

        /* Execute the fix */
        if (fix_execute(actions[i], ctx)) {
            successful_fixes++;
        } else {
            /* If a critical fix fails, we might want to stop */
            if (actions[i]->type == FIX_ACTION_INSTALL_PACKAGE ||
                actions[i]->type == FIX_ACTION_CREATE_FILE) {
                log_warning("Critical fix failed, stopping execution");
                break;
            }
        }
    }

    log_info("Applied %d of %zu fixes successfully", successful_fixes, count);

    /* If retry was requested and we applied some fixes, indicate it */
    if (retry_requested && successful_fixes > 0) {
        log_info("Fixes applied, retry build recommended");
    }

    return successful_fixes;
}