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
        } else {
            printf("Commands:\n");
            printf("  /memory save   - Save memory to disk\n");
            printf("  /memory clear  - Clear all memory\n");
            printf("  /memory test   - Add test data\n");
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
