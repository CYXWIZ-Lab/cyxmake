/**
 * @file tool_executor.c
 * @brief Tool execution implementation
 */

#include "cyxmake/tool_executor.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>  /* for _getcwd, _chdir */
    #define strdup _strdup
    #define popen _popen
    #define pclose _pclose
    #define getcwd _getcwd
    #define chdir _chdir
#else
    #include <unistd.h>
    #include <sys/wait.h>
#endif

/* Create default execution options */
ToolExecOptions* tool_exec_options_create(void) {
    ToolExecOptions* options = calloc(1, sizeof(ToolExecOptions));
    if (!options) return NULL;

    options->args = NULL;
    options->arg_count = 0;
    options->env_vars = NULL;
    options->env_var_count = 0;
    options->working_dir = NULL;
    options->timeout_sec = 0;
    options->capture_output = true;
    options->show_output = false;

    return options;
}

/* Free execution options */
void tool_exec_options_free(ToolExecOptions* options) {
    if (!options) return;

    if (options->args) {
        for (size_t i = 0; i < options->arg_count; i++) {
            free(options->args[i]);
        }
        free(options->args);
    }

    if (options->env_vars) {
        for (size_t i = 0; i < options->env_var_count; i++) {
            free(options->env_vars[i]);
        }
        free(options->env_vars);
    }

    free(options);
}

/* Free execution result */
void tool_exec_result_free(ToolExecResult* result) {
    if (!result) return;

    free(result->stdout_output);
    free(result->stderr_output);
    free(result);
}

/* Build command string from args */
static char* build_command_string(const char* tool_path, char* const* args) {
    /* Check if path needs quoting (contains spaces) */
    bool needs_quotes = (strchr(tool_path, ' ') != NULL);
    size_t total_len = strlen(tool_path) + 1;
    if (needs_quotes) total_len += 2;  /* Add quotes */

    /* Calculate total length */
    if (args) {
        for (int i = 0; args[i]; i++) {
            total_len += strlen(args[i]) + 1;  /* +1 for space */
        }
    }

    char* command = malloc(total_len + 1);
    if (!command) return NULL;

    /* Build command with quoted path if needed */
    if (needs_quotes) {
        strcpy(command, "\"");
        strcat(command, tool_path);
        strcat(command, "\"");
    } else {
        strcpy(command, tool_path);
    }

    if (args) {
        for (int i = 0; args[i]; i++) {
            strcat(command, " ");
            strcat(command, args[i]);
        }
    }

    return command;
}

/* Execute command and capture output */
static ToolExecResult* execute_with_capture(const char* command,
                                             const char* working_dir,
                                             bool show_output) {
    ToolExecResult* result = calloc(1, sizeof(ToolExecResult));
    if (!result) return NULL;

    time_t start_time = time(NULL);

    /* Change to working directory if specified */
    char* old_dir = NULL;
    if (working_dir) {
        old_dir = getcwd(NULL, 0);
        if (chdir(working_dir) != 0) {
            log_error("Failed to change to directory: %s", working_dir);
            free(result);
            free(old_dir);
            return NULL;
        }
    }

    /* Execute command and capture output */
    FILE* pipe = popen(command, "r");
    if (!pipe) {
        log_error("Failed to execute command: %s", command);
        result->success = false;
        result->exit_code = -1;
        if (old_dir) {
            chdir(old_dir);
            free(old_dir);
        }
        return result;
    }

    /* Capture output */
    char buffer[4096];
    size_t stdout_size = 0;
    size_t stdout_capacity = 4096;
    result->stdout_output = malloc(stdout_capacity);

    if (result->stdout_output) {
        result->stdout_output[0] = '\0';

        while (fgets(buffer, sizeof(buffer), pipe)) {
            size_t len = strlen(buffer);

            /* Show output in real-time if requested */
            if (show_output) {
                log_plain("%s", buffer);
            }

            /* Grow buffer if needed */
            if (stdout_size + len >= stdout_capacity) {
                stdout_capacity *= 2;
                char* new_output = realloc(result->stdout_output, stdout_capacity);
                if (!new_output) {
                    break;
                }
                result->stdout_output = new_output;
            }

            memcpy(result->stdout_output + stdout_size, buffer, len);
            stdout_size += len;
            result->stdout_output[stdout_size] = '\0';
        }
    }

    /* Get exit code */
    result->exit_code = pclose(pipe);

#ifdef _WIN32
    /* On Windows, pclose returns the raw exit code */
    result->success = (result->exit_code == 0);
#else
    /* On Unix, use WEXITSTATUS to get actual exit code */
    if (WIFEXITED(result->exit_code)) {
        result->exit_code = WEXITSTATUS(result->exit_code);
        result->success = (result->exit_code == 0);
    } else {
        result->success = false;
    }
#endif

    /* Calculate duration */
    result->duration_sec = difftime(time(NULL), start_time);

    /* Restore working directory */
    if (old_dir) {
        chdir(old_dir);
        free(old_dir);
    }

    return result;
}

/* Execute a tool command */
ToolExecResult* tool_execute(const ToolInfo* tool, const ToolExecOptions* options) {
    if (!tool || !tool->path) {
        log_error("Invalid tool or tool path");
        return NULL;
    }

    log_debug("Executing tool: %s (path: %s)", tool->name, tool->path);

    /* Build command string */
    char* command = NULL;
    if (options && options->args) {
        log_debug("Has args, count=%zu", options->arg_count);
        /* Convert args array to NULL-terminated array for build_command_string */
        char** null_term_args = calloc(options->arg_count + 1, sizeof(char*));
        if (!null_term_args) return NULL;

        for (size_t i = 0; i < options->arg_count; i++) {
            null_term_args[i] = options->args[i];
        }
        null_term_args[options->arg_count] = NULL;

        log_debug("Calling build_command_string");
        command = build_command_string(tool->path, null_term_args);
        log_debug("Command string: %s", command ? command : "NULL");
        free(null_term_args);
    } else {
        command = strdup(tool->path);
    }

    if (!command) return NULL;

    /* Execute */
    const char* working_dir = (options && options->working_dir) ? options->working_dir : NULL;
    bool show_output = (options && options->show_output);

    log_debug("About to execute_with_capture");
    ToolExecResult* result = execute_with_capture(command, working_dir, show_output);
    log_debug("Returned from execute_with_capture");

    free(command);
    return result;
}

/* Execute a tool by name */
ToolExecResult* tool_execute_by_name(const ToolRegistry* registry,
                                      const char* tool_name,
                                      const ToolExecOptions* options) {
    if (!registry || !tool_name) return NULL;

    const ToolInfo* tool = tool_registry_find(registry, tool_name);
    if (!tool) {
        log_error("Tool not found: %s", tool_name);
        return NULL;
    }

    if (!tool->is_available) {
        log_error("Tool not available: %s", tool_name);
        return NULL;
    }

    return tool_execute(tool, options);
}

/* Execute a command directly */
ToolExecResult* tool_execute_command(const char* command,
                                      char* const* args,
                                      const char* working_dir) {
    if (!command) return NULL;

    log_debug("Executing command: %s", command);

    /* Build full command */
    char* full_command = build_command_string(command, args);
    if (!full_command) return NULL;

    /* Execute */
    ToolExecResult* result = execute_with_capture(full_command, working_dir, false);

    free(full_command);
    return result;
}

/* Get the best available package manager */
const ToolInfo* package_get_default_manager(const ToolRegistry* registry) {
    if (!registry) return NULL;

    /* Priority order for package managers */
    const char* priority_order[] = {
#ifdef _WIN32
        "vcpkg",     /* Prefer vcpkg on Windows for C/C++ */
        "winget",
        "choco",
#elif defined(__APPLE__)
        "brew",      /* Prefer Homebrew on macOS */
        "vcpkg",
#else
        "apt",       /* Prefer apt on Linux */
        "apt-get",
        "dnf",
        "yum",
        "pacman",
        "vcpkg",
#endif
        NULL
    };

    for (int i = 0; priority_order[i]; i++) {
        const ToolInfo* tool = tool_registry_find(registry, priority_order[i]);
        if (tool && tool->is_available) {
            return tool;
        }
    }

    /* Fallback: return any available package manager */
    size_t count = 0;
    const ToolInfo** pkg_mgrs = tool_registry_find_by_type(
        registry, TOOL_TYPE_PACKAGE_MANAGER, &count);

    if (pkg_mgrs && count > 0) {
        for (size_t i = 0; i < count; i++) {
            if (pkg_mgrs[i]->is_available) {
                const ToolInfo* result = pkg_mgrs[i];
                free(pkg_mgrs);
                return result;
            }
        }
        free(pkg_mgrs);
    }

    return NULL;
}

/* Install a package */
ToolExecResult* package_install(const ToolRegistry* registry,
                                 const char* package_name,
                                 const ToolExecOptions* options) {
    if (!registry || !package_name) return NULL;

    const ToolInfo* pkg_mgr = package_get_default_manager(registry);
    if (!pkg_mgr) {
        log_error("No package manager available");
        return NULL;
    }

    log_info("Installing package '%s' using %s", package_name, pkg_mgr->name);

    /* Build install command based on package manager */
    ToolExecOptions* exec_opts = options ? (ToolExecOptions*)options : tool_exec_options_create();
    bool free_opts = (options == NULL);

    /* Allocate args array */
    char** args = calloc(4, sizeof(char*));
    if (!args) {
        if (free_opts) tool_exec_options_free(exec_opts);
        return NULL;
    }

    int arg_idx = 0;

    PackageManagerType type = (PackageManagerType)pkg_mgr->subtype;
    switch (type) {
        case PKG_MGR_APT:
            args[arg_idx++] = strdup("install");
            args[arg_idx++] = strdup("-y");
            args[arg_idx++] = strdup(package_name);
            break;

        case PKG_MGR_BREW:
            args[arg_idx++] = strdup("install");
            args[arg_idx++] = strdup(package_name);
            break;

        case PKG_MGR_VCPKG:
            args[arg_idx++] = strdup("install");
            args[arg_idx++] = strdup(package_name);
            break;

        case PKG_MGR_NPM:
            args[arg_idx++] = strdup("install");
            args[arg_idx++] = strdup(package_name);
            break;

        case PKG_MGR_PIP:
            args[arg_idx++] = strdup("install");
            args[arg_idx++] = strdup(package_name);
            break;

        case PKG_MGR_CARGO:
            args[arg_idx++] = strdup("install");
            args[arg_idx++] = strdup(package_name);
            break;

        default:
            args[arg_idx++] = strdup("install");
            args[arg_idx++] = strdup(package_name);
            break;
    }

    exec_opts->args = args;
    exec_opts->arg_count = arg_idx;
    exec_opts->show_output = true;

    ToolExecResult* result = tool_execute(pkg_mgr, exec_opts);

    /* Free args */
    for (int i = 0; i < arg_idx; i++) {
        free(args[i]);
    }
    free(args);

    if (free_opts) {
        tool_exec_options_free(exec_opts);
    }

    return result;
}

/* Update package manager cache */
ToolExecResult* package_update(const ToolRegistry* registry,
                                const ToolExecOptions* options) {
    if (!registry) return NULL;

    const ToolInfo* pkg_mgr = package_get_default_manager(registry);
    if (!pkg_mgr) {
        log_error("No package manager available");
        return NULL;
    }

    log_info("Updating package manager cache");

    ToolExecOptions* exec_opts = options ? (ToolExecOptions*)options : tool_exec_options_create();
    bool free_opts = (options == NULL);

    /* Build update command */
    char** args = calloc(2, sizeof(char*));
    if (!args) {
        if (free_opts) tool_exec_options_free(exec_opts);
        return NULL;
    }

    PackageManagerType type = (PackageManagerType)pkg_mgr->subtype;
    if (type == PKG_MGR_APT) {
        args[0] = strdup("update");
    } else if (type == PKG_MGR_BREW) {
        args[0] = strdup("update");
    } else {
        /* Most package managers don't need explicit update */
        free(args);
        if (free_opts) tool_exec_options_free(exec_opts);
        return NULL;
    }

    exec_opts->args = args;
    exec_opts->arg_count = 1;
    exec_opts->show_output = true;

    ToolExecResult* result = tool_execute(pkg_mgr, exec_opts);

    free(args[0]);
    free(args);

    if (free_opts) {
        tool_exec_options_free(exec_opts);
    }

    return result;
}

/* Search for a package */
bool package_search(const ToolRegistry* registry, const char* package_name) {
    if (!registry || !package_name) return false;

    const ToolInfo* pkg_mgr = package_get_default_manager(registry);
    if (!pkg_mgr) return false;

    /* Build search command */
    char** args = calloc(3, sizeof(char*));
    if (!args) return false;

    PackageManagerType type = (PackageManagerType)pkg_mgr->subtype;
    switch (type) {
        case PKG_MGR_APT:
            args[0] = strdup("search");
            args[1] = strdup(package_name);
            break;

        case PKG_MGR_BREW:
            args[0] = strdup("search");
            args[1] = strdup(package_name);
            break;

        case PKG_MGR_VCPKG:
            args[0] = strdup("search");
            args[1] = strdup(package_name);
            break;

        default:
            free(args);
            return false;
    }

    ToolExecOptions* options = tool_exec_options_create();
    options->args = args;
    options->arg_count = 2;
    options->show_output = false;

    ToolExecResult* result = tool_execute(pkg_mgr, options);

    bool found = (result && result->success && result->stdout_output &&
                  strlen(result->stdout_output) > 0);

    free(args[0]);
    free(args[1]);
    free(args);
    tool_exec_options_free(options);
    tool_exec_result_free(result);

    return found;
}
