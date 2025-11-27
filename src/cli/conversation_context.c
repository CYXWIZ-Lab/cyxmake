/**
 * @file conversation_context.c
 * @brief Conversation context implementation
 */

#include "cyxmake/conversation_context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_CAPACITY 64
#define DEFAULT_CONTEXT_WINDOW 10
#define FILE_PREVIEW_LINES 20

/* Helper to duplicate strings safely */
static char* safe_strdup(const char* s) {
    return s ? strdup(s) : NULL;
}

/* Free a single message */
static void free_message(ConversationMessage* msg) {
    if (!msg) return;
    free(msg->content);
    free(msg->target);
    msg->content = NULL;
    msg->target = NULL;
}

/* Free file context */
static void free_file_context(FileContext* fc) {
    if (!fc) return;
    free(fc->path);
    free(fc->content_preview);
    free(fc);
}

/* Free error context */
static void free_error_context(ErrorContext* ec) {
    if (!ec) return;
    free(ec->message);
    free(ec->source);
    free(ec->file);
    free(ec->suggested_fix);
    free(ec);
}

/* Free project snapshot */
static void free_project_snapshot(ProjectSnapshot* ps) {
    if (!ps) return;
    free(ps->project_type);
    free(ps->working_dir);

    if (ps->recent_files) {
        for (int i = 0; i < ps->recent_files_count; i++) {
            free(ps->recent_files[i]);
        }
        free(ps->recent_files);
    }

    if (ps->pending_actions) {
        for (int i = 0; i < ps->pending_actions_count; i++) {
            free(ps->pending_actions[i]);
        }
        free(ps->pending_actions);
    }

    free(ps);
}

/* Create conversation context */
ConversationContext* conversation_context_create(int max_messages) {
    ConversationContext* ctx = calloc(1, sizeof(ConversationContext));
    if (!ctx) return NULL;

    ctx->message_capacity = DEFAULT_CAPACITY;
    ctx->max_messages = max_messages > 0 ? max_messages : 0;
    ctx->context_window_size = DEFAULT_CONTEXT_WINDOW;
    ctx->include_file_content = true;
    ctx->include_tool_output = true;

    ctx->messages = calloc(ctx->message_capacity, sizeof(ConversationMessage));
    if (!ctx->messages) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

/* Free conversation context */
void conversation_context_free(ConversationContext* ctx) {
    if (!ctx) return;

    /* Free all messages */
    if (ctx->messages) {
        for (int i = 0; i < ctx->message_count; i++) {
            free_message(&ctx->messages[i]);
        }
        free(ctx->messages);
    }

    free_file_context(ctx->current_file);
    free_error_context(ctx->last_error);
    free_project_snapshot(ctx->project);

    free(ctx);
}

/* Add message to history */
void conversation_add_message(ConversationContext* ctx,
                               MessageRole role,
                               const char* content,
                               ContextIntent intent,
                               const char* target,
                               bool success) {
    if (!ctx || !content) return;

    /* Check if we need to grow capacity */
    if (ctx->message_count >= ctx->message_capacity) {
        /* If max_messages is set, remove oldest messages */
        if (ctx->max_messages > 0 && ctx->message_count >= ctx->max_messages) {
            /* Remove oldest message */
            free_message(&ctx->messages[0]);
            memmove(ctx->messages, ctx->messages + 1,
                    (ctx->message_count - 1) * sizeof(ConversationMessage));
            ctx->message_count--;
        } else {
            /* Grow capacity */
            int new_capacity = ctx->message_capacity * 2;
            ConversationMessage* new_msgs = realloc(ctx->messages,
                                                     new_capacity * sizeof(ConversationMessage));
            if (!new_msgs) return;
            ctx->messages = new_msgs;
            ctx->message_capacity = new_capacity;
        }
    }

    /* Add new message */
    ConversationMessage* msg = &ctx->messages[ctx->message_count];
    msg->role = role;
    msg->content = safe_strdup(content);
    msg->target = safe_strdup(target);
    msg->intent = intent;
    msg->timestamp = time(NULL);
    msg->success = success;

    ctx->message_count++;
}

/* Set current file context */
void conversation_set_file(ConversationContext* ctx,
                            const char* path,
                            const char* preview,
                            int line_count) {
    if (!ctx) return;

    /* Free existing file context */
    free_file_context(ctx->current_file);
    ctx->current_file = NULL;

    if (!path) return;

    ctx->current_file = calloc(1, sizeof(FileContext));
    if (!ctx->current_file) return;

    ctx->current_file->path = safe_strdup(path);
    ctx->current_file->content_preview = safe_strdup(preview);
    ctx->current_file->line_count = line_count;
    ctx->current_file->last_accessed = time(NULL);
}

/* Set last error context */
void conversation_set_error(ConversationContext* ctx,
                             const char* message,
                             const char* source,
                             const char* file,
                             int line) {
    if (!ctx) return;

    /* Free existing error context */
    free_error_context(ctx->last_error);
    ctx->last_error = NULL;

    if (!message) return;

    ctx->last_error = calloc(1, sizeof(ErrorContext));
    if (!ctx->last_error) return;

    ctx->last_error->message = safe_strdup(message);
    ctx->last_error->source = safe_strdup(source);
    ctx->last_error->file = safe_strdup(file);
    ctx->last_error->line = line;
    ctx->last_error->timestamp = time(NULL);
}

/* Clear last error */
void conversation_clear_error(ConversationContext* ctx) {
    if (!ctx) return;
    free_error_context(ctx->last_error);
    ctx->last_error = NULL;
}

/* Get context string for LLM */
char* conversation_get_context_string(ConversationContext* ctx, int count) {
    if (!ctx) return NULL;

    int window = count > 0 ? count : ctx->context_window_size;
    int start = ctx->message_count > window ? ctx->message_count - window : 0;

    /* Calculate buffer size */
    size_t buffer_size = 4096;
    char* buffer = malloc(buffer_size);
    if (!buffer) return NULL;
    buffer[0] = '\0';
    size_t offset = 0;

    /* Add current file context if available */
    if (ctx->current_file && ctx->include_file_content) {
        int written = snprintf(buffer + offset, buffer_size - offset,
                                "[Current file: %s (%d lines)]\n",
                                ctx->current_file->path,
                                ctx->current_file->line_count);
        if (written > 0) offset += written;
    }

    /* Add last error if available */
    if (ctx->last_error) {
        int written = snprintf(buffer + offset, buffer_size - offset,
                                "[Last error: %s]\n",
                                ctx->last_error->message);
        if (written > 0) offset += written;
    }

    /* Add recent messages */
    for (int i = start; i < ctx->message_count; i++) {
        ConversationMessage* msg = &ctx->messages[i];
        const char* role = message_role_name(msg->role);

        int written = snprintf(buffer + offset, buffer_size - offset,
                                "[%s]: %s\n",
                                role, msg->content);
        if (written > 0) offset += written;

        /* Ensure we don't overflow */
        if (offset >= buffer_size - 256) break;
    }

    return buffer;
}

/* Get context summary */
char* conversation_get_summary(ConversationContext* ctx) {
    if (!ctx) return NULL;

    size_t buffer_size = 2048;
    char* buffer = malloc(buffer_size);
    if (!buffer) return NULL;

    size_t offset = 0;

    offset += snprintf(buffer + offset, buffer_size - offset,
                        "Conversation Context\n");
    offset += snprintf(buffer + offset, buffer_size - offset,
                        "====================\n\n");

    /* Message stats */
    offset += snprintf(buffer + offset, buffer_size - offset,
                        "Messages: %d\n", ctx->message_count);

    /* Current file */
    if (ctx->current_file) {
        offset += snprintf(buffer + offset, buffer_size - offset,
                            "Current file: %s (%d lines)\n",
                            ctx->current_file->path,
                            ctx->current_file->line_count);
    } else {
        offset += snprintf(buffer + offset, buffer_size - offset,
                            "Current file: (none)\n");
    }

    /* Last error */
    if (ctx->last_error) {
        offset += snprintf(buffer + offset, buffer_size - offset,
                            "Last error: %s\n", ctx->last_error->message);
        if (ctx->last_error->file) {
            offset += snprintf(buffer + offset, buffer_size - offset,
                                "  in %s", ctx->last_error->file);
            if (ctx->last_error->line > 0) {
                offset += snprintf(buffer + offset, buffer_size - offset,
                                    ":%d", ctx->last_error->line);
            }
            offset += snprintf(buffer + offset, buffer_size - offset, "\n");
        }
    } else {
        offset += snprintf(buffer + offset, buffer_size - offset,
                            "Last error: (none)\n");
    }

    /* Recent intents */
    offset += snprintf(buffer + offset, buffer_size - offset,
                        "\nRecent activity:\n");

    int show_count = ctx->message_count < 5 ? ctx->message_count : 5;
    int start = ctx->message_count - show_count;

    for (int i = start; i < ctx->message_count; i++) {
        ConversationMessage* msg = &ctx->messages[i];
        if (msg->role == MSG_ROLE_USER) {
            /* Truncate long messages */
            char preview[64];
            strncpy(preview, msg->content, 60);
            preview[60] = '\0';
            if (strlen(msg->content) > 60) {
                strcat(preview, "...");
            }
            offset += snprintf(buffer + offset, buffer_size - offset,
                                "  - %s\n", preview);
        }
    }

    return buffer;
}

/* Get current file path */
const char* conversation_get_current_file(ConversationContext* ctx) {
    if (!ctx || !ctx->current_file) return NULL;
    return ctx->current_file->path;
}

/* Get last error message */
const char* conversation_get_last_error(ConversationContext* ctx) {
    if (!ctx || !ctx->last_error) return NULL;
    return ctx->last_error->message;
}

/* Resolve context references */
bool conversation_resolve_reference(ConversationContext* ctx,
                                     const char* input,
                                     char** resolved_target) {
    if (!ctx || !input || !resolved_target) return false;
    *resolved_target = NULL;

    /* Check for file references */
    if (strstr(input, "the file") || strstr(input, "that file") ||
        strstr(input, "this file") || strstr(input, "current file")) {
        if (ctx->current_file && ctx->current_file->path) {
            *resolved_target = strdup(ctx->current_file->path);
            return true;
        }
    }

    /* Check for error references */
    if (strstr(input, "the error") || strstr(input, "that error") ||
        strstr(input, "this error") || strstr(input, "last error") ||
        strstr(input, "fix it") || strstr(input, "fix that")) {
        if (ctx->last_error && ctx->last_error->message) {
            *resolved_target = strdup(ctx->last_error->message);
            return true;
        }
    }

    /* Check for "it" pronoun with recent file context */
    if (strstr(input, "read it") || strstr(input, "show it") ||
        strstr(input, "open it") || strstr(input, "edit it")) {
        if (ctx->current_file && ctx->current_file->path) {
            *resolved_target = strdup(ctx->current_file->path);
            return true;
        }
    }

    return false;
}

/* Get role name */
const char* message_role_name(MessageRole role) {
    switch (role) {
        case MSG_ROLE_USER:      return "User";
        case MSG_ROLE_ASSISTANT: return "Assistant";
        case MSG_ROLE_SYSTEM:    return "System";
        case MSG_ROLE_TOOL:      return "Tool";
        default:                 return "Unknown";
    }
}

/* Get intent name */
const char* context_intent_name(ContextIntent intent) {
    switch (intent) {
        case CTX_INTENT_BUILD:   return "Build";
        case CTX_INTENT_ANALYZE: return "Analyze";
        case CTX_INTENT_FILE_OP: return "File operation";
        case CTX_INTENT_INSTALL: return "Install";
        case CTX_INTENT_FIX:     return "Fix";
        case CTX_INTENT_EXPLAIN: return "Explain";
        case CTX_INTENT_OTHER:
        default:                 return "Other";
    }
}
