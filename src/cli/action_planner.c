/**
 * @file action_planner.c
 * @brief Multi-step action planning implementation
 */

#include "cyxmake/action_planner.h"
#include "cyxmake/repl.h"
#include "cyxmake/logger.h"
#include "cyxmake/file_ops.h"
#include "cyxmake/tool_executor.h"
#include "cyxmake/build_executor.h"
#include "cyxmake/permission.h"
#include "cyxmake/input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
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

/* Status symbols */
#define SYM_CHECK   "[OK]"
#define SYM_CROSS   "[X]"
#define SYM_BULLET  "*"
#define SYM_WARN    "[!]"
#define SYM_ARROW   ">"
#define SYM_PENDING "[ ]"
#define SYM_RUNNING "[.]"

/* ============================================================================
 * Plan Creation
 * ============================================================================ */

ActionPlan* plan_create(const char* title, const char* user_request) {
    ActionPlan* plan = calloc(1, sizeof(ActionPlan));
    if (!plan) return NULL;

    plan->title = title ? strdup(title) : strdup("Action Plan");
    plan->user_request = user_request ? strdup(user_request) : NULL;
    plan->state = PLAN_CREATED;
    plan->created_at = time(NULL);

    return plan;
}

ActionPlan* plan_from_ai_response(AIAgentResponse* response, const char* user_request) {
    if (!response || !response->actions) return NULL;

    /* Count actions to determine if this is a multi-step plan */
    int count = 0;
    for (AIAction* a = response->actions; a; a = a->next) {
        count++;
    }

    /* Create plan */
    char title[256];
    if (count == 1) {
        snprintf(title, sizeof(title), "Execute: %s",
                 ai_action_type_name(response->actions->type));
    } else {
        snprintf(title, sizeof(title), "Action Plan (%d steps)", count);
    }

    ActionPlan* plan = plan_create(title, user_request);
    if (!plan) return NULL;

    if (response->message) {
        plan->description = strdup(response->message);
    }

    /* Convert AI actions to plan steps */
    int step_num = 1;
    for (AIAction* a = response->actions; a; a = a->next) {
        /* Generate description */
        char desc[512];
        switch (a->type) {
            case AI_ACTION_READ_FILE:
                snprintf(desc, sizeof(desc), "Read file: %s", a->target ? a->target : "?");
                break;
            case AI_ACTION_CREATE_FILE:
                snprintf(desc, sizeof(desc), "Create file: %s", a->target ? a->target : "?");
                break;
            case AI_ACTION_DELETE_FILE:
                snprintf(desc, sizeof(desc), "Delete file: %s", a->target ? a->target : "?");
                break;
            case AI_ACTION_DELETE_DIR:
                snprintf(desc, sizeof(desc), "Delete directory: %s", a->target ? a->target : "?");
                break;
            case AI_ACTION_BUILD:
                snprintf(desc, sizeof(desc), "Build project%s%s",
                         a->target ? " in " : "", a->target ? a->target : "");
                break;
            case AI_ACTION_CLEAN:
                snprintf(desc, sizeof(desc), "Clean build artifacts");
                break;
            case AI_ACTION_INSTALL:
                snprintf(desc, sizeof(desc), "Install package: %s", a->target ? a->target : "?");
                break;
            case AI_ACTION_RUN_COMMAND:
                snprintf(desc, sizeof(desc), "Run: %s",
                         a->content ? a->content : (a->target ? a->target : "?"));
                break;
            case AI_ACTION_LIST_FILES:
                snprintf(desc, sizeof(desc), "List files in: %s", a->target ? a->target : ".");
                break;
            default:
                snprintf(desc, sizeof(desc), "%s", ai_action_type_name(a->type));
                break;
        }

        ActionStep* step = plan_add_step(plan, a->type, desc, a->target, a->content, a->reason);

        /* Set up rollback info based on action type */
        if (step) {
            switch (a->type) {
                case AI_ACTION_CREATE_FILE:
                    step_set_rollback(step, ROLLBACK_DELETE_FILE, a->target, NULL);
                    break;
                case AI_ACTION_DELETE_FILE:
                    /* Could save content for restore, but that's expensive */
                    step->can_rollback = false;
                    break;
                case AI_ACTION_INSTALL:
                    step_set_rollback(step, ROLLBACK_UNINSTALL, a->target, NULL);
                    break;
                default:
                    step->can_rollback = false;
                    break;
            }
        }

        step_num++;
    }

    return plan;
}

ActionStep* plan_add_step(ActionPlan* plan,
                          AIActionType action,
                          const char* description,
                          const char* target,
                          const char* content,
                          const char* reason) {
    if (!plan) return NULL;

    ActionStep* step = calloc(1, sizeof(ActionStep));
    if (!step) return NULL;

    step->step_number = ++plan->step_count;
    step->action = action;
    step->description = description ? strdup(description) : NULL;
    step->target = target ? strdup(target) : NULL;
    step->content = content ? strdup(content) : NULL;
    step->reason = reason ? strdup(reason) : NULL;
    step->status = STEP_PENDING;
    step->can_rollback = false;

    /* Append to list */
    step->prev = plan->last_step;
    if (plan->last_step) {
        plan->last_step->next = step;
    } else {
        plan->steps = step;
    }
    plan->last_step = step;

    return step;
}

void step_free(ActionStep* step) {
    if (!step) return;

    free(step->description);
    free(step->target);
    free(step->content);
    free(step->reason);
    free(step->error_message);
    free(step->rollback.target);
    free(step->rollback.original_content);
    free(step->rollback.custom_command);

    free(step);
}

void plan_free(ActionPlan* plan) {
    if (!plan) return;

    /* Free all steps */
    ActionStep* step = plan->steps;
    while (step) {
        ActionStep* next = step->next;
        step_free(step);
        step = next;
    }

    free(plan->title);
    free(plan->description);
    free(plan->user_request);
    free(plan->error_message);

    free(plan);
}

/* ============================================================================
 * Plan Display
 * ============================================================================ */

void step_display(ActionStep* step, bool colors_enabled) {
    if (!step) return;

    const char* status_sym;
    const char* status_color = "";

    switch (step->status) {
        case STEP_PENDING:
            status_sym = SYM_PENDING;
            status_color = COLOR_DIM;
            break;
        case STEP_IN_PROGRESS:
            status_sym = SYM_RUNNING;
            status_color = COLOR_YELLOW;
            break;
        case STEP_COMPLETED:
            status_sym = SYM_CHECK;
            status_color = COLOR_GREEN;
            break;
        case STEP_FAILED:
            status_sym = SYM_CROSS;
            status_color = COLOR_RED;
            break;
        case STEP_SKIPPED:
            status_sym = "[-]";
            status_color = COLOR_DIM;
            break;
        case STEP_ROLLED_BACK:
            status_sym = "[R]";
            status_color = COLOR_YELLOW;
            break;
        default:
            status_sym = "[?]";
            status_color = "";
            break;
    }

    if (colors_enabled) {
        printf("  %s%s%s %s%d.%s %s",
               status_color, status_sym, COLOR_RESET,
               COLOR_CYAN, step->step_number, COLOR_RESET,
               step->description ? step->description : "No description");

        if (step->reason) {
            printf("\n      %s%s%s", COLOR_DIM, step->reason, COLOR_RESET);
        }
        printf("\n");

        if (step->status == STEP_FAILED && step->error_message) {
            printf("      %sError: %s%s\n", COLOR_RED, step->error_message, COLOR_RESET);
        }
    } else {
        printf("  %s %d. %s",
               status_sym, step->step_number,
               step->description ? step->description : "No description");

        if (step->reason) {
            printf("\n      %s", step->reason);
        }
        printf("\n");

        if (step->status == STEP_FAILED && step->error_message) {
            printf("      Error: %s\n", step->error_message);
        }
    }
}

void plan_display(ActionPlan* plan, bool colors_enabled) {
    if (!plan) return;

    if (colors_enabled) {
        printf("\n%s%s%s\n", COLOR_BOLD, plan->title, COLOR_RESET);
        if (plan->description) {
            printf("%s%s%s\n", COLOR_DIM, plan->description, COLOR_RESET);
        }
        printf("\n%sSteps:%s\n", COLOR_CYAN, COLOR_RESET);
    } else {
        printf("\n%s\n", plan->title);
        if (plan->description) {
            printf("%s\n", plan->description);
        }
        printf("\nSteps:\n");
    }

    for (ActionStep* step = plan->steps; step; step = step->next) {
        step_display(step, colors_enabled);
    }

    printf("\n");
}

void plan_display_progress(ActionPlan* plan, bool colors_enabled) {
    if (!plan) return;

    int pending = 0, completed = 0, failed = 0;
    for (ActionStep* s = plan->steps; s; s = s->next) {
        switch (s->status) {
            case STEP_COMPLETED: completed++; break;
            case STEP_FAILED: failed++; break;
            case STEP_PENDING:
            case STEP_IN_PROGRESS: pending++; break;
            default: break;
        }
    }

    if (colors_enabled) {
        printf("%sProgress: %s%d%s/%d completed",
               COLOR_DIM, COLOR_GREEN, completed, COLOR_DIM, plan->step_count);
        if (failed > 0) {
            printf(", %s%d failed%s", COLOR_RED, failed, COLOR_DIM);
        }
        printf("%s\n", COLOR_RESET);
    } else {
        printf("Progress: %d/%d completed", completed, plan->step_count);
        if (failed > 0) {
            printf(", %d failed", failed);
        }
        printf("\n");
    }
}

/* ============================================================================
 * Plan Approval
 * ============================================================================ */

ApprovalMode plan_request_approval(ActionPlan* plan, ReplSession* session) {
    if (!plan || !session) return APPROVAL_DENIED;

    bool colors = session->config.colors_enabled;

    /* Display the plan */
    plan_display(plan, colors);

    /* Ask for approval */
    if (colors) {
        printf("%s%s Execute this plan?%s\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
        printf("  [%sY%s]es - Execute all steps\n", COLOR_GREEN, COLOR_RESET);
        printf("  [%sS%s]tep - Execute step-by-step\n", COLOR_CYAN, COLOR_RESET);
        printf("  [%sN%s]o  - Cancel\n", COLOR_RED, COLOR_RESET);
        printf("\n%sChoice [Y/s/n]: %s", COLOR_BOLD, COLOR_RESET);
    } else {
        printf("Execute this plan?\n");
        printf("  [Y]es - Execute all steps\n");
        printf("  [S]tep - Execute step-by-step\n");
        printf("  [N]o  - Cancel\n");
        printf("\nChoice [Y/s/n]: ");
    }
    fflush(stdout);

    /* Read response */
    char buf[64];
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        return APPROVAL_DENIED;
    }

    /* Parse response */
    char c = tolower(buf[0]);
    if (c == '\n' || c == '\r' || c == 'y') {
        plan->state = PLAN_APPROVED;
        plan->approval_mode = APPROVAL_ALL;
        return APPROVAL_ALL;
    } else if (c == 's') {
        plan->state = PLAN_APPROVED;
        plan->approval_mode = APPROVAL_STEP_BY_STEP;
        return APPROVAL_STEP_BY_STEP;
    } else {
        plan->state = PLAN_ABORTED;
        plan->approval_mode = APPROVAL_DENIED;
        return APPROVAL_DENIED;
    }
}

bool step_request_approval(ActionStep* step, ReplSession* session) {
    if (!step || !session) return false;

    bool colors = session->config.colors_enabled;

    if (colors) {
        printf("\n%s%s Step %d:%s %s\n",
               COLOR_CYAN, SYM_ARROW, step->step_number, COLOR_RESET,
               step->description ? step->description : "");
        if (step->reason) {
            printf("  %s%s%s\n", COLOR_DIM, step->reason, COLOR_RESET);
        }
        printf("\n%sExecute? [Y/n/skip]: %s", COLOR_BOLD, COLOR_RESET);
    } else {
        printf("\n> Step %d: %s\n",
               step->step_number,
               step->description ? step->description : "");
        if (step->reason) {
            printf("  %s\n", step->reason);
        }
        printf("\nExecute? [Y/n/skip]: ");
    }
    fflush(stdout);

    char buf[64];
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        return false;
    }

    char c = tolower(buf[0]);
    if (c == 'n') {
        /* Abort entire plan */
        return false;
    } else if (c == 's') {
        /* Skip this step */
        step->status = STEP_SKIPPED;
        return true;  /* Continue with next step */
    }

    /* Yes or empty - execute */
    return true;
}

/* ============================================================================
 * Plan Execution
 * ============================================================================ */

PlanExecOptions plan_exec_options_default(void) {
    return (PlanExecOptions){
        .stop_on_failure = true,
        .auto_rollback = false,
        .verbose = false,
        .dry_run = false,
        .step_delay_ms = 0
    };
}

bool step_execute(ActionStep* step, ReplSession* session) {
    if (!step || !session) return false;

    step->status = STEP_IN_PROGRESS;
    step->started_at = time(NULL);

    bool success = false;

    switch (step->action) {
        case AI_ACTION_READ_FILE:
            if (step->target && file_exists(step->target)) {
                printf("\n");
                file_read_display(step->target, 50);
                /* Update session context */
                free(session->current_file);
                session->current_file = strdup(step->target);
                success = true;
            } else {
                step->error_message = strdup("File not found");
            }
            break;

        case AI_ACTION_CREATE_FILE:
            if (step->target) {
                /* Save original content for rollback if file exists */
                if (file_exists(step->target) && step->can_rollback) {
                    char* content = file_read(step->target, NULL);
                    if (content) {
                        free(step->rollback.original_content);
                        step->rollback.original_content = content;
                        step->rollback.type = ROLLBACK_RESTORE_FILE;
                    }
                }

                const char* content = step->content ? step->content : "";
                if (file_write(step->target, content)) {
                    success = true;
                } else {
                    step->error_message = strdup("Failed to create file");
                }
            }
            break;

        case AI_ACTION_DELETE_FILE:
            if (step->target) {
                if (file_delete(step->target)) {
                    success = true;
                } else {
                    step->error_message = strdup("Failed to delete file");
                }
            }
            break;

        case AI_ACTION_DELETE_DIR:
            if (step->target) {
                if (dir_delete_recursive(step->target)) {
                    success = true;
                } else {
                    step->error_message = strdup("Failed to delete directory");
                }
            }
            break;

        case AI_ACTION_BUILD:
            {
                const char* build_dir = step->target ? step->target : "build";
                char cmd[1024];
                snprintf(cmd, sizeof(cmd), "cmake --build %s", build_dir);
                BuildResult* result = build_execute_command(cmd, session->working_dir);

                if (result && result->success) {
                    success = true;
                } else {
                    if (result && result->stderr_output) {
                        step->error_message = strdup(result->stderr_output);
                    } else {
                        step->error_message = strdup("Build failed");
                    }
                }
                build_result_free(result);
            }
            break;

        case AI_ACTION_CLEAN:
            {
                const char* build_dir = step->target ? step->target : "build";
                if (dir_delete_recursive(build_dir)) {
                    success = true;
                } else {
                    step->error_message = strdup("Failed to clean");
                }
            }
            break;

        case AI_ACTION_INSTALL:
            if (step->target) {
                ToolRegistry* registry = tool_registry_create();
                if (registry) {
                    tool_discover_all(registry);
                    ToolExecResult* result = package_install(registry, step->target, NULL);
                    if (result && result->success) {
                        success = true;
                    } else {
                        step->error_message = strdup("Package installation failed");
                    }
                    tool_exec_result_free(result);
                    tool_registry_free(registry);
                } else {
                    step->error_message = strdup("Could not initialize package manager");
                }
            }
            break;

        case AI_ACTION_RUN_COMMAND:
            if (step->content) {
                int ret = system(step->content);
                if (ret == 0) {
                    success = true;
                } else {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Command exited with code %d", ret);
                    step->error_message = strdup(msg);
                }
            }
            break;

        case AI_ACTION_LIST_FILES:
            {
                const char* dir = step->target ? step->target : ".";
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
            success = true;
            break;
    }

    step->completed_at = time(NULL);
    step->status = success ? STEP_COMPLETED : STEP_FAILED;

    return success;
}

bool plan_execute(ActionPlan* plan, ReplSession* session, const PlanExecOptions* options) {
    if (!plan || !session) return false;

    PlanExecOptions opts = options ? *options : plan_exec_options_default();
    bool colors = session->config.colors_enabled;

    plan->state = PLAN_EXECUTING;
    plan->started_at = time(NULL);

    if (colors) {
        printf("\n%s%s Executing plan: %s%s\n\n",
               COLOR_CYAN, SYM_BULLET, plan->title, COLOR_RESET);
    } else {
        printf("\nExecuting plan: %s\n\n", plan->title);
    }

    bool all_success = true;

    for (ActionStep* step = plan->steps; step; step = step->next) {
        if (step->status == STEP_SKIPPED) {
            continue;
        }

        /* Step-by-step approval */
        if (plan->approval_mode == APPROVAL_STEP_BY_STEP) {
            if (!step_request_approval(step, session)) {
                if (step->status != STEP_SKIPPED) {
                    /* User said no - abort */
                    plan->state = PLAN_ABORTED;
                    all_success = false;
                    break;
                }
                continue;  /* Skipped, continue to next */
            }
            if (step->status == STEP_SKIPPED) {
                continue;
            }
        }

        /* Display step being executed */
        if (colors) {
            printf("%s%s Step %d:%s %s\n",
                   COLOR_CYAN, SYM_ARROW, step->step_number, COLOR_RESET,
                   step->description ? step->description : "");
        } else {
            printf("> Step %d: %s\n", step->step_number,
                   step->description ? step->description : "");
        }

        /* Check permission */
        ActionType perm_type = ACTION_READ_FILE;  /* Default */
        switch (step->action) {
            case AI_ACTION_CREATE_FILE: perm_type = ACTION_CREATE_FILE; break;
            case AI_ACTION_DELETE_FILE: perm_type = ACTION_DELETE_FILE; break;
            case AI_ACTION_DELETE_DIR:  perm_type = ACTION_DELETE_DIR; break;
            case AI_ACTION_INSTALL:     perm_type = ACTION_INSTALL_PKG; break;
            case AI_ACTION_RUN_COMMAND: perm_type = ACTION_RUN_COMMAND; break;
            default: break;
        }

        const char* target = step->target ? step->target : "action";
        if (!permission_check(session->permissions, perm_type, target,
                              step->reason ? step->reason : "Plan execution")) {
            step->status = STEP_SKIPPED;
            if (colors) {
                printf("  %s%s Permission denied - skipped%s\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
            } else {
                printf("  Permission denied - skipped\n");
            }
            continue;
        }

        /* Execute if not dry run */
        bool success;
        if (opts.dry_run) {
            if (colors) {
                printf("  %s[DRY RUN] Would execute%s\n", COLOR_DIM, COLOR_RESET);
            } else {
                printf("  [DRY RUN] Would execute\n");
            }
            step->status = STEP_COMPLETED;
            success = true;
        } else {
            success = step_execute(step, session);
        }

        /* Display result */
        if (success) {
            plan->completed_count++;
            if (colors) {
                printf("  %s%s Done%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
            } else {
                printf("  %s Done\n", SYM_CHECK);
            }
        } else {
            plan->failed_count++;
            if (colors) {
                printf("  %s%s Failed%s", COLOR_RED, SYM_CROSS, COLOR_RESET);
                if (step->error_message) {
                    printf(": %s", step->error_message);
                }
                printf("\n");
            } else {
                printf("  %s Failed", SYM_CROSS);
                if (step->error_message) {
                    printf(": %s", step->error_message);
                }
                printf("\n");
            }

            all_success = false;

            if (opts.stop_on_failure) {
                if (opts.auto_rollback) {
                    plan_rollback(plan, session);
                }
                break;
            }
        }

        /* Delay between steps if configured */
        if (opts.step_delay_ms > 0 && step->next) {
            sleep_ms(opts.step_delay_ms);
        }
    }

    plan->completed_at = time(NULL);
    plan->state = all_success ? PLAN_COMPLETED : PLAN_FAILED;

    /* Summary */
    printf("\n");
    plan_display_progress(plan, colors);

    return all_success;
}

/* ============================================================================
 * Rollback
 * ============================================================================ */

void step_set_rollback(ActionStep* step,
                       RollbackType type,
                       const char* target,
                       const char* original_content) {
    if (!step) return;

    step->rollback.type = type;
    free(step->rollback.target);
    step->rollback.target = target ? strdup(target) : NULL;
    free(step->rollback.original_content);
    step->rollback.original_content = original_content ? strdup(original_content) : NULL;
    step->can_rollback = (type != ROLLBACK_NONE);
}

bool step_rollback(ActionStep* step, ReplSession* session) {
    if (!step || !step->can_rollback) return false;
    if (step->status != STEP_COMPLETED) return false;

    (void)session;  /* May use for logging in future */

    bool success = false;

    switch (step->rollback.type) {
        case ROLLBACK_DELETE_FILE:
            if (step->rollback.target) {
                success = file_delete(step->rollback.target);
            }
            break;

        case ROLLBACK_RESTORE_FILE:
            if (step->rollback.target && step->rollback.original_content) {
                success = file_write(step->rollback.target, step->rollback.original_content);
            }
            break;

        case ROLLBACK_DELETE_DIR:
            if (step->rollback.target) {
                success = dir_delete_recursive(step->rollback.target);
            }
            break;

        case ROLLBACK_UNINSTALL:
            /* Package uninstall is complex and platform-specific */
            /* For now, just mark as not rolled back */
            success = false;
            break;

        case ROLLBACK_CUSTOM:
            if (step->rollback.custom_command) {
                success = (system(step->rollback.custom_command) == 0);
            }
            break;

        case ROLLBACK_NONE:
        default:
            success = false;
            break;
    }

    if (success) {
        step->status = STEP_ROLLED_BACK;
    }

    return success;
}

int plan_rollback(ActionPlan* plan, ReplSession* session) {
    if (!plan) return 0;

    bool colors = session ? session->config.colors_enabled : false;
    int rolled_back = 0;

    if (colors) {
        printf("\n%s%s Rolling back...%s\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
    } else {
        printf("\nRolling back...\n");
    }

    /* Rollback in reverse order */
    for (ActionStep* step = plan->last_step; step; step = step->prev) {
        if (step->status == STEP_COMPLETED && step->can_rollback) {
            if (colors) {
                printf("  Rolling back step %d: %s... ",
                       step->step_number, step->description ? step->description : "");
            } else {
                printf("  Rolling back step %d... ", step->step_number);
            }

            if (step_rollback(step, session)) {
                rolled_back++;
                if (colors) {
                    printf("%s%s%s\n", COLOR_GREEN, SYM_CHECK, COLOR_RESET);
                } else {
                    printf("%s\n", SYM_CHECK);
                }
            } else {
                if (colors) {
                    printf("%s%s (cannot rollback)%s\n", COLOR_YELLOW, SYM_WARN, COLOR_RESET);
                } else {
                    printf("%s (cannot rollback)\n", SYM_WARN);
                }
            }
        }
    }

    if (rolled_back > 0) {
        plan->state = PLAN_ROLLED_BACK;
    }

    if (colors) {
        printf("%sRolled back %d step(s)%s\n", COLOR_DIM, rolled_back, COLOR_RESET);
    } else {
        printf("Rolled back %d step(s)\n", rolled_back);
    }

    return rolled_back;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* step_status_name(StepStatus status) {
    switch (status) {
        case STEP_PENDING:     return "pending";
        case STEP_IN_PROGRESS: return "in progress";
        case STEP_COMPLETED:   return "completed";
        case STEP_FAILED:      return "failed";
        case STEP_SKIPPED:     return "skipped";
        case STEP_ROLLED_BACK: return "rolled back";
        default:               return "unknown";
    }
}

const char* plan_state_name(PlanState state) {
    switch (state) {
        case PLAN_CREATED:     return "created";
        case PLAN_APPROVED:    return "approved";
        case PLAN_EXECUTING:   return "executing";
        case PLAN_COMPLETED:   return "completed";
        case PLAN_FAILED:      return "failed";
        case PLAN_ABORTED:     return "aborted";
        case PLAN_ROLLED_BACK: return "rolled back";
        default:               return "unknown";
    }
}

bool plan_has_pending_steps(ActionPlan* plan) {
    if (!plan) return false;

    for (ActionStep* s = plan->steps; s; s = s->next) {
        if (s->status == STEP_PENDING) return true;
    }
    return false;
}

ActionStep* plan_get_next_pending(ActionPlan* plan) {
    if (!plan) return NULL;

    for (ActionStep* s = plan->steps; s; s = s->next) {
        if (s->status == STEP_PENDING) return s;
    }
    return NULL;
}

ActionStep* plan_get_step(ActionPlan* plan, int step_number) {
    if (!plan) return NULL;

    for (ActionStep* s = plan->steps; s; s = s->next) {
        if (s->step_number == step_number) return s;
    }
    return NULL;
}
