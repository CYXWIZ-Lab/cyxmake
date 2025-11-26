/**
 * @file permission.c
 * @brief Permission system implementation for REPL actions
 */

#include "cyxmake/permission.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

/* ANSI color codes */
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"

/* Status symbols (ASCII for compatibility) */
#define SYM_WARN    "[!]"
#define SYM_LOCK    "[#]"

/* Default blocked paths (system directories) */
static const char* default_blocked_paths[] = {
#ifdef _WIN32
    "C:\\Windows",
    "C:\\Program Files",
    "C:\\Program Files (x86)",
#else
    "/usr",
    "/bin",
    "/sbin",
    "/etc",
    "/var",
    "/boot",
    "/root",
#endif
    NULL
};

/* Create default permission context */
PermissionContext* permission_context_create(void) {
    PermissionContext* ctx = calloc(1, sizeof(PermissionContext));
    if (!ctx) return NULL;

    /* Safe defaults - auto-approve only read operations */
    ctx->auto_approve_read = true;
    ctx->auto_approve_build = true;
    ctx->auto_approve_clean = false;
    ctx->auto_approve_create = false;
    ctx->auto_approve_modify = false;
    ctx->auto_approve_delete = false;
    ctx->auto_approve_install = false;
    ctx->auto_approve_command = false;

    ctx->colors_enabled = true;
    ctx->audit_callback = NULL;

    /* Copy default blocked paths */
    int count = 0;
    while (default_blocked_paths[count]) count++;

    ctx->blocked_paths = calloc(count + 1, sizeof(char*));
    ctx->blocked_count = count;
    for (int i = 0; i < count; i++) {
        ctx->blocked_paths[i] = strdup(default_blocked_paths[i]);
    }

    return ctx;
}

/* Free permission context */
void permission_context_free(PermissionContext* ctx) {
    if (!ctx) return;

    if (ctx->blocked_paths) {
        for (int i = 0; i < ctx->blocked_count; i++) {
            free((void*)ctx->blocked_paths[i]);
        }
        free(ctx->blocked_paths);
    }

    free(ctx);
}

/* Get permission level for an action type */
PermissionLevel permission_get_level(ActionType action) {
    switch (action) {
        case ACTION_READ_FILE:
        case ACTION_BUILD:
        case ACTION_ANALYZE:
        case ACTION_STATUS:
            return PERM_SAFE;

        case ACTION_CLEAN:
        case ACTION_CREATE_FILE:
        case ACTION_MODIFY_FILE:
        case ACTION_INSTALL_PKG:
        case ACTION_RUN_COMMAND:
            return PERM_ASK;

        case ACTION_DELETE_FILE:
        case ACTION_DELETE_DIR:
        case ACTION_SYSTEM_MODIFY:
            return PERM_DANGEROUS;

        default:
            return PERM_ASK;
    }
}

/* Check if action requires permission */
bool permission_needs_prompt(PermissionContext* ctx, ActionType action) {
    if (!ctx) return true;

    switch (action) {
        case ACTION_READ_FILE:
            return !ctx->auto_approve_read;
        case ACTION_BUILD:
        case ACTION_ANALYZE:
        case ACTION_STATUS:
            return !ctx->auto_approve_build;
        case ACTION_CLEAN:
            return !ctx->auto_approve_clean;
        case ACTION_CREATE_FILE:
            return !ctx->auto_approve_create;
        case ACTION_MODIFY_FILE:
            return !ctx->auto_approve_modify;
        case ACTION_DELETE_FILE:
        case ACTION_DELETE_DIR:
            return !ctx->auto_approve_delete;
        case ACTION_INSTALL_PKG:
            return !ctx->auto_approve_install;
        case ACTION_RUN_COMMAND:
            return !ctx->auto_approve_command;
        case ACTION_SYSTEM_MODIFY:
            return true;  /* Always ask for system modifications */
        default:
            return true;
    }
}

/* Get action type name for display */
const char* permission_action_name(ActionType action) {
    switch (action) {
        case ACTION_READ_FILE:      return "Read file";
        case ACTION_BUILD:          return "Build project";
        case ACTION_ANALYZE:        return "Analyze project";
        case ACTION_STATUS:         return "Show status";
        case ACTION_CLEAN:          return "Clean build";
        case ACTION_CREATE_FILE:    return "Create file";
        case ACTION_MODIFY_FILE:    return "Modify file";
        case ACTION_DELETE_FILE:    return "Delete file";
        case ACTION_DELETE_DIR:     return "Delete directory";
        case ACTION_INSTALL_PKG:    return "Install package";
        case ACTION_RUN_COMMAND:    return "Run command";
        case ACTION_SYSTEM_MODIFY:  return "Modify system";
        default:                    return "Unknown action";
    }
}

/* Read single character without echo */
static int read_char(void) {
#ifdef _WIN32
    /* Check if stdin is a console or piped */
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    if (GetConsoleMode(hStdin, &mode)) {
        /* Interactive console - use _getch() */
        return _getch();
    } else {
        /* Piped input - use getchar() */
        return getchar();
    }
#else
    struct termios old_term, new_term;
    int ch;

    /* Check if stdin is a tty */
    if (!isatty(STDIN_FILENO)) {
        return getchar();
    }

    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;
    new_term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    return ch;
#endif
}

/* Print permission prompt box */
static void print_permission_prompt(PermissionContext* ctx, const PermissionRequest* request,
                                     PermissionLevel level) {
    if (ctx->colors_enabled) {
        printf("\n");

        /* Header based on level */
        if (level == PERM_DANGEROUS) {
            printf("%s%s%s DANGEROUS ACTION%s\n", COLOR_BOLD, COLOR_RED, SYM_WARN, COLOR_RESET);
            printf("%s+----------------------------------------------+%s\n", COLOR_RED, COLOR_RESET);
        } else {
            printf("%s%s%s Permission Required%s\n", COLOR_BOLD, COLOR_YELLOW, SYM_WARN, COLOR_RESET);
            printf("%s+----------------------------------------------+%s\n", COLOR_YELLOW, COLOR_RESET);
        }

        /* Action and target */
        printf("%s| %sAction:%s  %-36s %s|%s\n",
               level == PERM_DANGEROUS ? COLOR_RED : COLOR_YELLOW,
               COLOR_BOLD, COLOR_RESET,
               permission_action_name(request->action),
               level == PERM_DANGEROUS ? COLOR_RED : COLOR_YELLOW,
               COLOR_RESET);

        if (request->target) {
            printf("%s| %sTarget:%s  %-36s %s|%s\n",
                   level == PERM_DANGEROUS ? COLOR_RED : COLOR_YELLOW,
                   COLOR_CYAN, COLOR_RESET,
                   request->target,
                   level == PERM_DANGEROUS ? COLOR_RED : COLOR_YELLOW,
                   COLOR_RESET);
        }

        if (request->reason) {
            printf("%s| %sReason:%s  %-36s %s|%s\n",
                   level == PERM_DANGEROUS ? COLOR_RED : COLOR_YELLOW,
                   COLOR_DIM, COLOR_RESET,
                   request->reason,
                   level == PERM_DANGEROUS ? COLOR_RED : COLOR_YELLOW,
                   COLOR_RESET);
        }

        printf("%s+----------------------------------------------+%s\n",
               level == PERM_DANGEROUS ? COLOR_RED : COLOR_YELLOW, COLOR_RESET);

        /* Options */
        printf("%s[%sY%s]es  [%sN%s]o  [%sA%s]lways  [%sV%s]iew  [%s?%s]Help%s\n",
               COLOR_DIM,
               COLOR_GREEN, COLOR_DIM,
               COLOR_RED, COLOR_DIM,
               COLOR_CYAN, COLOR_DIM,
               COLOR_BLUE, COLOR_DIM,
               COLOR_YELLOW, COLOR_DIM,
               COLOR_RESET);

        printf("%sChoice:%s ", COLOR_BOLD, COLOR_RESET);
    } else {
        printf("\n");

        if (level == PERM_DANGEROUS) {
            printf("%s DANGEROUS ACTION\n", SYM_WARN);
            printf("+----------------------------------------------+\n");
        } else {
            printf("%s Permission Required\n", SYM_WARN);
            printf("+----------------------------------------------+\n");
        }

        printf("| Action:  %-36s |\n", permission_action_name(request->action));

        if (request->target) {
            printf("| Target:  %-36s |\n", request->target);
        }

        if (request->reason) {
            printf("| Reason:  %-36s |\n", request->reason);
        }

        printf("+----------------------------------------------+\n");
        printf("[Y]es  [N]o  [A]lways  [V]iew  [?]Help\n");
        printf("Choice: ");
    }

    fflush(stdout);
}

/* Print help for permission prompt */
static void print_permission_help(PermissionContext* ctx) {
    if (ctx->colors_enabled) {
        printf("\n\n%sPermission Options:%s\n", COLOR_BOLD, COLOR_RESET);
        printf("  %sY%s / %sEnter%s - Allow this action\n", COLOR_GREEN, COLOR_RESET, COLOR_GREEN, COLOR_RESET);
        printf("  %sN%s         - Deny this action\n", COLOR_RED, COLOR_RESET);
        printf("  %sA%s         - Always allow this type of action\n", COLOR_CYAN, COLOR_RESET);
        printf("  %sV%s         - View more details\n", COLOR_BLUE, COLOR_RESET);
        printf("  %s?%s         - Show this help\n\n", COLOR_YELLOW, COLOR_RESET);
    } else {
        printf("\n\nPermission Options:\n");
        printf("  Y / Enter - Allow this action\n");
        printf("  N         - Deny this action\n");
        printf("  A         - Always allow this type of action\n");
        printf("  V         - View more details\n");
        printf("  ?         - Show this help\n\n");
    }
}

/* Print details view */
static void print_permission_details(PermissionContext* ctx, const PermissionRequest* request) {
    if (ctx->colors_enabled) {
        printf("\n\n%sDetails:%s\n", COLOR_BOLD, COLOR_RESET);
    } else {
        printf("\n\nDetails:\n");
    }

    printf("  Action type: %s\n", permission_action_name(request->action));
    printf("  Permission level: %s\n",
           permission_get_level(request->action) == PERM_SAFE ? "Safe" :
           permission_get_level(request->action) == PERM_ASK ? "Requires approval" :
           permission_get_level(request->action) == PERM_DANGEROUS ? "DANGEROUS" : "Blocked");

    if (request->target) {
        printf("  Target: %s\n", request->target);
    }

    if (request->reason) {
        printf("  Reason: %s\n", request->reason);
    }

    if (request->details) {
        printf("  Additional info:\n%s\n", request->details);
    }

    printf("\n");
}

/* Request permission from user */
PermissionResponse permission_request(PermissionContext* ctx, const PermissionRequest* request) {
    if (!ctx || !request) return PERM_RESPONSE_NO;

    PermissionLevel level = permission_get_level(request->action);

    /* Check if blocked */
    if (request->target && permission_is_blocked(ctx, request->target)) {
        if (ctx->colors_enabled) {
            printf("\n%s%s Blocked:%s %s is in a protected location%s\n",
                   COLOR_RED, SYM_LOCK, COLOR_RESET, request->target, COLOR_RESET);
        } else {
            printf("\n%s Blocked: %s is in a protected location\n", SYM_LOCK, request->target);
        }
        return PERM_RESPONSE_NO;
    }

    /* Check if auto-approved */
    if (!permission_needs_prompt(ctx, request->action)) {
        return PERM_RESPONSE_YES;
    }

    /* Show prompt and get response */
    while (1) {
        print_permission_prompt(ctx, request, level);

        int ch = read_char();
        printf("%c\n", ch);

        switch (tolower(ch)) {
            case 'y':
            case '\r':
            case '\n':
                /* Log if audit enabled */
                if (ctx->audit_callback) {
                    ctx->audit_callback(request, PERM_RESPONSE_YES);
                }
                return PERM_RESPONSE_YES;

            case 'n':
                if (ctx->audit_callback) {
                    ctx->audit_callback(request, PERM_RESPONSE_NO);
                }
                return PERM_RESPONSE_NO;

            case 'a':
                permission_set_auto_approve(ctx, request->action, true);
                if (ctx->colors_enabled) {
                    printf("%sAuto-approve enabled for: %s%s\n",
                           COLOR_GREEN, permission_action_name(request->action), COLOR_RESET);
                } else {
                    printf("Auto-approve enabled for: %s\n", permission_action_name(request->action));
                }
                if (ctx->audit_callback) {
                    ctx->audit_callback(request, PERM_RESPONSE_ALWAYS);
                }
                return PERM_RESPONSE_YES;

            case 'v':
                print_permission_details(ctx, request);
                continue;

            case '?':
                print_permission_help(ctx);
                continue;

            default:
                if (ctx->colors_enabled) {
                    printf("%sInvalid option. Press Y, N, A, V, or ?%s\n", COLOR_DIM, COLOR_RESET);
                } else {
                    printf("Invalid option. Press Y, N, A, V, or ?\n");
                }
                continue;
        }
    }
}

/* Quick permission check with prompt */
bool permission_check(PermissionContext* ctx, ActionType action,
                      const char* target, const char* reason) {
    PermissionRequest request = {
        .action = action,
        .description = permission_action_name(action),
        .target = target,
        .reason = reason,
        .details = NULL
    };

    PermissionResponse response = permission_request(ctx, &request);
    return (response == PERM_RESPONSE_YES);
}

/* Check if a path is blocked */
bool permission_is_blocked(PermissionContext* ctx, const char* path) {
    if (!ctx || !path || !ctx->blocked_paths) return false;

    for (int i = 0; i < ctx->blocked_count; i++) {
        if (ctx->blocked_paths[i]) {
            /* Check if path starts with blocked path */
            size_t blocked_len = strlen(ctx->blocked_paths[i]);
            if (strncmp(path, ctx->blocked_paths[i], blocked_len) == 0) {
                /* Make sure it's a directory boundary */
                char next = path[blocked_len];
                if (next == '\0' || next == '/' || next == '\\') {
                    return true;
                }
            }
        }
    }

    return false;
}

/* Add a blocked path */
void permission_block_path(PermissionContext* ctx, const char* path) {
    if (!ctx || !path) return;

    /* Reallocate blocked paths array */
    const char** new_paths = realloc(ctx->blocked_paths,
                                      (ctx->blocked_count + 2) * sizeof(char*));
    if (!new_paths) return;

    ctx->blocked_paths = new_paths;
    ctx->blocked_paths[ctx->blocked_count] = strdup(path);
    ctx->blocked_count++;
    ctx->blocked_paths[ctx->blocked_count] = NULL;
}

/* Update auto-approve setting for action */
void permission_set_auto_approve(PermissionContext* ctx, ActionType action, bool auto_approve) {
    if (!ctx) return;

    switch (action) {
        case ACTION_READ_FILE:
            ctx->auto_approve_read = auto_approve;
            break;
        case ACTION_BUILD:
        case ACTION_ANALYZE:
        case ACTION_STATUS:
            ctx->auto_approve_build = auto_approve;
            break;
        case ACTION_CLEAN:
            ctx->auto_approve_clean = auto_approve;
            break;
        case ACTION_CREATE_FILE:
            ctx->auto_approve_create = auto_approve;
            break;
        case ACTION_MODIFY_FILE:
            ctx->auto_approve_modify = auto_approve;
            break;
        case ACTION_DELETE_FILE:
        case ACTION_DELETE_DIR:
            ctx->auto_approve_delete = auto_approve;
            break;
        case ACTION_INSTALL_PKG:
            ctx->auto_approve_install = auto_approve;
            break;
        case ACTION_RUN_COMMAND:
            ctx->auto_approve_command = auto_approve;
            break;
        default:
            break;
    }
}
