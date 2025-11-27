/**
 * @file prompt_templates.h
 * @brief LLM prompt templates for build error analysis
 */

#ifndef CYXMAKE_PROMPT_TEMPLATES_H
#define CYXMAKE_PROMPT_TEMPLATES_H

#include "project_context.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct LLMContext LLMContext;

/**
 * Generate prompt for analyzing a build error
 * @param error_output The error output from the build
 * @param build_system The build system being used
 * @param project_lang Primary programming language
 * @return Allocated prompt string (caller must free)
 */
char* prompt_analyze_build_error(const char* error_output,
                                  BuildSystem build_system,
                                  const char* project_lang);

/**
 * Generate prompt for fixing a compilation error
 * @param filename The file with the error
 * @param line_number Line number of the error
 * @param error_msg The compiler error message
 * @param code_snippet Code around the error (optional)
 * @return Allocated prompt string (caller must free)
 */
char* prompt_fix_compilation_error(const char* filename,
                                    int line_number,
                                    const char* error_msg,
                                    const char* code_snippet);

/**
 * Generate prompt for resolving missing dependencies
 * @param dependency Name of the missing dependency
 * @param build_system The build system being used
 * @param os_type Operating system type (Windows/Linux/macOS)
 * @return Allocated prompt string (caller must free)
 */
char* prompt_resolve_dependency(const char* dependency,
                                BuildSystem build_system,
                                const char* os_type);

/**
 * Generate prompt for understanding a linker error
 * @param error_output The linker error output
 * @param undefined_symbols List of undefined symbols (optional)
 * @return Allocated prompt string (caller must free)
 */
char* prompt_analyze_linker_error(const char* error_output,
                                   const char* undefined_symbols);

/**
 * Generate prompt for optimizing build configuration
 * @param ctx Project context
 * @param build_time Current build time in seconds
 * @return Allocated prompt string (caller must free)
 */
char* prompt_optimize_build(const ProjectContext* ctx, double build_time);

/**
 * Generate prompt for creating build configuration
 * @param project_type Type of project (library/application/etc)
 * @param language Primary programming language
 * @param dependencies List of dependencies (comma-separated)
 * @return Allocated prompt string (caller must free)
 */
char* prompt_create_build_config(const char* project_type,
                                  const char* language,
                                  const char* dependencies);

/**
 * Generate smart prompt based on error analysis
 * Automatically detects error type and generates appropriate prompt
 * @param error_output The complete error output
 * @param ctx Project context (optional, for better analysis)
 * @return Allocated prompt string (caller must free)
 */
char* prompt_smart_error_analysis(const char* error_output,
                                   const ProjectContext* ctx);

/**
 * Format LLM response for display
 * @param response Raw response from LLM
 * @return Formatted response (caller must free)
 */
char* format_llm_response(const char* response);

/* ========================================================================
 * REPL Context-Aware Prompts
 * ======================================================================== */

/**
 * Generate prompt to explain code or concept with context
 * @param query What to explain
 * @param current_file Current file being discussed (optional)
 * @param file_content File content or snippet (optional)
 * @param conversation_context Recent conversation for context (optional)
 * @return Allocated prompt string (caller must free)
 */
char* prompt_explain_with_context(const char* query,
                                   const char* current_file,
                                   const char* file_content,
                                   const char* conversation_context);

/**
 * Generate prompt to fix an error with context
 * @param error_message The error to fix
 * @param current_file Current file (optional)
 * @param file_content File content (optional)
 * @param conversation_context Recent conversation (optional)
 * @return Allocated prompt string (caller must free)
 */
char* prompt_fix_with_context(const char* error_message,
                               const char* current_file,
                               const char* file_content,
                               const char* conversation_context);

/**
 * Generate prompt for general AI assistance
 * @param user_query The user's question
 * @param conversation_context Recent conversation (optional)
 * @return Allocated prompt string (caller must free)
 */
char* prompt_general_assistance(const char* user_query,
                                 const char* conversation_context);

/* ========================================================================
 * AI Agent System
 * ======================================================================== */

/**
 * Action types the AI agent can perform
 */
typedef enum {
    AI_ACTION_NONE,         /* No action, just respond */
    AI_ACTION_READ_FILE,    /* Read a file */
    AI_ACTION_CREATE_FILE,  /* Create a file with content */
    AI_ACTION_DELETE_FILE,  /* Delete a file */
    AI_ACTION_DELETE_DIR,   /* Delete a directory */
    AI_ACTION_BUILD,        /* Build the project */
    AI_ACTION_CLEAN,        /* Clean build artifacts */
    AI_ACTION_INSTALL,      /* Install a package */
    AI_ACTION_RUN_COMMAND,  /* Run a shell command */
    AI_ACTION_LIST_FILES,   /* List files in directory */
    AI_ACTION_MULTI         /* Multiple actions in sequence */
} AIActionType;

/**
 * AI agent action
 */
typedef struct AIAction {
    AIActionType type;
    char* target;           /* File path, package name, etc. */
    char* content;          /* File content for create, command for run */
    char* reason;           /* Why this action is needed */
    struct AIAction* next;  /* For chained actions */
} AIAction;

/**
 * AI agent response
 */
typedef struct {
    char* message;          /* Text response to show user */
    AIAction* actions;      /* Actions to perform (can be NULL) */
    bool needs_confirmation;/* Whether to ask user before executing */
} AIAgentResponse;

/**
 * Generate prompt for AI agent with available actions
 * @param user_request User's natural language request
 * @param current_dir Current working directory
 * @param current_file Current file being discussed (optional)
 * @param last_error Last error message (optional)
 * @param conversation_context Recent conversation (optional)
 * @return Allocated prompt string (caller must free)
 */
char* prompt_ai_agent(const char* user_request,
                       const char* current_dir,
                       const char* current_file,
                       const char* last_error,
                       const char* conversation_context);

/**
 * Parse AI agent response to extract actions
 * @param response Raw AI response text
 * @return Parsed agent response (caller must free with ai_agent_response_free)
 */
AIAgentResponse* parse_ai_agent_response(const char* response);

/**
 * Free AI action chain
 * @param action Action to free (frees entire chain)
 */
void ai_action_free(AIAction* action);

/**
 * Free AI agent response
 * @param response Response to free
 */
void ai_agent_response_free(AIAgentResponse* response);

/**
 * Get action type name
 * @param type Action type
 * @return Static string name
 */
const char* ai_action_type_name(AIActionType type);

/* ========================================================================
 * Natural Language Command Parsing
 * ======================================================================== */

/**
 * Intent types for natural language commands
 */
typedef enum {
    INTENT_BUILD,           /* Build the project */
    INTENT_INIT,            /* Initialize/analyze project */
    INTENT_CLEAN,           /* Clean build artifacts */
    INTENT_TEST,            /* Run tests */
    INTENT_CREATE_FILE,     /* Create a new file */
    INTENT_READ_FILE,       /* Read/show file contents */
    INTENT_EXPLAIN,         /* Explain something */
    INTENT_FIX,             /* Fix an error or issue */
    INTENT_INSTALL,         /* Install a package/dependency */
    INTENT_STATUS,          /* Show project/AI status */
    INTENT_HELP,            /* Get help */
    INTENT_UNKNOWN          /* Unknown intent - ask AI */
} CommandIntent;

/**
 * Parsed natural language command
 */
typedef struct {
    CommandIntent intent;
    char* target;           /* File, package, or other target */
    char* details;          /* Additional details from the command */
    double confidence;      /* Confidence in intent detection (0.0-1.0) */
} ParsedCommand;

/**
 * Parse a natural language command locally (fast, no AI)
 * Uses keyword matching for common patterns
 * @param input Natural language command string
 * @return Parsed command (caller must free with parsed_command_free)
 */
ParsedCommand* parse_command_local(const char* input);

/**
 * Parse a natural language command using AI (slower, more accurate)
 * @param input Natural language command string
 * @param llm LLM context for AI parsing
 * @return Parsed command (caller must free with parsed_command_free)
 */
ParsedCommand* parse_command_with_ai(const char* input, LLMContext* llm);

/**
 * Free a parsed command
 * @param cmd Parsed command to free
 */
void parsed_command_free(ParsedCommand* cmd);

/**
 * Generate prompt for parsing natural language command
 * @param user_input The natural language command
 * @return Allocated prompt string (caller must free)
 */
char* prompt_parse_command(const char* user_input);

/**
 * Execute a natural language command
 * Combines local parsing with AI fallback
 * @param input Natural language command string
 * @param llm LLM context (optional, for AI parsing)
 * @param project_path Project directory
 * @return Execution result message (caller must free)
 */
char* execute_natural_command(const char* input,
                               LLMContext* llm,
                               const char* project_path);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_PROMPT_TEMPLATES_H */