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
    {"ai",      NULL,   "AI status and commands",       cmd_ai},
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
