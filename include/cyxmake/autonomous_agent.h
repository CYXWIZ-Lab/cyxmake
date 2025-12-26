/**
 * @file autonomous_agent.h
 * @brief Autonomous AI Agent with Tool Use
 *
 * This implements a true autonomous agent that can:
 * - Read and write files
 * - Execute commands
 * - Reason about problems
 * - Self-correct on failures
 * - Complete complex multi-step tasks
 */

#ifndef CYXMAKE_AUTONOMOUS_AGENT_H
#define CYXMAKE_AUTONOMOUS_AGENT_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct AIProvider AIProvider;
typedef struct ToolRegistry ToolRegistry;

/* ============================================================================
 * Tool System
 * ============================================================================ */

/**
 * Tool result from execution
 */
typedef struct {
    bool success;
    char* output;
    char* error;
} ToolResult;

/**
 * Tool handler function type
 * @param args JSON string of arguments
 * @param working_dir Current working directory
 * @return Tool result (caller must free)
 */
typedef ToolResult* (*AgentToolHandler)(const char* args, const char* working_dir);

/**
 * Tool definition for the agent
 */
typedef struct {
    const char* name;
    const char* description;
    const char* parameters_json;  /* JSON schema for parameters */
    AgentToolHandler handler;
} AgentTool;

/**
 * Tool call from AI
 */
typedef struct {
    char* id;           /* Unique ID for this call */
    char* name;         /* Tool name */
    char* arguments;    /* JSON arguments */
} AgentToolCall;

/* ============================================================================
 * Agent Message System
 * ============================================================================ */

typedef enum {
    CHAT_MSG_SYSTEM,
    CHAT_MSG_USER,
    CHAT_MSG_ASSISTANT,
    CHAT_MSG_TOOL
} ChatMessageRole;

typedef struct {
    ChatMessageRole role;
    char* content;
    char* tool_call_id;     /* For tool results */
    AgentToolCall* tool_calls;  /* For assistant messages with tool calls */
    int tool_call_count;
} ChatMessage;

/* ============================================================================
 * Autonomous Agent
 * ============================================================================ */

typedef struct {
    int max_iterations;     /* Maximum reasoning steps (default: 20) */
    int max_tokens;         /* Max tokens per response (default: 4096) */
    float temperature;      /* Creativity (default: 0.7) */
    bool verbose;           /* Print reasoning steps */
    bool require_approval;  /* Ask before dangerous actions */
    const char* working_dir; /* Working directory for file operations */
} AgentConfig;

typedef struct AutonomousAgent AutonomousAgent;

/**
 * Create default agent configuration
 */
AgentConfig agent_config_default(void);

/**
 * Create an autonomous agent
 * @param ai AI provider for reasoning
 * @param config Agent configuration
 * @return New agent or NULL on error
 */
AutonomousAgent* agent_create(AIProvider* ai, const AgentConfig* config);

/**
 * Free an agent
 */
void agent_free(AutonomousAgent* agent);

/**
 * Run the agent on a task
 * @param agent The agent
 * @param task Natural language task description
 * @return Final response (caller must free) or NULL on error
 */
char* agent_run(AutonomousAgent* agent, const char* task);

/**
 * Add a custom tool to the agent
 * @param agent The agent
 * @param tool Tool definition
 * @return true on success
 */
bool agent_add_tool(AutonomousAgent* agent, const AgentTool* tool);

/**
 * Set the working directory
 */
void agent_set_working_dir(AutonomousAgent* agent, const char* path);

/**
 * Get the last error message
 * Note: Named autonomous_agent_get_error to avoid conflict with agent_registry.h
 */
const char* autonomous_agent_get_error(AutonomousAgent* agent);

/**
 * Clear conversation history (start fresh)
 */
void agent_clear_history(AutonomousAgent* agent);

/* ============================================================================
 * Built-in Tools
 * ============================================================================ */

/**
 * Register all built-in tools (read, write, execute, list, search)
 */
void agent_register_builtin_tools(AutonomousAgent* agent);

/* Individual tool handlers */
ToolResult* tool_read_file(const char* args, const char* working_dir);
ToolResult* tool_write_file(const char* args, const char* working_dir);
ToolResult* tool_execute_cmd(const char* args, const char* working_dir);
ToolResult* tool_list_directory(const char* args, const char* working_dir);
ToolResult* tool_search_files(const char* args, const char* working_dir);
ToolResult* tool_search_content(const char* args, const char* working_dir);

/**
 * Free a tool result
 */
void tool_result_free(ToolResult* result);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_AUTONOMOUS_AGENT_H */
