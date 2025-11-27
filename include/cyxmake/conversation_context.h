/**
 * @file conversation_context.h
 * @brief Conversation context management for REPL
 *
 * Stores message history, tracks context (current file, last error),
 * and provides context to LLM for better responses.
 */

#ifndef CYXMAKE_CONVERSATION_CONTEXT_H
#define CYXMAKE_CONVERSATION_CONTEXT_H

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Message roles in conversation
 */
typedef enum {
    MSG_ROLE_USER,      /* User input */
    MSG_ROLE_ASSISTANT, /* CyxMake response */
    MSG_ROLE_SYSTEM,    /* System messages (errors, status) */
    MSG_ROLE_TOOL       /* Tool execution output */
} MessageRole;

/**
 * Intent categories for context tracking
 */
typedef enum {
    CTX_INTENT_BUILD,
    CTX_INTENT_ANALYZE,
    CTX_INTENT_FILE_OP,
    CTX_INTENT_INSTALL,
    CTX_INTENT_FIX,
    CTX_INTENT_EXPLAIN,
    CTX_INTENT_OTHER
} ContextIntent;

/**
 * A single message in the conversation
 */
typedef struct {
    MessageRole role;
    char* content;          /* Message text */
    char* target;           /* Target file/package/etc (optional) */
    ContextIntent intent;   /* Detected intent */
    time_t timestamp;       /* When message was added */
    bool success;           /* Whether action succeeded (for assistant/tool) */
} ConversationMessage;

/**
 * File context - currently active file
 */
typedef struct {
    char* path;             /* Full path to file */
    char* content_preview;  /* First N lines for context */
    int line_count;         /* Total lines in file */
    time_t last_accessed;   /* When file was last read */
} FileContext;

/**
 * Error context - last error for "fix it" commands
 */
typedef struct {
    char* message;          /* Error message */
    char* source;           /* Source of error (build, tool, etc) */
    char* file;             /* Related file (if applicable) */
    int line;               /* Line number (if applicable) */
    char* suggested_fix;    /* AI-generated fix suggestion */
    time_t timestamp;
} ErrorContext;

/**
 * Project context snapshot
 */
typedef struct {
    char* project_type;     /* CMake, Make, Cargo, etc */
    char* working_dir;      /* Current working directory */
    char** recent_files;    /* Recently accessed files */
    int recent_files_count;
    char** pending_actions; /* Actions queued for execution */
    int pending_actions_count;
} ProjectSnapshot;

/**
 * Main conversation context
 */
struct ConversationContext {
    /* Message history */
    ConversationMessage* messages;
    int message_count;
    int message_capacity;
    int max_messages;       /* Max messages to keep (0 = unlimited) */

    /* Current context */
    FileContext* current_file;
    ErrorContext* last_error;
    ProjectSnapshot* project;

    /* Context settings */
    int context_window_size;    /* Number of recent messages to include */
    bool include_file_content;  /* Include file previews in context */
    bool include_tool_output;   /* Include tool output in context */
};

typedef struct ConversationContext ConversationContext;

/**
 * Create a new conversation context
 * @param max_messages Maximum messages to store (0 = unlimited)
 * @return New context or NULL on error
 */
ConversationContext* conversation_context_create(int max_messages);

/**
 * Free conversation context
 * @param ctx Context to free
 */
void conversation_context_free(ConversationContext* ctx);

/**
 * Add a message to conversation history
 * @param ctx Conversation context
 * @param role Message role
 * @param content Message content
 * @param intent Detected intent
 * @param target Target file/package (can be NULL)
 * @param success Whether action succeeded
 */
void conversation_add_message(ConversationContext* ctx,
                               MessageRole role,
                               const char* content,
                               ContextIntent intent,
                               const char* target,
                               bool success);

/**
 * Set current file context
 * @param ctx Conversation context
 * @param path File path
 * @param preview First N lines (can be NULL)
 * @param line_count Total lines in file
 */
void conversation_set_file(ConversationContext* ctx,
                            const char* path,
                            const char* preview,
                            int line_count);

/**
 * Set last error context
 * @param ctx Conversation context
 * @param message Error message
 * @param source Error source
 * @param file Related file (can be NULL)
 * @param line Line number (-1 if not applicable)
 */
void conversation_set_error(ConversationContext* ctx,
                             const char* message,
                             const char* source,
                             const char* file,
                             int line);

/**
 * Clear last error
 * @param ctx Conversation context
 */
void conversation_clear_error(ConversationContext* ctx);

/**
 * Get recent messages for LLM context
 * @param ctx Conversation context
 * @param count Number of messages to get (0 = use context_window_size)
 * @return Formatted string for LLM (caller must free)
 */
char* conversation_get_context_string(ConversationContext* ctx, int count);

/**
 * Get context summary for display
 * @param ctx Conversation context
 * @return Formatted summary string (caller must free)
 */
char* conversation_get_summary(ConversationContext* ctx);

/**
 * Check if user is referring to "the file" or "current file"
 * @param ctx Conversation context
 * @return Current file path or NULL
 */
const char* conversation_get_current_file(ConversationContext* ctx);

/**
 * Check if user is referring to "the error" or "last error"
 * @param ctx Conversation context
 * @return Last error message or NULL
 */
const char* conversation_get_last_error(ConversationContext* ctx);

/**
 * Detect if input refers to previous context
 * (e.g., "fix it", "show the file", "what was the error")
 * @param ctx Conversation context
 * @param input User input
 * @param resolved_target Output: resolved target (caller must free)
 * @return true if reference was resolved
 */
bool conversation_resolve_reference(ConversationContext* ctx,
                                     const char* input,
                                     char** resolved_target);

/**
 * Get message role name for display
 * @param role Message role
 * @return Role name string
 */
const char* message_role_name(MessageRole role);

/**
 * Get intent name for display
 * @param intent Context intent
 * @return Intent name string
 */
const char* context_intent_name(ContextIntent intent);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_CONVERSATION_CONTEXT_H */
