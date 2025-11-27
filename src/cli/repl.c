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
#include "cyxmake/conversation_context.h"
#include "cyxmake/build_executor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define F_OK 0
#define access _access
#else
#include <unistd.h>
#endif

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

    /* Initialize conversation context */
    session->conversation = conversation_context_create(session->config.history_size);

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

    /* Free conversation context */
    conversation_context_free(session->conversation);

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

/* Convert command intent to context intent */
static ContextIntent intent_to_context(CommandIntent intent) {
    switch (intent) {
        case INTENT_BUILD:
        case INTENT_CLEAN:
        case INTENT_TEST:
            return CTX_INTENT_BUILD;
        case INTENT_INIT:
        case INTENT_STATUS:
            return CTX_INTENT_ANALYZE;
        case INTENT_READ_FILE:
        case INTENT_CREATE_FILE:
            return CTX_INTENT_FILE_OP;
        case INTENT_INSTALL:
            return CTX_INTENT_INSTALL;
        case INTENT_FIX:
            return CTX_INTENT_FIX;
        case INTENT_EXPLAIN:
            return CTX_INTENT_EXPLAIN;
        default:
            return CTX_INTENT_OTHER;
    }
}

/* Map AI action type to permission action type */
static ActionType ai_action_to_permission(AIActionType action_type) {
    switch (action_type) {
        case AI_ACTION_READ_FILE:    return ACTION_READ_FILE;
        case AI_ACTION_CREATE_FILE:  return ACTION_CREATE_FILE;
        case AI_ACTION_DELETE_FILE:  return ACTION_DELETE_FILE;
        case AI_ACTION_DELETE_DIR:   return ACTION_DELETE_DIR;
        case AI_ACTION_BUILD:        return ACTION_BUILD;
        case AI_ACTION_CLEAN:        return ACTION_DELETE_DIR;
        case AI_ACTION_INSTALL:      return ACTION_INSTALL_PKG;
        case AI_ACTION_RUN_COMMAND:  return ACTION_RUN_COMMAND;
        case AI_ACTION_LIST_FILES:   return ACTION_READ_FILE;
        default:                     return ACTION_READ_FILE;
    }
}

/* Execute a single AI action */
static bool execute_single_ai_action(ReplSession* session, AIAction* action) {
    if (!session || !action) return false;

    const char* action_name = ai_action_type_name(action->type);

    /* Log what we're doing */
    if (session->config.colors_enabled) {
        printf("%s%s Executing: %s%s", COLOR_CYAN, SYM_BULLET, action_name, COLOR_RESET);
        if (action->target) {
            printf(" - %s", action->target);
        }
        printf("\n");
        if (action->reason) {
            printf("  %s%s%s\n", COLOR_DIM, action->reason, COLOR_RESET);
        }
    } else {
        printf("Executing: %s", action_name);
        if (action->target) printf(" - %s", action->target);
        printf("\n");
        if (action->reason) printf("  %s\n", action->reason);
    }

    bool success = false;

    switch (action->type) {
        case AI_ACTION_READ_FILE:
            if (action->target) {
                if (file_exists(action->target)) {
                    printf("\n");
                    file_read_display(action->target, 50);
                    /* Update session context */
                    free(session->current_file);
                    session->current_file = strdup(action->target);
                    if (session->conversation) {
                        conversation_set_file(session->conversation, action->target, NULL, 0);
                    }
                    success = true;
                } else {
                    printf("File not found: %s\n", action->target);
                }
            }
            break;

        case AI_ACTION_CREATE_FILE:
            if (action->target) {
                const char* content = action->content ? action->content : "";
                if (file_write(action->target, content)) {
                    if (session->config.colors_enabled) {
                        printf("%s%s Created: %s%s\n", COLOR_GREEN, SYM_CHECK, action->target, COLOR_RESET);
                    } else {
                        printf("Created: %s\n", action->target);
                    }
                    success = true;
                } else {
                    printf("Failed to create file: %s\n", action->target);
                }
            }
            break;

        case AI_ACTION_DELETE_FILE:
            if (action->target) {
                if (file_delete(action->target)) {
                    if (session->config.colors_enabled) {
                        printf("%s%s Deleted: %s%s\n", COLOR_GREEN, SYM_CHECK, action->target, COLOR_RESET);
                    } else {
                        printf("Deleted: %s\n", action->target);
                    }
                    success = true;
                } else {
                    printf("Failed to delete file: %s\n", action->target);
                }
            }
            break;

        case AI_ACTION_DELETE_DIR:
            if (action->target) {
                if (dir_delete_recursive(action->target)) {
                    if (session->config.colors_enabled) {
                        printf("%s%s Deleted directory: %s%s\n", COLOR_GREEN, SYM_CHECK, action->target, COLOR_RESET);
                    } else {
                        printf("Deleted directory: %s\n", action->target);
                    }
                    success = true;
                } else {
                    printf("Failed to delete directory: %s\n", action->target);
                }
            }
            break;

        case AI_ACTION_BUILD:
            {
                const char* build_dir = action->target ? action->target : "build";
                printf("Building project in %s...\n", build_dir);

                /* Use cmake command to build */
                char cmd[1024];
                snprintf(cmd, sizeof(cmd), "cmake --build %s", build_dir);
                BuildResult* result = build_execute_command(cmd, session->working_dir);

                if (result && result->success) {
                    if (session->config.colors_enabled) {
                        printf("%s%s Build completed successfully%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
                    } else {
                        printf("Build completed successfully\n");
                    }
                    success = true;
                } else {
                    if (session->config.colors_enabled) {
                        printf("%s%s Build failed%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
                    } else {
                        printf("Build failed\n");
                    }
                    if (result && result->stderr_output) {
                        printf("%s\n", result->stderr_output);
                        /* Store error for later fixing */
                        free(session->last_error);
                        session->last_error = strdup(result->stderr_output);
                        if (session->conversation) {
                            conversation_set_error(session->conversation, result->stderr_output, "build", NULL, 0);
                        }
                    }
                }
                build_result_free(result);
            }
            break;

        case AI_ACTION_CLEAN:
            {
                const char* build_dir = action->target ? action->target : "build";
                if (dir_delete_recursive(build_dir)) {
                    if (session->config.colors_enabled) {
                        printf("%s%s Cleaned: %s%s\n", COLOR_GREEN, SYM_CHECK, build_dir, COLOR_RESET);
                    } else {
                        printf("Cleaned: %s\n", build_dir);
                    }
                    success = true;
                } else {
                    printf("Failed to clean build directory\n");
                }
            }
            break;

        case AI_ACTION_INSTALL:
            if (action->target) {
                printf("Installing package: %s\n", action->target);

                ToolRegistry* registry = tool_registry_create();
                if (registry) {
                    tool_discover_all(registry);
                    const ToolInfo* pkg_mgr = package_get_default_manager(registry);

                    if (pkg_mgr) {
                        printf("Using: %s\n", pkg_mgr->display_name);
                        ToolExecResult* result = package_install(registry, action->target, NULL);
                        if (result && result->success) {
                            if (session->config.colors_enabled) {
                                printf("%s%s Installed: %s%s\n", COLOR_GREEN, SYM_CHECK, action->target, COLOR_RESET);
                            } else {
                                printf("Installed: %s\n", action->target);
                            }
                            success = true;
                        } else {
                            printf("Failed to install package\n");
                        }
                        tool_exec_result_free(result);
                    } else {
                        printf("No package manager found\n");
                    }
                    tool_registry_free(registry);
                }
            }
            break;

        case AI_ACTION_RUN_COMMAND:
            if (action->content) {
                printf("Running: %s\n", action->content);

                /* Execute command using system() */
                int ret = system(action->content);
                success = (ret == 0);

                if (!success) {
                    if (session->config.colors_enabled) {
                        printf("%s%s Command failed with exit code %d%s\n",
                               COLOR_RED, SYM_CROSS, ret, COLOR_RESET);
                    } else {
                        printf("Command failed with exit code %d\n", ret);
                    }
                }
            }
            break;

        case AI_ACTION_LIST_FILES:
            {
                const char* dir = action->target ? action->target : ".";
                printf("Files in %s:\n", dir);
#ifdef _WIN32
                char cmd[1024];
                snprintf(cmd, sizeof(cmd), "dir /B \"%s\"", dir);
                system(cmd);
#else
                char cmd[1024];
                snprintf(cmd, sizeof(cmd), "ls -la \"%s\"", dir);
                system(cmd);
#endif
                success = true;
            }
            break;

        case AI_ACTION_NONE:
        case AI_ACTION_MULTI:
        default:
            success = true;  /* No-op is always successful */
            break;
    }

    return success;
}

/* Execute AI agent response with permission checks */
static bool execute_ai_agent_response(ReplSession* session, AIAgentResponse* response) {
    if (!session || !response) return false;

    /* Display AI message */
    if (response->message) {
        if (session->config.colors_enabled) {
            printf("\n%s%sAI:%s %s\n\n", COLOR_BOLD, COLOR_MAGENTA, COLOR_RESET, response->message);
        } else {
            printf("\nAI: %s\n\n", response->message);
        }
    }

    /* If no actions, we're done */
    if (!response->actions) {
        return true;
    }

    /* Count actions */
    int action_count = 0;
    for (AIAction* a = response->actions; a; a = a->next) {
        action_count++;
    }

    /* Check if we need confirmation */
    if (response->needs_confirmation) {
        if (session->config.colors_enabled) {
            printf("%s%s The AI wants to perform %d action(s):%s\n",
                   COLOR_YELLOW, SYM_WARN, action_count, COLOR_RESET);
        } else {
            printf("The AI wants to perform %d action(s):\n", action_count);
        }

        /* List all planned actions */
        int idx = 1;
        for (AIAction* a = response->actions; a; a = a->next) {
            printf("  %d. %s", idx++, ai_action_type_name(a->type));
            if (a->target) printf(": %s", a->target);
            printf("\n");
        }
        printf("\n");
    }

    /* Execute each action with permission checks */
    bool all_success = true;
    for (AIAction* action = response->actions; action; action = action->next) {
        /* Check permission for this action */
        ActionType perm_type = ai_action_to_permission(action->type);
        const char* target = action->target ? action->target :
                             (action->content ? action->content : "AI action");

        char reason_buf[256];
        snprintf(reason_buf, sizeof(reason_buf), "AI agent: %s",
                 action->reason ? action->reason : ai_action_type_name(action->type));

        if (!permission_check(session->permissions, perm_type, target, reason_buf)) {
            if (session->config.colors_enabled) {
                printf("%s%s Permission denied for: %s%s\n",
                       COLOR_RED, SYM_CROSS, ai_action_type_name(action->type), COLOR_RESET);
            } else {
                printf("Permission denied for: %s\n", ai_action_type_name(action->type));
            }
            continue;
        }

        /* Execute the action */
        if (!execute_single_ai_action(session, action)) {
            all_success = false;
            /* Continue with other actions unless critical */
        }
    }

    return all_success;
}

/* Execute natural language command */
static bool execute_natural_language(ReplSession* session, const char* input) {
    /* Check for context references (e.g., "fix it", "show the file") */
    char* resolved_target = NULL;
    if (session->conversation) {
        conversation_resolve_reference(session->conversation, input, &resolved_target);
    }

    /* Add user message to conversation */
    if (session->conversation) {
        conversation_add_message(session->conversation, MSG_ROLE_USER, input,
                                  CTX_INTENT_OTHER, resolved_target, true);
    }

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
                    /* Remember current file in session */
                    free(session->current_file);
                    session->current_file = strdup(cmd->target);
                    /* Update conversation context */
                    if (session->conversation) {
                        conversation_set_file(session->conversation, cmd->target, NULL, 0);
                        conversation_add_message(session->conversation, MSG_ROLE_ASSISTANT,
                                                  "Displayed file content", CTX_INTENT_FILE_OP,
                                                  cmd->target, true);
                    }
                } else {
                    if (session->config.colors_enabled) {
                        printf("%s%s File not found: %s%s\n", COLOR_RED, SYM_CROSS, cmd->target, COLOR_RESET);
                    } else {
                        printf("Error: File not found: %s\n", cmd->target);
                    }
                }
            } else if (resolved_target) {
                /* Use resolved file from context */
                if (file_exists(resolved_target)) {
                    printf("\n");
                    file_read_display(resolved_target, 50);
                } else {
                    printf("File not found: %s\n", resolved_target);
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
            if (session->llm && llm_is_ready(session->llm)) {
                /* Get conversation context */
                char* context_str = NULL;
                const char* current_file = NULL;
                if (session->conversation) {
                    context_str = conversation_get_context_string(session->conversation, 5);
                    current_file = conversation_get_current_file(session->conversation);
                }

                /* Generate prompt */
                char* prompt = prompt_explain_with_context(
                    cmd->details ? cmd->details : input,
                    current_file,
                    NULL,  /* file content - could read if needed */
                    context_str
                );

                if (prompt) {
                    if (session->config.colors_enabled) {
                        printf("%sThinking...%s\n", COLOR_DIM, COLOR_RESET);
                    } else {
                        printf("Thinking...\n");
                    }

                    char* response = llm_query_simple(session->llm, prompt, 512);
                    if (response) {
                        printf("\n%s\n", response);

                        /* Add to conversation */
                        if (session->conversation) {
                            conversation_add_message(session->conversation, MSG_ROLE_ASSISTANT,
                                                      response, CTX_INTENT_EXPLAIN, NULL, true);
                        }
                        free(response);
                    } else {
                        printf("AI could not generate a response.\n");
                    }
                    free(prompt);
                }
                free(context_str);
            } else {
                if (session->config.colors_enabled) {
                    printf("%s%s AI not available%s\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
                    printf("%sTo enable AI, load a model with test-llm command%s\n", COLOR_DIM, COLOR_RESET);
                } else {
                    printf("AI not available. Load a model to enable AI features.\n");
                }
            }
            break;

        case INTENT_FIX:
            {
                /* Get error context */
                const char* error_to_fix = NULL;
                const char* current_file = NULL;

                if (session->conversation) {
                    error_to_fix = conversation_get_last_error(session->conversation);
                    current_file = conversation_get_current_file(session->conversation);
                }

                /* Try session's last_error as fallback */
                if (!error_to_fix && session->last_error) {
                    error_to_fix = session->last_error;
                }

                /* Or use the target from command (e.g., "fix the undefined reference error") */
                if (!error_to_fix && cmd->target) {
                    error_to_fix = cmd->target;
                }

                if (!error_to_fix) {
                    if (session->config.colors_enabled) {
                        printf("%s%s No error to fix%s\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
                        printf("%sRun a build first or specify the error%s\n", COLOR_DIM, COLOR_RESET);
                    } else {
                        printf("No error to fix. Run a build first or specify the error.\n");
                    }
                    break;
                }

                if (session->llm && llm_is_ready(session->llm)) {
                    /* Get conversation context */
                    char* context_str = NULL;
                    if (session->conversation) {
                        context_str = conversation_get_context_string(session->conversation, 5);
                    }

                    /* Generate prompt */
                    char* prompt = prompt_fix_with_context(
                        error_to_fix,
                        current_file,
                        NULL,  /* file content */
                        context_str
                    );

                    if (prompt) {
                        if (session->config.colors_enabled) {
                            printf("%sAnalyzing error...%s\n", COLOR_DIM, COLOR_RESET);
                        } else {
                            printf("Analyzing error...\n");
                        }

                        char* response = llm_query_simple(session->llm, prompt, 512);
                        if (response) {
                            if (session->config.colors_enabled) {
                                printf("\n%sSuggested fix:%s\n", COLOR_CYAN, COLOR_RESET);
                            } else {
                                printf("\nSuggested fix:\n");
                            }
                            printf("%s\n", response);

                            /* Add to conversation */
                            if (session->conversation) {
                                conversation_add_message(session->conversation, MSG_ROLE_ASSISTANT,
                                                          response, CTX_INTENT_FIX, NULL, true);
                            }
                            free(response);
                        } else {
                            printf("AI could not generate a fix suggestion.\n");
                        }
                        free(prompt);
                    }
                    free(context_str);
                } else {
                    /* Fallback: show error and suggest manual fix */
                    if (session->config.colors_enabled) {
                        printf("%s%s AI not available%s\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
                        printf("\n%sError:%s %s\n", COLOR_RED, COLOR_RESET, error_to_fix);
                        printf("\n%sTip: Load an AI model for automatic fix suggestions%s\n",
                               COLOR_DIM, COLOR_RESET);
                    } else {
                        printf("AI not available.\n\nError: %s\n", error_to_fix);
                        printf("\nTip: Load an AI model for automatic fix suggestions\n");
                    }
                }
            }
            break;

        case INTENT_UNKNOWN:
        default:
            /* If AI is available, use AI agent for complex requests */
            if (session->llm && llm_is_ready(session->llm)) {
                if (session->config.colors_enabled) {
                    printf("%sAsking AI agent...%s\n", COLOR_DIM, COLOR_RESET);
                } else {
                    printf("Asking AI agent...\n");
                }

                /* Get conversation context */
                char* context_str = NULL;
                const char* current_file = NULL;
                const char* last_error = NULL;

                if (session->conversation) {
                    context_str = conversation_get_context_string(session->conversation, 5);
                    current_file = conversation_get_current_file(session->conversation);
                    last_error = conversation_get_last_error(session->conversation);
                }

                /* Generate AI agent prompt */
                char* prompt = prompt_ai_agent(
                    input,
                    session->working_dir,
                    current_file,
                    last_error,
                    context_str
                );

                if (prompt) {
                    /* Query AI for response with actions */
                    char* ai_response = llm_query_simple(session->llm, prompt, 1024);

                    if (ai_response) {
                        /* Parse the AI response */
                        AIAgentResponse* agent_response = parse_ai_agent_response(ai_response);

                        if (agent_response) {
                            /* Execute the AI agent's actions */
                            execute_ai_agent_response(session, agent_response);

                            /* Add to conversation */
                            if (session->conversation && agent_response->message) {
                                conversation_add_message(session->conversation, MSG_ROLE_ASSISTANT,
                                                          agent_response->message, CTX_INTENT_OTHER,
                                                          NULL, true);
                            }

                            ai_agent_response_free(agent_response);
                        } else {
                            /* Couldn't parse response, show raw */
                            printf("\n%s\n", ai_response);
                        }
                        free(ai_response);
                    } else {
                        printf("AI did not respond.\n");
                    }
                    free(prompt);
                }
                free(context_str);
            } else {
                /* No AI available */
                if (session->config.colors_enabled) {
                    printf("%s%s I didn't understand that.%s\n",
                           COLOR_YELLOW, SYM_WARN, COLOR_RESET);
                    printf("%sLoad an AI model with '/ai load <model>' or try /help for commands.%s\n",
                           COLOR_DIM, COLOR_RESET);
                } else {
                    printf("I didn't understand that.\n");
                    printf("Load an AI model with '/ai load <model>' or try /help for commands.\n");
                }
            }
            break;
    }

    parsed_command_free(cmd);
    free(resolved_target);
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
