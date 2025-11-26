/**
 * @file repl.c
 * @brief Interactive REPL implementation for CyxMake
 */

#include "cyxmake/repl.h"
#include "cyxmake/slash_commands.h"
#include "cyxmake/cyxmake.h"
#include "cyxmake/logger.h"
#include "cyxmake/llm_interface.h"
#include "cyxmake/prompt_templates.h"
#include "cyxmake/file_ops.h"
#include "cyxmake/tool_executor.h"
#include "cyxmake/permission.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <direct.h>
#define CLEAR_SCREEN "cls"
#else
#include <unistd.h>
#include <termios.h>
#define CLEAR_SCREEN "clear"
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

/* Box drawing characters (ASCII for compatibility) */
#define BOX_TL "+"
#define BOX_TR "+"
#define BOX_BL "+"
#define BOX_BR "+"
#define BOX_H  "-"
#define BOX_V  "|"

/* Status symbols (ASCII for compatibility) */
#define SYM_CHECK   "[OK]"
#define SYM_CROSS   "[X]"
#define SYM_BULLET  "*"
#define SYM_WARN    "[!]"

/* Maximum input line length */
#define MAX_INPUT_LENGTH 4096

/* Default configuration */
ReplConfig repl_config_default(void) {
    return (ReplConfig){
        .prompt = "cyxmake> ",
        .colors_enabled = true,
        .show_welcome = true,
        .history_size = 100,
        .verbose = false
    };
}

/* Create REPL session */
ReplSession* repl_session_create(const ReplConfig* config, Orchestrator* orch) {
    ReplSession* session = calloc(1, sizeof(ReplSession));
    if (!session) return NULL;

    /* Apply configuration */
    if (config) {
        session->config = *config;
    } else {
        session->config = repl_config_default();
    }

    /* Use provided orchestrator or create new one */
    if (orch) {
        session->orchestrator = orch;
    }
    /* Note: orchestrator can be NULL, will be created lazily if needed */

    /* Initialize history */
    session->history_capacity = session->config.history_size > 0
                                ? session->config.history_size : 100;
    session->history = calloc(session->history_capacity, sizeof(char*));
    session->history_count = 0;

    /* Get working directory */
    char cwd[1024];
#ifdef _WIN32
    if (_getcwd(cwd, sizeof(cwd))) {
#else
    if (getcwd(cwd, sizeof(cwd))) {
#endif
        session->working_dir = strdup(cwd);
    }

    /* Initialize permission system */
    session->permissions = permission_context_create();
    if (session->permissions) {
        session->permissions->colors_enabled = session->config.colors_enabled;
    }

    session->running = true;
    session->command_count = 0;

    return session;
}

/* Free REPL session */
void repl_session_free(ReplSession* session) {
    if (!session) return;

    /* Free history */
    if (session->history) {
        for (int i = 0; i < session->history_count; i++) {
            free(session->history[i]);
        }
        free(session->history);
    }

    free(session->working_dir);
    free(session->last_error);
    free(session->current_file);

    /* Free permission context */
    permission_context_free(session->permissions);

    /* Note: Don't free orchestrator - it may be shared */

    free(session);
}

/* Add to history */
void repl_history_add(ReplSession* session, const char* input) {
    if (!session || !input || strlen(input) == 0) return;

    /* Don't add duplicates of last entry */
    if (session->history_count > 0 &&
        strcmp(session->history[session->history_count - 1], input) == 0) {
        return;
    }

    /* If at capacity, remove oldest */
    if (session->history_count >= session->history_capacity) {
        free(session->history[0]);
        memmove(session->history, session->history + 1,
                (session->history_capacity - 1) * sizeof(char*));
        session->history_count--;
    }

    session->history[session->history_count++] = strdup(input);
}

/* Print welcome banner */
void repl_print_welcome(ReplSession* session) {
    if (!session->config.show_welcome) return;

    const char* version = cyxmake_version();

    if (session->config.colors_enabled) {
        printf("\n");
        printf("%s%s", COLOR_CYAN, BOX_TL);
        for (int i = 0; i < 62; i++) printf("%s", BOX_H);
        printf("%s%s\n", BOX_TR, COLOR_RESET);

        printf("%s%s%s", COLOR_CYAN, BOX_V, COLOR_RESET);
        printf("  %s%sCyxMake v%s%s - AI Build Assistant",
               COLOR_BOLD, COLOR_GREEN, version, COLOR_RESET);
        for (int i = 0; i < (int)(62 - 28 - strlen(version)); i++) printf(" ");
        printf("%s%s%s\n", COLOR_CYAN, BOX_V, COLOR_RESET);

        printf("%s%s%s", COLOR_CYAN, BOX_V, COLOR_RESET);
        printf("  %sType naturally or %s/help%s for commands%s",
               COLOR_DIM, COLOR_YELLOW, COLOR_DIM, COLOR_RESET);
        for (int i = 0; i < 22; i++) printf(" ");
        printf("%s%s%s\n", COLOR_CYAN, BOX_V, COLOR_RESET);

        printf("%s%s", COLOR_CYAN, BOX_BL);
        for (int i = 0; i < 62; i++) printf("%s", BOX_H);
        printf("%s%s\n", BOX_BR, COLOR_RESET);
        printf("\n");
    } else {
        printf("\n");
        printf("+----------------------------------------------------------------+\n");
        printf("|  CyxMake v%s - AI Build Assistant                            |\n", version);
        printf("|  Type naturally or /help for commands                          |\n");
        printf("+----------------------------------------------------------------+\n");
        printf("\n");
    }
}

/* Print prompt */
void repl_print_prompt(ReplSession* session) {
    if (session->config.colors_enabled) {
        printf("%s%s%s", COLOR_BOLD, COLOR_GREEN, session->config.prompt);
        printf("%s", COLOR_RESET);
    } else {
        printf("%s", session->config.prompt);
    }
    fflush(stdout);
}

/* Read a line of input */
static char* read_input_line(void) {
    static char buffer[MAX_INPUT_LENGTH];

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return NULL;  /* EOF or error */
    }

    /* Remove trailing newline */
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
        len--;
    }
    if (len > 0 && buffer[len - 1] == '\r') {
        buffer[len - 1] = '\0';
    }

    return buffer;
}

/* Execute natural language command */
static bool execute_natural_language(ReplSession* session, const char* input) {
    /* Parse the command locally first */
    ParsedCommand* cmd = parse_command_local(input);

    if (!cmd) {
        if (session->config.colors_enabled) {
            printf("%s%s Failed to understand command%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
        } else {
            printf("Error: Failed to understand command\n");
        }
        return true;
    }

    /* Show what we detected */
    if (session->config.colors_enabled) {
        printf("%s%s%s Detected: %s%s%s (%.0f%% confidence)\n", SYM_BULLET,
               COLOR_BLUE, COLOR_RESET,
               COLOR_CYAN,
               cmd->intent == INTENT_BUILD ? "BUILD" :
               cmd->intent == INTENT_INIT ? "INIT" :
               cmd->intent == INTENT_CLEAN ? "CLEAN" :
               cmd->intent == INTENT_TEST ? "TEST" :
               cmd->intent == INTENT_CREATE_FILE ? "CREATE FILE" :
               cmd->intent == INTENT_READ_FILE ? "READ FILE" :
               cmd->intent == INTENT_EXPLAIN ? "EXPLAIN" :
               cmd->intent == INTENT_FIX ? "FIX" :
               cmd->intent == INTENT_INSTALL ? "INSTALL" :
               cmd->intent == INTENT_STATUS ? "STATUS" :
               cmd->intent == INTENT_HELP ? "HELP" : "UNKNOWN",
               COLOR_RESET,
               cmd->confidence * 100);
    } else {
        printf("Detected: %s (%.0f%% confidence)\n",
               cmd->intent == INTENT_BUILD ? "BUILD" :
               cmd->intent == INTENT_INIT ? "INIT" :
               cmd->intent == INTENT_CLEAN ? "CLEAN" :
               cmd->intent == INTENT_TEST ? "TEST" :
               cmd->intent == INTENT_CREATE_FILE ? "CREATE FILE" :
               cmd->intent == INTENT_READ_FILE ? "READ FILE" :
               cmd->intent == INTENT_EXPLAIN ? "EXPLAIN" :
               cmd->intent == INTENT_FIX ? "FIX" :
               cmd->intent == INTENT_INSTALL ? "INSTALL" :
               cmd->intent == INTENT_STATUS ? "STATUS" :
               cmd->intent == INTENT_HELP ? "HELP" : "UNKNOWN",
               cmd->confidence * 100);
    }

    if (cmd->target) {
        printf("  Target: %s\n", cmd->target);
    }

    /* Execute based on intent */
    switch (cmd->intent) {
        case INTENT_BUILD:
            cmd_build(session, NULL);
            break;

        case INTENT_INIT:
            cmd_init(session, NULL);
            break;

        case INTENT_CLEAN:
            cmd_clean(session, NULL);
            break;

        case INTENT_STATUS:
            cmd_status(session, NULL);
            break;

        case INTENT_HELP:
            cmd_help(session, NULL);
            break;

        case INTENT_READ_FILE:
            if (cmd->target) {
                if (file_exists(cmd->target)) {
                    printf("\n");
                    file_read_display(cmd->target, 50);
                    /* Remember current file */
                    free(session->current_file);
                    session->current_file = strdup(cmd->target);
                } else {
                    if (session->config.colors_enabled) {
                        printf("%s%s File not found: %s%s\n", COLOR_RED, SYM_CROSS, cmd->target, COLOR_RESET);
                    } else {
                        printf("Error: File not found: %s\n", cmd->target);
                    }
                }
            } else {
                printf("Please specify a file to read\n");
            }
            break;

        case INTENT_CREATE_FILE:
            if (cmd->target) {
                if (file_exists(cmd->target)) {
                    if (session->config.colors_enabled) {
                        printf("%s%s File already exists: %s%s\n", COLOR_YELLOW, SYM_WARN, cmd->target, COLOR_RESET);
                    } else {
                        printf("Warning: File already exists: %s\n", cmd->target);
                    }
                } else {
                    /* Ask permission to create file */
                    if (!permission_check(session->permissions, ACTION_CREATE_FILE,
                                          cmd->target, "User requested file creation")) {
                        if (session->config.colors_enabled) {
                            printf("%s%s File creation denied%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
                        } else {
                            printf("File creation denied\n");
                        }
                        break;
                    }

                    /* Determine template based on extension */
                    const char* content = "";
                    if (strstr(cmd->target, ".c")) {
                        content = "/**\n * @file \n * @brief \n */\n\n#include <stdio.h>\n\nint main(void) {\n    return 0;\n}\n";
                    } else if (strstr(cmd->target, ".h")) {
                        content = "/**\n * @file \n * @brief \n */\n\n#ifndef _H\n#define _H\n\n#endif\n";
                    } else if (strstr(cmd->target, ".md")) {
                        content = "# Title\n\n## Description\n\n";
                    } else if (strstr(cmd->target, ".py")) {
                        content = "#!/usr/bin/env python3\n\ndef main():\n    pass\n\nif __name__ == '__main__':\n    main()\n";
                    }

                    if (file_write(cmd->target, content)) {
                        if (session->config.colors_enabled) {
                            printf("%s%s Created: %s%s\n", COLOR_GREEN, SYM_CHECK, cmd->target, COLOR_RESET);
                        } else {
                            printf("Created: %s\n", cmd->target);
                        }
                    } else {
                        if (session->config.colors_enabled) {
                            printf("%s%s Failed to create file%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
                        } else {
                            printf("Error: Failed to create file\n");
                        }
                    }
                }
            } else {
                printf("Please specify a file to create\n");
            }
            break;

        case INTENT_INSTALL:
            if (cmd->target) {
                /* Ask permission to install package */
                if (!permission_check(session->permissions, ACTION_INSTALL_PKG,
                                      cmd->target, "User requested package installation")) {
                    if (session->config.colors_enabled) {
                        printf("%s%s Package installation denied%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
                    } else {
                        printf("Package installation denied\n");
                    }
                    break;
                }

                printf("Installing package: %s\n", cmd->target);

                ToolRegistry* registry = tool_registry_create();
                if (registry) {
                    tool_discover_all(registry);
                    const ToolInfo* pkg_mgr = package_get_default_manager(registry);

                    if (pkg_mgr) {
                        printf("Using: %s\n", pkg_mgr->display_name);
                        ToolExecResult* result = package_install(registry, cmd->target, NULL);
                        if (result && result->success) {
                            if (session->config.colors_enabled) {
                                printf("%s%s Installed: %s%s\n", COLOR_GREEN, SYM_CHECK, cmd->target, COLOR_RESET);
                            } else {
                                printf("Installed: %s\n", cmd->target);
                            }
                        } else {
                            if (session->config.colors_enabled) {
                                printf("%s%s Failed to install%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
                            } else {
                                printf("Error: Failed to install\n");
                            }
                        }
                        tool_exec_result_free(result);
                    } else {
                        printf("No package manager found\n");
                    }
                    tool_registry_free(registry);
                }
            } else {
                printf("Please specify a package to install\n");
            }
            break;

        case INTENT_TEST:
            printf("Running tests...\n");
            printf("(Test execution not yet implemented)\n");
            break;

        case INTENT_EXPLAIN:
        case INTENT_FIX:
            printf("This requires AI analysis...\n");
            printf("(AI-powered %s not yet implemented)\n",
                   cmd->intent == INTENT_EXPLAIN ? "explain" : "fix");
            break;

        case INTENT_UNKNOWN:
        default:
            if (session->config.colors_enabled) {
                printf("%s%s I didn't understand that. Try /help for commands.%s\n",
                       COLOR_YELLOW, SYM_WARN, COLOR_RESET);
            } else {
                printf("I didn't understand that. Try /help for commands.\n");
            }
            break;
    }

    parsed_command_free(cmd);
    return true;
}

/* Process a single input line */
bool repl_process_input(ReplSession* session, const char* input) {
    if (!session || !input) return true;

    /* Skip empty input */
    const char* p = input;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0') return true;

    /* Add to history */
    repl_history_add(session, input);
    session->command_count++;

    /* Check if it's a slash command */
    if (is_slash_command(p)) {
        return execute_slash_command(session, p);
    }

    /* Otherwise, treat as natural language */
    return execute_natural_language(session, p);
}

/* Main REPL loop */
int repl_run(ReplSession* session) {
    if (!session) return 1;

    /* Print welcome */
    repl_print_welcome(session);

    /* Main loop */
    while (session->running) {
        repl_print_prompt(session);

        char* input = read_input_line();
        if (input == NULL) {
            /* EOF (Ctrl+D) */
            printf("\n");
            break;
        }

        if (!repl_process_input(session, input)) {
            break;
        }
    }

    /* Goodbye message */
    if (session->config.colors_enabled) {
        printf("%sGoodbye!%s\n", COLOR_DIM, COLOR_RESET);
    } else {
        printf("Goodbye!\n");
    }

    return 0;
}
