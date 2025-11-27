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

    if (file_exists("CMakeLists.txt")) {
        /* CMake project */
        if (!file_exists("build")) {
            if (session->config.colors_enabled) {
                printf("  %s%s%s Configuring CMake...\n", COLOR_BLUE, SYM_BULLET, COLOR_RESET);
            }
#ifdef _WIN32
            result = system("cmake -B build -S .");
#else
            result = system("cmake -B build -S . 2>&1");
#endif
        }

        if (result == 0 || file_exists("build")) {
            if (session->config.colors_enabled) {
                printf("  %s%s%s Compiling...\n", COLOR_BLUE, SYM_BULLET, COLOR_RESET);
            }
#ifdef _WIN32
            result = system("cmake --build build");
#else
            result = system("cmake --build build 2>&1");
#endif
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
        session->last_error = strdup("Build failed");
    }

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
