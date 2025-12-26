/**
 * @file slash_commands.c
 * @brief Slash command handlers for REPL
 */

#include "cyxmake/slash_commands.h"
#include "cyxmake/cyxmake.h"
#include "cyxmake/logger.h"
#include "cyxmake/build_executor.h"
#include "cyxmake/file_ops.h"
#include "cyxmake/conversation_context.h"
#include "cyxmake/llm_interface.h"
#include "cyxmake/ai_provider.h"
#include "cyxmake/project_graph.h"
#include "cyxmake/project_context.h"
#include "cyxmake/smart_agent.h"
#include "cyxmake/error_recovery.h"
#include "cyxmake/cache_manager.h"
#include "cyxmake/action_planner.h"
#include "cyxmake/tool_executor.h"
#include "cyxmake/project_generator.h"
#include "cyxmake/agent_registry.h"
#include "cyxmake/agent_coordinator.h"
#include "cyxmake/agent_comm.h"
#include "cyxmake/task_queue.h"
#include "cyxmake/distributed/distributed.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define CLEAR_CMD "cls"
#else
#include <unistd.h>
#include <sys/wait.h>
#define CLEAR_CMD "clear"
#endif

/* ANSI color codes */
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"

/* Status symbols (ASCII for compatibility) */
#define SYM_CHECK   "[OK]"
#define SYM_CROSS   "[X]"
#define SYM_BULLET  "*"
#define SYM_WARN    "[!]"
#define SYM_ARROW   "->"

/* Helper to convert ErrorPatternType to string */
static const char* error_pattern_type_name(ErrorPatternType type) {
    switch (type) {
        case ERROR_PATTERN_MISSING_FILE:       return "Missing File";
        case ERROR_PATTERN_MISSING_LIBRARY:    return "Missing Library";
        case ERROR_PATTERN_MISSING_HEADER:     return "Missing Header";
        case ERROR_PATTERN_PERMISSION_DENIED:  return "Permission Denied";
        case ERROR_PATTERN_DISK_FULL:          return "Disk Full";
        case ERROR_PATTERN_SYNTAX_ERROR:       return "Syntax Error";
        case ERROR_PATTERN_UNDEFINED_REFERENCE: return "Undefined Reference";
        case ERROR_PATTERN_VERSION_MISMATCH:   return "Version Mismatch";
        case ERROR_PATTERN_CMAKE_VERSION:      return "CMake Version";
        case ERROR_PATTERN_NETWORK_ERROR:      return "Network Error";
        case ERROR_PATTERN_TIMEOUT:            return "Timeout";
        case ERROR_PATTERN_UNKNOWN:
        default:                               return "Unknown";
    }
}

/* Slash command definitions */
static const SlashCommand slash_commands[] = {
    {"help",    "h",    "Show available commands",      cmd_help},
    {"exit",    "q",    "Exit CyxMake",                 cmd_exit},
    {"quit",    NULL,   "Exit CyxMake",                 cmd_exit},
    {"clear",   "cls",  "Clear the screen",             cmd_clear},
    {"init",    "i",    "Initialize/analyze project",   cmd_init},
    {"build",   "b",    "Build the project",            cmd_build},
    {"clean",   "c",    "Clean build artifacts",        cmd_clean},
    {"status",  "s",    "Show project status",          cmd_status},
    {"config",  "cfg",  "Show configuration",           cmd_config},
    {"history", "hist", "Show command history",         cmd_history},
    {"version", "v",    "Show version info",            cmd_version},
    {"context", "ctx",  "Show conversation context",    cmd_context},
    {"ai",      NULL,   "AI status and commands",       cmd_ai},
    {"graph",   "g",    "Analyze project dependencies", cmd_graph},
    {"memory",  "m",    "Show/manage agent memory",     cmd_memory},
    {"recover", "r",    "Attempt to fix last error",    cmd_recover},
    {"fix",     NULL,   "Attempt to fix last error",    cmd_recover},
    {"create",  NULL,   "Create project from description", cmd_create},
    {"agent",   "a",    "Manage named agents",              cmd_agent},
    /* Distributed build commands */
    {"coordinator", "coord", "Manage distributed build coordinator", cmd_coordinator},
    {"workers",     "dw",    "List and manage remote workers",       cmd_workers},
    {"dbuild",      "db",    "Build using distributed workers",      cmd_dbuild},
    {NULL, NULL, NULL, NULL}
};

/* Check if input is a slash command */
bool is_slash_command(const char* input) {
    if (!input) return false;

    /* Skip leading whitespace */
    while (*input == ' ' || *input == '\t') input++;

    return *input == '/';
}

/* Get all slash commands */
const SlashCommand* get_slash_commands(int* out_count) {
    if (out_count) {
        int count = 0;
        for (int i = 0; slash_commands[i].name != NULL; i++) {
            count++;
        }
        *out_count = count;
    }
    return slash_commands;
}

/* Execute a slash command */
bool execute_slash_command(ReplSession* session, const char* input) {
    if (!session || !input) return true;

    /* Skip leading whitespace and the slash */
    while (*input == ' ' || *input == '\t') input++;
    if (*input == '/') input++;

    /* Extract command name */
    char cmd_name[64];
    int i = 0;
    while (*input && !isspace(*input) && i < 63) {
        cmd_name[i++] = tolower(*input);
        input++;
    }
    cmd_name[i] = '\0';

    /* Skip whitespace to get args */
    while (*input == ' ' || *input == '\t') input++;
    const char* args = (*input) ? input : NULL;

    /* Find matching command */
    for (i = 0; slash_commands[i].name != NULL; i++) {
        if (strcmp(cmd_name, slash_commands[i].name) == 0 ||
            (slash_commands[i].alias && strcmp(cmd_name, slash_commands[i].alias) == 0)) {
            return slash_commands[i].handler(session, args);
        }
    }

    /* Command not found */
    if (session->config.colors_enabled) {
        printf("%s%s Unknown command: /%s%s\n", COLOR_RED, SYM_CROSS, cmd_name, COLOR_RESET);
        printf("%sType /help for available commands%s\n", COLOR_DIM, COLOR_RESET);
    } else {
        printf("Unknown command: /%s\n", cmd_name);
        printf("Type /help for available commands\n");
    }

    return true;
}

/* /help command */
bool cmd_help(ReplSession* session, const char* args) {
    (void)args;

    if (session->config.colors_enabled) {
        printf("\n%s%sAvailable Commands:%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);

        printf("%sSlash Commands:%s\n", COLOR_YELLOW, COLOR_RESET);
        for (int i = 0; slash_commands[i].name != NULL; i++) {
            /* Skip aliases that point to same handler */
            if (i > 0 && slash_commands[i].handler == slash_commands[i-1].handler &&
                strcmp(slash_commands[i].description, slash_commands[i-1].description) == 0) {
                continue;
            }

            printf("  %s/%s%s", COLOR_GREEN, slash_commands[i].name, COLOR_RESET);
            if (slash_commands[i].alias) {
                printf(" %s(/%s)%s", COLOR_DIM, slash_commands[i].alias, COLOR_RESET);
            }
            printf("\n      %s%s%s\n", COLOR_DIM, slash_commands[i].description, COLOR_RESET);
        }

        printf("\n%sNatural Language:%s\n", COLOR_YELLOW, COLOR_RESET);
        printf("  %sJust type naturally:%s\n", COLOR_DIM, COLOR_RESET);
        printf("    • %sbuild the project%s\n", COLOR_CYAN, COLOR_RESET);
        printf("    • %sread main.c%s\n", COLOR_CYAN, COLOR_RESET);
        printf("    • %sclean up build files%s\n", COLOR_CYAN, COLOR_RESET);
        printf("    • %screate a new file hello.c%s\n", COLOR_CYAN, COLOR_RESET);
        printf("    • %sinstall curl%s\n", COLOR_CYAN, COLOR_RESET);
        printf("\n");
    } else {
        printf("\nAvailable Commands:\n\n");

        printf("Slash Commands:\n");
        for (int i = 0; slash_commands[i].name != NULL; i++) {
            if (i > 0 && slash_commands[i].handler == slash_commands[i-1].handler &&
                strcmp(slash_commands[i].description, slash_commands[i-1].description) == 0) {
                continue;
            }

            printf("  /%s", slash_commands[i].name);
            if (slash_commands[i].alias) {
                printf(" (/%s)", slash_commands[i].alias);
            }
            printf("\n      %s\n", slash_commands[i].description);
        }

        printf("\nNatural Language:\n");
        printf("  Just type naturally:\n");
        printf("    - build the project\n");
        printf("    - read main.c\n");
        printf("    - clean up build files\n");
        printf("    - create a new file hello.c\n");
        printf("    - install curl\n");
        printf("\n");
    }

    return true;
}

/* /exit command */
bool cmd_exit(ReplSession* session, const char* args) {
    (void)args;
    session->running = false;
    return false;  /* Signal to exit REPL */
}

/* /clear command */
bool cmd_clear(ReplSession* session, const char* args) {
    (void)session;
    (void)args;

    system(CLEAR_CMD);
    return true;
}

/* /init command */
bool cmd_init(ReplSession* session, const char* args) {
    (void)args;

    if (session->config.colors_enabled) {
        printf("%s%s%s Analyzing project...\n", COLOR_BLUE, SYM_BULLET, COLOR_RESET);
    } else {
        printf("Analyzing project...\n");
    }

    /* Check for common build files */
    bool found_cmake = file_exists("CMakeLists.txt");
    bool found_make = file_exists("Makefile");
    bool found_cargo = file_exists("Cargo.toml");
    bool found_package = file_exists("package.json");
    bool found_meson = file_exists("meson.build");

    int source_count = 0;
    int header_count = 0;

    /* Count source files in current directory (simple scan) */
    /* For a full implementation, would recursively scan */

    if (session->config.colors_enabled) {
        printf("\n%sProject Analysis:%s\n", COLOR_BOLD, COLOR_RESET);

        if (found_cmake) {
            printf("  %s%s%s Build system: %sCMake%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET, COLOR_CYAN, COLOR_RESET);
        } else if (found_make) {
            printf("  %s%s%s Build system: %sMake%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET, COLOR_CYAN, COLOR_RESET);
        } else if (found_cargo) {
            printf("  %s%s%s Build system: %sCargo (Rust)%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET, COLOR_CYAN, COLOR_RESET);
        } else if (found_package) {
            printf("  %s%s%s Build system: %snpm (Node.js)%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET, COLOR_CYAN, COLOR_RESET);
        } else if (found_meson) {
            printf("  %s%s%s Build system: %sMeson%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET, COLOR_CYAN, COLOR_RESET);
        } else {
            printf("  %s%s%s No build system detected\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
        }

        if (session->working_dir) {
            printf("  %s%s%s Working directory: %s%s%s\n",
                   COLOR_BLUE, SYM_BULLET, COLOR_RESET, COLOR_DIM, session->working_dir, COLOR_RESET);
        }

        printf("\n%s%s Project initialized%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
    } else {
        printf("\nProject Analysis:\n");

        if (found_cmake) {
            printf("  Build system: CMake\n");
        } else if (found_make) {
            printf("  Build system: Make\n");
        } else if (found_cargo) {
            printf("  Build system: Cargo (Rust)\n");
        } else if (found_package) {
            printf("  Build system: npm (Node.js)\n");
        } else if (found_meson) {
            printf("  Build system: Meson\n");
        } else {
            printf("  No build system detected\n");
        }

        if (session->working_dir) {
            printf("  Working directory: %s\n", session->working_dir);
        }

        printf("\nProject initialized\n");
    }

    return true;
}

/* Execute a command and capture output */
static char* execute_and_capture(const char* command, int* exit_code) {
    char* output = malloc(1024 * 1024);  /* 1MB buffer */
    if (!output) {
        *exit_code = -1;
        return NULL;
    }
    output[0] = '\0';

#ifdef _WIN32
    char cmd_with_redirect[2048];
    snprintf(cmd_with_redirect, sizeof(cmd_with_redirect), "%s 2>&1", command);
    FILE* pipe = _popen(cmd_with_redirect, "r");
#else
    char cmd_with_redirect[2048];
    snprintf(cmd_with_redirect, sizeof(cmd_with_redirect), "%s 2>&1", command);
    FILE* pipe = popen(cmd_with_redirect, "r");
#endif

    if (!pipe) {
        *exit_code = -1;
        free(output);
        return NULL;
    }

    size_t offset = 0;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL && offset < 1024 * 1024 - 1) {
        size_t len = strlen(buffer);
        if (offset + len < 1024 * 1024) {
            memcpy(output + offset, buffer, len);
            offset += len;
            /* Print output in real-time */
            printf("%s", buffer);
        }
    }
    output[offset] = '\0';

#ifdef _WIN32
    *exit_code = _pclose(pipe);
#else
    int status = pclose(pipe);
    *exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif

    return output;
}

/* Try to automatically fix CMake version errors */
static bool try_fix_cmake_version(const char* build_output, bool colors_enabled) {
    /* Check if this is a CMake version compatibility error */
    if (!strstr(build_output, "Compatibility with CMake <") &&
        !strstr(build_output, "cmake_minimum_required")) {
        return false;
    }

    if (colors_enabled) {
        printf("\n%s%s%s Detected CMake version compatibility error\n",
               COLOR_YELLOW, SYM_ARROW, COLOR_RESET);
        printf("  %s%s%s Auto-fixing cmake_minimum_required version...\n",
               COLOR_BLUE, SYM_BULLET, COLOR_RESET);
    } else {
        printf("\nDetected CMake version compatibility error\n");
        printf("Auto-fixing cmake_minimum_required version...\n");
    }

    /* Read CMakeLists.txt */
    FILE* file = fopen("CMakeLists.txt", "r");
    if (!file) {
        if (colors_enabled) {
            printf("  %s%s Could not open CMakeLists.txt%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
        }
        return false;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* content = malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return false;
    }

    size_t bytes_read = fread(content, 1, file_size, file);
    fclose(file);
    content[bytes_read] = '\0';

    /* Find cmake_minimum_required and VERSION */
    char* cmake_req = strstr(content, "cmake_minimum_required");
    if (!cmake_req) {
        free(content);
        return false;
    }

    char* version_start = strstr(cmake_req, "VERSION");
    if (!version_start) {
        version_start = strstr(cmake_req, "version");
    }

    if (!version_start) {
        free(content);
        return false;
    }

    /* Skip "VERSION" and whitespace */
    version_start += strlen("VERSION");
    while (*version_start && (*version_start == ' ' || *version_start == '\t')) {
        version_start++;
    }

    /* Find end of version number */
    char* version_end = version_start;
    while (*version_end && *version_end != ')' && *version_end != ' ' &&
           *version_end != '\t' && *version_end != '\n' && *version_end != '\r') {
        version_end++;
    }

    /* Extract old version for logging */
    size_t old_version_len = version_end - version_start;
    char old_version[32] = {0};
    if (old_version_len < sizeof(old_version)) {
        memcpy(old_version, version_start, old_version_len);
    }

    const char* new_version = "3.10";

    if (colors_enabled) {
        printf("  %s%s%s Updating VERSION %s -> %s\n",
               COLOR_BLUE, SYM_BULLET, COLOR_RESET, old_version, new_version);
    } else {
        printf("  Updating VERSION %s -> %s\n", old_version, new_version);
    }

    /* Build new content */
    size_t prefix_len = version_start - content;
    size_t suffix_len = bytes_read - (version_end - content);
    size_t new_version_len = strlen(new_version);
    size_t new_size = prefix_len + new_version_len + suffix_len + 1;

    char* new_content = malloc(new_size);
    if (!new_content) {
        free(content);
        return false;
    }

    memcpy(new_content, content, prefix_len);
    memcpy(new_content + prefix_len, new_version, new_version_len);
    memcpy(new_content + prefix_len + new_version_len, version_end, suffix_len);
    new_content[prefix_len + new_version_len + suffix_len] = '\0';

    free(content);

    /* Write back to file */
    file = fopen("CMakeLists.txt", "w");
    if (!file) {
        free(new_content);
        return false;
    }

    fputs(new_content, file);
    fclose(file);
    free(new_content);

    if (colors_enabled) {
        printf("  %s%s CMakeLists.txt updated successfully%s\n",
               COLOR_GREEN, SYM_CHECK, COLOR_RESET);
    } else {
        printf("  CMakeLists.txt updated successfully\n");
    }

    return true;
}

/* /build command */
bool cmd_build(ReplSession* session, const char* args) {
    (void)args;

    if (session->config.colors_enabled) {
        printf("%s%s%s Building project...\n", COLOR_BLUE, SYM_BULLET, COLOR_RESET);
    } else {
        printf("Building project...\n");
    }

    /* Detect build system and run appropriate command */
    int result = -1;
    char* build_output = NULL;
    int max_retries = 3;
    int retry_count = 0;

    if (file_exists("CMakeLists.txt")) {
        /* CMake project - with error recovery */
        while (retry_count < max_retries) {
            /* Configure step */
            if (!file_exists("build/CMakeCache.txt")) {
                if (session->config.colors_enabled) {
                    printf("  %s%s%s Configuring CMake...\n", COLOR_BLUE, SYM_BULLET, COLOR_RESET);
                } else {
                    printf("  Configuring CMake...\n");
                }

                free(build_output);
                build_output = execute_and_capture("cmake -B build -S .", &result);

                /* Check for errors and try to fix */
                if (result != 0 && build_output) {
                    if (try_fix_cmake_version(build_output, session->config.colors_enabled)) {
                        /* Clean old build artifacts and retry */
                        dir_delete_recursive("build");
                        retry_count++;
                        if (session->config.colors_enabled) {
                            printf("\n%s%s%s Retrying build (attempt %d/%d)...\n",
                                   COLOR_YELLOW, SYM_ARROW, COLOR_RESET, retry_count + 1, max_retries);
                        } else {
                            printf("\nRetrying build (attempt %d/%d)...\n", retry_count + 1, max_retries);
                        }
                        continue;
                    }
                    break;  /* Unknown error, stop retrying */
                }
            }

            /* Build step */
            if (result == 0 || file_exists("build")) {
                if (session->config.colors_enabled) {
                    printf("  %s%s%s Compiling...\n", COLOR_BLUE, SYM_BULLET, COLOR_RESET);
                } else {
                    printf("  Compiling...\n");
                }

                free(build_output);
                build_output = execute_and_capture("cmake --build build", &result);
            }

            break;  /* Success or unrecoverable error */
        }
    } else if (file_exists("Makefile")) {
        /* Make project */
        result = system("make");
    } else if (file_exists("Cargo.toml")) {
        /* Rust project */
        result = system("cargo build");
    } else if (file_exists("package.json")) {
        /* Node.js project */
        result = system("npm run build");
    } else if (file_exists("meson.build")) {
        /* Meson project */
        if (!file_exists("builddir")) {
            result = system("meson setup builddir");
        }
        if (result == 0 || file_exists("builddir")) {
            result = system("meson compile -C builddir");
        }
    } else {
        if (session->config.colors_enabled) {
            printf("%s%s No build system detected%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
            printf("%sSupported: CMake, Make, Cargo, npm, Meson%s\n", COLOR_DIM, COLOR_RESET);
        } else {
            printf("Error: No build system detected\n");
            printf("Supported: CMake, Make, Cargo, npm, Meson\n");
        }
        return true;
    }

    if (result == 0) {
        if (session->config.colors_enabled) {
            printf("%s%s Build successful%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
        } else {
            printf("Build successful\n");
        }
    } else {
        if (session->config.colors_enabled) {
            printf("%s%s Build failed (exit code: %d)%s\n", COLOR_RED, SYM_CROSS, result, COLOR_RESET);
        } else {
            printf("Build failed (exit code: %d)\n", result);
        }

        /* Store last error for potential "fix" commands */
        free(session->last_error);
        session->last_error = build_output ? strdup(build_output) : strdup("Build failed");

        /* Record in conversation context */
        if (session->conversation) {
            conversation_set_error(session->conversation,
                                   session->last_error, "build", NULL, 0);
        }

        /* Offer automatic recovery if AI/tools available */
        bool has_recovery = (session->llm && llm_is_ready(session->llm)) ||
                            (session->current_provider && ai_provider_is_ready(session->current_provider)) ||
                            (session->orchestrator && cyxmake_get_tools(session->orchestrator));

        if (has_recovery) {
            if (session->config.colors_enabled) {
                printf("\n%sWould you like to attempt automatic recovery? [y/N]: %s",
                       COLOR_CYAN, COLOR_RESET);
            } else {
                printf("\nWould you like to attempt automatic recovery? [y/N]: ");
            }
            fflush(stdout);

            char response[16] = {0};
            if (fgets(response, sizeof(response), stdin) != NULL &&
                (response[0] == 'y' || response[0] == 'Y')) {
                cmd_recover(session, NULL);
            }
        }
    }

    free(build_output);
    return true;
}

/* /clean command */
bool cmd_clean(ReplSession* session, const char* args) {
    (void)args;

    if (session->config.colors_enabled) {
        printf("%s%s%s Cleaning build artifacts...\n", COLOR_BLUE, SYM_BULLET, COLOR_RESET);
    } else {
        printf("Cleaning build artifacts...\n");
    }

    bool cleaned = false;

    /* Clean common build directories */
    const char* build_dirs[] = {"build", "builddir", ".cyxmake", "target", "node_modules", NULL};

    for (int i = 0; build_dirs[i] != NULL; i++) {
        if (file_exists(build_dirs[i])) {
            if (dir_delete_recursive(build_dirs[i])) {
                if (session->config.colors_enabled) {
                    printf("  %s%s%s Removed %s%s%s\n",
                           COLOR_GREEN, SYM_CHECK, COLOR_RESET, COLOR_DIM, build_dirs[i], COLOR_RESET);
                } else {
                    printf("  Removed %s\n", build_dirs[i]);
                }
                cleaned = true;
            }
        }
    }

    /* Also try make clean if Makefile exists */
    if (file_exists("Makefile")) {
        system("make clean 2>/dev/null");
        cleaned = true;
    }

    if (cleaned) {
        if (session->config.colors_enabled) {
            printf("%s%s Clean complete%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
        } else {
            printf("Clean complete\n");
        }
    } else {
        if (session->config.colors_enabled) {
            printf("%s%s%s Nothing to clean%s\n", COLOR_BLUE, SYM_BULLET, COLOR_RESET, COLOR_RESET);
        } else {
            printf("Nothing to clean\n");
        }
    }

    return true;
}

/* /status command */
bool cmd_status(ReplSession* session, const char* args) {
    (void)args;

    if (session->config.colors_enabled) {
        printf("\n%s%sCyxMake Status%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);

        printf("%sSession:%s\n", COLOR_YELLOW, COLOR_RESET);
        printf("  Commands executed: %s%d%s\n", COLOR_GREEN, session->command_count, COLOR_RESET);
        printf("  History entries: %s%d%s\n", COLOR_GREEN, session->history_count, COLOR_RESET);

        if (session->working_dir) {
            printf("  Working directory: %s%s%s\n", COLOR_DIM, session->working_dir, COLOR_RESET);
        }

        if (session->current_file) {
            printf("  Current file: %s%s%s\n", COLOR_CYAN, session->current_file, COLOR_RESET);
        }

        if (session->last_error) {
            printf("  Last error: %s%s%s\n", COLOR_RED, session->last_error, COLOR_RESET);
        }

        printf("\n%sConfiguration:%s\n", COLOR_YELLOW, COLOR_RESET);
        printf("  Colors: %s%s%s\n",
               session->config.colors_enabled ? COLOR_GREEN : COLOR_RED,
               session->config.colors_enabled ? "enabled" : "disabled",
               COLOR_RESET);
        printf("  Verbose: %s%s%s\n",
               session->config.verbose ? COLOR_GREEN : COLOR_DIM,
               session->config.verbose ? "enabled" : "disabled",
               COLOR_RESET);

        printf("\n");
    } else {
        printf("\nCyxMake Status\n\n");

        printf("Session:\n");
        printf("  Commands executed: %d\n", session->command_count);
        printf("  History entries: %d\n", session->history_count);

        if (session->working_dir) {
            printf("  Working directory: %s\n", session->working_dir);
        }

        if (session->current_file) {
            printf("  Current file: %s\n", session->current_file);
        }

        if (session->last_error) {
            printf("  Last error: %s\n", session->last_error);
        }

        printf("\nConfiguration:\n");
        printf("  Colors: %s\n", session->config.colors_enabled ? "enabled" : "disabled");
        printf("  Verbose: %s\n", session->config.verbose ? "enabled" : "disabled");

        printf("\n");
    }

    return true;
}

/* /config command */
bool cmd_config(ReplSession* session, const char* args) {
    if (args && strlen(args) > 0) {
        /* Parse config set command */
        char key[64], value[64];
        if (sscanf(args, "set %63s %63s", key, value) == 2) {
            if (strcmp(key, "colors") == 0) {
                session->config.colors_enabled = (strcmp(value, "on") == 0 ||
                                                   strcmp(value, "true") == 0 ||
                                                   strcmp(value, "1") == 0);
                printf("Colors: %s\n", session->config.colors_enabled ? "enabled" : "disabled");
            } else if (strcmp(key, "verbose") == 0) {
                session->config.verbose = (strcmp(value, "on") == 0 ||
                                           strcmp(value, "true") == 0 ||
                                           strcmp(value, "1") == 0);
                printf("Verbose: %s\n", session->config.verbose ? "enabled" : "disabled");
            } else {
                printf("Unknown config key: %s\n", key);
            }
            return true;
        }
    }

    /* Show current config */
    if (session->config.colors_enabled) {
        printf("\n%s%sConfiguration:%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
        printf("  %sprompt%s = \"%s\"\n", COLOR_YELLOW, COLOR_RESET, session->config.prompt);
        printf("  %scolors%s = %s\n", COLOR_YELLOW, COLOR_RESET,
               session->config.colors_enabled ? "true" : "false");
        printf("  %sverbose%s = %s\n", COLOR_YELLOW, COLOR_RESET,
               session->config.verbose ? "true" : "false");
        printf("  %shistory_size%s = %d\n", COLOR_YELLOW, COLOR_RESET,
               session->config.history_size);
        printf("\n%sUsage:%s /config set <key> <value>\n", COLOR_DIM, COLOR_RESET);
        printf("%sExample:%s /config set colors off\n\n", COLOR_DIM, COLOR_RESET);
    } else {
        printf("\nConfiguration:\n\n");
        printf("  prompt = \"%s\"\n", session->config.prompt);
        printf("  colors = %s\n", session->config.colors_enabled ? "true" : "false");
        printf("  verbose = %s\n", session->config.verbose ? "true" : "false");
        printf("  history_size = %d\n", session->config.history_size);
        printf("\nUsage: /config set <key> <value>\n");
        printf("Example: /config set colors off\n\n");
    }

    return true;
}

/* /history command */
bool cmd_history(ReplSession* session, const char* args) {
    (void)args;

    if (session->history_count == 0) {
        if (session->config.colors_enabled) {
            printf("%sNo command history%s\n", COLOR_DIM, COLOR_RESET);
        } else {
            printf("No command history\n");
        }
        return true;
    }

    if (session->config.colors_enabled) {
        printf("\n%s%sCommand History:%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
        for (int i = 0; i < session->history_count; i++) {
            printf("  %s%3d%s  %s\n", COLOR_DIM, i + 1, COLOR_RESET, session->history[i]);
        }
        printf("\n");
    } else {
        printf("\nCommand History:\n\n");
        for (int i = 0; i < session->history_count; i++) {
            printf("  %3d  %s\n", i + 1, session->history[i]);
        }
        printf("\n");
    }

    return true;
}

/* /version command */
bool cmd_version(ReplSession* session, const char* args) {
    (void)args;

    const char* version = cyxmake_version();

    if (session->config.colors_enabled) {
        printf("\n%s%sCyxMake%s v%s%s%s\n",
               COLOR_BOLD, COLOR_GREEN, COLOR_RESET,
               COLOR_CYAN, version, COLOR_RESET);
        printf("%sAI-Powered Build Automation%s\n\n", COLOR_DIM, COLOR_RESET);
    } else {
        printf("\nCyxMake v%s\n", version);
        printf("AI-Powered Build Automation\n\n");
    }

    return true;
}

/* /context command */
bool cmd_context(ReplSession* session, const char* args) {
    (void)args;

    if (!session->conversation) {
        if (session->config.colors_enabled) {
            printf("%sNo conversation context available%s\n", COLOR_DIM, COLOR_RESET);
        } else {
            printf("No conversation context available\n");
        }
        return true;
    }

    if (session->config.colors_enabled) {
        printf("\n%s%sConversation Context%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);

        /* Messages count */
        printf("%sMessages:%s %d\n", COLOR_YELLOW, COLOR_RESET,
               session->conversation->message_count);

        /* Current file */
        const char* current_file = conversation_get_current_file(session->conversation);
        if (current_file) {
            printf("%sCurrent file:%s %s%s%s\n", COLOR_YELLOW, COLOR_RESET,
                   COLOR_CYAN, current_file, COLOR_RESET);
        } else {
            printf("%sCurrent file:%s %s(none)%s\n", COLOR_YELLOW, COLOR_RESET,
                   COLOR_DIM, COLOR_RESET);
        }

        /* Last error */
        const char* last_error = conversation_get_last_error(session->conversation);
        if (last_error) {
            printf("%sLast error:%s %s%s%s\n", COLOR_YELLOW, COLOR_RESET,
                   COLOR_RED, last_error, COLOR_RESET);
        } else {
            printf("%sLast error:%s %s(none)%s\n", COLOR_YELLOW, COLOR_RESET,
                   COLOR_DIM, COLOR_RESET);
        }

        /* Recent messages */
        printf("\n%sRecent activity:%s\n", COLOR_YELLOW, COLOR_RESET);
        int start = session->conversation->message_count > 5 ?
                    session->conversation->message_count - 5 : 0;
        for (int i = start; i < session->conversation->message_count; i++) {
            ConversationMessage* msg = &session->conversation->messages[i];
            const char* role_color = msg->role == MSG_ROLE_USER ? COLOR_GREEN :
                                     msg->role == MSG_ROLE_ASSISTANT ? COLOR_BLUE :
                                     msg->role == MSG_ROLE_SYSTEM ? COLOR_YELLOW : COLOR_DIM;

            /* Truncate long messages */
            char preview[64];
            strncpy(preview, msg->content, 56);
            preview[56] = '\0';
            if (strlen(msg->content) > 56) {
                strcat(preview, "...");
            }

            printf("  %s[%s]%s %s\n", role_color,
                   message_role_name(msg->role), COLOR_RESET, preview);
        }

        printf("\n");
    } else {
        printf("\nConversation Context\n\n");

        printf("Messages: %d\n", session->conversation->message_count);

        const char* current_file = conversation_get_current_file(session->conversation);
        printf("Current file: %s\n", current_file ? current_file : "(none)");

        const char* last_error = conversation_get_last_error(session->conversation);
        printf("Last error: %s\n", last_error ? last_error : "(none)");

        printf("\nRecent activity:\n");
        int start = session->conversation->message_count > 5 ?
                    session->conversation->message_count - 5 : 0;
        for (int i = start; i < session->conversation->message_count; i++) {
            ConversationMessage* msg = &session->conversation->messages[i];
            char preview[64];
            strncpy(preview, msg->content, 56);
            preview[56] = '\0';
            if (strlen(msg->content) > 56) {
                strcat(preview, "...");
            }
            printf("  [%s] %s\n", message_role_name(msg->role), preview);
        }

        printf("\n");
    }

    return true;
}

/* /ai command */
bool cmd_ai(ReplSession* session, const char* args) {
    if (args && strlen(args) > 0) {
        /* Handle subcommands */
        if (strncmp(args, "providers", 9) == 0 || strncmp(args, "list", 4) == 0) {
            /* /ai providers - list available providers */
            if (session->config.colors_enabled) {
                printf("\n%s%sAI Providers%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
            } else {
                printf("\nAI Providers\n\n");
            }

            if (!session->ai_registry || ai_registry_count(session->ai_registry) == 0) {
                if (session->config.colors_enabled) {
                    printf("%sNo providers configured%s\n", COLOR_DIM, COLOR_RESET);
                    printf("\n%sTo configure providers:%s\n", COLOR_YELLOW, COLOR_RESET);
                    printf("  1. Copy %scyxmake.example.toml%s to %scyxmake.toml%s\n",
                           COLOR_CYAN, COLOR_RESET, COLOR_CYAN, COLOR_RESET);
                    printf("  2. Configure your API keys and provider settings\n");
                    printf("  3. Restart CyxMake\n\n");
                } else {
                    printf("No providers configured\n\n");
                    printf("To configure providers:\n");
                    printf("  1. Copy cyxmake.example.toml to cyxmake.toml\n");
                    printf("  2. Configure your API keys and provider settings\n");
                    printf("  3. Restart CyxMake\n\n");
                }
            } else {
                const char* names[16];
                int count = ai_registry_list(session->ai_registry, names, 16);

                for (int i = 0; i < count; i++) {
                    AIProvider* prov = ai_registry_get(session->ai_registry, names[i]);
                    if (!prov) continue;

                    bool is_current = (prov == session->current_provider);
                    const char* status = ai_provider_status_to_string(ai_provider_status(prov));
                    const char* type = ai_provider_type_to_string(prov->config.type);

                    if (session->config.colors_enabled) {
                        printf("  %s%s%s %s%s%s (%s) - %s%s%s\n",
                               is_current ? COLOR_GREEN : "",
                               is_current ? "*" : " ",
                               COLOR_RESET,
                               COLOR_CYAN, names[i], COLOR_RESET,
                               type,
                               status[0] == 'r' ? COLOR_GREEN : COLOR_YELLOW,
                               status, COLOR_RESET);
                        if (prov->config.model) {
                            printf("      Model: %s%s%s\n", COLOR_DIM, prov->config.model, COLOR_RESET);
                        }
                    } else {
                        printf("  %s %s (%s) - %s\n",
                               is_current ? "*" : " ",
                               names[i], type, status);
                        if (prov->config.model) {
                            printf("      Model: %s\n", prov->config.model);
                        }
                    }
                }
                printf("\n");
            }
            return true;
        }
        else if (strncmp(args, "use ", 4) == 0 || strncmp(args, "switch ", 7) == 0) {
            /* /ai use <provider> - switch to a provider */
            const char* provider_name = args + (args[0] == 'u' ? 4 : 7);
            while (*provider_name == ' ' || *provider_name == '\t') provider_name++;

            if (!session->ai_registry) {
                printf("No AI providers configured.\n");
                return true;
            }

            AIProvider* provider = ai_registry_get(session->ai_registry, provider_name);
            if (!provider) {
                if (session->config.colors_enabled) {
                    printf("%s%s Provider not found: %s%s\n",
                           COLOR_RED, SYM_CROSS, provider_name, COLOR_RESET);
                    printf("%sUse '/ai providers' to list available providers%s\n",
                           COLOR_DIM, COLOR_RESET);
                } else {
                    printf("Provider not found: %s\n", provider_name);
                    printf("Use '/ai providers' to list available providers\n");
                }
                return true;
            }

            /* Initialize if not ready */
            if (!ai_provider_is_ready(provider)) {
                if (!ai_provider_init(provider)) {
                    if (session->config.colors_enabled) {
                        printf("%s%s Failed to initialize provider: %s%s\n",
                               COLOR_RED, SYM_CROSS, provider_name, COLOR_RESET);
                        const char* err = ai_provider_error(provider);
                        if (err) printf("  %s%s%s\n", COLOR_DIM, err, COLOR_RESET);
                    } else {
                        printf("Failed to initialize provider: %s\n", provider_name);
                        const char* err = ai_provider_error(provider);
                        if (err) printf("  %s\n", err);
                    }
                    return true;
                }
            }

            session->current_provider = provider;

            if (session->config.colors_enabled) {
                printf("%s%s Switched to provider: %s%s%s\n",
                       COLOR_GREEN, SYM_CHECK, COLOR_CYAN, provider_name, COLOR_RESET);
                if (provider->config.model) {
                    printf("  Model: %s%s%s\n", COLOR_DIM, provider->config.model, COLOR_RESET);
                }
            } else {
                printf("Switched to provider: %s\n", provider_name);
                if (provider->config.model) {
                    printf("  Model: %s\n", provider->config.model);
                }
            }
            return true;
        }
        else if (strncmp(args, "test", 4) == 0) {
            /* /ai test - test current provider */
            if (!session->current_provider) {
                if (!session->llm || !llm_is_ready(session->llm)) {
                    printf("No AI provider active. Use '/ai use <provider>' or '/ai load <model>'.\n");
                    return true;
                }
            }

            if (session->config.colors_enabled) {
                printf("%sTesting AI...%s\n", COLOR_DIM, COLOR_RESET);
            }

            char* response = NULL;

            /* Try cloud provider first */
            if (session->current_provider && ai_provider_is_ready(session->current_provider)) {
                response = ai_provider_query(session->current_provider,
                    "Say 'Hello! AI is working.' in one short sentence.", 50);
            }
            /* Fall back to local LLM */
            else if (session->llm && llm_is_ready(session->llm)) {
                response = llm_query_simple(session->llm,
                    "Say 'Hello! AI is working.' in one short sentence.", 50);
            }

            if (response) {
                if (session->config.colors_enabled) {
                    printf("%s%s AI response:%s %s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET, response);
                } else {
                    printf("AI response: %s\n", response);
                }
                free(response);
            } else {
                if (session->config.colors_enabled) {
                    printf("%s%s AI test failed%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
                    if (session->current_provider) {
                        const char* err = ai_provider_error(session->current_provider);
                        if (err) printf("  %s%s%s\n", COLOR_DIM, err, COLOR_RESET);
                    }
                } else {
                    printf("AI test failed.\n");
                }
            }
            return true;
        }
        else if (strncmp(args, "load", 4) == 0) {
            /* /ai load [path] - load a local model */
            const char* path = args + 4;
            while (*path == ' ' || *path == '\t') path++;

            if (*path == '\0') {
                path = NULL;
            }

            if (session->config.colors_enabled) {
                printf("%sLoading local AI model...%s\n", COLOR_DIM, COLOR_RESET);
            } else {
                printf("Loading local AI model...\n");
            }

            char* model_path = path ? strdup(path) : llm_get_default_model_path();
            if (!model_path) {
                printf("Could not determine model path.\n");
                return true;
            }

            if (!llm_validate_model_file(model_path)) {
                if (session->config.colors_enabled) {
                    printf("%s%s Model file not found or invalid: %s%s\n",
                           COLOR_RED, SYM_CROSS, model_path, COLOR_RESET);
                    printf("\n%sTo download a model:%s\n", COLOR_YELLOW, COLOR_RESET);
                    printf("  mkdir -p ~/.cyxmake/models\n");
                    printf("  # Download Qwen2.5-Coder-3B (recommended):\n");
                    printf("  wget https://huggingface.co/Qwen/Qwen2.5-Coder-3B-Instruct-GGUF/resolve/main/qwen2.5-coder-3b-instruct-q4_k_m.gguf\n");
                    printf("  mv qwen2.5-coder-3b-instruct-q4_k_m.gguf ~/.cyxmake/models/\n");
                    printf("\n%sOr configure cloud providers in cyxmake.toml%s\n", COLOR_DIM, COLOR_RESET);
                } else {
                    printf("Model file not found: %s\n", model_path);
                }
                free(model_path);
                return true;
            }

            if (session->llm) {
                llm_shutdown(session->llm);
                session->llm = NULL;
            }

            LLMConfig* config = llm_config_default();
            config->model_path = model_path;
            config->verbose = session->config.verbose;

            session->llm = llm_init(config);
            llm_config_free(config);

            if (session->llm) {
                /* Clear cloud provider when using local */
                session->current_provider = NULL;

                if (session->config.colors_enabled) {
                    printf("%s%s Local AI model loaded!%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
                } else {
                    printf("Local AI model loaded!\n");
                }

                LLMModelInfo* info = llm_get_model_info(session->llm);
                if (info) {
                    printf("  Model: %s\n", info->model_name);
                    printf("  Context: %d tokens\n", info->context_length);
                    if (info->n_gpu_layers > 0) {
                        printf("  GPU: %s (%d layers)\n",
                               llm_gpu_backend_name(info->gpu_backend), info->n_gpu_layers);
                    } else {
                        printf("  Running on CPU\n");
                    }
                    llm_model_info_free(info);
                }
            } else {
                if (session->config.colors_enabled) {
                    printf("%s%s Failed to load AI model%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
                } else {
                    printf("Failed to load AI model.\n");
                }
            }

            free(model_path);
            return true;
        }
        else if (strncmp(args, "unload", 6) == 0) {
            /* /ai unload - unload all AI */
            if (session->llm) {
                llm_shutdown(session->llm);
                session->llm = NULL;
            }
            session->current_provider = NULL;
            printf("AI unloaded.\n");
            return true;
        }
    }

    /* Show AI status */
    if (session->config.colors_enabled) {
        printf("\n%s%sAI Status%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);

        /* Show cloud provider status */
        if (session->current_provider) {
            const char* name = session->current_provider->config.name;
            const char* type = ai_provider_type_to_string(session->current_provider->config.type);
            const char* model = session->current_provider->config.model;
            const char* status = ai_provider_status_to_string(
                ai_provider_status(session->current_provider));

            printf("%sCloud Provider:%s %s%s%s (%s)\n", COLOR_YELLOW, COLOR_RESET,
                   COLOR_GREEN, name, COLOR_RESET, type);
            printf("  Model: %s\n", model ? model : "(default)");
            printf("  Status: %s%s%s\n",
                   status[0] == 'r' ? COLOR_GREEN : COLOR_YELLOW, status, COLOR_RESET);
        }
        /* Show local LLM status */
        else if (session->llm && llm_is_ready(session->llm)) {
            printf("%sLocal LLM:%s %sLoaded%s\n", COLOR_YELLOW, COLOR_RESET,
                   COLOR_GREEN, COLOR_RESET);

            LLMModelInfo* info = llm_get_model_info(session->llm);
            if (info) {
                printf("  Model: %s\n", info->model_name);
                printf("  Context: %d tokens\n", info->context_length);
                printf("  Backend: %s", llm_gpu_backend_name(info->gpu_backend));
                if (info->n_gpu_layers > 0) {
                    printf(" (%d GPU layers)", info->n_gpu_layers);
                }
                printf("\n");
                llm_model_info_free(info);
            }
        } else {
            printf("%sStatus:%s %sNo AI active%s\n", COLOR_YELLOW, COLOR_RESET,
                   COLOR_RED, COLOR_RESET);
        }

        /* Show configured providers count */
        if (session->ai_registry) {
            int count = ai_registry_count(session->ai_registry);
            if (count > 0) {
                printf("\n%sConfigured providers:%s %d\n", COLOR_YELLOW, COLOR_RESET, count);
            }
        }

        printf("\n%sCommands:%s\n", COLOR_YELLOW, COLOR_RESET);
        printf("  %s/ai providers%s     - List available providers\n", COLOR_CYAN, COLOR_RESET);
        printf("  %s/ai use <name>%s    - Switch to a provider\n", COLOR_CYAN, COLOR_RESET);
        printf("  %s/ai test%s          - Test current AI\n", COLOR_CYAN, COLOR_RESET);
        printf("  %s/ai load [path]%s   - Load local GGUF model\n", COLOR_CYAN, COLOR_RESET);
        printf("  %s/ai unload%s        - Unload AI\n", COLOR_CYAN, COLOR_RESET);
        printf("\n%sConfiguration:%s cyxmake.toml\n", COLOR_YELLOW, COLOR_RESET);
        printf("\n");
    } else {
        printf("\nAI Status\n\n");

        if (session->current_provider) {
            const char* name = session->current_provider->config.name;
            const char* type = ai_provider_type_to_string(session->current_provider->config.type);
            const char* model = session->current_provider->config.model;

            printf("Cloud Provider: %s (%s)\n", name, type);
            printf("  Model: %s\n", model ? model : "(default)");
        } else if (session->llm && llm_is_ready(session->llm)) {
            printf("Local LLM: Loaded\n");
            LLMModelInfo* info = llm_get_model_info(session->llm);
            if (info) {
                printf("  Model: %s\n", info->model_name);
                printf("  Context: %d tokens\n", info->context_length);
                llm_model_info_free(info);
            }
        } else {
            printf("Status: No AI active\n");
        }

        if (session->ai_registry) {
            int count = ai_registry_count(session->ai_registry);
            if (count > 0) {
                printf("\nConfigured providers: %d\n", count);
            }
        }

        printf("\nCommands:\n");
        printf("  /ai providers     - List available providers\n");
        printf("  /ai use <name>    - Switch to a provider\n");
        printf("  /ai test          - Test current AI\n");
        printf("  /ai load [path]   - Load local GGUF model\n");
        printf("  /ai unload        - Unload AI\n");
        printf("\nConfiguration: cyxmake.toml\n");
        printf("\n");
    }

    return true;
}

/* /graph command - analyze project dependencies */
bool cmd_graph(ReplSession* session, const char* args) {
    bool colors = session->config.colors_enabled;

    if (colors) {
        printf("\n%s%s=== Project Dependency Graph ===%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    } else {
        printf("\n=== Project Dependency Graph ===\n\n");
    }

    /* Ensure project graph exists */
    if (!session->project_graph) {
        if (session->working_dir) {
            session->project_graph = project_graph_create(session->working_dir);
        }
        if (!session->project_graph) {
            if (colors) {
                printf("%s%s Error: Could not initialize project graph%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Error: Could not initialize project graph\n", SYM_CROSS);
            }
            return true;
        }
    }

    /* Parse subcommands */
    if (args && *args) {
        while (*args == ' ') args++;

        if (strncmp(args, "analyze", 7) == 0 || strncmp(args, "build", 5) == 0) {
            /* Build the graph from source files */
            if (colors) {
                printf("%sAnalyzing project files...%s\n", COLOR_YELLOW, COLOR_RESET);
            } else {
                printf("Analyzing project files...\n");
            }

            /* Use project analyzer to get source files */
            size_t file_count = 0;
            SourceFile** files = scan_source_files(session->working_dir, LANG_UNKNOWN, &file_count);

            if (files && file_count > 0) {
                if (project_graph_build(session->project_graph, files, file_count)) {
                    if (colors) {
                        printf("\n%s%s Graph built successfully!%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
                    } else {
                        printf("\n%s Graph built successfully!\n", SYM_CHECK);
                    }
                } else {
                    if (colors) {
                        printf("%s%s Failed to build graph%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
                    } else {
                        printf("%s Failed to build graph\n", SYM_CROSS);
                    }
                }

                /* Free source files */
                for (size_t i = 0; i < file_count; i++) {
                    if (files[i]) {
                        free(files[i]->path);
                        free(files[i]);
                    }
                }
                free(files);
            } else {
                printf("No source files found to analyze.\n");
            }
        } else if (strncmp(args, "summary", 7) == 0 || strncmp(args, "stats", 5) == 0) {
            /* Show graph summary */
            if (!session->project_graph->is_complete) {
                printf("Graph not built yet. Run '/graph analyze' first.\n");
                return true;
            }

            char* summary = project_graph_summarize(session->project_graph);
            if (summary) {
                printf("%s", summary);
                free(summary);
            }
        } else if (strncmp(args, "deps ", 5) == 0 || strncmp(args, "dependencies ", 13) == 0) {
            /* Show dependencies for a file */
            const char* path = args + (args[0] == 'd' && args[1] == 'e' && args[2] == 'p' ? 5 : 13);
            while (*path == ' ') path++;

            GraphNode* node = project_graph_find(session->project_graph, path);
            if (node) {
                if (colors) {
                    printf("%s%s depends on:%s\n", COLOR_BOLD, node->relative_path, COLOR_RESET);
                } else {
                    printf("%s depends on:\n", node->relative_path);
                }

                if (node->depends_on_count == 0) {
                    printf("  (no dependencies)\n");
                } else {
                    for (int i = 0; i < node->depends_on_count; i++) {
                        printf("  %s %s\n", SYM_BULLET, node->depends_on[i]->relative_path);
                    }
                }

                printf("\n");
                if (colors) {
                    printf("%sDepended on by:%s\n", COLOR_BOLD, COLOR_RESET);
                } else {
                    printf("Depended on by:\n");
                }

                if (node->depended_by_count == 0) {
                    printf("  (nothing depends on this file)\n");
                } else {
                    for (int i = 0; i < node->depended_by_count; i++) {
                        printf("  %s %s\n", SYM_BULLET, node->depended_by[i]->relative_path);
                    }
                }
            } else {
                printf("File not found in graph: %s\n", path);
            }
        } else if (strncmp(args, "impact ", 7) == 0) {
            /* Show impact analysis */
            const char* path = args + 7;
            while (*path == ' ') path++;

            int count = 0;
            GraphNode** affected = project_graph_impact_analysis(session->project_graph, path, &count);

            if (affected && count > 0) {
                if (colors) {
                    printf("%sChanging %s would affect %d files:%s\n", COLOR_YELLOW, path, count, COLOR_RESET);
                } else {
                    printf("Changing %s would affect %d files:\n", path, count);
                }

                for (int i = 0; i < count; i++) {
                    printf("  %s %s\n", SYM_BULLET, affected[i]->relative_path);
                }
                free(affected);
            } else {
                printf("No files would be affected by changes to %s\n", path);
            }
        } else if (strncmp(args, "hotspots", 8) == 0) {
            /* Show most imported files */
            int count = 0;
            GraphNode** hotspots = project_graph_get_hotspots(session->project_graph, 10, &count);

            if (hotspots && count > 0) {
                if (colors) {
                    printf("%s%sMost imported files (hotspots):%s\n", COLOR_BOLD, COLOR_YELLOW, COLOR_RESET);
                } else {
                    printf("Most imported files (hotspots):\n");
                }

                for (int i = 0; i < count && i < 10; i++) {
                    if (hotspots[i]->depended_by_count > 0) {
                        printf("  %2d. %s (%d dependents)\n",
                               i + 1,
                               hotspots[i]->relative_path,
                               hotspots[i]->depended_by_count);
                    }
                }
                free(hotspots);
            } else {
                printf("No hotspots found. Run '/graph analyze' first.\n");
            }
        } else if (strncmp(args, "external", 8) == 0) {
            /* Show external dependencies */
            if (session->project_graph->external_dep_count > 0) {
                if (colors) {
                    printf("%s%sExternal dependencies:%s\n", COLOR_BOLD, COLOR_MAGENTA, COLOR_RESET);
                } else {
                    printf("External dependencies:\n");
                }

                for (int i = 0; i < session->project_graph->external_dep_count; i++) {
                    printf("  %s %s\n", SYM_BULLET, session->project_graph->external_deps[i]);
                }
            } else {
                printf("No external dependencies found. Run '/graph analyze' first.\n");
            }
        } else {
            printf("Unknown subcommand: %s\n", args);
            printf("Use '/graph' for help.\n");
        }
    } else {
        /* Show help */
        if (colors) {
            printf("%sUsage:%s\n", COLOR_BOLD, COLOR_RESET);
            printf("  %s/graph analyze%s     - Build the dependency graph\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/graph summary%s     - Show graph statistics\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/graph deps <file>%s - Show dependencies for a file\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/graph impact <file>%s - Show files affected by changes\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/graph hotspots%s    - Show most imported files\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/graph external%s    - Show external dependencies\n", COLOR_CYAN, COLOR_RESET);
        } else {
            printf("Usage:\n");
            printf("  /graph analyze      - Build the dependency graph\n");
            printf("  /graph summary      - Show graph statistics\n");
            printf("  /graph deps <file>  - Show dependencies for a file\n");
            printf("  /graph impact <file> - Show files affected by changes\n");
            printf("  /graph hotspots     - Show most imported files\n");
            printf("  /graph external     - Show external dependencies\n");
        }

        /* Show current status */
        printf("\n");
        if (session->project_graph->is_complete) {
            if (colors) {
                printf("%sStatus: Graph built (%d files, %d imports)%s\n",
                       COLOR_GREEN,
                       session->project_graph->node_count,
                       session->project_graph->total_imports,
                       COLOR_RESET);
            } else {
                printf("Status: Graph built (%d files, %d imports)\n",
                       session->project_graph->node_count,
                       session->project_graph->total_imports);
            }
        } else {
            printf("Status: Graph not built. Run '/graph analyze' to build.\n");
        }
    }

    printf("\n");
    return true;
}

/* /memory command - show/manage agent memory */
bool cmd_memory(ReplSession* session, const char* args) {
    bool colors = session->config.colors_enabled;

    if (colors) {
        printf("\n%s%s=== Agent Memory ===%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    } else {
        printf("\n=== Agent Memory ===\n\n");
    }

    /* Check if smart agent exists */
    if (!session->smart_agent) {
        printf("Smart Agent not initialized (no AI provider configured).\n\n");
        return true;
    }

    AgentMemory* mem = session->smart_agent->memory;
    if (!mem) {
        printf("No memory available.\n\n");
        return true;
    }

    /* Parse subcommands */
    if (args && *args) {
        while (*args == ' ') args++;

        if (strncmp(args, "save", 4) == 0) {
            /* Force save memory */
            if (session->working_dir) {
                char memory_path[1024];
                snprintf(memory_path, sizeof(memory_path), "%s/.cyxmake/agent_memory.json",
                         session->working_dir);
                if (agent_memory_save(mem, memory_path)) {
                    if (colors) {
                        printf("%s%s Memory saved to: %s%s\n", COLOR_GREEN, SYM_CHECK, memory_path, COLOR_RESET);
                    } else {
                        printf("%s Memory saved to: %s\n", SYM_CHECK, memory_path);
                    }
                } else {
                    if (colors) {
                        printf("%s%s Failed to save memory%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
                    } else {
                        printf("%s Failed to save memory\n", SYM_CROSS);
                    }
                }
            } else {
                printf("No working directory set.\n");
            }
        } else if (strncmp(args, "clear", 5) == 0) {
            /* Clear memory */
            agent_memory_free(mem);
            session->smart_agent->memory = agent_memory_create();
            if (colors) {
                printf("%s%s Memory cleared%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
            } else {
                printf("%s Memory cleared\n", SYM_CHECK);
            }
        } else if (strncmp(args, "test", 4) == 0) {
            /* Test learning by adding sample data */
            smart_agent_learn_success(session->smart_agent, "cmake --build .", "build");
            smart_agent_learn_success(session->smart_agent, "ctest --output-on-failure", "test");
            smart_agent_learn_failure(session->smart_agent, "make install", "permission denied");
            if (colors) {
                printf("%s%s Added test learning data%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
            } else {
                printf("%s Added test learning data\n", SYM_CHECK);
            }
        } else if (strncmp(args, "state", 5) == 0 && (args[5] == ' ' || args[5] == '\0')) {
            /* /memory state - Access shared state for multi-agent system */
            const char* state_args = args[5] == ' ' ? args + 6 : "";
            while (*state_args == ' ') state_args++;

            /* Get shared state from orchestrator */
            SharedState* shared = session->orchestrator ?
                                  cyxmake_get_shared_state(session->orchestrator) : NULL;

            if (!shared) {
                if (colors) {
                    printf("%s%s Shared state not available%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
                    printf("%s(Multi-agent system not initialized)%s\n", COLOR_DIM, COLOR_RESET);
                } else {
                    printf("%s Shared state not available\n", SYM_CROSS);
                    printf("(Multi-agent system not initialized)\n");
                }
                printf("\n");
                return true;
            }

            if (strncmp(state_args, "get ", 4) == 0) {
                /* /memory state get <key> */
                const char* key = state_args + 4;
                while (*key == ' ') key++;

                if (!*key) {
                    printf("Usage: /memory state get <key>\n");
                } else {
                    char* value = shared_state_get(shared, key);
                    if (value) {
                        if (colors) {
                            printf("%s%s%s = %s%s%s\n", COLOR_CYAN, key, COLOR_RESET,
                                   COLOR_GREEN, value, COLOR_RESET);
                        } else {
                            printf("%s = %s\n", key, value);
                        }
                        free(value);
                    } else {
                        if (colors) {
                            printf("%sKey '%s' not found%s\n", COLOR_DIM, key, COLOR_RESET);
                        } else {
                            printf("Key '%s' not found\n", key);
                        }
                    }
                }
            } else if (strncmp(state_args, "set ", 4) == 0) {
                /* /memory state set <key> <value> */
                const char* rest = state_args + 4;
                while (*rest == ' ') rest++;

                /* Parse key and value */
                char key[256] = {0};
                const char* value = NULL;

                const char* space = strchr(rest, ' ');
                if (space) {
                    size_t key_len = space - rest;
                    if (key_len < sizeof(key)) {
                        strncpy(key, rest, key_len);
                        key[key_len] = '\0';
                        value = space + 1;
                        while (*value == ' ') value++;
                    }
                }

                if (!key[0] || !value || !*value) {
                    printf("Usage: /memory state set <key> <value>\n");
                } else {
                    if (shared_state_set(shared, key, value)) {
                        if (colors) {
                            printf("%s%s%s Set '%s%s%s' = '%s%s%s'\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET,
                                   COLOR_CYAN, key, COLOR_RESET, COLOR_GREEN, value, COLOR_RESET);
                        } else {
                            printf("%s Set '%s' = '%s'\n", SYM_CHECK, key, value);
                        }
                    } else {
                        if (colors) {
                            printf("%s%s Failed to set key (may be locked)%s\n",
                                   COLOR_RED, SYM_CROSS, COLOR_RESET);
                        } else {
                            printf("%s Failed to set key (may be locked)\n", SYM_CROSS);
                        }
                    }
                }
            } else if (strncmp(state_args, "delete ", 7) == 0 || strncmp(state_args, "del ", 4) == 0) {
                /* /memory state delete <key> */
                const char* key = state_args + (strncmp(state_args, "del ", 4) == 0 ? 4 : 7);
                while (*key == ' ') key++;

                if (!*key) {
                    printf("Usage: /memory state delete <key>\n");
                } else {
                    if (shared_state_delete(shared, key)) {
                        if (colors) {
                            printf("%s%s%s Deleted '%s%s%s'\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET,
                                   COLOR_CYAN, key, COLOR_RESET);
                        } else {
                            printf("%s Deleted '%s'\n", SYM_CHECK, key);
                        }
                    } else {
                        if (colors) {
                            printf("%s%s Key '%s' not found or locked%s\n",
                                   COLOR_RED, SYM_CROSS, key, COLOR_RESET);
                        } else {
                            printf("%s Key '%s' not found or locked\n", SYM_CROSS, key);
                        }
                    }
                }
            } else if (strcmp(state_args, "save") == 0) {
                /* /memory state save */
                if (shared_state_save(shared)) {
                    if (colors) {
                        printf("%s%s Shared state saved%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
                    } else {
                        printf("%s Shared state saved\n", SYM_CHECK);
                    }
                } else {
                    if (colors) {
                        printf("%s%s Failed to save shared state%s\n",
                               COLOR_RED, SYM_CROSS, COLOR_RESET);
                        printf("%s(No persistence path configured)%s\n", COLOR_DIM, COLOR_RESET);
                    } else {
                        printf("%s Failed to save shared state\n", SYM_CROSS);
                        printf("(No persistence path configured)\n");
                    }
                }
            } else if (strcmp(state_args, "clear") == 0) {
                /* /memory state clear */
                shared_state_clear(shared);
                if (colors) {
                    printf("%s%s Shared state cleared%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
                } else {
                    printf("%s Shared state cleared\n", SYM_CHECK);
                }
            } else {
                /* /memory state - List all entries */
                int count = 0;
                char** keys = shared_state_keys(shared, &count);

                if (colors) {
                    printf("%sShared State:%s", COLOR_BOLD, COLOR_RESET);
                    if (count == 0) {
                        printf(" %s(empty)%s\n", COLOR_DIM, COLOR_RESET);
                    } else {
                        printf(" %s(%d entries)%s\n", COLOR_DIM, count, COLOR_RESET);
                    }
                } else {
                    printf("Shared State:");
                    if (count == 0) {
                        printf(" (empty)\n");
                    } else {
                        printf(" (%d entries)\n", count);
                    }
                }

                if (keys) {
                    for (int i = 0; i < count; i++) {
                        char* value = shared_state_get(shared, keys[i]);
                        const char* locker = shared_state_locked_by(shared, keys[i]);

                        if (colors) {
                            printf("  %s%s%s = %s%s%s",
                                   COLOR_CYAN, keys[i], COLOR_RESET,
                                   COLOR_GREEN, value ? value : "(null)", COLOR_RESET);
                            if (locker) {
                                printf(" %s[locked by %s]%s", COLOR_YELLOW, locker, COLOR_RESET);
                            }
                        } else {
                            printf("  %s = %s", keys[i], value ? value : "(null)");
                            if (locker) {
                                printf(" [locked by %s]", locker);
                            }
                        }
                        printf("\n");

                        free(value);
                        free(keys[i]);
                    }
                    free(keys);
                }

                printf("\n");
                if (colors) {
                    printf("%sCommands:%s\n", COLOR_BOLD, COLOR_RESET);
                    printf("  %s/memory state%s           - List all entries\n", COLOR_CYAN, COLOR_RESET);
                    printf("  %s/memory state get%s <key> - Get value for key\n", COLOR_CYAN, COLOR_RESET);
                    printf("  %s/memory state set%s <key> <value> - Set key/value\n", COLOR_CYAN, COLOR_RESET);
                    printf("  %s/memory state delete%s <key>      - Delete key\n", COLOR_CYAN, COLOR_RESET);
                    printf("  %s/memory state save%s      - Force save to disk\n", COLOR_CYAN, COLOR_RESET);
                    printf("  %s/memory state clear%s     - Clear all entries\n", COLOR_CYAN, COLOR_RESET);
                } else {
                    printf("Commands:\n");
                    printf("  /memory state           - List all entries\n");
                    printf("  /memory state get <key> - Get value for key\n");
                    printf("  /memory state set <key> <value> - Set key/value\n");
                    printf("  /memory state delete <key>      - Delete key\n");
                    printf("  /memory state save      - Force save to disk\n");
                    printf("  /memory state clear     - Clear all entries\n");
                }
            }
        } else {
            printf("Unknown subcommand: %s\n", args);
            printf("Use '/memory' for help.\n");
        }
    } else {
        /* Show memory status and contents */
        if (colors) {
            printf("%sMemory Statistics:%s\n", COLOR_BOLD, COLOR_RESET);
        } else {
            printf("Memory Statistics:\n");
        }
        printf("  Commands recorded: %d\n", mem->command_count);
        printf("  Error fixes learned: %d\n", mem->fix_count);
        printf("  Prefers verbose: %s\n", mem->prefers_verbose ? "yes" : "no");
        printf("  Prefers parallel: %s\n", mem->prefers_parallel ? "yes" : "no");
        if (mem->preferred_config) {
            printf("  Preferred config: %s\n", mem->preferred_config);
        }

        /* Show recent commands */
        if (mem->command_count > 0) {
            printf("\n");
            if (colors) {
                printf("%sRecent Commands:%s\n", COLOR_BOLD, COLOR_RESET);
            } else {
                printf("Recent Commands:\n");
            }
            int start = mem->command_count > 5 ? mem->command_count - 5 : 0;
            for (int i = start; i < mem->command_count; i++) {
                const char* status = mem->command_successes[i] ? SYM_CHECK : SYM_CROSS;
                if (colors) {
                    const char* color = mem->command_successes[i] ? COLOR_GREEN : COLOR_RED;
                    printf("  %s%s%s %s\n", color, status, COLOR_RESET,
                           mem->recent_commands[i] ? mem->recent_commands[i] : "(null)");
                } else {
                    printf("  %s %s\n", status, mem->recent_commands[i] ? mem->recent_commands[i] : "(null)");
                }
            }
        }

        /* Show learned fixes */
        if (mem->fix_count > 0) {
            printf("\n");
            if (colors) {
                printf("%sLearned Error Fixes:%s\n", COLOR_BOLD, COLOR_RESET);
            } else {
                printf("Learned Error Fixes:\n");
            }
            int start = mem->fix_count > 5 ? mem->fix_count - 5 : 0;
            for (int i = start; i < mem->fix_count; i++) {
                if (colors) {
                    printf("  %s%s%s -> %s%s%s\n",
                           COLOR_RED, mem->error_signatures[i] ? mem->error_signatures[i] : "?", COLOR_RESET,
                           COLOR_GREEN, mem->successful_fixes[i] ? mem->successful_fixes[i] : "?", COLOR_RESET);
                } else {
                    printf("  %s -> %s\n",
                           mem->error_signatures[i] ? mem->error_signatures[i] : "?",
                           mem->successful_fixes[i] ? mem->successful_fixes[i] : "?");
                }
            }
        }

        /* Show help */
        printf("\n");
        if (colors) {
            printf("%sCommands:%s\n", COLOR_BOLD, COLOR_RESET);
            printf("  %s/memory save%s   - Save memory to disk\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/memory clear%s  - Clear all memory\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/memory test%s   - Add test data\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/memory state%s  - Multi-agent shared state\n", COLOR_CYAN, COLOR_RESET);
        } else {
            printf("Commands:\n");
            printf("  /memory save   - Save memory to disk\n");
            printf("  /memory clear  - Clear all memory\n");
            printf("  /memory test   - Add test data\n");
            printf("  /memory state  - Multi-agent shared state\n");
        }
    }

    printf("\n");
    return true;
}

/* /recover command - attempt to fix last error */
bool cmd_recover(ReplSession* session, const char* args) {
    (void)args;
    bool colors = session->config.colors_enabled;

    /* Get last error from conversation context or session */
    const char* last_error = NULL;
    if (session->conversation) {
        last_error = conversation_get_last_error(session->conversation);
    }
    if (!last_error && session->last_error) {
        last_error = session->last_error;
    }

    if (!last_error) {
        if (colors) {
            printf("%s%s No error to recover from%s\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
            printf("%sRun /build first or specify an error%s\n", COLOR_DIM, COLOR_RESET);
        } else {
            printf("%s No error to recover from\n", SYM_WARN);
            printf("Run /build first or specify an error\n");
        }
        return true;
    }

    if (colors) {
        printf("\n%s%s=== Error Recovery ===%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    } else {
        printf("\n=== Error Recovery ===\n\n");
    }

    /* Get or create project context */
    ProjectContext* project_ctx = NULL;
    if (session->working_dir) {
        project_ctx = cache_load(session->working_dir);
        if (!project_ctx) {
            project_ctx = project_analyze(session->working_dir, NULL);
        }
    }

    if (!project_ctx) {
        if (colors) {
            printf("%s%s Could not analyze project context%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
        } else {
            printf("%s Could not analyze project context\n", SYM_CROSS);
        }
        return true;
    }

    /* Create mock build result from stored error */
    BuildResult mock_result = {
        .success = false,
        .exit_code = 1,
        .stderr_output = (char*)last_error,
        .stdout_output = NULL
    };

    /* Get LLM context if available */
    LLMContext* llm = session->llm;
    if (!llm && session->orchestrator) {
        llm = cyxmake_get_llm(session->orchestrator);
    }

    /* Diagnose the error */
    ErrorDiagnosis* diagnosis = NULL;
    if (llm && llm_is_ready(llm)) {
        if (colors) {
            printf("%sUsing AI for error analysis...%s\n", COLOR_DIM, COLOR_RESET);
        }
        diagnosis = error_diagnose_with_llm(&mock_result, project_ctx, llm);
    } else {
        diagnosis = error_diagnose(&mock_result, project_ctx);
    }

    if (!diagnosis) {
        if (colors) {
            printf("%s%s Could not diagnose error%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
        } else {
            printf("%s Could not diagnose error\n", SYM_CROSS);
        }
        project_context_free(project_ctx);
        return true;
    }

    /* Display diagnosis */
    if (colors) {
        printf("%sDiagnosis:%s %s\n", COLOR_YELLOW, COLOR_RESET, diagnosis->diagnosis);
        printf("%sConfidence:%s %.0f%%\n", COLOR_YELLOW, COLOR_RESET,
               diagnosis->confidence * 100);
        printf("%sError Type:%s %s\n\n", COLOR_YELLOW, COLOR_RESET,
               error_pattern_type_name(diagnosis->pattern_type));
    } else {
        printf("Diagnosis: %s\n", diagnosis->diagnosis);
        printf("Confidence: %.0f%%\n", diagnosis->confidence * 100);
        printf("Error Type: %s\n\n", error_pattern_type_name(diagnosis->pattern_type));
    }

    if (diagnosis->fix_count == 0) {
        if (colors) {
            printf("%s%s No automated fixes available%s\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
            printf("%sManual intervention may be required%s\n", COLOR_DIM, COLOR_RESET);
        } else {
            printf("%s No automated fixes available\n", SYM_WARN);
            printf("Manual intervention may be required\n");
        }
        error_diagnosis_free(diagnosis);
        project_context_free(project_ctx);
        return true;
    }

    /* Display proposed fixes */
    if (colors) {
        printf("%s%sProposed Fixes:%s\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    } else {
        printf("Proposed Fixes:\n");
    }

    for (size_t i = 0; i < diagnosis->fix_count; i++) {
        FixAction* fix = diagnosis->suggested_fixes[i];
        if (!fix) continue;

        if (colors) {
            printf("  %s%zu.%s %s%s%s\n", COLOR_GREEN, i + 1, COLOR_RESET,
                   COLOR_CYAN, fix->description, COLOR_RESET);
            if (fix->command) {
                printf("      %sCommand: %s%s\n", COLOR_DIM, fix->command, COLOR_RESET);
            }
            if (fix->target) {
                printf("      %sTarget: %s%s\n", COLOR_DIM, fix->target, COLOR_RESET);
            }
        } else {
            printf("  %zu. %s\n", i + 1, fix->description);
            if (fix->command) {
                printf("      Command: %s\n", fix->command);
            }
            if (fix->target) {
                printf("      Target: %s\n", fix->target);
            }
        }
    }
    printf("\n");

    /* Ask for approval */
    if (colors) {
        printf("%sApply these fixes? [Y/n]: %s", COLOR_CYAN, COLOR_RESET);
    } else {
        printf("Apply these fixes? [Y/n]: ");
    }
    fflush(stdout);

    char response[16] = {0};
    if (fgets(response, sizeof(response), stdin) == NULL) {
        response[0] = 'n';
    }

    /* Default to 'yes' if just enter pressed */
    if (response[0] == '\n' || response[0] == '\0' ||
        response[0] == 'y' || response[0] == 'Y') {

        /* Get tool registry */
        ToolRegistry* tools = NULL;
        if (session->orchestrator) {
            tools = cyxmake_get_tools(session->orchestrator);
        }

        /* Execute fixes */
        int successful = 0;
        for (size_t i = 0; i < diagnosis->fix_count; i++) {
            FixAction* fix = diagnosis->suggested_fixes[i];
            if (!fix) continue;

            if (colors) {
                printf("\n%s%s%s Applying fix %zu: %s%s\n",
                       COLOR_BLUE, SYM_ARROW, COLOR_RESET,
                       i + 1, fix->description, COLOR_RESET);
            } else {
                printf("\n%s Applying fix %zu: %s\n", SYM_ARROW, i + 1, fix->description);
            }

            bool result = false;
            if (session->permissions) {
                result = fix_execute_with_permission(fix, project_ctx, tools, session->permissions);
            } else {
                result = fix_execute_with_tools(fix, project_ctx, tools);
            }

            if (result) {
                successful++;
                if (colors) {
                    printf("  %s%s Fix applied successfully%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
                } else {
                    printf("  %s Fix applied successfully\n", SYM_CHECK);
                }
            } else {
                if (colors) {
                    printf("  %s%s Fix failed%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
                } else {
                    printf("  %s Fix failed\n", SYM_CROSS);
                }
            }
        }

        /* Invalidate cache after fixes */
        if (successful > 0 && session->working_dir) {
            cache_invalidate(session->working_dir);
        }

        /* Summary */
        printf("\n");
        if (colors) {
            if (successful > 0) {
                printf("%s%s Applied %d of %zu fixes successfully%s\n",
                       COLOR_GREEN, SYM_CHECK, successful, diagnosis->fix_count, COLOR_RESET);
                printf("%sTip: Run /build to retry the build%s\n", COLOR_DIM, COLOR_RESET);
            } else {
                printf("%s%s No fixes were applied%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
            }
        } else {
            if (successful > 0) {
                printf("%s Applied %d of %zu fixes successfully\n", SYM_CHECK, successful, diagnosis->fix_count);
                printf("Tip: Run /build to retry the build\n");
            } else {
                printf("%s No fixes were applied\n", SYM_CROSS);
            }
        }

        /* Record in conversation */
        if (session->conversation && successful > 0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Applied %d fix(es) for: %s",
                     successful, error_pattern_type_name(diagnosis->pattern_type));
            conversation_add_message(session->conversation, MSG_ROLE_SYSTEM,
                                      msg, CTX_INTENT_BUILD, NULL, true);
        }
    } else {
        if (colors) {
            printf("%sFixes cancelled%s\n", COLOR_DIM, COLOR_RESET);
        } else {
            printf("Fixes cancelled\n");
        }
    }

    error_diagnosis_free(diagnosis);
    project_context_free(project_ctx);
    return true;
}

/* /create - Create a new project from natural language description */
bool cmd_create(ReplSession* session, const char* args) {
    (void)session;  /* May use orchestrator in future */

    if (!args || strlen(args) == 0) {
        printf("Usage: /create <description> [output_path]\n");
        printf("\nExamples:\n");
        printf("  /create C++ game with SDL2\n");
        printf("  /create python web api called myapi\n");
        printf("  /create rust cli tool named mycli\n");
        printf("  /create go rest server\n");
        return true;
    }

    /* Check if last word looks like a path (contains / or \\ or is ".") */
    const char* output_path = ".";
    char* description = strdup(args);
    if (!description) {
        log_error("Memory allocation failed");
        return true;
    }

    /* Trim trailing whitespace */
    size_t len = strlen(description);
    while (len > 0 && (description[len-1] == ' ' || description[len-1] == '\t')) {
        description[--len] = '\0';
    }

    /* Check if last word is a path (contains / or \\) */
    char* last_space = strrchr(description, ' ');
    if (last_space) {
        char* potential_path = last_space + 1;
        /* If it contains path separators, treat as output path */
        if (strchr(potential_path, '/') || strchr(potential_path, '\\')) {
            output_path = potential_path;
            *last_space = '\0';  /* Truncate description */
        }
    }

    bool colors = session->config.colors_enabled;

    if (colors) {
        printf("\n%s%sCreating project from description...%s\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    } else {
        printf("\nCreating project from description...\n");
    }

    /* Parse the description */
    ProjectSpec* spec = project_spec_parse(description);
    if (!spec) {
        log_error("Failed to parse project description");
        free(description);
        return true;
    }

    /* Show what we detected */
    if (colors) {
        printf("%sDetected:%s\n", COLOR_YELLOW, COLOR_RESET);
        printf("  Language:     %s%s%s\n", COLOR_GREEN, language_to_string(spec->language), COLOR_RESET);
        printf("  Project name: %s%s%s\n", COLOR_GREEN, spec->name, COLOR_RESET);
        printf("  Type:         %s%s%s\n", COLOR_GREEN,
               spec->type == PROJECT_GAME ? "Game" :
               spec->type == PROJECT_LIBRARY ? "Library" :
               spec->type == PROJECT_CLI ? "CLI" :
               spec->type == PROJECT_WEB ? "Web" :
               spec->type == PROJECT_GUI ? "GUI" : "Executable", COLOR_RESET);
        if (spec->dependency_count > 0) {
            printf("  Dependencies: ");
            for (int i = 0; i < spec->dependency_count; i++) {
                printf("%s%s%s%s", COLOR_CYAN, spec->dependencies[i], COLOR_RESET,
                       i < spec->dependency_count - 1 ? ", " : "");
            }
            printf("\n");
        }
    } else {
        printf("Detected:\n");
        printf("  Language:     %s\n", language_to_string(spec->language));
        printf("  Project name: %s\n", spec->name);
        printf("  Type:         %s\n",
               spec->type == PROJECT_GAME ? "Game" :
               spec->type == PROJECT_LIBRARY ? "Library" :
               spec->type == PROJECT_CLI ? "CLI" :
               spec->type == PROJECT_WEB ? "Web" :
               spec->type == PROJECT_GUI ? "GUI" : "Executable");
        if (spec->dependency_count > 0) {
            printf("  Dependencies: ");
            for (int i = 0; i < spec->dependency_count; i++) {
                printf("%s%s", spec->dependencies[i],
                       i < spec->dependency_count - 1 ? ", " : "");
            }
            printf("\n");
        }
    }
    printf("\n");

    /* Generate the project */
    GenerationResult* result = project_generate(spec, output_path);
    project_spec_free(spec);
    free(description);

    if (!result) {
        log_error("Failed to generate project");
        return true;
    }

    if (result->success) {
        if (colors) {
            printf("%s%s Project created successfully!%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
            printf("%sCreated %d files in: %s%s\n", COLOR_DIM, result->file_count, result->output_path, COLOR_RESET);
        } else {
            printf("%s Project created successfully!\n", SYM_CHECK);
            printf("Created %d files in: %s\n", result->file_count, result->output_path);
        }
    } else {
        if (colors) {
            printf("%s%s Failed to create project%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
            if (result->error_message) {
                printf("%sError: %s%s\n", COLOR_DIM, result->error_message, COLOR_RESET);
            }
        } else {
            printf("%s Failed to create project\n", SYM_CROSS);
            if (result->error_message) {
                printf("Error: %s\n", result->error_message);
            }
        }
    }

    generation_result_free(result);
    return true;
}

/* Helper to get agent registry from session */
static AgentRegistry* get_agent_registry(ReplSession* session) {
    if (!session || !session->orchestrator) return NULL;
    return cyxmake_get_agent_registry(session->orchestrator);
}

/* Helper to get agent coordinator from session */
static AgentCoordinator* get_coordinator(ReplSession* session) {
    if (!session || !session->orchestrator) return NULL;
    return cyxmake_get_coordinator(session->orchestrator);
}

/* Helper to print agent state with color */
static void print_agent_state(AgentState state, bool colors) {
    const char* state_str = agent_state_to_string(state);
    if (colors) {
        const char* color = COLOR_DIM;
        switch (state) {
            case AGENT_STATE_IDLE: color = COLOR_GREEN; break;
            case AGENT_STATE_RUNNING: color = COLOR_YELLOW; break;
            case AGENT_STATE_COMPLETED: color = COLOR_GREEN; break;
            case AGENT_STATE_ERROR: color = COLOR_RED; break;
            case AGENT_STATE_TERMINATED: color = COLOR_RED; break;
            default: color = COLOR_DIM; break;
        }
        printf("%s%s%s", color, state_str, COLOR_RESET);
    } else {
        printf("%s", state_str);
    }
}

/* /agent command - manage named agents */
bool cmd_agent(ReplSession* session, const char* args) {
    bool colors = session->config.colors_enabled;
    AgentRegistry* registry = get_agent_registry(session);

    if (!args || *args == '\0') {
        /* Show agent help */
        if (colors) {
            printf("\n%s%s=== Agent System ===%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
            printf("%sManage named agents for parallel task execution%s\n\n", COLOR_DIM, COLOR_RESET);

            printf("%sUsage:%s\n", COLOR_BOLD, COLOR_RESET);
            printf("  %s/agent list%s                     - List all agents\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/agent spawn <name> <type>%s      - Create new agent\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/agent spawn <name> <type> --mock%s - Create agent in mock mode\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/agent assign <name> <task>%s     - Assign task to agent\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/agent status <name>%s            - Show agent status\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/agent get <name> [key]%s         - Show agent settings\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/agent set <name> <key> <val>%s   - Configure agent settings\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/agent terminate <name>%s         - Stop an agent\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/agent wait <name>%s              - Wait for agent to complete\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/agent send <from> <to> <msg>%s   - Send message between agents\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/agent inbox <name>%s             - Check agent's messages\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/agent broadcast <from> <msg>%s   - Broadcast to all agents\n", COLOR_CYAN, COLOR_RESET);
            printf("\n%sConflict Resolution:%s\n", COLOR_BOLD, COLOR_RESET);
            printf("  %s/agent conflicts%s                - List pending conflicts\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/agent resolve%s                  - Resolve next conflict\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/agent lock <name> <resource>%s   - Request resource lock\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s/agent unlock <name> <resource>%s - Release resource lock\n", COLOR_CYAN, COLOR_RESET);

            printf("\n%sAgent Types:%s\n", COLOR_BOLD, COLOR_RESET);
            printf("  %ssmart%s  - Intelligent reasoning agent (SmartAgent)\n", COLOR_GREEN, COLOR_RESET);
            printf("  %sbuild%s  - Specialized build agent (AIBuildAgent)\n", COLOR_GREEN, COLOR_RESET);
            printf("  %sauto%s   - Autonomous tool-using agent (AutonomousAgent)\n", COLOR_GREEN, COLOR_RESET);

            printf("\n%sOptions:%s\n", COLOR_BOLD, COLOR_RESET);
            printf("  %s--mock%s - Run in mock mode (no AI backend required, for testing)\n", COLOR_YELLOW, COLOR_RESET);

            printf("\n%sExamples:%s\n", COLOR_BOLD, COLOR_RESET);
            printf("  /agent spawn builder build\n");
            printf("  /agent spawn helper smart --mock\n");
            printf("  /agent assign builder \"Build with Release config\"\n");
            printf("  /agent list\n\n");
        } else {
            printf("\n=== Agent System ===\n\n");
            printf("Manage named agents for parallel task execution\n\n");

            printf("Usage:\n");
            printf("  /agent list                      - List all agents\n");
            printf("  /agent spawn <name> <type>       - Create new agent\n");
            printf("  /agent spawn <name> <type> --mock - Create agent in mock mode\n");
            printf("  /agent assign <name> <task>      - Assign task to agent\n");
            printf("  /agent status <name>             - Show agent status\n");
            printf("  /agent get <name> [key]          - Show agent settings\n");
            printf("  /agent set <name> <key> <val>    - Configure agent settings\n");
            printf("  /agent terminate <name>          - Stop an agent\n");
            printf("  /agent wait <name>               - Wait for agent to complete\n");
            printf("  /agent send <from> <to> <msg>    - Send message between agents\n");
            printf("  /agent inbox <name>              - Check agent's messages\n");
            printf("  /agent broadcast <from> <msg>    - Broadcast to all agents\n");
            printf("\nConflict Resolution:\n");
            printf("  /agent conflicts                 - List pending conflicts\n");
            printf("  /agent resolve                   - Resolve next conflict\n");
            printf("  /agent lock <name> <resource>    - Request resource lock\n");
            printf("  /agent unlock <name> <resource>  - Release resource lock\n");

            printf("\nAgent Types:\n");
            printf("  smart  - Intelligent reasoning agent\n");
            printf("  build  - Specialized build agent\n");
            printf("  auto   - Autonomous tool-using agent\n");

            printf("\nOptions:\n");
            printf("  --mock - Run in mock mode (no AI backend required, for testing)\n");

            printf("\nExamples:\n");
            printf("  /agent spawn builder build\n");
            printf("  /agent spawn helper smart --mock\n");
            printf("  /agent assign builder \"Build with Release config\"\n");
            printf("  /agent list\n\n");
        }
        return true;
    }

    /* Skip leading whitespace */
    while (*args == ' ' || *args == '\t') args++;

    /* Parse subcommand */
    if (strncmp(args, "list", 4) == 0) {
        /* /agent list - List all agents */
        if (colors) {
            printf("\n%s%sActive Agents%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
        } else {
            printf("\nActive Agents\n\n");
        }

        int agent_count = 0;

        /* List agents from registry */
        if (registry) {
            int count = 0;
            AgentInstance** agents = agent_registry_list(registry, &count);

            if (agents && count > 0) {
                /* Print header */
                if (colors) {
                    printf("  %s%-14s %-10s %-12s %s%s\n",
                           COLOR_DIM, "NAME", "TYPE", "STATE", "TASK", COLOR_RESET);
                    printf("  %s─────────────────────────────────────────────%s\n",
                           COLOR_DIM, COLOR_RESET);
                } else {
                    printf("  %-14s %-10s %-12s %s\n", "NAME", "TYPE", "STATE", "TASK");
                    printf("  ---------------------------------------------\n");
                }

                for (int i = 0; i < count; i++) {
                    AgentInstance* agent = agents[i];
                    if (!agent) continue;

                    const char* type_str = agent_type_to_string(agent->type);
                    AgentState state = agent_get_state(agent);
                    const char* task_desc = agent->current_task ?
                        agent->current_task->description : "(none)";

                    if (colors) {
                        printf("  %s*%s %s%-12s%s %s%-8s%s ",
                               COLOR_GREEN, COLOR_RESET,
                               COLOR_CYAN, agent->name, COLOR_RESET,
                               COLOR_YELLOW, type_str, COLOR_RESET);
                        print_agent_state(state, true);
                        printf(" %s%s%s\n", COLOR_DIM, task_desc, COLOR_RESET);
                    } else {
                        printf("  * %-12s %-8s %-10s %s\n",
                               agent->name, type_str,
                               agent_state_to_string(state), task_desc);
                    }
                    agent_count++;
                }
                /* Note: agents array is internal to registry, do not free */
            }
        }

        if (agent_count == 0) {
            if (colors) {
                printf("  %sNo agents running%s\n", COLOR_DIM, COLOR_RESET);
                printf("\n  %sSpawn an agent with: /agent spawn <name> <type>%s\n",
                       COLOR_DIM, COLOR_RESET);
            } else {
                printf("  No agents running\n");
                printf("\n  Spawn an agent with: /agent spawn <name> <type>\n");
            }
        }

        printf("\n");
        return true;
    }
    else if (strncmp(args, "spawn ", 6) == 0) {
        /* /agent spawn <name> <type> [--mock] */
        const char* params = args + 6;
        while (*params == ' ') params++;

        char name[64] = {0};
        char type[32] = {0};
        char extra[32] = {0};
        bool mock_mode = false;

        int parsed = sscanf(params, "%63s %31s %31s", name, type, extra);
        if (parsed < 2) {
            if (colors) {
                printf("%s%s Usage: /agent spawn <name> <type> [--mock]%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
                printf("%sTypes: smart, build, auto%s\n", COLOR_DIM, COLOR_RESET);
                printf("%sOptions: --mock (run without AI backend)%s\n", COLOR_DIM, COLOR_RESET);
            } else {
                printf("%s Usage: /agent spawn <name> <type> [--mock]\n", SYM_CROSS);
                printf("Types: smart, build, auto\n");
                printf("Options: --mock (run without AI backend)\n");
            }
            return true;
        }

        /* Check for --mock flag */
        if (parsed >= 3 && strcmp(extra, "--mock") == 0) {
            mock_mode = true;
        }

        /* Validate and parse type */
        AgentType agent_type;
        if (!agent_type_from_string(type, &agent_type)) {
            if (colors) {
                printf("%s%s Unknown agent type: %s%s\n",
                       COLOR_RED, SYM_CROSS, type, COLOR_RESET);
                printf("%sValid types: smart, build, auto%s\n", COLOR_DIM, COLOR_RESET);
            } else {
                printf("%s Unknown agent type: %s\n", SYM_CROSS, type);
                printf("Valid types: smart, build, auto\n");
            }
            return true;
        }

        /* Check if registry is available */
        if (!registry) {
            if (colors) {
                printf("%s%s Agent system not initialized%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Agent system not initialized\n", SYM_CROSS);
            }
            return true;
        }

        /* Check if agent with this name already exists */
        AgentInstance* existing = agent_registry_get(registry, name);
        if (existing) {
            if (colors) {
                printf("%s%s Agent '%s%s%s' already exists%s\n",
                       COLOR_RED, SYM_CROSS,
                       COLOR_CYAN, name, COLOR_RED, COLOR_RESET);
            } else {
                printf("%s Agent '%s' already exists\n", SYM_CROSS, name);
            }
            return true;
        }

        /* Create agent config */
        AgentInstanceConfig config = agent_config_defaults();
        config.mock_mode = mock_mode;

        /* Create the agent */
        AgentInstance* agent = agent_registry_create_agent(registry, name, agent_type, &config);
        if (!agent) {
            if (colors) {
                printf("%s%s Failed to create agent '%s'%s\n",
                       COLOR_RED, SYM_CROSS, name, COLOR_RESET);
            } else {
                printf("%s Failed to create agent '%s'\n", SYM_CROSS, name);
            }
            return true;
        }

        /* Start the agent */
        agent_start(agent);

        if (colors) {
            printf("%s%s Created agent '%s%s%s' (type: %s%s%s, state: ",
                   COLOR_GREEN, SYM_CHECK,
                   COLOR_CYAN, name, COLOR_RESET,
                   COLOR_YELLOW, type, COLOR_RESET);
            print_agent_state(agent_get_state(agent), true);
            printf(")%s", COLOR_RESET);
            if (mock_mode) {
                printf(" %s[MOCK MODE]%s", COLOR_YELLOW, COLOR_RESET);
            }
            printf("\n");
        } else {
            printf("%s Created agent '%s' (type: %s, state: %s)%s\n",
                   SYM_CHECK, name, type, agent_state_to_string(agent_get_state(agent)),
                   mock_mode ? " [MOCK MODE]" : "");
        }

        return true;
    }
    else if (strncmp(args, "assign ", 7) == 0) {
        /* /agent assign <name> <task> */
        const char* params = args + 7;
        while (*params == ' ') params++;

        char name[64] = {0};
        char task_desc[512] = {0};

        /* Extract name (first word) */
        int i = 0;
        while (*params && *params != ' ' && i < 63) {
            name[i++] = *params++;
        }
        name[i] = '\0';

        /* Skip whitespace to get task */
        while (*params == ' ') params++;

        /* Handle quoted task or unquoted */
        if (*params == '"') {
            params++;  /* Skip opening quote */
            i = 0;
            while (*params && *params != '"' && i < 511) {
                task_desc[i++] = *params++;
            }
            task_desc[i] = '\0';
        } else {
            strncpy(task_desc, params, 511);
            task_desc[511] = '\0';
        }

        if (name[0] == '\0' || task_desc[0] == '\0') {
            if (colors) {
                printf("%s%s Usage: /agent assign <name> <task>%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
                printf("%sExample: /agent assign builder \"Build with debug symbols\"%s\n",
                       COLOR_DIM, COLOR_RESET);
            } else {
                printf("%s Usage: /agent assign <name> <task>\n", SYM_CROSS);
                printf("Example: /agent assign builder \"Build with debug symbols\"\n");
            }
            return true;
        }

        /* Check registry */
        if (!registry) {
            if (colors) {
                printf("%s%s Agent system not initialized%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Agent system not initialized\n", SYM_CROSS);
            }
            return true;
        }

        /* Find the agent */
        AgentInstance* agent = agent_registry_get(registry, name);
        if (!agent) {
            if (colors) {
                printf("%s%s Agent '%s%s%s' not found%s\n",
                       COLOR_RED, SYM_CROSS,
                       COLOR_CYAN, name, COLOR_RED, COLOR_RESET);
                printf("%sUse '/agent list' to see available agents%s\n",
                       COLOR_DIM, COLOR_RESET);
            } else {
                printf("%s Agent '%s' not found\n", SYM_CROSS, name);
                printf("Use '/agent list' to see available agents\n");
            }
            return true;
        }

        /* Run the task asynchronously */
        if (agent_run_async(agent, task_desc)) {
            if (colors) {
                printf("%s%s Task assigned to '%s%s%s': %s%s\n",
                       COLOR_GREEN, SYM_CHECK,
                       COLOR_CYAN, name, COLOR_RESET,
                       task_desc, COLOR_RESET);
                printf("%sAgent executing task asynchronously...%s\n",
                       COLOR_DIM, COLOR_RESET);
            } else {
                printf("%s Task assigned to '%s': %s\n", SYM_CHECK, name, task_desc);
                printf("Agent executing task asynchronously...\n");
            }
        } else {
            if (colors) {
                printf("%s%s Failed to assign task to '%s'%s\n",
                       COLOR_RED, SYM_CROSS, name, COLOR_RESET);
                const char* err = agent_get_error(agent);
                if (err) {
                    printf("%sError: %s%s\n", COLOR_DIM, err, COLOR_RESET);
                }
            } else {
                printf("%s Failed to assign task to '%s'\n", SYM_CROSS, name);
                const char* err = agent_get_error(agent);
                if (err) {
                    printf("Error: %s\n", err);
                }
            }
        }

        return true;
    }
    else if (strncmp(args, "status", 6) == 0) {
        /* /agent status [name] */
        const char* name = args + 6;
        while (*name == ' ') name++;

        if (*name == '\0') {
            /* Show all agent statuses */
            cmd_agent(session, "list");
            return true;
        }

        /* Check registry */
        if (!registry) {
            if (colors) {
                printf("%s%s Agent system not initialized%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Agent system not initialized\n", SYM_CROSS);
            }
            return true;
        }

        /* Find the agent */
        AgentInstance* agent = agent_registry_get(registry, name);
        if (!agent) {
            if (colors) {
                printf("%s%s Agent '%s%s%s' not found%s\n",
                       COLOR_RED, SYM_CROSS,
                       COLOR_CYAN, name, COLOR_RED, COLOR_RESET);
            } else {
                printf("%s Agent '%s' not found\n", SYM_CROSS, name);
            }
            return true;
        }

        /* Display agent status */
        AgentState state = agent_get_state(agent);
        const char* type_str = agent_type_to_string(agent->type);
        const char* task_desc = agent->current_task ?
            agent->current_task->description : "(none)";

        if (colors) {
            printf("\n%s%sAgent Status: %s%s%s\n\n", COLOR_BOLD, COLOR_CYAN,
                   COLOR_GREEN, agent->name, COLOR_RESET);
            printf("  ID:           %s%s%s\n", COLOR_DIM, agent->id, COLOR_RESET);
            printf("  Type:         %s%s%s\n", COLOR_YELLOW, type_str, COLOR_RESET);
            printf("  State:        ");
            print_agent_state(state, true);
            printf("\n");
            printf("  Current Task: %s%s%s\n", COLOR_DIM, task_desc, COLOR_RESET);
            printf("  Tasks Done:   %d\n", agent->tasks_completed);
            printf("  Tasks Failed: %d\n", agent->tasks_failed);
            printf("  Runtime:      %.2f sec\n", agent->total_runtime_sec);

            if (agent->last_error) {
                printf("  Last Error:   %s%s%s\n", COLOR_RED, agent->last_error, COLOR_RESET);
            }

            const char* result = agent_get_result(agent);
            if (result) {
                printf("  Last Result:  %s%.50s%s%s\n", COLOR_DIM, result,
                       strlen(result) > 50 ? "..." : "", COLOR_RESET);
            }
        } else {
            printf("\nAgent Status: %s\n\n", agent->name);
            printf("  ID:           %s\n", agent->id);
            printf("  Type:         %s\n", type_str);
            printf("  State:        %s\n", agent_state_to_string(state));
            printf("  Current Task: %s\n", task_desc);
            printf("  Tasks Done:   %d\n", agent->tasks_completed);
            printf("  Tasks Failed: %d\n", agent->tasks_failed);
            printf("  Runtime:      %.2f sec\n", agent->total_runtime_sec);

            if (agent->last_error) {
                printf("  Last Error:   %s\n", agent->last_error);
            }
        }

        printf("\n");
        return true;
    }
    else if (strncmp(args, "terminate ", 10) == 0) {
        /* /agent terminate <name> */
        const char* name = args + 10;
        while (*name == ' ') name++;

        if (*name == '\0') {
            if (colors) {
                printf("%s%s Usage: /agent terminate <name>%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Usage: /agent terminate <name>\n", SYM_CROSS);
            }
            return true;
        }

        /* Check registry */
        if (!registry) {
            if (colors) {
                printf("%s%s Agent system not initialized%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Agent system not initialized\n", SYM_CROSS);
            }
            return true;
        }

        /* Find the agent */
        AgentInstance* agent = agent_registry_get(registry, name);
        if (!agent) {
            if (colors) {
                printf("%s%s Agent '%s%s%s' not found%s\n",
                       COLOR_RED, SYM_CROSS,
                       COLOR_CYAN, name, COLOR_RED, COLOR_RESET);
            } else {
                printf("%s Agent '%s' not found\n", SYM_CROSS, name);
            }
            return true;
        }

        /* Terminate the agent */
        if (agent_terminate(agent)) {
            if (colors) {
                printf("%s%s Agent '%s%s%s' terminated%s\n",
                       COLOR_GREEN, SYM_CHECK,
                       COLOR_CYAN, name, COLOR_RESET,
                       COLOR_RESET);
            } else {
                printf("%s Agent '%s' terminated\n", SYM_CHECK, name);
            }
        } else {
            if (colors) {
                printf("%s%s Failed to terminate agent '%s'%s\n",
                       COLOR_RED, SYM_CROSS, name, COLOR_RESET);
            } else {
                printf("%s Failed to terminate agent '%s'\n", SYM_CROSS, name);
            }
        }

        return true;
    }
    else if (strncmp(args, "wait ", 5) == 0) {
        /* /agent wait <name> */
        const char* name = args + 5;
        while (*name == ' ') name++;

        if (*name == '\0') {
            if (colors) {
                printf("%s%s Usage: /agent wait <name>%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Usage: /agent wait <name>\n", SYM_CROSS);
            }
            return true;
        }

        /* Check registry */
        if (!registry) {
            if (colors) {
                printf("%s%s Agent system not initialized%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Agent system not initialized\n", SYM_CROSS);
            }
            return true;
        }

        /* Find the agent */
        AgentInstance* agent = agent_registry_get(registry, name);
        if (!agent) {
            if (colors) {
                printf("%s%s Agent '%s%s%s' not found%s\n",
                       COLOR_RED, SYM_CROSS,
                       COLOR_CYAN, name, COLOR_RED, COLOR_RESET);
            } else {
                printf("%s Agent '%s' not found\n", SYM_CROSS, name);
            }
            return true;
        }

        /* Check if already finished */
        if (agent_is_finished(agent)) {
            AgentState state = agent_get_state(agent);
            if (colors) {
                printf("%s%s Agent '%s%s%s' already finished (state: ",
                       COLOR_GREEN, SYM_CHECK,
                       COLOR_CYAN, name, COLOR_RESET);
                print_agent_state(state, true);
                printf(")%s\n", COLOR_RESET);
            } else {
                printf("%s Agent '%s' already finished (state: %s)\n",
                       SYM_CHECK, name, agent_state_to_string(state));
            }
            return true;
        }

        /* Wait for the agent */
        if (colors) {
            printf("%sWaiting for agent '%s%s%s' to complete...%s\n",
                   COLOR_DIM, COLOR_CYAN, name, COLOR_DIM, COLOR_RESET);
        } else {
            printf("Waiting for agent '%s' to complete...\n", name);
        }

        /* Wait with 5 minute timeout */
        bool completed = agent_wait(agent, 300000);
        AgentState final_state = agent_get_state(agent);

        if (completed) {
            if (final_state == AGENT_STATE_COMPLETED) {
                if (colors) {
                    printf("%s%s Agent '%s%s%s' completed successfully%s\n",
                           COLOR_GREEN, SYM_CHECK,
                           COLOR_CYAN, name, COLOR_RESET, COLOR_RESET);
                } else {
                    printf("%s Agent '%s' completed successfully\n", SYM_CHECK, name);
                }

                /* Show result if available */
                const char* result = agent_get_result(agent);
                if (result && strlen(result) > 0) {
                    if (colors) {
                        printf("%sResult: %s%s\n", COLOR_DIM, result, COLOR_RESET);
                    } else {
                        printf("Result: %s\n", result);
                    }
                }
            } else if (final_state == AGENT_STATE_ERROR) {
                if (colors) {
                    printf("%s%s Agent '%s%s%s' finished with error%s\n",
                           COLOR_RED, SYM_CROSS,
                           COLOR_CYAN, name, COLOR_RED, COLOR_RESET);
                } else {
                    printf("%s Agent '%s' finished with error\n", SYM_CROSS, name);
                }

                const char* err = agent_get_error(agent);
                if (err) {
                    if (colors) {
                        printf("%sError: %s%s\n", COLOR_RED, err, COLOR_RESET);
                    } else {
                        printf("Error: %s\n", err);
                    }
                }
            } else {
                if (colors) {
                    printf("%s%s Agent '%s%s%s' finished (state: ",
                           COLOR_GREEN, SYM_CHECK,
                           COLOR_CYAN, name, COLOR_RESET);
                    print_agent_state(final_state, true);
                    printf(")%s\n", COLOR_RESET);
                } else {
                    printf("%s Agent '%s' finished (state: %s)\n",
                           SYM_CHECK, name, agent_state_to_string(final_state));
                }
            }
        } else {
            if (colors) {
                printf("%s%s Timeout waiting for agent '%s'%s\n",
                       COLOR_RED, SYM_CROSS, name, COLOR_RESET);
            } else {
                printf("%s Timeout waiting for agent '%s'\n", SYM_CROSS, name);
            }
        }

        return true;
    }
    else if (strncmp(args, "remove ", 7) == 0 || strncmp(args, "delete ", 7) == 0) {
        /* /agent remove <name> - Remove agent from registry */
        const char* name = args + 7;
        while (*name == ' ') name++;

        if (*name == '\0') {
            if (colors) {
                printf("%s%s Usage: /agent remove <name>%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Usage: /agent remove <name>\n", SYM_CROSS);
            }
            return true;
        }

        if (!registry) {
            if (colors) {
                printf("%s%s Agent system not initialized%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Agent system not initialized\n", SYM_CROSS);
            }
            return true;
        }

        if (agent_registry_remove(registry, name)) {
            if (colors) {
                printf("%s%s Agent '%s%s%s' removed%s\n",
                       COLOR_GREEN, SYM_CHECK,
                       COLOR_CYAN, name, COLOR_RESET, COLOR_RESET);
            } else {
                printf("%s Agent '%s' removed\n", SYM_CHECK, name);
            }
        } else {
            if (colors) {
                printf("%s%s Agent '%s%s%s' not found%s\n",
                       COLOR_RED, SYM_CROSS,
                       COLOR_CYAN, name, COLOR_RED, COLOR_RESET);
            } else {
                printf("%s Agent '%s' not found\n", SYM_CROSS, name);
            }
        }

        return true;
    }
    else if (strncmp(args, "set", 3) == 0 && (args[3] == ' ' || args[3] == '\0')) {
        /* /agent set <name> <key> <value> - Configure agent settings */
        const char* params = args[3] == ' ' ? args + 4 : "";
        while (*params == ' ') params++;

        char name[64] = {0};
        char key[32] = {0};
        char value[128] = {0};

        int parsed = sscanf(params, "%63s %31s %127s", name, key, value);
        if (parsed < 3) {
            if (colors) {
                printf("%s%s Usage: /agent set <name> <key> <value>%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
                printf("\n%sConfigurable settings:%s\n", COLOR_BOLD, COLOR_RESET);
                printf("  %stimeout%s      - Task timeout in seconds (0 = no timeout)\n",
                       COLOR_CYAN, COLOR_RESET);
                printf("  %stemperature%s  - LLM temperature (0.0-1.0)\n",
                       COLOR_CYAN, COLOR_RESET);
                printf("  %smax_tokens%s   - Max tokens per response\n",
                       COLOR_CYAN, COLOR_RESET);
                printf("  %smax_retries%s  - Max retries on failure\n",
                       COLOR_CYAN, COLOR_RESET);
                printf("  %sverbose%s      - Enable verbose output (true/false)\n",
                       COLOR_CYAN, COLOR_RESET);
                printf("  %smock%s         - Mock mode (true/false)\n",
                       COLOR_CYAN, COLOR_RESET);
                printf("  %sread_only%s    - Prevent file modifications (true/false)\n",
                       COLOR_CYAN, COLOR_RESET);
                printf("\n%sExample: /agent set builder temperature 0.7%s\n",
                       COLOR_DIM, COLOR_RESET);
            } else {
                printf("%s Usage: /agent set <name> <key> <value>\n", SYM_CROSS);
                printf("\nConfigurable settings:\n");
                printf("  timeout      - Task timeout in seconds (0 = no timeout)\n");
                printf("  temperature  - LLM temperature (0.0-1.0)\n");
                printf("  max_tokens   - Max tokens per response\n");
                printf("  max_retries  - Max retries on failure\n");
                printf("  verbose      - Enable verbose output (true/false)\n");
                printf("  mock         - Mock mode (true/false)\n");
                printf("  read_only    - Prevent file modifications (true/false)\n");
                printf("\nExample: /agent set builder temperature 0.7\n");
            }
            return true;
        }

        /* Check registry */
        if (!registry) {
            if (colors) {
                printf("%s%s Agent system not initialized%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Agent system not initialized\n", SYM_CROSS);
            }
            return true;
        }

        /* Find the agent */
        AgentInstance* agent = agent_registry_get(registry, name);
        if (!agent) {
            if (colors) {
                printf("%s%s Agent '%s%s%s' not found%s\n",
                       COLOR_RED, SYM_CROSS,
                       COLOR_CYAN, name, COLOR_RED, COLOR_RESET);
            } else {
                printf("%s Agent '%s' not found\n", SYM_CROSS, name);
            }
            return true;
        }

        /* Parse and apply the setting */
        bool success = false;
        char old_value[64] = {0};

        if (strcmp(key, "timeout") == 0) {
            int timeout = atoi(value);
            snprintf(old_value, sizeof(old_value), "%d", agent->config.timeout_sec);
            agent->config.timeout_sec = timeout;
            success = true;
        }
        else if (strcmp(key, "temperature") == 0) {
            float temp = (float)atof(value);
            if (temp >= 0.0f && temp <= 1.0f) {
                snprintf(old_value, sizeof(old_value), "%.2f", agent->config.temperature);
                agent->config.temperature = temp;
                success = true;
            } else {
                if (colors) {
                    printf("%s%s Temperature must be between 0.0 and 1.0%s\n",
                           COLOR_RED, SYM_CROSS, COLOR_RESET);
                } else {
                    printf("%s Temperature must be between 0.0 and 1.0\n", SYM_CROSS);
                }
                return true;
            }
        }
        else if (strcmp(key, "max_tokens") == 0) {
            int tokens = atoi(value);
            snprintf(old_value, sizeof(old_value), "%d", agent->config.max_tokens);
            agent->config.max_tokens = tokens;
            success = true;
        }
        else if (strcmp(key, "max_retries") == 0) {
            int retries = atoi(value);
            snprintf(old_value, sizeof(old_value), "%d", agent->config.max_retries);
            agent->config.max_retries = retries;
            success = true;
        }
        else if (strcmp(key, "verbose") == 0) {
            bool val = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 ||
                       strcmp(value, "yes") == 0 || strcmp(value, "on") == 0);
            snprintf(old_value, sizeof(old_value), "%s", agent->config.verbose ? "true" : "false");
            agent->config.verbose = val;
            success = true;
        }
        else if (strcmp(key, "mock") == 0 || strcmp(key, "mock_mode") == 0) {
            bool val = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 ||
                       strcmp(value, "yes") == 0 || strcmp(value, "on") == 0);
            snprintf(old_value, sizeof(old_value), "%s", agent->config.mock_mode ? "true" : "false");
            agent->config.mock_mode = val;
            success = true;
        }
        else if (strcmp(key, "read_only") == 0) {
            bool val = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 ||
                       strcmp(value, "yes") == 0 || strcmp(value, "on") == 0);
            snprintf(old_value, sizeof(old_value), "%s", agent->config.read_only ? "true" : "false");
            agent->config.read_only = val;
            success = true;
        }
        else {
            if (colors) {
                printf("%s%s Unknown setting: %s%s%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_CYAN, key, COLOR_RESET);
                printf("%sUse '/agent set' to see available settings%s\n",
                       COLOR_DIM, COLOR_RESET);
            } else {
                printf("%s Unknown setting: %s\n", SYM_CROSS, key);
                printf("Use '/agent set' to see available settings\n");
            }
            return true;
        }

        if (success) {
            if (colors) {
                printf("%s%s Agent '%s%s%s' setting '%s%s%s' changed: %s%s%s -> %s%s%s\n",
                       COLOR_GREEN, SYM_CHECK,
                       COLOR_CYAN, name, COLOR_RESET,
                       COLOR_YELLOW, key, COLOR_RESET,
                       COLOR_DIM, old_value, COLOR_RESET,
                       COLOR_GREEN, value, COLOR_RESET);
            } else {
                printf("%s Agent '%s' setting '%s' changed: %s -> %s\n",
                       SYM_CHECK, name, key, old_value, value);
            }
        }

        return true;
    }
    else if (strncmp(args, "get", 3) == 0 && (args[3] == ' ' || args[3] == '\0')) {
        /* /agent get <name> [key] - Show agent settings */
        const char* params = args[3] == ' ' ? args + 4 : "";
        while (*params == ' ') params++;

        char name[64] = {0};
        char key[32] = {0};

        int parsed = sscanf(params, "%63s %31s", name, key);
        if (parsed < 1 || name[0] == '\0') {
            if (colors) {
                printf("%s%s Usage: /agent get <name> [key]%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
                printf("%sOmit [key] to show all settings%s\n", COLOR_DIM, COLOR_RESET);
            } else {
                printf("%s Usage: /agent get <name> [key]\n", SYM_CROSS);
                printf("Omit [key] to show all settings\n");
            }
            return true;
        }

        /* Check registry */
        if (!registry) {
            if (colors) {
                printf("%s%s Agent system not initialized%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Agent system not initialized\n", SYM_CROSS);
            }
            return true;
        }

        /* Find the agent */
        AgentInstance* agent = agent_registry_get(registry, name);
        if (!agent) {
            if (colors) {
                printf("%s%s Agent '%s%s%s' not found%s\n",
                       COLOR_RED, SYM_CROSS,
                       COLOR_CYAN, name, COLOR_RED, COLOR_RESET);
            } else {
                printf("%s Agent '%s' not found\n", SYM_CROSS, name);
            }
            return true;
        }

        /* Show specific key or all settings */
        if (key[0] != '\0') {
            /* Show specific setting */
            char value[128] = {0};
            bool found = true;

            if (strcmp(key, "timeout") == 0) {
                snprintf(value, sizeof(value), "%d", agent->config.timeout_sec);
            } else if (strcmp(key, "temperature") == 0) {
                snprintf(value, sizeof(value), "%.2f", agent->config.temperature);
            } else if (strcmp(key, "max_tokens") == 0) {
                snprintf(value, sizeof(value), "%d", agent->config.max_tokens);
            } else if (strcmp(key, "max_retries") == 0) {
                snprintf(value, sizeof(value), "%d", agent->config.max_retries);
            } else if (strcmp(key, "max_iterations") == 0) {
                snprintf(value, sizeof(value), "%d", agent->config.max_iterations);
            } else if (strcmp(key, "verbose") == 0) {
                snprintf(value, sizeof(value), "%s", agent->config.verbose ? "true" : "false");
            } else if (strcmp(key, "mock") == 0 || strcmp(key, "mock_mode") == 0) {
                snprintf(value, sizeof(value), "%s", agent->config.mock_mode ? "true" : "false");
            } else if (strcmp(key, "read_only") == 0) {
                snprintf(value, sizeof(value), "%s", agent->config.read_only ? "true" : "false");
            } else if (strcmp(key, "auto_start") == 0) {
                snprintf(value, sizeof(value), "%s", agent->config.auto_start ? "true" : "false");
            } else {
                found = false;
            }

            if (found) {
                if (colors) {
                    printf("%s%s%s.%s%s%s = %s%s%s\n",
                           COLOR_CYAN, name, COLOR_RESET,
                           COLOR_YELLOW, key, COLOR_RESET,
                           COLOR_GREEN, value, COLOR_RESET);
                } else {
                    printf("%s.%s = %s\n", name, key, value);
                }
            } else {
                if (colors) {
                    printf("%s%s Unknown setting: %s%s%s\n",
                           COLOR_RED, SYM_CROSS, COLOR_CYAN, key, COLOR_RESET);
                } else {
                    printf("%s Unknown setting: %s\n", SYM_CROSS, key);
                }
            }
        } else {
            /* Show all settings */
            if (colors) {
                printf("\n%s%sAgent Settings: %s%s%s\n\n",
                       COLOR_BOLD, COLOR_CYAN, COLOR_GREEN, name, COLOR_RESET);

                printf("  %s%-14s%s %s%d%s seconds\n",
                       COLOR_YELLOW, "timeout", COLOR_RESET,
                       COLOR_GREEN, agent->config.timeout_sec, COLOR_RESET);
                printf("  %s%-14s%s %s%.2f%s\n",
                       COLOR_YELLOW, "temperature", COLOR_RESET,
                       COLOR_GREEN, agent->config.temperature, COLOR_RESET);
                printf("  %s%-14s%s %s%d%s\n",
                       COLOR_YELLOW, "max_tokens", COLOR_RESET,
                       COLOR_GREEN, agent->config.max_tokens, COLOR_RESET);
                printf("  %s%-14s%s %s%d%s\n",
                       COLOR_YELLOW, "max_retries", COLOR_RESET,
                       COLOR_GREEN, agent->config.max_retries, COLOR_RESET);
                printf("  %s%-14s%s %s%d%s\n",
                       COLOR_YELLOW, "max_iterations", COLOR_RESET,
                       COLOR_GREEN, agent->config.max_iterations, COLOR_RESET);
                printf("  %s%-14s%s %s%s%s\n",
                       COLOR_YELLOW, "verbose", COLOR_RESET,
                       agent->config.verbose ? COLOR_GREEN : COLOR_DIM,
                       agent->config.verbose ? "true" : "false", COLOR_RESET);
                printf("  %s%-14s%s %s%s%s\n",
                       COLOR_YELLOW, "mock", COLOR_RESET,
                       agent->config.mock_mode ? COLOR_GREEN : COLOR_DIM,
                       agent->config.mock_mode ? "true" : "false", COLOR_RESET);
                printf("  %s%-14s%s %s%s%s\n",
                       COLOR_YELLOW, "read_only", COLOR_RESET,
                       agent->config.read_only ? COLOR_GREEN : COLOR_DIM,
                       agent->config.read_only ? "true" : "false", COLOR_RESET);
                printf("\n");
            } else {
                printf("\nAgent Settings: %s\n\n", name);
                printf("  %-14s %d seconds\n", "timeout", agent->config.timeout_sec);
                printf("  %-14s %.2f\n", "temperature", agent->config.temperature);
                printf("  %-14s %d\n", "max_tokens", agent->config.max_tokens);
                printf("  %-14s %d\n", "max_retries", agent->config.max_retries);
                printf("  %-14s %d\n", "max_iterations", agent->config.max_iterations);
                printf("  %-14s %s\n", "verbose", agent->config.verbose ? "true" : "false");
                printf("  %-14s %s\n", "mock", agent->config.mock_mode ? "true" : "false");
                printf("  %-14s %s\n", "read_only", agent->config.read_only ? "true" : "false");
                printf("\n");
            }
        }

        return true;
    }
    else if (strncmp(args, "send ", 5) == 0) {
        /* /agent send <from> <to> <message> - Send message between agents */
        const char* params = args + 5;
        while (*params == ' ') params++;

        char from_name[64] = {0};
        char to_name[64] = {0};
        char message[512] = {0};

        /* Parse: from to "message" or from to message */
        int n = 0;
        if (sscanf(params, "%63s %63s %n", from_name, to_name, &n) >= 2) {
            const char* msg_start = params + n;
            /* Skip quotes if present */
            if (*msg_start == '"') {
                msg_start++;
                const char* end = strchr(msg_start, '"');
                if (end) {
                    size_t len = end - msg_start;
                    if (len >= sizeof(message)) len = sizeof(message) - 1;
                    strncpy(message, msg_start, len);
                } else {
                    strncpy(message, msg_start, sizeof(message) - 1);
                }
            } else {
                strncpy(message, msg_start, sizeof(message) - 1);
            }
        }

        if (!from_name[0] || !to_name[0] || !message[0]) {
            if (colors) {
                printf("%s%s Usage: /agent send <from> <to> <message>%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
                printf("%sExample: /agent send builder tester \"Build complete\"%s\n",
                       COLOR_DIM, COLOR_RESET);
            } else {
                printf("%s Usage: /agent send <from> <to> <message>\n", SYM_CROSS);
            }
            return true;
        }

        if (!registry || !registry->message_bus) {
            if (colors) {
                printf("%s%s Message bus not initialized%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Message bus not initialized\n", SYM_CROSS);
            }
            return true;
        }

        /* Find sender agent */
        AgentInstance* from_agent = agent_registry_get(registry, from_name);
        if (!from_agent) {
            if (colors) {
                printf("%s%s Sender agent '%s%s%s' not found%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_CYAN, from_name, COLOR_RED, COLOR_RESET);
            } else {
                printf("%s Sender agent '%s' not found\n", SYM_CROSS, from_name);
            }
            return true;
        }

        /* Find receiver agent */
        AgentInstance* to_agent = agent_registry_get(registry, to_name);
        if (!to_agent) {
            if (colors) {
                printf("%s%s Receiver agent '%s%s%s' not found%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_CYAN, to_name, COLOR_RED, COLOR_RESET);
            } else {
                printf("%s Receiver agent '%s' not found\n", SYM_CROSS, to_name);
            }
            return true;
        }

        /* Create and send message */
        AgentMessage* msg = message_create(MSG_TYPE_CUSTOM, from_agent->id,
                                           to_agent->id, message);
        if (msg) {
            msg->sender_name = strdup(from_name);
            if (message_bus_send(registry->message_bus, msg)) {
                if (colors) {
                    printf("%s%s Message sent: %s%s%s -> %s%s%s%s\n",
                           COLOR_GREEN, SYM_CHECK,
                           COLOR_CYAN, from_name, COLOR_RESET,
                           COLOR_CYAN, to_name, COLOR_RESET, COLOR_RESET);
                    printf("  %s\"%s\"%s\n", COLOR_DIM, message, COLOR_RESET);
                } else {
                    printf("%s Message sent: %s -> %s\n", SYM_CHECK, from_name, to_name);
                    printf("  \"%s\"\n", message);
                }
            } else {
                if (colors) {
                    printf("%s%s Failed to send message%s\n",
                           COLOR_RED, SYM_CROSS, COLOR_RESET);
                } else {
                    printf("%s Failed to send message\n", SYM_CROSS);
                }
                message_free(msg);
            }
        } else {
            if (colors) {
                printf("%s%s Failed to create message%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Failed to create message\n", SYM_CROSS);
            }
        }

        return true;
    }
    else if (strncmp(args, "inbox ", 6) == 0 || strncmp(args, "messages ", 9) == 0) {
        /* /agent inbox <name> - Check agent's message inbox */
        const char* name = args + (strncmp(args, "inbox ", 6) == 0 ? 6 : 9);
        while (*name == ' ') name++;

        if (!*name) {
            if (colors) {
                printf("%s%s Usage: /agent inbox <name>%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Usage: /agent inbox <name>\n", SYM_CROSS);
            }
            return true;
        }

        if (!registry || !registry->message_bus) {
            if (colors) {
                printf("%s%s Message bus not initialized%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Message bus not initialized\n", SYM_CROSS);
            }
            return true;
        }

        AgentInstance* agent = agent_registry_get(registry, name);
        if (!agent) {
            if (colors) {
                printf("%s%s Agent '%s%s%s' not found%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_CYAN, name, COLOR_RED, COLOR_RESET);
            } else {
                printf("%s Agent '%s' not found\n", SYM_CROSS, name);
            }
            return true;
        }

        if (colors) {
            printf("\n%s%sInbox for '%s':%s\n\n",
                   COLOR_BOLD, COLOR_CYAN, name, COLOR_RESET);
        } else {
            printf("\nInbox for '%s':\n\n", name);
        }

        /* Try to receive messages (non-blocking) */
        int msg_count = 0;
        AgentMessage* msg;
        while ((msg = message_bus_try_receive(registry->message_bus, agent->id)) != NULL) {
            msg_count++;
            if (colors) {
                printf("  %s[%d]%s From: %s%s%s\n",
                       COLOR_YELLOW, msg_count, COLOR_RESET,
                       COLOR_CYAN, msg->sender_name ? msg->sender_name : msg->sender_id, COLOR_RESET);
                printf("      %s\"%s\"%s\n", COLOR_GREEN, msg->payload_json, COLOR_RESET);
            } else {
                printf("  [%d] From: %s\n", msg_count,
                       msg->sender_name ? msg->sender_name : msg->sender_id);
                printf("      \"%s\"\n", msg->payload_json);
            }
            message_free(msg);
        }

        if (msg_count == 0) {
            if (colors) {
                printf("  %s(no messages)%s\n", COLOR_DIM, COLOR_RESET);
            } else {
                printf("  (no messages)\n");
            }
        } else {
            if (colors) {
                printf("\n  %s%d message(s) retrieved%s\n", COLOR_DIM, msg_count, COLOR_RESET);
            } else {
                printf("\n  %d message(s) retrieved\n", msg_count);
            }
        }
        printf("\n");

        return true;
    }
    else if (strncmp(args, "broadcast ", 10) == 0) {
        /* /agent broadcast <from> <message> - Broadcast to all agents */
        const char* params = args + 10;
        while (*params == ' ') params++;

        char from_name[64] = {0};
        char message[512] = {0};

        int n = 0;
        if (sscanf(params, "%63s %n", from_name, &n) >= 1) {
            const char* msg_start = params + n;
            if (*msg_start == '"') {
                msg_start++;
                const char* end = strchr(msg_start, '"');
                if (end) {
                    size_t len = end - msg_start;
                    if (len >= sizeof(message)) len = sizeof(message) - 1;
                    strncpy(message, msg_start, len);
                } else {
                    strncpy(message, msg_start, sizeof(message) - 1);
                }
            } else {
                strncpy(message, msg_start, sizeof(message) - 1);
            }
        }

        if (!from_name[0] || !message[0]) {
            if (colors) {
                printf("%s%s Usage: /agent broadcast <from> <message>%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
                printf("%sExample: /agent broadcast coordinator \"Start phase 2\"%s\n",
                       COLOR_DIM, COLOR_RESET);
            } else {
                printf("%s Usage: /agent broadcast <from> <message>\n", SYM_CROSS);
            }
            return true;
        }

        if (!registry || !registry->message_bus) {
            if (colors) {
                printf("%s%s Message bus not initialized%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Message bus not initialized\n", SYM_CROSS);
            }
            return true;
        }

        AgentInstance* from_agent = agent_registry_get(registry, from_name);
        if (!from_agent) {
            if (colors) {
                printf("%s%s Sender agent '%s%s%s' not found%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_CYAN, from_name, COLOR_RED, COLOR_RESET);
            } else {
                printf("%s Sender agent '%s' not found\n", SYM_CROSS, from_name);
            }
            return true;
        }

        /* Create broadcast message (NULL receiver = broadcast) */
        AgentMessage* msg = message_create(MSG_TYPE_CUSTOM, from_agent->id,
                                           NULL, message);
        if (msg) {
            msg->sender_name = strdup(from_name);
            /* Note: message_bus_broadcast takes ownership and frees the message */
            if (message_bus_broadcast(registry->message_bus, msg)) {
                if (colors) {
                    printf("%s%s Broadcast from %s%s%s to all agents%s\n",
                           COLOR_GREEN, SYM_CHECK,
                           COLOR_CYAN, from_name, COLOR_RESET, COLOR_RESET);
                    printf("  %s\"%s\"%s\n", COLOR_DIM, message, COLOR_RESET);
                } else {
                    printf("%s Broadcast from %s to all agents\n", SYM_CHECK, from_name);
                    printf("  \"%s\"\n", message);
                }
            } else {
                if (colors) {
                    printf("%s%s Failed to broadcast message%s\n",
                           COLOR_RED, SYM_CROSS, COLOR_RESET);
                } else {
                    printf("%s Failed to broadcast message\n", SYM_CROSS);
                }
            }
            /* Do NOT free msg here - message_bus_broadcast takes ownership */
        }

        return true;
    }
    else if (strncmp(args, "conflicts", 9) == 0) {
        /* /agent conflicts - List pending conflicts */
        AgentCoordinator* coord = get_coordinator(session);
        if (!coord) {
            if (colors) {
                printf("%s%s Agent coordinator not initialized%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Agent coordinator not initialized\n", SYM_CROSS);
            }
            return true;
        }

        char* report = coordinator_conflict_report(coord);
        if (report) {
            if (colors) {
                printf("\n%s%s%s%s\n", COLOR_BOLD, COLOR_CYAN, report, COLOR_RESET);
            } else {
                printf("\n%s\n", report);
            }
            free(report);
        }
        return true;
    }
    else if (strncmp(args, "resolve", 7) == 0) {
        /* /agent resolve - Resolve pending conflict */
        AgentCoordinator* coord = get_coordinator(session);
        if (!coord) {
            if (colors) {
                printf("%s%s Agent coordinator not initialized%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Agent coordinator not initialized\n", SYM_CROSS);
            }
            return true;
        }

        AgentConflict* conflict = coordinator_detect_conflict(coord);
        if (!conflict) {
            if (colors) {
                printf("%s%s No pending conflicts%s\n",
                       COLOR_GREEN, SYM_CHECK, COLOR_RESET);
            } else {
                printf("%s No pending conflicts\n", SYM_CHECK);
            }
            return true;
        }

        /* Display conflict info */
        if (colors) {
            printf("\n%s%s=== Conflict Detected ===%s\n\n", COLOR_BOLD, COLOR_YELLOW, COLOR_RESET);
            printf("%sType:%s %s\n", COLOR_BOLD, COLOR_RESET, conflict_type_to_string(conflict->type));
            printf("%sResource:%s %s\n", COLOR_BOLD, COLOR_RESET, conflict->resource_id);
            printf("\n%sAgents:%s\n", COLOR_BOLD, COLOR_RESET);
            printf("  %s[1]%s %s%s%s: %s\n",
                   COLOR_CYAN, COLOR_RESET,
                   COLOR_GREEN, conflict->agent1_name ? conflict->agent1_name : conflict->agent1_id, COLOR_RESET,
                   conflict->agent1_action ? conflict->agent1_action : "(unknown action)");
            printf("  %s[2]%s %s%s%s: %s\n",
                   COLOR_CYAN, COLOR_RESET,
                   COLOR_GREEN, conflict->agent2_name ? conflict->agent2_name : conflict->agent2_id, COLOR_RESET,
                   conflict->agent2_action ? conflict->agent2_action : "(unknown action)");
            printf("\n%sChoose resolution:%s\n", COLOR_BOLD, COLOR_RESET);
            printf("  %s1%s - Let '%s' proceed first\n", COLOR_CYAN, COLOR_RESET,
                   conflict->agent1_name ? conflict->agent1_name : "Agent 1");
            printf("  %s2%s - Let '%s' proceed first\n", COLOR_CYAN, COLOR_RESET,
                   conflict->agent2_name ? conflict->agent2_name : "Agent 2");
            printf("  %s3%s - Both proceed (sequential)\n", COLOR_CYAN, COLOR_RESET);
            printf("  %s4%s - Cancel both\n", COLOR_CYAN, COLOR_RESET);
            printf("\n%sChoice [1-4]: %s", COLOR_BOLD, COLOR_RESET);
        } else {
            printf("\n=== Conflict Detected ===\n\n");
            printf("Type: %s\n", conflict_type_to_string(conflict->type));
            printf("Resource: %s\n", conflict->resource_id);
            printf("\nAgents:\n");
            printf("  [1] %s: %s\n",
                   conflict->agent1_name ? conflict->agent1_name : conflict->agent1_id,
                   conflict->agent1_action ? conflict->agent1_action : "(unknown action)");
            printf("  [2] %s: %s\n",
                   conflict->agent2_name ? conflict->agent2_name : conflict->agent2_id,
                   conflict->agent2_action ? conflict->agent2_action : "(unknown action)");
            printf("\nChoose resolution:\n");
            printf("  1 - Let '%s' proceed first\n",
                   conflict->agent1_name ? conflict->agent1_name : "Agent 1");
            printf("  2 - Let '%s' proceed first\n",
                   conflict->agent2_name ? conflict->agent2_name : "Agent 2");
            printf("  3 - Both proceed (sequential)\n");
            printf("  4 - Cancel both\n");
            printf("\nChoice [1-4]: ");
        }
        fflush(stdout);

        /* Read user choice */
        char choice_str[16];
        if (fgets(choice_str, sizeof(choice_str), stdin)) {
            int choice = atoi(choice_str);
            ResolutionResult result;

            switch (choice) {
                case 1: result = RESOLUTION_RESULT_AGENT1; break;
                case 2: result = RESOLUTION_RESULT_AGENT2; break;
                case 3: result = RESOLUTION_RESULT_BOTH; break;
                case 4: result = RESOLUTION_RESULT_NEITHER; break;
                default:
                    if (colors) {
                        printf("%s%s Invalid choice, defaulting to agent 1%s\n",
                               COLOR_YELLOW, SYM_WARN, COLOR_RESET);
                    } else {
                        printf("%s Invalid choice, defaulting to agent 1\n", SYM_WARN);
                    }
                    result = RESOLUTION_RESULT_AGENT1;
            }

            /* Apply resolution */
            conflict->resolution = result;
            conflict->resolved_at = time(NULL);

            if (colors) {
                printf("%s%s Conflict resolved: %s%s%s\n",
                       COLOR_GREEN, SYM_CHECK,
                       COLOR_CYAN, resolution_result_to_string(result), COLOR_RESET);
            } else {
                printf("%s Conflict resolved: %s\n", SYM_CHECK, resolution_result_to_string(result));
            }
        }

        return true;
    }
    else if (strncmp(args, "lock ", 5) == 0) {
        /* /agent lock <name> <resource> - Request resource lock */
        const char* params = args + 5;
        while (*params == ' ') params++;

        char name[64] = {0};
        char resource[256] = {0};

        int parsed = sscanf(params, "%63s %255s", name, resource);
        if (parsed < 2) {
            if (colors) {
                printf("%s%s Usage: /agent lock <name> <resource>%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
                printf("%sExample: /agent lock builder CMakeLists.txt%s\n",
                       COLOR_DIM, COLOR_RESET);
            } else {
                printf("%s Usage: /agent lock <name> <resource>\n", SYM_CROSS);
            }
            return true;
        }

        AgentCoordinator* coord = get_coordinator(session);
        if (!coord) {
            if (colors) {
                printf("%s%s Agent coordinator not initialized%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Agent coordinator not initialized\n", SYM_CROSS);
            }
            return true;
        }

        AgentInstance* agent = agent_registry_get(registry, name);
        if (!agent) {
            if (colors) {
                printf("%s%s Agent '%s%s%s' not found%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_CYAN, name, COLOR_RED, COLOR_RESET);
            } else {
                printf("%s Agent '%s' not found\n", SYM_CROSS, name);
            }
            return true;
        }

        if (coordinator_request_resource(coord, agent->id, resource, "lock request")) {
            if (colors) {
                printf("%s%s Agent '%s%s%s' locked resource '%s%s%s'%s\n",
                       COLOR_GREEN, SYM_CHECK,
                       COLOR_CYAN, name, COLOR_RESET,
                       COLOR_YELLOW, resource, COLOR_RESET, COLOR_RESET);
            } else {
                printf("%s Agent '%s' locked resource '%s'\n", SYM_CHECK, name, resource);
            }
        } else {
            if (colors) {
                printf("%s%s Resource '%s%s%s' already locked by another agent%s\n",
                       COLOR_YELLOW, SYM_WARN,
                       COLOR_CYAN, resource, COLOR_YELLOW, COLOR_RESET);
                printf("%sUse '/agent conflicts' to see pending conflicts%s\n",
                       COLOR_DIM, COLOR_RESET);
            } else {
                printf("%s Resource '%s' already locked by another agent\n", SYM_WARN, resource);
                printf("Use '/agent conflicts' to see pending conflicts\n");
            }
        }

        return true;
    }
    else if (strncmp(args, "unlock ", 7) == 0) {
        /* /agent unlock <name> <resource> - Release resource lock */
        const char* params = args + 7;
        while (*params == ' ') params++;

        char name[64] = {0};
        char resource[256] = {0};

        int parsed = sscanf(params, "%63s %255s", name, resource);
        if (parsed < 2) {
            if (colors) {
                printf("%s%s Usage: /agent unlock <name> <resource>%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Usage: /agent unlock <name> <resource>\n", SYM_CROSS);
            }
            return true;
        }

        AgentCoordinator* coord = get_coordinator(session);
        if (!coord) {
            if (colors) {
                printf("%s%s Agent coordinator not initialized%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Agent coordinator not initialized\n", SYM_CROSS);
            }
            return true;
        }

        AgentInstance* agent = agent_registry_get(registry, name);
        if (!agent) {
            if (colors) {
                printf("%s%s Agent '%s%s%s' not found%s\n",
                       COLOR_RED, SYM_CROSS, COLOR_CYAN, name, COLOR_RED, COLOR_RESET);
            } else {
                printf("%s Agent '%s' not found\n", SYM_CROSS, name);
            }
            return true;
        }

        coordinator_release_resource(coord, agent->id, resource);
        if (colors) {
            printf("%s%s Agent '%s%s%s' released resource '%s%s%s'%s\n",
                   COLOR_GREEN, SYM_CHECK,
                   COLOR_CYAN, name, COLOR_RESET,
                   COLOR_YELLOW, resource, COLOR_RESET, COLOR_RESET);
        } else {
            printf("%s Agent '%s' released resource '%s'\n", SYM_CHECK, name, resource);
        }

        return true;
    }
    else {
        /* Unknown subcommand */
        if (colors) {
            printf("%s%s Unknown subcommand: %s%s\n",
                   COLOR_RED, SYM_CROSS, args, COLOR_RESET);
            printf("%sUse '/agent' for help%s\n", COLOR_DIM, COLOR_RESET);
        } else {
            printf("%s Unknown subcommand: %s\n", SYM_CROSS, args);
            printf("Use '/agent' for help\n");
        }
    }

    return true;
}

/* ============================================================
 * Distributed Build Commands
 * ============================================================ */

/* Global coordinator instance for the session */
static Coordinator* g_coordinator = NULL;

bool cmd_coordinator(ReplSession* session, const char* args) {
    bool colors = session->config.colors_enabled;

    if (!args || *args == '\0') {
        /* Show help */
        if (colors) {
            printf("\n%s%s=== Coordinator Management ===%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
            printf("%sUsage:%s /coordinator <command> [options]\n\n", COLOR_BOLD, COLOR_RESET);
            printf("%sCommands:%s\n", COLOR_BOLD, COLOR_RESET);
            printf("  %sstart%s [--port PORT]  Start the coordinator server\n", COLOR_GREEN, COLOR_RESET);
            printf("  %sstop%s                 Stop the coordinator\n", COLOR_GREEN, COLOR_RESET);
            printf("  %sstatus%s               Show coordinator status\n", COLOR_GREEN, COLOR_RESET);
            printf("  %stoken%s                Generate a worker auth token\n", COLOR_GREEN, COLOR_RESET);
            printf("\n%sExamples:%s\n", COLOR_BOLD, COLOR_RESET);
            printf("  /coordinator start --port 9876\n");
            printf("  /coord status\n\n");
        } else {
            printf("\n=== Coordinator Management ===\n\n");
            printf("Usage: /coordinator <command> [options]\n\n");
            printf("Commands:\n");
            printf("  start [--port PORT]  Start the coordinator server\n");
            printf("  stop                 Stop the coordinator\n");
            printf("  status               Show coordinator status\n");
            printf("  token                Generate a worker auth token\n\n");
        }
        return true;
    }

    /* Parse subcommand */
    char subcmd[64] = {0};
    const char* subcmd_args = NULL;

    const char* space = strchr(args, ' ');
    if (space) {
        size_t len = (size_t)(space - args);
        if (len >= sizeof(subcmd)) len = sizeof(subcmd) - 1;
        strncpy(subcmd, args, len);
        subcmd_args = space + 1;
        while (*subcmd_args == ' ') subcmd_args++;
    } else {
        strncpy(subcmd, args, sizeof(subcmd) - 1);
    }

    if (strcmp(subcmd, "start") == 0) {
        /* Start coordinator */
        if (g_coordinator && coordinator_is_running(g_coordinator)) {
            if (colors) {
                printf("%s%s Coordinator already running%s\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
            } else {
                printf("%s Coordinator already running\n", SYM_WARN);
            }
            return true;
        }

        /* Parse port option */
        uint16_t port = 9876;
        if (subcmd_args && strstr(subcmd_args, "--port")) {
            const char* port_str = strstr(subcmd_args, "--port");
            port_str += 6;
            while (*port_str == ' ') port_str++;
            port = (uint16_t)atoi(port_str);
            if (port == 0) port = 9876;
        }

        if (colors) {
            printf("%s%s Starting coordinator on port %d...%s\n", COLOR_BLUE, SYM_BULLET, port, COLOR_RESET);
        } else {
            printf("%s Starting coordinator on port %d...\n", SYM_BULLET, port);
        }

        /* Create coordinator config */
        DistributedCoordinatorConfig config = distributed_coordinator_config_default();
        config.port = port;
        config.max_workers = 64;
        config.max_concurrent_builds = 16;
        config.enable_cache = true;

        /* Create and start coordinator */
        if (g_coordinator) {
            distributed_coordinator_free(g_coordinator);
        }
        g_coordinator = distributed_coordinator_create(&config);

        if (!g_coordinator) {
            if (colors) {
                printf("%s%s Failed to create coordinator%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Failed to create coordinator\n", SYM_CROSS);
            }
            return true;
        }

        if (!coordinator_start(g_coordinator)) {
            if (colors) {
                printf("%s%s Failed to start coordinator%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Failed to start coordinator\n", SYM_CROSS);
            }
            distributed_coordinator_free(g_coordinator);
            g_coordinator = NULL;
            return true;
        }

        if (colors) {
            printf("%s%s Coordinator started on port %d%s\n", COLOR_GREEN, SYM_CHECK, port, COLOR_RESET);
            printf("%s  Workers can connect to: ws://localhost:%d%s\n", COLOR_DIM, port, COLOR_RESET);
        } else {
            printf("%s Coordinator started on port %d\n", SYM_CHECK, port);
            printf("  Workers can connect to: ws://localhost:%d\n", port);
        }
    }
    else if (strcmp(subcmd, "stop") == 0) {
        /* Stop coordinator */
        if (!g_coordinator || !coordinator_is_running(g_coordinator)) {
            if (colors) {
                printf("%s%s Coordinator not running%s\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
            } else {
                printf("%s Coordinator not running\n", SYM_WARN);
            }
            return true;
        }

        if (colors) {
            printf("%s%s Stopping coordinator...%s\n", COLOR_BLUE, SYM_BULLET, COLOR_RESET);
        } else {
            printf("%s Stopping coordinator...\n", SYM_BULLET);
        }

        coordinator_stop(g_coordinator);
        distributed_coordinator_free(g_coordinator);
        g_coordinator = NULL;

        if (colors) {
            printf("%s%s Coordinator stopped%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
        } else {
            printf("%s Coordinator stopped\n", SYM_CHECK);
        }
    }
    else if (strcmp(subcmd, "status") == 0) {
        /* Show coordinator status */
        if (!g_coordinator) {
            if (colors) {
                printf("%s%s Coordinator not initialized%s\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
            } else {
                printf("%s Coordinator not initialized\n", SYM_WARN);
            }
            return true;
        }

        CoordinatorStatus status = coordinator_get_status(g_coordinator);

        if (colors) {
            printf("\n%s%s=== Coordinator Status ===%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
            printf("  %sRunning:%s      %s%s%s\n", COLOR_BOLD, COLOR_RESET,
                   status.running ? COLOR_GREEN : COLOR_RED,
                   status.running ? "Yes" : "No", COLOR_RESET);
            printf("  %sWorkers:%s      %d connected, %d online\n", COLOR_BOLD, COLOR_RESET,
                   status.connected_workers, status.online_workers);
            printf("  %sBuilds:%s       %d active\n", COLOR_BOLD, COLOR_RESET, status.active_builds);
            printf("  %sJobs:%s         %d pending, %d running\n", COLOR_BOLD, COLOR_RESET,
                   status.pending_jobs, status.running_jobs);
            printf("  %sCache:%s        %.1f MB (%.1f%% hit rate)\n", COLOR_BOLD, COLOR_RESET,
                   (double)status.cache_size / (1024 * 1024), status.cache_hit_rate * 100);
            printf("  %sUptime:%s       %ld seconds\n\n", COLOR_BOLD, COLOR_RESET, (long)status.uptime_sec);
        } else {
            printf("\n=== Coordinator Status ===\n\n");
            printf("  Running:      %s\n", status.running ? "Yes" : "No");
            printf("  Workers:      %d connected, %d online\n", status.connected_workers, status.online_workers);
            printf("  Builds:       %d active\n", status.active_builds);
            printf("  Jobs:         %d pending, %d running\n", status.pending_jobs, status.running_jobs);
            printf("  Cache:        %.1f MB (%.1f%% hit rate)\n",
                   (double)status.cache_size / (1024 * 1024), status.cache_hit_rate * 100);
            printf("  Uptime:       %ld seconds\n\n", (long)status.uptime_sec);
        }
    }
    else if (strcmp(subcmd, "token") == 0) {
        /* Generate auth token */
        if (!g_coordinator) {
            if (colors) {
                printf("%s%s Coordinator not running. Start it first with '/coordinator start'%s\n",
                       COLOR_YELLOW, SYM_WARN, COLOR_RESET);
            } else {
                printf("%s Coordinator not running. Start it first with '/coordinator start'\n", SYM_WARN);
            }
            return true;
        }

        char* token = coordinator_generate_worker_token(g_coordinator, "cli-worker", 86400);
        if (token) {
            if (colors) {
                printf("\n%s%s Worker Token Generated%s\n\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
                printf("  %sToken:%s %s\n", COLOR_BOLD, COLOR_RESET, token);
                printf("  %sExpires:%s in 24 hours\n\n", COLOR_BOLD, COLOR_RESET);
                printf("%sUse this token when starting a worker:%s\n", COLOR_DIM, COLOR_RESET);
                printf("  cyxmake worker start --token %s\n\n", token);
            } else {
                printf("\n%s Worker Token Generated\n\n", SYM_CHECK);
                printf("  Token: %s\n", token);
                printf("  Expires: in 24 hours\n\n");
                printf("Use this token when starting a worker:\n");
                printf("  cyxmake worker start --token %s\n\n", token);
            }
            free(token);
        } else {
            if (colors) {
                printf("%s%s Failed to generate token%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Failed to generate token\n", SYM_CROSS);
            }
        }
    }
    else {
        if (colors) {
            printf("%s%s Unknown subcommand: %s%s\n", COLOR_RED, SYM_CROSS, subcmd, COLOR_RESET);
            printf("%sUse '/coordinator' for help%s\n", COLOR_DIM, COLOR_RESET);
        } else {
            printf("%s Unknown subcommand: %s\n", SYM_CROSS, subcmd);
            printf("Use '/coordinator' for help\n");
        }
    }

    return true;
}

/* Worker list print callback context */
typedef struct {
    bool colors;
} WorkerPrintContext;

static void print_worker_info(RemoteWorker* w, void* data) {
    WorkerPrintContext* ctx = (WorkerPrintContext*)data;
    const char* state_color = "";
    if (ctx->colors) {
        switch (w->state) {
            case WORKER_STATE_ONLINE: state_color = COLOR_GREEN; break;
            case WORKER_STATE_BUSY: state_color = COLOR_YELLOW; break;
            case WORKER_STATE_OFFLINE: state_color = COLOR_RED; break;
            default: state_color = COLOR_DIM; break;
        }
    }

    char jobs_str[16];
    snprintf(jobs_str, sizeof(jobs_str), "%d/%d", w->active_jobs, w->max_jobs);

    char cpu_str[16];
    snprintf(cpu_str, sizeof(cpu_str), "%.0f%%", w->cpu_usage * 100);

    char health_str[16];
    snprintf(health_str, sizeof(health_str), "%.0f%%", w->health_score * 100);

    if (ctx->colors) {
        printf("  %-20s %s%-12s%s %-8s %-10s %-8s\n",
               w->name ? w->name : w->id,
               state_color, worker_state_name(w->state), COLOR_RESET,
               jobs_str, cpu_str, health_str);
    } else {
        printf("  %-20s %-12s %-8s %-10s %-8s\n",
               w->name ? w->name : w->id,
               worker_state_name(w->state),
               jobs_str, cpu_str, health_str);
    }
}

bool cmd_workers(ReplSession* session, const char* args) {
    bool colors = session->config.colors_enabled;

    if (!args || *args == '\0' || strcmp(args, "list") == 0) {
        /* List all workers */
        if (!g_coordinator) {
            if (colors) {
                printf("%s%s Coordinator not running. Start it with '/coordinator start'%s\n",
                       COLOR_YELLOW, SYM_WARN, COLOR_RESET);
            } else {
                printf("%s Coordinator not running. Start it with '/coordinator start'\n", SYM_WARN);
            }
            return true;
        }

        WorkerRegistry* registry = coordinator_get_registry(g_coordinator);
        if (!registry) {
            if (colors) {
                printf("%s%s Worker registry not available%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("%s Worker registry not available\n", SYM_CROSS);
            }
            return true;
        }

        int total = worker_registry_get_count(registry);
        int online = worker_registry_get_online_count(registry);
        int slots = worker_registry_get_available_slots(registry);

        if (colors) {
            printf("\n%s%s=== Remote Workers ===%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
            printf("  %sTotal:%s %d workers, %d online, %d job slots available\n\n",
                   COLOR_BOLD, COLOR_RESET, total, online, slots);
        } else {
            printf("\n=== Remote Workers ===\n\n");
            printf("  Total: %d workers, %d online, %d job slots available\n\n", total, online, slots);
        }

        if (total == 0) {
            if (colors) {
                printf("  %sNo workers registered%s\n\n", COLOR_DIM, COLOR_RESET);
                printf("  %sTo add a worker, run on the worker machine:%s\n", COLOR_DIM, COLOR_RESET);
                printf("    cyxmake worker start --coordinator <host>:9876\n\n");
            } else {
                printf("  No workers registered\n\n");
                printf("  To add a worker, run on the worker machine:\n");
                printf("    cyxmake worker start --coordinator <host>:9876\n\n");
            }
            return true;
        }

        /* Print header */
        if (colors) {
            printf("  %s%-20s %-12s %-8s %-10s %-8s%s\n",
                   COLOR_BOLD, "NAME", "STATE", "JOBS", "CPU", "HEALTH", COLOR_RESET);
            printf("  %s────────────────────────────────────────────────────────────%s\n", COLOR_DIM, COLOR_RESET);
        } else {
            printf("  %-20s %-12s %-8s %-10s %-8s\n", "NAME", "STATE", "JOBS", "CPU", "HEALTH");
            printf("  ────────────────────────────────────────────────────────────\n");
        }

        /* Iterate and print workers */
        WorkerPrintContext ctx = { colors };
        worker_registry_foreach(registry, print_worker_info, &ctx);

        printf("\n");
        return true;
    }

    /* Parse subcommand */
    char subcmd[64] = {0};
    const char* space = strchr(args, ' ');
    if (space) {
        size_t len = (size_t)(space - args);
        if (len >= sizeof(subcmd)) len = sizeof(subcmd) - 1;
        strncpy(subcmd, args, len);
    } else {
        strncpy(subcmd, args, sizeof(subcmd) - 1);
    }

    if (strcmp(subcmd, "help") == 0) {
        if (colors) {
            printf("\n%s%s=== Worker Management ===%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
            printf("%sUsage:%s /workers <command>\n\n", COLOR_BOLD, COLOR_RESET);
            printf("%sCommands:%s\n", COLOR_BOLD, COLOR_RESET);
            printf("  %slist%s            List all registered workers (default)\n", COLOR_GREEN, COLOR_RESET);
            printf("  %sstats%s           Show detailed worker statistics\n", COLOR_GREEN, COLOR_RESET);
            printf("  %sremove <name>%s   Remove a worker from registry\n", COLOR_GREEN, COLOR_RESET);
            printf("\n");
        } else {
            printf("\n=== Worker Management ===\n\n");
            printf("Usage: /workers <command>\n\n");
            printf("Commands:\n");
            printf("  list            List all registered workers (default)\n");
            printf("  stats           Show detailed worker statistics\n");
            printf("  remove <name>   Remove a worker from registry\n\n");
        }
    }
    else if (strcmp(subcmd, "stats") == 0) {
        if (!g_coordinator) {
            if (colors) {
                printf("%s%s Coordinator not running%s\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
            } else {
                printf("%s Coordinator not running\n", SYM_WARN);
            }
            return true;
        }

        WorkerRegistry* registry = coordinator_get_registry(g_coordinator);
        int slots = worker_registry_get_available_slots(registry);
        int online = worker_registry_get_online_count(registry);

        if (colors) {
            printf("\n%s%s=== Worker Statistics ===%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
            printf("  %sOnline Workers:%s    %d\n", COLOR_BOLD, COLOR_RESET, online);
            printf("  %sAvailable Slots:%s   %d\n", COLOR_BOLD, COLOR_RESET, slots);
            printf("\n");
        } else {
            printf("\n=== Worker Statistics ===\n\n");
            printf("  Online Workers:    %d\n", online);
            printf("  Available Slots:   %d\n\n", slots);
        }
    }
    else {
        if (colors) {
            printf("%s%s Unknown subcommand: %s%s\n", COLOR_RED, SYM_CROSS, subcmd, COLOR_RESET);
            printf("%sUse '/workers help' for usage%s\n", COLOR_DIM, COLOR_RESET);
        } else {
            printf("%s Unknown subcommand: %s\n", SYM_CROSS, subcmd);
            printf("Use '/workers help' for usage\n");
        }
    }

    return true;
}

bool cmd_dbuild(ReplSession* session, const char* args) {
    bool colors = session->config.colors_enabled;

    if (args && strcmp(args, "help") == 0) {
        args = NULL;  /* Show help */
    }

    if (!args || *args == '\0') {
        /* Show help */
        if (colors) {
            printf("\n%s%s=== Distributed Build ===%s\n\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
            printf("%sUsage:%s /dbuild [options]\n\n", COLOR_BOLD, COLOR_RESET);
            printf("%sOptions:%s\n", COLOR_BOLD, COLOR_RESET);
            printf("  %s--strategy <name>%s   Distribution strategy:\n", COLOR_GREEN, COLOR_RESET);
            printf("                       compile-units  Distribute source files\n");
            printf("                       targets        Distribute build targets\n");
            printf("                       whole-project  Build on single worker\n");
            printf("                       hybrid         Auto-select (default)\n");
            printf("  %s--jobs <N>%s          Maximum parallel jobs\n", COLOR_GREEN, COLOR_RESET);
            printf("  %s--verbose%s           Show detailed progress\n", COLOR_GREEN, COLOR_RESET);
            printf("\n%sExamples:%s\n", COLOR_BOLD, COLOR_RESET);
            printf("  /dbuild --strategy compile-units\n");
            printf("  /db --jobs 16 --verbose\n\n");
        } else {
            printf("\n=== Distributed Build ===\n\n");
            printf("Usage: /dbuild [options]\n\n");
            printf("Options:\n");
            printf("  --strategy <name>   Distribution strategy\n");
            printf("  --jobs <N>          Maximum parallel jobs\n");
            printf("  --verbose           Show detailed progress\n\n");
        }
        return true;
    }

    /* Check coordinator is running */
    if (!g_coordinator || !coordinator_is_running(g_coordinator)) {
        if (colors) {
            printf("%s%s Coordinator not running%s\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
            printf("%sStart the coordinator first with '/coordinator start'%s\n", COLOR_DIM, COLOR_RESET);
        } else {
            printf("%s Coordinator not running\n", SYM_WARN);
            printf("Start the coordinator first with '/coordinator start'\n");
        }
        return true;
    }

    /* Check for workers */
    WorkerRegistry* registry = coordinator_get_registry(g_coordinator);
    int online = worker_registry_get_online_count(registry);
    if (online == 0) {
        if (colors) {
            printf("%s%s No workers online%s\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
            printf("%sRegister workers first. See '/workers help'%s\n", COLOR_DIM, COLOR_RESET);
        } else {
            printf("%s No workers online\n", SYM_WARN);
            printf("Register workers first. See '/workers help'\n");
        }
        return true;
    }

    /* Parse options */
    DistributionStrategy strategy = DIST_STRATEGY_HYBRID;
    int max_jobs = 0;
    bool verbose = false;

    if (strstr(args, "--strategy")) {
        if (strstr(args, "compile-units")) {
            strategy = DIST_STRATEGY_COMPILE_UNITS;
        } else if (strstr(args, "targets")) {
            strategy = DIST_STRATEGY_TARGETS;
        } else if (strstr(args, "whole-project")) {
            strategy = DIST_STRATEGY_WHOLE_PROJECT;
        }
    }

    const char* jobs_str = strstr(args, "--jobs");
    if (jobs_str) {
        jobs_str += 6;
        while (*jobs_str == ' ') jobs_str++;
        max_jobs = atoi(jobs_str);
    }

    if (strstr(args, "--verbose") || strstr(args, "-v")) {
        verbose = true;
    }

    /* Create build options */
    DistributedBuildOptions opts = distributed_build_options_default();
    opts.strategy = strategy;
    opts.max_parallel_jobs = max_jobs > 0 ? max_jobs : worker_registry_get_available_slots(registry);
    opts.verbose = verbose;

    const char* strategy_name = "hybrid";
    switch (strategy) {
        case DIST_STRATEGY_COMPILE_UNITS: strategy_name = "compile-units"; break;
        case DIST_STRATEGY_TARGETS: strategy_name = "targets"; break;
        case DIST_STRATEGY_WHOLE_PROJECT: strategy_name = "whole-project"; break;
        default: break;
    }

    if (colors) {
        printf("\n%s%s Starting distributed build%s\n", COLOR_BLUE, SYM_BULLET, COLOR_RESET);
        printf("  %sStrategy:%s    %s\n", COLOR_BOLD, COLOR_RESET, strategy_name);
        printf("  %sMax Jobs:%s    %d\n", COLOR_BOLD, COLOR_RESET, opts.max_parallel_jobs);
        printf("  %sWorkers:%s     %d online\n\n", COLOR_BOLD, COLOR_RESET, online);
    } else {
        printf("\n%s Starting distributed build\n", SYM_BULLET);
        printf("  Strategy:    %s\n", strategy_name);
        printf("  Max Jobs:    %d\n", opts.max_parallel_jobs);
        printf("  Workers:     %d online\n\n", online);
    }

    /* Submit build */
    BuildSession* build_session = coordinator_submit_build(g_coordinator, ".", &opts);
    if (!build_session) {
        if (colors) {
            printf("%s%s Failed to submit distributed build%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
        } else {
            printf("%s Failed to submit distributed build\n", SYM_CROSS);
        }
        return true;
    }

    if (colors) {
        printf("%s%s Build submitted: %s%s%s\n", COLOR_GREEN, SYM_CHECK, COLOR_CYAN, build_session->build_id, COLOR_RESET);
    } else {
        printf("%s Build submitted: %s\n", SYM_CHECK, build_session->build_id);
    }

    /* Wait for completion with progress */
    if (colors) {
        printf("%s%s Waiting for build to complete...%s\n", COLOR_BLUE, SYM_BULLET, COLOR_RESET);
    } else {
        printf("%s Waiting for build to complete...\n", SYM_BULLET);
    }

    bool success = coordinator_wait_build(g_coordinator, build_session->build_id, 3600);
    DistributedBuildResult* result = coordinator_get_build_result(g_coordinator, build_session->build_id);

    if (result) {
        if (result->success) {
            if (colors) {
                printf("\n%s%s Build successful!%s\n\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
                printf("  %sDuration:%s     %.2f seconds\n", COLOR_BOLD, COLOR_RESET, result->duration_sec);
                printf("  %sJobs:%s         %d completed\n", COLOR_BOLD, COLOR_RESET, result->jobs_completed);
                if (result->cache_hits > 0) {
                    printf("  %sCache Hits:%s   %d\n", COLOR_BOLD, COLOR_RESET, result->cache_hits);
                }
                printf("\n");
            } else {
                printf("\n%s Build successful!\n\n", SYM_CHECK);
                printf("  Duration:     %.2f seconds\n", result->duration_sec);
                printf("  Jobs:         %d completed\n", result->jobs_completed);
                printf("\n");
            }
        } else {
            if (colors) {
                printf("\n%s%s Build failed%s\n\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
                if (result->error_message) {
                    printf("  %sError:%s %s\n\n", COLOR_BOLD, COLOR_RESET, result->error_message);
                }
            } else {
                printf("\n%s Build failed\n\n", SYM_CROSS);
                if (result->error_message) {
                    printf("  Error: %s\n\n", result->error_message);
                }
            }

            /* Store error for recovery */
            if (session->last_error) {
                free(session->last_error);
                session->last_error = NULL;
            }
            if (result->error_message) {
                session->last_error = strdup(result->error_message);
            }
        }
        distributed_build_result_free(result);
    } else if (!success) {
        if (colors) {
            printf("\n%s%s Build timed out or failed%s\n\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
        } else {
            printf("\n%s Build timed out or failed\n\n", SYM_CROSS);
        }
    }

    return true;
}
