/**
 * @file autonomous_agent.c
 * @brief Autonomous AI Agent Implementation
 *
 * This is the REAL intelligent agent - it can read, write, execute,
 * reason, and self-correct just like Claude Code.
 */

#include "cyxmake/autonomous_agent.h"
#include "cyxmake/ai_provider.h"
#include "cyxmake/logger.h"
#include "cyxmake/file_ops.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define getcwd _getcwd
#define chdir _chdir
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

/* Maximum tools */
#define MAX_TOOLS 32
#define MAX_MESSAGES 100
#define MAX_OUTPUT_SIZE (1024 * 1024)

/* Agent structure */
struct AutonomousAgent {
    AIProvider* ai;
    AgentConfig config;

    /* Tools */
    AgentTool tools[MAX_TOOLS];
    int tool_count;

    /* Conversation */
    AgentMessage messages[MAX_MESSAGES];
    int message_count;

    /* State */
    char* last_error;
    char* working_dir;
};

/* System prompt for the agent */
static const char* AGENT_SYSTEM_PROMPT =
    "You are an autonomous build agent with full access to the filesystem and shell.\n"
    "Your job is to help users build, create, and manage software projects.\n\n"

    "You have access to the following tools:\n"
    "- read_file: Read the contents of a file\n"
    "- write_file: Write content to a file (creates or overwrites)\n"
    "- execute: Run a shell command and get the output\n"
    "- list_directory: List files and folders in a directory\n"
    "- search_files: Find files matching a pattern\n"
    "- search_content: Search for text in files\n\n"

    "When given a task:\n"
    "1. THINK about what you need to do\n"
    "2. USE TOOLS to gather information and take action\n"
    "3. OBSERVE the results\n"
    "4. If something fails, TRY A DIFFERENT APPROACH\n"
    "5. Continue until the task is complete\n\n"

    "Be proactive - don't just describe what to do, actually DO it.\n"
    "If you need to read a file, use read_file.\n"
    "If you need to create a file, use write_file.\n"
    "If you need to run a command, use execute.\n\n"

    "Always explain what you're doing and why.";

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static char* strdup_safe(const char* s) {
    return s ? strdup(s) : NULL;
}

static void set_error(AutonomousAgent* agent, const char* error) {
    free(agent->last_error);
    agent->last_error = strdup_safe(error);
}

static char* resolve_path(const char* path, const char* working_dir) {
    if (!path) return NULL;

    /* If absolute path, use as-is */
    if (path[0] == '/' || (path[0] != '\0' && path[1] == ':')) {
        return strdup(path);
    }

    /* Relative path - combine with working dir */
    if (working_dir) {
        size_t len = strlen(working_dir) + strlen(path) + 2;
        char* full = malloc(len);
        if (full) {
            snprintf(full, len, "%s/%s", working_dir, path);
        }
        return full;
    }

    return strdup(path);
}

/* ============================================================================
 * Tool Implementations
 * ============================================================================ */

ToolResult* tool_result_create(bool success, const char* output, const char* error) {
    ToolResult* result = calloc(1, sizeof(ToolResult));
    if (!result) return NULL;
    result->success = success;
    result->output = strdup_safe(output);
    result->error = strdup_safe(error);
    return result;
}

void tool_result_free(ToolResult* result) {
    if (!result) return;
    free(result->output);
    free(result->error);
    free(result);
}

/* Read file tool */
ToolResult* tool_read_file(const char* args, const char* working_dir) {
    cJSON* json = cJSON_Parse(args);
    if (!json) {
        return tool_result_create(false, NULL, "Invalid JSON arguments");
    }

    cJSON* path_item = cJSON_GetObjectItem(json, "path");
    if (!path_item || !cJSON_IsString(path_item)) {
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Missing 'path' argument");
    }

    char* full_path = resolve_path(path_item->valuestring, working_dir);
    if (!full_path) {
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Failed to resolve path");
    }

    FILE* file = fopen(full_path, "r");
    if (!file) {
        char error[512];
        snprintf(error, sizeof(error), "Cannot open file: %s", full_path);
        free(full_path);
        cJSON_Delete(json);
        return tool_result_create(false, NULL, error);
    }

    /* Read file content */
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size > MAX_OUTPUT_SIZE) {
        size = MAX_OUTPUT_SIZE;
    }

    char* content = malloc(size + 1);
    if (!content) {
        fclose(file);
        free(full_path);
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Out of memory");
    }

    size_t read = fread(content, 1, size, file);
    content[read] = '\0';
    fclose(file);

    ToolResult* result = tool_result_create(true, content, NULL);

    free(content);
    free(full_path);
    cJSON_Delete(json);

    return result;
}

/* Write file tool */
ToolResult* tool_write_file(const char* args, const char* working_dir) {
    cJSON* json = cJSON_Parse(args);
    if (!json) {
        return tool_result_create(false, NULL, "Invalid JSON arguments");
    }

    cJSON* path_item = cJSON_GetObjectItem(json, "path");
    cJSON* content_item = cJSON_GetObjectItem(json, "content");

    if (!path_item || !cJSON_IsString(path_item)) {
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Missing 'path' argument");
    }

    if (!content_item || !cJSON_IsString(content_item)) {
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Missing 'content' argument");
    }

    char* full_path = resolve_path(path_item->valuestring, working_dir);
    if (!full_path) {
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Failed to resolve path");
    }

    FILE* file = fopen(full_path, "w");
    if (!file) {
        char error[512];
        snprintf(error, sizeof(error), "Cannot create file: %s", full_path);
        free(full_path);
        cJSON_Delete(json);
        return tool_result_create(false, NULL, error);
    }

    fputs(content_item->valuestring, file);
    fclose(file);

    char msg[512];
    snprintf(msg, sizeof(msg), "Successfully wrote %zu bytes to %s",
             strlen(content_item->valuestring), path_item->valuestring);

    ToolResult* result = tool_result_create(true, msg, NULL);

    free(full_path);
    cJSON_Delete(json);

    return result;
}

/* Execute command tool */
ToolResult* tool_execute_cmd(const char* args, const char* working_dir) {
    cJSON* json = cJSON_Parse(args);
    if (!json) {
        return tool_result_create(false, NULL, "Invalid JSON arguments");
    }

    cJSON* cmd_item = cJSON_GetObjectItem(json, "command");
    if (!cmd_item || !cJSON_IsString(cmd_item)) {
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Missing 'command' argument");
    }

    const char* command = cmd_item->valuestring;

    /* Save current directory */
    char* old_dir = NULL;
    if (working_dir) {
        old_dir = getcwd(NULL, 0);
        chdir(working_dir);
    }

    /* Build command with stderr redirect */
    char cmd_buf[4096];
    snprintf(cmd_buf, sizeof(cmd_buf), "%s 2>&1", command);

    FILE* pipe = popen(cmd_buf, "r");
    if (!pipe) {
        if (old_dir) {
            chdir(old_dir);
            free(old_dir);
        }
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Failed to execute command");
    }

    /* Read output */
    char* output = malloc(MAX_OUTPUT_SIZE);
    if (!output) {
        pclose(pipe);
        if (old_dir) {
            chdir(old_dir);
            free(old_dir);
        }
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Out of memory");
    }

    size_t offset = 0;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) && offset < MAX_OUTPUT_SIZE - 1) {
        size_t len = strlen(buffer);
        if (offset + len < MAX_OUTPUT_SIZE) {
            memcpy(output + offset, buffer, len);
            offset += len;
        }
    }
    output[offset] = '\0';

    int status = pclose(pipe);
#ifdef _WIN32
    int exit_code = status;
#else
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif

    /* Restore directory */
    if (old_dir) {
        chdir(old_dir);
        free(old_dir);
    }

    /* Create result with exit code info */
    char result_msg[MAX_OUTPUT_SIZE + 256];
    snprintf(result_msg, sizeof(result_msg),
             "Exit code: %d\n\n%s", exit_code, output);

    ToolResult* result = tool_result_create(exit_code == 0, result_msg, NULL);

    free(output);
    cJSON_Delete(json);

    return result;
}

/* List directory tool */
ToolResult* tool_list_directory(const char* args, const char* working_dir) {
    cJSON* json = cJSON_Parse(args);
    if (!json) {
        return tool_result_create(false, NULL, "Invalid JSON arguments");
    }

    cJSON* path_item = cJSON_GetObjectItem(json, "path");
    const char* path = ".";
    if (path_item && cJSON_IsString(path_item)) {
        path = path_item->valuestring;
    }

    char* full_path = resolve_path(path, working_dir);
    if (!full_path) {
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Failed to resolve path");
    }

    /* Use ls or dir command */
    char cmd[1024];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "dir /b \"%s\" 2>&1", full_path);
#else
    snprintf(cmd, sizeof(cmd), "ls -la \"%s\" 2>&1", full_path);
#endif

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        free(full_path);
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Failed to list directory");
    }

    char* output = malloc(MAX_OUTPUT_SIZE);
    if (!output) {
        pclose(pipe);
        free(full_path);
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Out of memory");
    }

    size_t offset = 0;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) && offset < MAX_OUTPUT_SIZE - 1) {
        size_t len = strlen(buffer);
        if (offset + len < MAX_OUTPUT_SIZE) {
            memcpy(output + offset, buffer, len);
            offset += len;
        }
    }
    output[offset] = '\0';

    pclose(pipe);

    ToolResult* result = tool_result_create(true, output, NULL);

    free(output);
    free(full_path);
    cJSON_Delete(json);

    return result;
}

/* Search files tool */
ToolResult* tool_search_files(const char* args, const char* working_dir) {
    cJSON* json = cJSON_Parse(args);
    if (!json) {
        return tool_result_create(false, NULL, "Invalid JSON arguments");
    }

    cJSON* pattern_item = cJSON_GetObjectItem(json, "pattern");
    if (!pattern_item || !cJSON_IsString(pattern_item)) {
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Missing 'pattern' argument");
    }

    cJSON* path_item = cJSON_GetObjectItem(json, "path");
    const char* path = ".";
    if (path_item && cJSON_IsString(path_item)) {
        path = path_item->valuestring;
    }

    char* full_path = resolve_path(path, working_dir);
    if (!full_path) {
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Failed to resolve path");
    }

    /* Use find or dir command */
    char cmd[1024];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "dir /s /b \"%s\\%s\" 2>&1",
             full_path, pattern_item->valuestring);
#else
    snprintf(cmd, sizeof(cmd), "find \"%s\" -name \"%s\" 2>&1",
             full_path, pattern_item->valuestring);
#endif

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        free(full_path);
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Failed to search files");
    }

    char* output = malloc(MAX_OUTPUT_SIZE);
    if (!output) {
        pclose(pipe);
        free(full_path);
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Out of memory");
    }

    size_t offset = 0;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) && offset < MAX_OUTPUT_SIZE - 1) {
        size_t len = strlen(buffer);
        if (offset + len < MAX_OUTPUT_SIZE) {
            memcpy(output + offset, buffer, len);
            offset += len;
        }
    }
    output[offset] = '\0';

    pclose(pipe);

    ToolResult* result = tool_result_create(true,
        offset > 0 ? output : "No files found", NULL);

    free(output);
    free(full_path);
    cJSON_Delete(json);

    return result;
}

/* Search content tool */
ToolResult* tool_search_content(const char* args, const char* working_dir) {
    cJSON* json = cJSON_Parse(args);
    if (!json) {
        return tool_result_create(false, NULL, "Invalid JSON arguments");
    }

    cJSON* pattern_item = cJSON_GetObjectItem(json, "pattern");
    if (!pattern_item || !cJSON_IsString(pattern_item)) {
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Missing 'pattern' argument");
    }

    cJSON* path_item = cJSON_GetObjectItem(json, "path");
    const char* path = ".";
    if (path_item && cJSON_IsString(path_item)) {
        path = path_item->valuestring;
    }

    char* full_path = resolve_path(path, working_dir);
    if (!full_path) {
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Failed to resolve path");
    }

    /* Use grep or findstr */
    char cmd[1024];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "findstr /s /n /i \"%s\" \"%s\\*\" 2>&1",
             pattern_item->valuestring, full_path);
#else
    snprintf(cmd, sizeof(cmd), "grep -rn \"%s\" \"%s\" 2>&1",
             pattern_item->valuestring, full_path);
#endif

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        free(full_path);
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Failed to search content");
    }

    char* output = malloc(MAX_OUTPUT_SIZE);
    if (!output) {
        pclose(pipe);
        free(full_path);
        cJSON_Delete(json);
        return tool_result_create(false, NULL, "Out of memory");
    }

    size_t offset = 0;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) && offset < MAX_OUTPUT_SIZE - 1) {
        size_t len = strlen(buffer);
        if (offset + len < MAX_OUTPUT_SIZE) {
            memcpy(output + offset, buffer, len);
            offset += len;
        }
    }
    output[offset] = '\0';

    pclose(pipe);

    ToolResult* result = tool_result_create(true,
        offset > 0 ? output : "No matches found", NULL);

    free(output);
    free(full_path);
    cJSON_Delete(json);

    return result;
}

/* ============================================================================
 * Tool Registration
 * ============================================================================ */

static const char* READ_FILE_SCHEMA =
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"path\": {"
    "      \"type\": \"string\","
    "      \"description\": \"Path to the file to read\""
    "    }"
    "  },"
    "  \"required\": [\"path\"]"
    "}";

static const char* WRITE_FILE_SCHEMA =
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"path\": {"
    "      \"type\": \"string\","
    "      \"description\": \"Path to the file to write\""
    "    },"
    "    \"content\": {"
    "      \"type\": \"string\","
    "      \"description\": \"Content to write to the file\""
    "    }"
    "  },"
    "  \"required\": [\"path\", \"content\"]"
    "}";

static const char* EXECUTE_SCHEMA =
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"command\": {"
    "      \"type\": \"string\","
    "      \"description\": \"Shell command to execute\""
    "    }"
    "  },"
    "  \"required\": [\"command\"]"
    "}";

static const char* LIST_DIR_SCHEMA =
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"path\": {"
    "      \"type\": \"string\","
    "      \"description\": \"Directory path to list (default: current directory)\""
    "    }"
    "  }"
    "}";

static const char* SEARCH_FILES_SCHEMA =
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"pattern\": {"
    "      \"type\": \"string\","
    "      \"description\": \"File pattern to search for (e.g., *.txt, CMakeLists.txt)\""
    "    },"
    "    \"path\": {"
    "      \"type\": \"string\","
    "      \"description\": \"Directory to search in (default: current directory)\""
    "    }"
    "  },"
    "  \"required\": [\"pattern\"]"
    "}";

static const char* SEARCH_CONTENT_SCHEMA =
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"pattern\": {"
    "      \"type\": \"string\","
    "      \"description\": \"Text pattern to search for in files\""
    "    },"
    "    \"path\": {"
    "      \"type\": \"string\","
    "      \"description\": \"Directory to search in (default: current directory)\""
    "    }"
    "  },"
    "  \"required\": [\"pattern\"]"
    "}";

void agent_register_builtin_tools(AutonomousAgent* agent) {
    if (!agent) return;

    AgentTool tools[] = {
        {
            "read_file",
            "Read the contents of a file. Use this to understand code, configs, READMEs, etc.",
            READ_FILE_SCHEMA,
            tool_read_file
        },
        {
            "write_file",
            "Write content to a file. Creates the file if it doesn't exist, overwrites if it does.",
            WRITE_FILE_SCHEMA,
            tool_write_file
        },
        {
            "execute",
            "Execute a shell command. Use for building, running tests, git, npm, cmake, etc.",
            EXECUTE_SCHEMA,
            tool_execute_cmd
        },
        {
            "list_directory",
            "List files and directories. Use to explore project structure.",
            LIST_DIR_SCHEMA,
            tool_list_directory
        },
        {
            "search_files",
            "Find files matching a pattern. Use to locate specific files.",
            SEARCH_FILES_SCHEMA,
            tool_search_files
        },
        {
            "search_content",
            "Search for text in files. Use to find code patterns, errors, etc.",
            SEARCH_CONTENT_SCHEMA,
            tool_search_content
        }
    };

    for (size_t i = 0; i < sizeof(tools) / sizeof(tools[0]); i++) {
        agent_add_tool(agent, &tools[i]);
    }
}

/* ============================================================================
 * Agent Core
 * ============================================================================ */

AgentConfig agent_config_default(void) {
    AgentConfig config = {
        .max_iterations = 20,
        .max_tokens = 4096,
        .temperature = 0.7f,
        .verbose = true,
        .require_approval = false,
        .working_dir = NULL
    };
    return config;
}

AutonomousAgent* agent_create(AIProvider* ai, const AgentConfig* config) {
    if (!ai) return NULL;

    AutonomousAgent* agent = calloc(1, sizeof(AutonomousAgent));
    if (!agent) return NULL;

    agent->ai = ai;

    if (config) {
        agent->config = *config;
    } else {
        agent->config = agent_config_default();
    }

    /* Set working directory */
    if (agent->config.working_dir) {
        agent->working_dir = strdup(agent->config.working_dir);
    } else {
        agent->working_dir = getcwd(NULL, 0);
    }

    /* Register built-in tools */
    agent_register_builtin_tools(agent);

    return agent;
}

void agent_free(AutonomousAgent* agent) {
    if (!agent) return;

    /* Free messages */
    for (int i = 0; i < agent->message_count; i++) {
        free(agent->messages[i].content);
        free(agent->messages[i].tool_call_id);
        if (agent->messages[i].tool_calls) {
            for (int j = 0; j < agent->messages[i].tool_call_count; j++) {
                free(agent->messages[i].tool_calls[j].id);
                free(agent->messages[i].tool_calls[j].name);
                free(agent->messages[i].tool_calls[j].arguments);
            }
            free(agent->messages[i].tool_calls);
        }
    }

    free(agent->last_error);
    free(agent->working_dir);
    free(agent);
}

bool agent_add_tool(AutonomousAgent* agent, const AgentTool* tool) {
    if (!agent || !tool || agent->tool_count >= MAX_TOOLS) return false;

    agent->tools[agent->tool_count++] = *tool;
    return true;
}

void agent_set_working_dir(AutonomousAgent* agent, const char* path) {
    if (!agent) return;
    free(agent->working_dir);
    agent->working_dir = strdup_safe(path);
}

const char* agent_get_error(AutonomousAgent* agent) {
    return agent ? agent->last_error : NULL;
}

void agent_clear_history(AutonomousAgent* agent) {
    if (!agent) return;

    for (int i = 0; i < agent->message_count; i++) {
        free(agent->messages[i].content);
        free(agent->messages[i].tool_call_id);
        if (agent->messages[i].tool_calls) {
            for (int j = 0; j < agent->messages[i].tool_call_count; j++) {
                free(agent->messages[i].tool_calls[j].id);
                free(agent->messages[i].tool_calls[j].name);
                free(agent->messages[i].tool_calls[j].arguments);
            }
            free(agent->messages[i].tool_calls);
        }
    }
    agent->message_count = 0;
}

/* Build tools JSON for API */
static char* build_tools_json(AutonomousAgent* agent) {
    cJSON* tools = cJSON_CreateArray();

    for (int i = 0; i < agent->tool_count; i++) {
        cJSON* tool = cJSON_CreateObject();
        cJSON_AddStringToObject(tool, "type", "function");

        cJSON* func = cJSON_CreateObject();
        cJSON_AddStringToObject(func, "name", agent->tools[i].name);
        cJSON_AddStringToObject(func, "description", agent->tools[i].description);

        cJSON* params = cJSON_Parse(agent->tools[i].parameters_json);
        if (params) {
            cJSON_AddItemToObject(func, "parameters", params);
        }

        cJSON_AddItemToObject(tool, "function", func);
        cJSON_AddItemToArray(tools, tool);
    }

    char* result = cJSON_PrintUnformatted(tools);
    cJSON_Delete(tools);
    return result;
}

/* Build messages JSON for API */
static char* build_messages_json(AutonomousAgent* agent) {
    cJSON* messages = cJSON_CreateArray();

    /* Add system message */
    cJSON* sys_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(sys_msg, "role", "system");
    cJSON_AddStringToObject(sys_msg, "content", AGENT_SYSTEM_PROMPT);
    cJSON_AddItemToArray(messages, sys_msg);

    /* Add conversation messages */
    for (int i = 0; i < agent->message_count; i++) {
        cJSON* msg = cJSON_CreateObject();

        const char* role = "user";
        switch (agent->messages[i].role) {
            case AGENT_MSG_SYSTEM: role = "system"; break;
            case AGENT_MSG_USER: role = "user"; break;
            case AGENT_MSG_ASSISTANT: role = "assistant"; break;
            case AGENT_MSG_TOOL: role = "tool"; break;
        }
        cJSON_AddStringToObject(msg, "role", role);

        if (agent->messages[i].content) {
            cJSON_AddStringToObject(msg, "content", agent->messages[i].content);
        }

        if (agent->messages[i].tool_call_id) {
            cJSON_AddStringToObject(msg, "tool_call_id", agent->messages[i].tool_call_id);
        }

        if (agent->messages[i].tool_calls && agent->messages[i].tool_call_count > 0) {
            cJSON* tool_calls = cJSON_CreateArray();
            for (int j = 0; j < agent->messages[i].tool_call_count; j++) {
                cJSON* tc = cJSON_CreateObject();
                cJSON_AddStringToObject(tc, "id", agent->messages[i].tool_calls[j].id);
                cJSON_AddStringToObject(tc, "type", "function");

                cJSON* func = cJSON_CreateObject();
                cJSON_AddStringToObject(func, "name", agent->messages[i].tool_calls[j].name);
                cJSON_AddStringToObject(func, "arguments", agent->messages[i].tool_calls[j].arguments);
                cJSON_AddItemToObject(tc, "function", func);

                cJSON_AddItemToArray(tool_calls, tc);
            }
            cJSON_AddItemToObject(msg, "tool_calls", tool_calls);
        }

        cJSON_AddItemToArray(messages, msg);
    }

    char* result = cJSON_PrintUnformatted(messages);
    cJSON_Delete(messages);
    return result;
}

/* Execute a tool and return result */
static ToolResult* execute_tool(AutonomousAgent* agent, const char* name, const char* args) {
    for (int i = 0; i < agent->tool_count; i++) {
        if (strcmp(agent->tools[i].name, name) == 0) {
            if (agent->config.verbose) {
                log_info("Executing tool: %s", name);
            }
            return agent->tools[i].handler(args, agent->working_dir);
        }
    }
    return tool_result_create(false, NULL, "Unknown tool");
}

/* Add message to conversation */
static void add_message(AutonomousAgent* agent, AgentMessageRole role,
                        const char* content, const char* tool_call_id) {
    if (agent->message_count >= MAX_MESSAGES) {
        /* Shift messages to make room */
        free(agent->messages[0].content);
        free(agent->messages[0].tool_call_id);
        memmove(&agent->messages[0], &agent->messages[1],
                sizeof(AgentMessage) * (MAX_MESSAGES - 1));
        agent->message_count = MAX_MESSAGES - 1;
    }

    AgentMessage* msg = &agent->messages[agent->message_count++];
    msg->role = role;
    msg->content = strdup_safe(content);
    msg->tool_call_id = strdup_safe(tool_call_id);
    msg->tool_calls = NULL;
    msg->tool_call_count = 0;
}

/* Main agent loop */
char* agent_run(AutonomousAgent* agent, const char* task) {
    if (!agent || !task) return NULL;

    if (agent->config.verbose) {
        log_info("Agent starting task: %s", task);
        if (agent->ai) {
            log_info("Using AI provider: %s at %s",
                     agent->ai->config.name ? agent->ai->config.name : "(unnamed)",
                     agent->ai->config.base_url ? agent->ai->config.base_url : "(no url)");
        }
    }

    /* Add user message */
    add_message(agent, AGENT_MSG_USER, task, NULL);

    char* final_response = NULL;

    for (int iter = 0; iter < agent->config.max_iterations; iter++) {
        if (agent->config.verbose) {
            log_debug("Iteration %d/%d", iter + 1, agent->config.max_iterations);
        }

        /* Build request */
        char* messages_json = build_messages_json(agent);
        char* tools_json = build_tools_json(agent);

        if (!messages_json || !tools_json) {
            free(messages_json);
            free(tools_json);
            set_error(agent, "Failed to build request");
            break;
        }

        /* Create AI request with tools */
        AIRequest* request = ai_request_create();
        if (!request) {
            free(messages_json);
            free(tools_json);
            set_error(agent, "Failed to create request");
            break;
        }

        /* Parse messages and add to request */
        cJSON* msgs = cJSON_Parse(messages_json);
        if (msgs) {
            cJSON* msg;
            cJSON_ArrayForEach(msg, msgs) {
                cJSON* role = cJSON_GetObjectItem(msg, "role");
                cJSON* content = cJSON_GetObjectItem(msg, "content");

                if (role && cJSON_IsString(role) && content && cJSON_IsString(content)) {
                    AIMessageRole ai_role = AI_ROLE_USER;
                    if (strcmp(role->valuestring, "system") == 0) ai_role = AI_ROLE_SYSTEM;
                    else if (strcmp(role->valuestring, "assistant") == 0) ai_role = AI_ROLE_ASSISTANT;

                    ai_request_add_message(request, ai_role, content->valuestring);
                }
            }
            cJSON_Delete(msgs);
        }

        request->max_tokens = agent->config.max_tokens;
        request->temperature = agent->config.temperature;
        /* Duplicate tools_json since ai_request_free will free it */
        request->tools_json = strdup(tools_json);

        /* Call AI */
        AIResponse* response = ai_provider_complete(agent->ai, request);

        free(messages_json);
        ai_request_free(request);  /* This frees request->tools_json (the copy) */

        if (!response) {
            free(tools_json);
            set_error(agent, "AI request failed");
            break;
        }

        if (!response->success) {
            set_error(agent, response->error ? response->error : "Unknown AI error");
            ai_response_free(response);
            free(tools_json);
            break;
        }

        /* Check for tool calls */
        if (response->tool_calls && response->tool_call_count > 0) {
            /* Bounds check before adding assistant message */
            if (agent->message_count >= MAX_MESSAGES) {
                /* Shift messages to make room */
                free(agent->messages[0].content);
                free(agent->messages[0].tool_call_id);
                if (agent->messages[0].tool_calls) {
                    for (int j = 0; j < agent->messages[0].tool_call_count; j++) {
                        free(agent->messages[0].tool_calls[j].id);
                        free(agent->messages[0].tool_calls[j].name);
                        free(agent->messages[0].tool_calls[j].arguments);
                    }
                    free(agent->messages[0].tool_calls);
                }
                memmove(&agent->messages[0], &agent->messages[1],
                        sizeof(AgentMessage) * (MAX_MESSAGES - 1));
                agent->message_count = MAX_MESSAGES - 1;
            }

            /* Add assistant message with tool calls */
            AgentMessage* asst_msg = &agent->messages[agent->message_count++];
            memset(asst_msg, 0, sizeof(AgentMessage));
            asst_msg->role = AGENT_MSG_ASSISTANT;
            asst_msg->content = strdup_safe(response->content);
            asst_msg->tool_call_id = NULL;
            asst_msg->tool_calls = calloc(response->tool_call_count, sizeof(AgentToolCall));
            asst_msg->tool_call_count = response->tool_call_count;

            /* Execute each tool call */
            for (int i = 0; i < response->tool_call_count; i++) {
                AIToolCall* tc = &response->tool_calls[i];

                /* Copy tool call info */
                asst_msg->tool_calls[i].id = strdup_safe(tc->id);
                asst_msg->tool_calls[i].name = strdup_safe(tc->name);
                asst_msg->tool_calls[i].arguments = strdup_safe(tc->arguments);

                if (agent->config.verbose) {
                    log_info("Tool call: %s(%s)", tc->name, tc->arguments);
                }

                /* Execute tool */
                ToolResult* result = execute_tool(agent, tc->name, tc->arguments);

                /* Add tool result message */
                char* result_content = result->success ? result->output : result->error;
                if (!result_content) result_content = "No output";

                add_message(agent, AGENT_MSG_TOOL, result_content, tc->id);

                if (agent->config.verbose) {
                    if (result->success) {
                        log_success("Tool succeeded");
                    } else {
                        log_warning("Tool failed: %s", result->error ? result->error : "unknown");
                    }
                }

                tool_result_free(result);
            }
        } else {
            /* No tool calls - AI is done */
            if (response->content && strlen(response->content) > 0) {
                final_response = strdup(response->content);

                if (agent->config.verbose) {
                    log_success("Agent completed task");
                }
            } else {
                /* No content and no tool calls - AI gave empty response */
                if (agent->config.verbose) {
                    log_warning("AI returned empty response (no content, no tool calls)");
                }
                /* Set a default response so we don't loop forever */
                final_response = strdup("I've completed the task but have no additional information to provide.");
            }

            ai_response_free(response);
            free(tools_json);
            break;
        }

        ai_response_free(response);
        free(tools_json);
    }

    if (!final_response && !agent->last_error) {
        set_error(agent, "Max iterations reached without completing task");
    }

    return final_response;
}
