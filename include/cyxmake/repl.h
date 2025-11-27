/**
 * @file repl.h
 * @brief Interactive REPL (Read-Eval-Print-Loop) for CyxMake
 */

#ifndef CYXMAKE_REPL_H
#define CYXMAKE_REPL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct Orchestrator Orchestrator;
typedef struct LLMContext LLMContext;
typedef struct PermissionContext PermissionContext;
typedef struct ConversationContext ConversationContext;
typedef struct AIProviderRegistry AIProviderRegistry;
typedef struct AIProvider AIProvider;

/**
 * REPL configuration
 */
typedef struct {
    const char* prompt;          /* Prompt string (default: "cyxmake> ") */
    bool colors_enabled;         /* Use colored output */
    bool show_welcome;           /* Show welcome message on start */
    int history_size;            /* Max history entries (0 = no limit) */
    bool verbose;                /* Verbose output mode */
} ReplConfig;

/**
 * REPL session state
 */
typedef struct ReplSession {
    ReplConfig config;
    Orchestrator* orchestrator;
    LLMContext* llm;
    PermissionContext* permissions;      /* Permission system */
    ConversationContext* conversation;   /* Conversation context */
    AIProviderRegistry* ai_registry;     /* Multi-provider AI registry */
    AIProvider* current_provider;        /* Currently active AI provider */

    /* Session state */
    bool running;
    int command_count;

    /* History */
    char** history;
    int history_count;
    int history_capacity;

    /* Current context */
    char* working_dir;
    char* last_error;
    char* current_file;
} ReplSession;

/**
 * Create default REPL configuration
 * @return Default configuration
 */
ReplConfig repl_config_default(void);

/**
 * Create a new REPL session
 * @param config Configuration (NULL for defaults)
 * @param orch Orchestrator instance (can be NULL, will create one)
 * @return New session or NULL on error
 */
ReplSession* repl_session_create(const ReplConfig* config, Orchestrator* orch);

/**
 * Free a REPL session
 * @param session Session to free
 */
void repl_session_free(ReplSession* session);

/**
 * Run the REPL loop (blocking)
 * @param session REPL session
 * @return Exit code (0 = normal exit)
 */
int repl_run(ReplSession* session);

/**
 * Process a single input line
 * @param session REPL session
 * @param input User input string
 * @return true to continue, false to exit
 */
bool repl_process_input(ReplSession* session, const char* input);

/**
 * Add entry to history
 * @param session REPL session
 * @param input Input to add
 */
void repl_history_add(ReplSession* session, const char* input);

/**
 * Print the welcome banner
 * @param session REPL session
 */
void repl_print_welcome(ReplSession* session);

/**
 * Print the prompt
 * @param session REPL session
 */
void repl_print_prompt(ReplSession* session);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_REPL_H */
