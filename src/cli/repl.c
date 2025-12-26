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
#include "cyxmake/ai_provider.h"
#include "cyxmake/input.h"
#include "cyxmake/action_planner.h"
#include "cyxmake/smart_agent.h"
#include "cyxmake/project_graph.h"
#include "cyxmake/project_context.h"
#include "cyxmake/autonomous_agent.h"
#include "cyxmake/error_recovery.h"
#include "cyxmake/cache_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

/**
 * Strip non-ASCII characters from a string (in-place)
 * Replaces multi-byte UTF-8 sequences with spaces or removes them
 */
static void strip_non_ascii(char* str) {
    if (!str) return;

    char* src = str;
    char* dst = str;

    while (*src) {
        unsigned char c = (unsigned char)*src;

        if (c < 0x80) {
            /* ASCII character - keep it */
            *dst++ = *src++;
        } else if (c >= 0xC0 && c < 0xE0) {
            /* 2-byte UTF-8 sequence - skip */
            src += 2;
        } else if (c >= 0xE0 && c < 0xF0) {
            /* 3-byte UTF-8 sequence (includes most emojis) - skip */
            src += 3;
        } else if (c >= 0xF0) {
            /* 4-byte UTF-8 sequence (includes complex emojis) - skip */
            src += 4;
        } else {
            /* Continuation byte or invalid - skip one byte */
            src++;
        }
    }
    *dst = '\0';
}

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

    /* Initialize AI provider registry and load from config */
    session->ai_registry = ai_registry_create();
    if (session->ai_registry) {
        int loaded = ai_registry_load_config(session->ai_registry, NULL);
        if (loaded > 0) {
            log_debug("Loaded %d AI providers from config", loaded);
            /* Set current provider to default */
            session->current_provider = ai_registry_get_default(session->ai_registry);
            if (session->current_provider) {
                log_info("Using AI provider: %s at %s",
                         session->current_provider->config.name,
                         session->current_provider->config.base_url);
            }
        } else {
            /* No config, try to create from environment */
            AIProvider* env_provider = ai_provider_from_env();
            if (env_provider) {
                log_debug("Using AI provider from environment");
                session->current_provider = env_provider;
            }
        }
    }

    /* Initialize input context with line editing support */
    session->input = input_context_create(session->config.history_size);
    if (session->input) {
        input_set_colors(session->input, session->config.colors_enabled);
        input_set_completion_callback(session->input, input_complete_combined);
    }

    /* Initialize Smart Agent if AI provider is available */
    if (session->current_provider) {
        ToolRegistry* tools = session->orchestrator ?
                              cyxmake_get_tools(session->orchestrator) : NULL;
        session->smart_agent = smart_agent_create(session->current_provider, tools);
        if (session->smart_agent) {
            session->smart_agent->verbose = session->config.verbose;
            session->smart_agent->explain_actions = true;

            /* Try to load existing agent memory */
            if (session->working_dir) {
                char memory_path[1024];
                snprintf(memory_path, sizeof(memory_path), "%s/.cyxmake/agent_memory.json",
                         session->working_dir);
                AgentMemory* loaded = agent_memory_load(memory_path);
                if (loaded) {
                    agent_memory_free(session->smart_agent->memory);
                    session->smart_agent->memory = loaded;
                    log_debug("Loaded agent memory: %d commands, %d fixes",
                              loaded->command_count, loaded->fix_count);
                }
            }

            log_debug("Smart Agent initialized");
        }
    }

    /* Initialize Project Graph for dependency analysis */
    if (session->working_dir) {
        session->project_graph = project_graph_create(session->working_dir);
        if (session->project_graph) {
            log_debug("Project graph initialized for: %s", session->working_dir);
        }
    }

    /* Initialize Autonomous Agent for tool-using AI */
    if (session->current_provider) {
        AgentConfig agent_cfg = agent_config_default();
        agent_cfg.verbose = session->config.verbose;
        agent_cfg.working_dir = session->working_dir;
        agent_cfg.max_iterations = 20;  /* Allow more steps for complex tasks */
        agent_cfg.require_approval = false;  /* Auto-approve for now */

        session->autonomous_agent = agent_create(session->current_provider, &agent_cfg);
        if (session->autonomous_agent) {
            /* Note: builtin tools are already registered in agent_create() */
            log_debug("Autonomous Agent initialized with tool use support");
        }
    }

    /* Initialize Error Recovery Context */
    {
        RecoveryStrategy strategy = {
            .max_retries = 3,
            .retry_delay_ms = 1000,
            .backoff_multiplier = 2.0f,
            .max_delay_ms = 30000,
            .use_ai_analysis = (session->current_provider != NULL || session->llm != NULL),
            .auto_apply_fixes = false  /* Require permission in REPL mode */
        };
        session->recovery_ctx = recovery_context_create(&strategy);
        if (session->recovery_ctx) {
            /* Wire up LLM if available */
            if (session->llm) {
                recovery_set_llm(session->recovery_ctx, session->llm);
            }
            /* Wire up tool registry if available */
            ToolRegistry* tools = session->orchestrator ?
                                  cyxmake_get_tools(session->orchestrator) : NULL;
            if (tools) {
                recovery_set_tools(session->recovery_ctx, tools);
            }
            log_debug("Error Recovery context initialized");
        }
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

    /* Free conversation context */
    conversation_context_free(session->conversation);

    /* Free AI provider registry */
    ai_registry_free(session->ai_registry);
    /* Note: current_provider is freed by registry, don't double-free */

    /* Free input context */
    input_context_free(session->input);

    /* Save and free Smart Agent */
    if (session->smart_agent && session->smart_agent->memory && session->working_dir) {
        char memory_path[1024];
        snprintf(memory_path, sizeof(memory_path), "%s/.cyxmake/agent_memory.json",
                 session->working_dir);
        if (agent_memory_save(session->smart_agent->memory, memory_path)) {
            log_debug("Saved agent memory to: %s", memory_path);
        }
    }
    smart_agent_free(session->smart_agent);

    /* Free Project Graph */
    project_graph_free(session->project_graph);

    /* Free Autonomous Agent */
    agent_free(session->autonomous_agent);

    /* Free Error Recovery Context */
    recovery_context_free(session->recovery_ctx);

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
        /* Strip non-ASCII characters for Windows console compatibility */
        strip_non_ascii(response->message);

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

    /* For multi-step actions (2+), use the action planner for better UX */
    if (action_count >= 2 || response->needs_confirmation) {
        /* Create plan from AI response */
        ActionPlan* plan = plan_from_ai_response(response, NULL);
        if (!plan) {
            if (session->config.colors_enabled) {
                printf("%s%s Failed to create action plan%s\n", COLOR_RED, SYM_CROSS, COLOR_RESET);
            } else {
                printf("Failed to create action plan\n");
            }
            return false;
        }

        /* Request approval */
        ApprovalMode approval = plan_request_approval(plan, session);
        if (approval == APPROVAL_DENIED) {
            if (session->config.colors_enabled) {
                printf("%s%s Plan cancelled%s\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
            } else {
                printf("Plan cancelled\n");
            }
            plan_free(plan);
            return true;  /* Not an error, just user choice */
        }

        /* Execute plan */
        PlanExecOptions opts = plan_exec_options_default();
        opts.stop_on_failure = true;
        opts.auto_rollback = false;  /* Let user decide */

        bool success = plan_execute(plan, session, &opts);

        /* If failed, offer rollback */
        if (!success && plan->completed_count > 0) {
            if (session->config.colors_enabled) {
                printf("\n%s%s Some steps failed. Rollback completed steps? [y/N]: %s",
                       COLOR_YELLOW, SYM_WARN, COLOR_RESET);
            } else {
                printf("\nSome steps failed. Rollback completed steps? [y/N]: ");
            }
            fflush(stdout);

            char buf[64];
            if (fgets(buf, sizeof(buf), stdin) && (buf[0] == 'y' || buf[0] == 'Y')) {
                plan_rollback(plan, session);
            }
        }

        plan_free(plan);
        return success;
    }

    /* Single action - execute directly with permission check */
    AIAction* action = response->actions;
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
        return false;
    }

    return execute_single_ai_action(session, action);
}

/* Map SmartIntentType to CommandIntent */
static CommandIntent smart_intent_to_command_intent(SmartIntentType smart_intent) {
    switch (smart_intent) {
        case SMART_INTENT_BUILD:     return INTENT_BUILD;
        case SMART_INTENT_CLEAN:     return INTENT_CLEAN;
        case SMART_INTENT_TEST:      return INTENT_TEST;
        case SMART_INTENT_RUN:       return INTENT_STATUS;  /* Closest match */
        case SMART_INTENT_FIX:       return INTENT_FIX;
        case SMART_INTENT_INSTALL:   return INTENT_INSTALL;
        case SMART_INTENT_CONFIGURE: return INTENT_INIT;
        case SMART_INTENT_EXPLAIN:   return INTENT_EXPLAIN;
        case SMART_INTENT_CREATE:    return INTENT_CREATE_FILE;
        case SMART_INTENT_READ:      return INTENT_READ_FILE;
        case SMART_INTENT_HELP:      return INTENT_HELP;
        default:                     return INTENT_UNKNOWN;
    }
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

    /* Check if this is a complex multi-step task that should use autonomous agent */
    bool is_complex_task = false;
    const char* complex_keywords[] = {
        "and then", "and also", "after that", "first", "second", "next",
        "step by step", "explore", "analyze", "understand", "figure out",
        "find out", "investigate", "discover", "learn about", "tell me about",
        "create a project", "set up", "initialize new", "scaffold",
        NULL
    };

    /* Check for complex task indicators */
    char* input_lower = strdup(input);
    if (input_lower) {
        for (char* p = input_lower; *p; p++) *p = tolower((unsigned char)*p);

        for (int i = 0; complex_keywords[i]; i++) {
            if (strstr(input_lower, complex_keywords[i])) {
                is_complex_task = true;
                break;
            }
        }
        /* Also consider input length - longer requests are likely complex */
        if (strlen(input) > 80) {
            is_complex_task = true;
        }
        free(input_lower);
    }

    /* Route complex tasks directly to autonomous agent */
    if (is_complex_task && session->autonomous_agent) {
        if (session->config.colors_enabled) {
            printf("%s%s Complex task detected - using Autonomous Agent...%s\n\n",
                   COLOR_DIM, SYM_BULLET, COLOR_RESET);
        } else {
            printf("Complex task detected - using Autonomous Agent...\n\n");
        }

        agent_set_working_dir(session->autonomous_agent, session->working_dir);
        char* result = agent_run(session->autonomous_agent, input);

        if (result) {
            strip_non_ascii(result);
            printf("\n%s%s%s\n", COLOR_GREEN, result, COLOR_RESET);

            if (session->conversation) {
                conversation_add_message(session->conversation, MSG_ROLE_ASSISTANT,
                                          result, CTX_INTENT_OTHER, NULL, true);
            }
            free(result);
        } else {
            const char* error = autonomous_agent_get_error(session->autonomous_agent);
            if (error) {
                printf("%sAgent error: %s%s\n", COLOR_RED, error, COLOR_RESET);
            } else {
                printf("%sAgent could not complete the task.%s\n", COLOR_YELLOW, COLOR_RESET);
            }
        }

        agent_clear_history(session->autonomous_agent);
        free(resolved_target);
        return true;
    }

    /* Try Smart Agent for intent understanding if available */
    SmartIntent* smart_intent = NULL;
    if (session->smart_agent) {
        /* Update Smart Agent context */
        smart_agent_set_conversation(session->smart_agent, session->conversation);

        smart_intent = smart_agent_understand(session->smart_agent, input);
        if (smart_intent && session->config.verbose) {
            if (session->config.colors_enabled) {
                printf("%s[Smart Agent] Intent: %s (%.0f%% confidence)%s\n",
                       COLOR_DIM,
                       smart_intent_type_to_string(smart_intent->primary_intent),
                       smart_intent->overall_confidence * 100,
                       COLOR_RESET);
                if (smart_intent->ai_interpretation) {
                    printf("%s  AI interpretation: %s%s\n",
                           COLOR_DIM, smart_intent->ai_interpretation, COLOR_RESET);
                }
            }
        }
    }

    /* Parse the command - use Smart Agent result or fall back to local parsing */
    ParsedCommand* cmd = NULL;

    if (smart_intent && smart_intent->overall_confidence > 0.5f) {
        /* Use Smart Agent's understanding */
        cmd = calloc(1, sizeof(ParsedCommand));
        if (cmd) {
            cmd->intent = smart_intent_to_command_intent(smart_intent->primary_intent);
            cmd->confidence = smart_intent->overall_confidence;

            /* Extract target from smart intent */
            if (smart_intent->file_ref_count > 0 && smart_intent->file_references) {
                cmd->target = strdup(smart_intent->file_references[0]);
            } else if (smart_intent->package_ref_count > 0 && smart_intent->package_references) {
                cmd->target = strdup(smart_intent->package_references[0]);
            } else if (smart_intent->target_ref_count > 0 && smart_intent->target_references) {
                cmd->target = strdup(smart_intent->target_references[0]);
            }

            /* Handle context references */
            if (smart_intent->references_last_error && session->conversation) {
                const char* last_err = conversation_get_last_error(session->conversation);
                if (last_err && !cmd->target) {
                    cmd->details = strdup(last_err);
                }
            }
            if (smart_intent->references_last_file && session->conversation) {
                const char* last_file = conversation_get_current_file(session->conversation);
                if (last_file && !cmd->target) {
                    cmd->target = strdup(last_file);
                }
            }
        }
    }

    /* Fall back to local parsing if Smart Agent didn't help */
    if (!cmd) {
        cmd = parse_command_local(input);
    }

    /* Clean up smart intent */
    smart_intent_free(smart_intent);

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

    /* Define confidence threshold - below this, route to AI if available */
    const float AI_ROUTING_THRESHOLD = 0.6f;

    /* Check if we should route to AI instead of local execution */
    bool has_ai = (session->current_provider && ai_provider_is_ready(session->current_provider)) ||
                  (session->llm && llm_is_ready(session->llm));
    bool low_confidence = cmd->confidence < AI_ROUTING_THRESHOLD && cmd->confidence > 0;

    if (has_ai && low_confidence && cmd->intent != INTENT_UNKNOWN) {
        /* Route to AI for low confidence commands */
        if (session->config.colors_enabled) {
            printf("%sLow confidence - routing to AI...%s\n", COLOR_DIM, COLOR_RESET);
        } else {
            printf("Low confidence - routing to AI...\n");
        }
        cmd->intent = INTENT_UNKNOWN;  /* Force AI routing */
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
            {
                /* Check if any AI is available (local LLM or cloud provider) */
                bool ai_available = (session->llm && llm_is_ready(session->llm)) ||
                                    (session->current_provider && ai_provider_is_ready(session->current_provider));

                if (ai_available) {
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

                        char* response = NULL;

                        /* Use cloud provider if available, otherwise local LLM */
                        if (session->current_provider && ai_provider_is_ready(session->current_provider)) {
                            response = ai_provider_query(session->current_provider, prompt, 1024);
                        } else if (session->llm && llm_is_ready(session->llm)) {
                            response = llm_query_simple(session->llm, prompt, 512);
                        }

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
                        printf("%sTo enable AI, configure a provider in cyxmake.toml%s\n", COLOR_DIM, COLOR_RESET);
                    } else {
                        printf("AI not available. Configure a provider in cyxmake.toml.\n");
                    }
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

                /* Check if any AI is available */
                bool ai_available = (session->llm && llm_is_ready(session->llm)) ||
                                    (session->current_provider && ai_provider_is_ready(session->current_provider));

                if (ai_available) {
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

                        char* response = NULL;

                        /* Use cloud provider if available, otherwise local LLM */
                        if (session->current_provider && ai_provider_is_ready(session->current_provider)) {
                            response = ai_provider_query(session->current_provider, prompt, 1024);
                        } else if (session->llm && llm_is_ready(session->llm)) {
                            response = llm_query_simple(session->llm, prompt, 512);
                        }

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
            /* Use Autonomous Agent for complex tasks that require tool use */
            {
                if (session->autonomous_agent) {
                    if (session->config.colors_enabled) {
                        printf("%sAutonomous Agent thinking...%s\n\n", COLOR_DIM, COLOR_RESET);
                    } else {
                        printf("Autonomous Agent thinking...\n\n");
                    }
                    fflush(stdout);  /* Ensure message is displayed before blocking call */

                    /* Update working directory if changed */
                    agent_set_working_dir(session->autonomous_agent, session->working_dir);

                    /* Run the autonomous agent on the task */
                    char* result = agent_run(session->autonomous_agent, input);

                    if (result) {
                        /* Display the result */
                        strip_non_ascii(result);
                        printf("\n%s%s%s\n", COLOR_GREEN, result, COLOR_RESET);

                        /* Add to conversation */
                        if (session->conversation) {
                            conversation_add_message(session->conversation, MSG_ROLE_ASSISTANT,
                                                      result, CTX_INTENT_OTHER, NULL, true);
                        }
                        free(result);
                    } else {
                        const char* error = autonomous_agent_get_error(session->autonomous_agent);
                        if (error) {
                            printf("%sAgent error: %s%s\n", COLOR_RED, error, COLOR_RESET);
                        } else {
                            printf("%sAgent could not complete the task.%s\n", COLOR_YELLOW, COLOR_RESET);
                        }
                    }

                    /* Clear history for next task */
                    agent_clear_history(session->autonomous_agent);
                }
                /* Fall back to simple AI query if no autonomous agent */
                else if (session->current_provider && ai_provider_is_ready(session->current_provider)) {
                    if (session->config.colors_enabled) {
                        printf("%sAsking AI...%s\n", COLOR_DIM, COLOR_RESET);
                    } else {
                        printf("Asking AI...\n");
                    }
                    fflush(stdout);  /* Ensure message is displayed before blocking call */

                    char* response = ai_provider_query(session->current_provider, input, 1024);
                    if (response) {
                        strip_non_ascii(response);
                        printf("\n%s\n", response);

                        if (session->conversation) {
                            conversation_add_message(session->conversation, MSG_ROLE_ASSISTANT,
                                                      response, CTX_INTENT_OTHER, NULL, true);
                        }
                        free(response);
                    } else {
                        printf("AI did not respond.\n");
                    }
                }
                /* No AI available */
                else {
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

    /* Add to input history */
    if (session->input) {
        input_history_add(session->input, input);
    }

    /* Add to legacy history */
    repl_history_add(session, input);
    session->command_count++;

    /* Check if it's a slash command */
    if (is_slash_command(p)) {
        return execute_slash_command(session, p);
    }

    /* Otherwise, treat as natural language */
    return execute_natural_language(session, p);
}

/* Build colored prompt string */
static char* build_colored_prompt(ReplSession* session) {
    static char prompt_buf[256];

    if (session->config.colors_enabled) {
        snprintf(prompt_buf, sizeof(prompt_buf), "%s%s%s%s",
                 COLOR_BOLD, COLOR_GREEN, session->config.prompt, COLOR_RESET);
    } else {
        snprintf(prompt_buf, sizeof(prompt_buf), "%s", session->config.prompt);
    }

    return prompt_buf;
}

/* Main REPL loop */
int repl_run(ReplSession* session) {
    if (!session) return 1;

    /* Print welcome */
    repl_print_welcome(session);

    /* Main loop */
    while (session->running) {
        char* line;

        /* Use advanced input if available, otherwise fall back to simple input */
        if (session->input) {
            line = input_readline(session->input, build_colored_prompt(session));
        } else {
            repl_print_prompt(session);
            line = read_input_line();
        }

        if (line == NULL) {
            /* EOF (Ctrl+D) */
            break;
        }

        /* Skip empty input */
        const char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') {
            continue;
        }

        /* Add to input history */
        if (session->input) {
            input_history_add(session->input, line);
        }

        /* Also add to legacy history for compatibility */
        repl_history_add(session, line);
        session->command_count++;

        /* Check if it's a slash command */
        if (is_slash_command(p)) {
            if (!execute_slash_command(session, p)) {
                break;
            }
        } else {
            /* Otherwise, treat as natural language */
            if (!execute_natural_language(session, p)) {
                break;
            }
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
