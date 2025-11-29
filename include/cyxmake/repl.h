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

/* Forward declarations - with guards to prevent redefinition */
#ifndef CYXMAKE_ORCHESTRATOR_FWD
#define CYXMAKE_ORCHESTRATOR_FWD
typedef struct Orchestrator Orchestrator;
#endif

#ifndef CYXMAKE_LLM_CONTEXT_FWD
#define CYXMAKE_LLM_CONTEXT_FWD
typedef struct LLMContext LLMContext;
#endif

#ifndef CYXMAKE_PERMISSION_CONTEXT_FWD
#define CYXMAKE_PERMISSION_CONTEXT_FWD
typedef struct PermissionContext PermissionContext;
#endif

#ifndef CYXMAKE_CONVERSATION_CONTEXT_FWD
#define CYXMAKE_CONVERSATION_CONTEXT_FWD
typedef struct ConversationContext ConversationContext;
#endif

#ifndef CYXMAKE_AI_PROVIDER_REGISTRY_FWD
#define CYXMAKE_AI_PROVIDER_REGISTRY_FWD
typedef struct AIProviderRegistry AIProviderRegistry;
#endif

#ifndef CYXMAKE_AI_PROVIDER_FWD
#define CYXMAKE_AI_PROVIDER_FWD
typedef struct AIProvider AIProvider;
#endif

#ifndef CYXMAKE_INPUT_CONTEXT_FWD
#define CYXMAKE_INPUT_CONTEXT_FWD
typedef struct InputContext InputContext;
#endif

#ifndef CYXMAKE_SMART_AGENT_FWD
#define CYXMAKE_SMART_AGENT_FWD
typedef struct SmartAgent SmartAgent;
#endif

#ifndef CYXMAKE_PROJECT_GRAPH_FWD
#define CYXMAKE_PROJECT_GRAPH_FWD
typedef struct ProjectGraph ProjectGraph;
#endif

#ifndef CYXMAKE_AUTONOMOUS_AGENT_FWD
#define CYXMAKE_AUTONOMOUS_AGENT_FWD
typedef struct AutonomousAgent AutonomousAgent;
#endif

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
    InputContext* input;                 /* Line editing context */
    SmartAgent* smart_agent;             /* Intelligent reasoning agent */
    ProjectGraph* project_graph;         /* Project dependency graph */
    AutonomousAgent* autonomous_agent;   /* True autonomous agent with tool use */

    /* Session state */
    bool running;
    int command_count;

    /* History (deprecated - use input->history instead) */
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
