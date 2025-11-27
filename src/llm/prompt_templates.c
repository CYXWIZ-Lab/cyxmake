/**
 * @file prompt_templates.c
 * @brief LLM prompt templates for build error analysis
 */

#include "cyxmake/prompt_templates.h"
#include "cyxmake/llm_interface.h"
#include "cyxmake/project_context.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

/* Maximum prompt size */
#define MAX_PROMPT_SIZE 8192

/**
 * Create a system prompt for the coding assistant
 */
static const char* get_system_prompt(void) {
    return "You are an expert build system assistant specialized in diagnosing "
           "and fixing compilation errors. Provide concise, actionable solutions. "
           "Focus on the most likely cause and fix. Be specific about commands to run.";
}

/**
 * Generate prompt for analyzing a build error
 * @param error_output The error output from the build
 * @param build_system The build system being used
 * @param project_lang Primary programming language
 * @return Allocated prompt string (caller must free)
 */
char* prompt_analyze_build_error(const char* error_output,
                                  BuildSystem build_system,
                                  const char* project_lang) {
    if (!error_output) return NULL;

    char* prompt = malloc(MAX_PROMPT_SIZE);
    if (!prompt) return NULL;

    const char* build_sys = build_system_to_string(build_system);

    snprintf(prompt, MAX_PROMPT_SIZE,
        "I'm getting a build error in my %s project using %s.\n\n"
        "Error output:\n"
        "```\n"
        "%.4000s\n"
        "```\n\n"
        "Please:\n"
        "1. Identify the main error\n"
        "2. Explain the likely cause\n"
        "3. Provide the specific fix\n"
        "4. If it's a missing dependency, show the install command\n\n"
        "Keep your response concise and actionable.",
        project_lang ? project_lang : "code",
        build_sys,
        error_output
    );

    return prompt;
}

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
                                    const char* code_snippet) {
    if (!error_msg) return NULL;

    char* prompt = malloc(MAX_PROMPT_SIZE);
    if (!prompt) return NULL;

    if (code_snippet) {
        snprintf(prompt, MAX_PROMPT_SIZE,
            "File: %s\n"
            "Line: %d\n"
            "Error: %s\n\n"
            "Code:\n"
            "```\n"
            "%.1000s\n"
            "```\n\n"
            "Provide a fix for this compilation error. "
            "Show the corrected code and explain the issue briefly.",
            filename ? filename : "unknown",
            line_number,
            error_msg,
            code_snippet
        );
    } else {
        snprintf(prompt, MAX_PROMPT_SIZE,
            "File: %s\n"
            "Line: %d\n"
            "Error: %s\n\n"
            "What's the likely cause of this error and how can I fix it? "
            "Be specific and concise.",
            filename ? filename : "unknown",
            line_number,
            error_msg
        );
    }

    return prompt;
}

/**
 * Generate prompt for resolving missing dependencies
 * @param dependency Name of the missing dependency
 * @param build_system The build system being used
 * @param os_type Operating system type (Windows/Linux/macOS)
 * @return Allocated prompt string (caller must free)
 */
char* prompt_resolve_dependency(const char* dependency,
                                BuildSystem build_system,
                                const char* os_type) {
    if (!dependency) return NULL;

    char* prompt = malloc(MAX_PROMPT_SIZE);
    if (!prompt) return NULL;

    const char* build_sys = build_system_to_string(build_system);

    snprintf(prompt, MAX_PROMPT_SIZE,
        "I need to install '%s' for my %s project on %s.\n\n"
        "Please provide:\n"
        "1. The package manager command to install it\n"
        "2. Alternative installation methods if the first doesn't work\n"
        "3. How to verify it's installed correctly\n"
        "4. Common issues and solutions\n\n"
        "Be concise and practical.",
        dependency,
        build_sys,
        os_type ? os_type : "this system"
    );

    return prompt;
}

/**
 * Generate prompt for understanding a linker error
 * @param error_output The linker error output
 * @param undefined_symbols List of undefined symbols (optional)
 * @return Allocated prompt string (caller must free)
 */
char* prompt_analyze_linker_error(const char* error_output,
                                   const char* undefined_symbols) {
    if (!error_output) return NULL;

    char* prompt = malloc(MAX_PROMPT_SIZE);
    if (!prompt) return NULL;

    if (undefined_symbols) {
        snprintf(prompt, MAX_PROMPT_SIZE,
            "I'm getting a linker error:\n\n"
            "```\n"
            "%.2000s\n"
            "```\n\n"
            "Undefined symbols:\n"
            "%.1000s\n\n"
            "What libraries or source files am I missing? "
            "How do I fix this?",
            error_output,
            undefined_symbols
        );
    } else {
        snprintf(prompt, MAX_PROMPT_SIZE,
            "I'm getting this linker error:\n\n"
            "```\n"
            "%.3000s\n"
            "```\n\n"
            "What's causing this and how do I fix it? "
            "Be specific about what to add to my build configuration.",
            error_output
        );
    }

    return prompt;
}

/**
 * Generate prompt for optimizing build configuration
 * @param ctx Project context
 * @param build_time Current build time in seconds
 * @return Allocated prompt string (caller must free)
 */
char* prompt_optimize_build(const ProjectContext* ctx, double build_time) {
    if (!ctx) return NULL;

    char* prompt = malloc(MAX_PROMPT_SIZE);
    if (!prompt) return NULL;

    const char* build_sys = build_system_to_string(ctx->build_system.type);

    const char* lang_str = language_to_string(ctx->primary_language);

    snprintf(prompt, MAX_PROMPT_SIZE,
        "My %s project using %s takes %.1f seconds to build.\n"
        "Project size: %zu source files\n"
        "Primary language: %s\n\n"
        "Suggest optimizations to speed up the build:\n"
        "1. Build system configuration changes\n"
        "2. Parallel build settings\n"
        "3. Caching strategies\n"
        "4. Incremental build improvements\n\n"
        "Focus on practical changes with the biggest impact.",
        lang_str,
        build_sys,
        build_time,
        ctx->source_file_count,
        lang_str
    );

    return prompt;
}

/**
 * Generate prompt for creating build configuration
 * @param project_type Type of project (library/application/etc)
 * @param language Primary programming language
 * @param dependencies List of dependencies (comma-separated)
 * @return Allocated prompt string (caller must free)
 */
char* prompt_create_build_config(const char* project_type,
                                  const char* language,
                                  const char* dependencies) {
    if (!language) return NULL;

    char* prompt = malloc(MAX_PROMPT_SIZE);
    if (!prompt) return NULL;

    snprintf(prompt, MAX_PROMPT_SIZE,
        "Create a minimal CMakeLists.txt for a %s %s project.\n"
        "%s%s%s"
        "\n"
        "Requirements:\n"
        "1. Use modern CMake (3.20+)\n"
        "2. Set up proper target with PUBLIC/PRIVATE/INTERFACE\n"
        "3. Enable reasonable warnings\n"
        "4. Support Debug and Release builds\n\n"
        "Keep it minimal but complete.",
        language,
        project_type ? project_type : "application",
        dependencies ? "Dependencies: " : "",
        dependencies ? dependencies : "",
        dependencies ? "\n" : ""
    );

    return prompt;
}

/**
 * Parse error type from compiler output
 */
typedef enum {
    ERROR_COMPILATION,
    ERROR_LINKER,
    ERROR_MISSING_HEADER,
    ERROR_MISSING_LIB,
    ERROR_SYNTAX,
    ERROR_UNKNOWN
} ErrorType;

static ErrorType detect_error_type(const char* error_output) {
    if (!error_output) return ERROR_UNKNOWN;

    /* Check for common patterns */
    if (strstr(error_output, "undefined reference") ||
        strstr(error_output, "unresolved external symbol") ||
        strstr(error_output, "ld returned") ||
        strstr(error_output, "LINK :")) {
        return ERROR_LINKER;
    }

    if (strstr(error_output, "cannot find -l") ||
        strstr(error_output, "library not found")) {
        return ERROR_MISSING_LIB;
    }

    if (strstr(error_output, "No such file or directory") ||
        strstr(error_output, "cannot open include file") ||
        strstr(error_output, "fatal error:")) {
        return ERROR_MISSING_HEADER;
    }

    if (strstr(error_output, "syntax error") ||
        strstr(error_output, "expected") ||
        strstr(error_output, "before")) {
        return ERROR_SYNTAX;
    }

    if (strstr(error_output, "error:") ||
        strstr(error_output, "error C")) {
        return ERROR_COMPILATION;
    }

    return ERROR_UNKNOWN;
}

/**
 * Generate smart prompt based on error analysis
 * @param error_output The complete error output
 * @param ctx Project context
 * @return Allocated prompt string (caller must free)
 */
char* prompt_smart_error_analysis(const char* error_output,
                                   const ProjectContext* ctx) {
    if (!error_output) return NULL;

    ErrorType error_type = detect_error_type(error_output);

    switch (error_type) {
        case ERROR_LINKER: {
            /* Extract undefined symbols if possible */
            char symbols[1024] = {0};
            const char* p = strstr(error_output, "undefined reference to");
            if (p) {
                /* Try to extract symbol names */
                snprintf(symbols, sizeof(symbols), "%.500s", p);
            }
            return prompt_analyze_linker_error(error_output,
                                                symbols[0] ? symbols : NULL);
        }

        case ERROR_MISSING_LIB: {
            /* Extract library name */
            const char* p = strstr(error_output, "cannot find -l");
            if (p) {
                char lib_name[256] = {0};
                sscanf(p, "cannot find -l%255s", lib_name);
                return prompt_resolve_dependency(lib_name,
                                                  ctx ? ctx->build_system.type : BUILD_UNKNOWN,
#ifdef _WIN32
                                                  "Windows");
#elif __APPLE__
                                                  "macOS");
#else
                                                  "Linux");
#endif
            }
            break;
        }

        case ERROR_MISSING_HEADER: {
            /* Extract header name */
            const char* p = strstr(error_output, "No such file or directory");
            if (!p) p = strstr(error_output, "cannot open include file");
            if (p) {
                /* Try to find the header name in quotes */
                const char* start = strchr(error_output, '"');
                const char* end = start ? strchr(start + 1, '"') : NULL;
                if (start && end) {
                    size_t len = end - start - 1;
                    char* header = malloc(len + 1);
                    strncpy(header, start + 1, len);
                    header[len] = '\0';

                    char* prompt = prompt_resolve_dependency(header,
                                                              ctx ? ctx->build_system.type : BUILD_UNKNOWN,
#ifdef _WIN32
                                                              "Windows");
#elif __APPLE__
                                                              "macOS");
#else
                                                              "Linux");
#endif
                    free(header);
                    return prompt;
                }
            }
            break;
        }

        default:
            /* Fall back to general error analysis */
            return prompt_analyze_build_error(error_output,
                                               ctx ? ctx->build_system.type : BUILD_UNKNOWN,
                                               ctx ? language_to_string(ctx->primary_language) : NULL);
    }

    /* Default fallback */
    return prompt_analyze_build_error(error_output,
                                       ctx ? ctx->build_system.type : BUILD_UNKNOWN,
                                       ctx ? language_to_string(ctx->primary_language) : NULL);
}

/**
 * Format LLM response for display
 * @param response Raw response from LLM
 * @return Formatted response (caller must free)
 */
char* format_llm_response(const char* response) {
    if (!response) return NULL;

    /* For now, just return a copy. Could add formatting later */
    return strdup(response);
}

/* ========================================================================
 * REPL Context-Aware Prompts
 * ======================================================================== */

char* prompt_explain_with_context(const char* query,
                                   const char* current_file,
                                   const char* file_content,
                                   const char* conversation_context) {
    if (!query) return NULL;

    char* prompt = malloc(MAX_PROMPT_SIZE);
    if (!prompt) return NULL;

    size_t offset = 0;

    /* System instruction */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "You are a helpful coding assistant. Explain concepts clearly and concisely.\n\n");

    /* Add conversation context if available */
    if (conversation_context && strlen(conversation_context) > 0) {
        offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
            "Recent conversation:\n%s\n\n", conversation_context);
    }

    /* Add current file context if available */
    if (current_file) {
        offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
            "Current file: %s\n", current_file);
    }

    if (file_content && strlen(file_content) > 0) {
        offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
            "\nFile content:\n```\n%.2000s\n```\n\n", file_content);
    }

    /* The actual query */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "User question: %s\n\n"
        "Provide a clear, concise explanation. "
        "If referring to specific code, quote the relevant lines.",
        query);

    return prompt;
}

char* prompt_fix_with_context(const char* error_message,
                               const char* current_file,
                               const char* file_content,
                               const char* conversation_context) {
    if (!error_message) return NULL;

    char* prompt = malloc(MAX_PROMPT_SIZE);
    if (!prompt) return NULL;

    size_t offset = 0;

    /* System instruction */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "You are an expert debugger. Analyze the error and provide a fix.\n\n");

    /* Add conversation context if available */
    if (conversation_context && strlen(conversation_context) > 0) {
        offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
            "Recent context:\n%s\n\n", conversation_context);
    }

    /* Add file context */
    if (current_file) {
        offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
            "File: %s\n", current_file);
    }

    if (file_content && strlen(file_content) > 0) {
        offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
            "\nCode:\n```\n%.2000s\n```\n\n", file_content);
    }

    /* The error */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "Error to fix:\n%s\n\n"
        "Provide:\n"
        "1. The cause of this error\n"
        "2. The specific fix (show corrected code if applicable)\n"
        "3. How to prevent this in the future\n\n"
        "Be concise and actionable.",
        error_message);

    return prompt;
}

char* prompt_general_assistance(const char* user_query,
                                 const char* conversation_context) {
    if (!user_query) return NULL;

    char* prompt = malloc(MAX_PROMPT_SIZE);
    if (!prompt) return NULL;

    size_t offset = 0;

    /* System instruction */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "You are CyxMake, an AI build assistant. Help with build systems, "
        "compilation, debugging, and general development questions.\n\n");

    /* Add conversation context if available */
    if (conversation_context && strlen(conversation_context) > 0) {
        offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
            "Conversation history:\n%s\n\n", conversation_context);
    }

    /* The query */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "User: %s\n\n"
        "Respond helpfully and concisely.",
        user_query);

    return prompt;
}

/* ========================================================================
 * AI Agent System
 * ======================================================================== */

char* prompt_ai_agent(const char* user_request,
                       const char* current_dir,
                       const char* current_file,
                       const char* last_error,
                       const char* conversation_context) {
    if (!user_request) return NULL;

    char* prompt = malloc(MAX_PROMPT_SIZE);
    if (!prompt) return NULL;

    size_t offset = 0;

    /* System instruction with available actions */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "You are CyxMake AI Agent. You can perform actions to help the user.\n\n"
        "AVAILABLE ACTIONS:\n"
        "- read_file: Read and display a file's contents\n"
        "- create_file: Create a new file with specified content\n"
        "- delete_file: Delete a file\n"
        "- delete_dir: Delete a directory and its contents\n"
        "- build: Build the project\n"
        "- clean: Clean build artifacts\n"
        "- install: Install a package/dependency\n"
        "- run_command: Run a shell command\n"
        "- list_files: List files in a directory\n"
        "- none: Just respond without performing an action\n\n");

    /* Add context */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "CURRENT CONTEXT:\n"
        "- Working directory: %s\n",
        current_dir ? current_dir : ".");

    if (current_file) {
        offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
            "- Current file: %s\n", current_file);
    }

    if (last_error) {
        offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
            "- Last error: %s\n", last_error);
    }

    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset, "\n");

    /* Add conversation context if available */
    if (conversation_context && strlen(conversation_context) > 0) {
        offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
            "RECENT CONVERSATION:\n%s\n\n", conversation_context);
    }

    /* User request */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "USER REQUEST: %s\n\n", user_request);

    /* Response format */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "Respond with JSON in this exact format:\n"
        "```json\n"
        "{\n"
        "  \"message\": \"Brief explanation of what you will do\",\n"
        "  \"actions\": [\n"
        "    {\n"
        "      \"action\": \"<action_type>\",\n"
        "      \"target\": \"<file path, package name, or directory>\",\n"
        "      \"content\": \"<file content for create_file, command for run_command, or null>\",\n"
        "      \"reason\": \"<brief reason for this action>\"\n"
        "    }\n"
        "  ],\n"
        "  \"needs_confirmation\": true\n"
        "}\n"
        "```\n\n"
        "RULES:\n"
        "1. Set needs_confirmation to true for destructive actions (delete, run_command)\n"
        "2. Set needs_confirmation to false for safe actions (read, list, build)\n"
        "3. For multiple steps, include multiple actions in order\n"
        "4. If unsure or request is unclear, set action to 'none' and ask for clarification\n"
        "5. Only include the JSON, no other text\n");

    return prompt;
}

/* Parse action type from string */
static AIActionType parse_action_type(const char* action_str) {
    if (!action_str) return AI_ACTION_NONE;

    if (strcmp(action_str, "read_file") == 0) return AI_ACTION_READ_FILE;
    if (strcmp(action_str, "create_file") == 0) return AI_ACTION_CREATE_FILE;
    if (strcmp(action_str, "delete_file") == 0) return AI_ACTION_DELETE_FILE;
    if (strcmp(action_str, "delete_dir") == 0) return AI_ACTION_DELETE_DIR;
    if (strcmp(action_str, "build") == 0) return AI_ACTION_BUILD;
    if (strcmp(action_str, "clean") == 0) return AI_ACTION_CLEAN;
    if (strcmp(action_str, "install") == 0) return AI_ACTION_INSTALL;
    if (strcmp(action_str, "run_command") == 0) return AI_ACTION_RUN_COMMAND;
    if (strcmp(action_str, "list_files") == 0) return AI_ACTION_LIST_FILES;

    return AI_ACTION_NONE;
}

/* Extract JSON string value */
static char* extract_json_string(const char* json, const char* key) {
    if (!json || !key) return NULL;

    /* Build search pattern: "key": " */
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);

    const char* start = strstr(json, pattern);
    if (!start) return NULL;

    start += strlen(pattern);

    /* Skip whitespace */
    while (*start == ' ' || *start == '\t' || *start == '\n') start++;

    /* Check for null */
    if (strncmp(start, "null", 4) == 0) return NULL;

    /* Find opening quote */
    if (*start != '"') return NULL;
    start++;

    /* Find closing quote (handle escaped quotes) */
    const char* end = start;
    while (*end && !(*end == '"' && *(end-1) != '\\')) {
        end++;
    }

    if (*end != '"') return NULL;

    /* Extract string */
    size_t len = end - start;
    char* result = malloc(len + 1);
    if (!result) return NULL;

    /* Copy and unescape basic escapes */
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (start[i] == '\\' && i + 1 < len) {
            i++;
            switch (start[i]) {
                case 'n': result[j++] = '\n'; break;
                case 't': result[j++] = '\t'; break;
                case '"': result[j++] = '"'; break;
                case '\\': result[j++] = '\\'; break;
                default: result[j++] = start[i]; break;
            }
        } else {
            result[j++] = start[i];
        }
    }
    result[j] = '\0';

    return result;
}

/* Extract JSON boolean value */
static bool extract_json_bool(const char* json, const char* key, bool default_val) {
    if (!json || !key) return default_val;

    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);

    const char* start = strstr(json, pattern);
    if (!start) return default_val;

    start += strlen(pattern);
    while (*start == ' ' || *start == '\t' || *start == '\n') start++;

    if (strncmp(start, "true", 4) == 0) return true;
    if (strncmp(start, "false", 5) == 0) return false;

    return default_val;
}

AIAgentResponse* parse_ai_agent_response(const char* response) {
    if (!response) return NULL;

    AIAgentResponse* result = calloc(1, sizeof(AIAgentResponse));
    if (!result) return NULL;

    /* Find JSON block (look for ```json or just {) */
    const char* json_start = strstr(response, "```json");
    if (json_start) {
        json_start += 7;
        while (*json_start == '\n' || *json_start == '\r') json_start++;
    } else {
        json_start = strchr(response, '{');
    }

    if (!json_start) {
        /* No JSON found, treat as plain message */
        result->message = strdup(response);
        result->needs_confirmation = false;
        return result;
    }

    /* Find end of JSON */
    const char* json_end = strstr(json_start, "```");
    if (!json_end) {
        /* Find last closing brace */
        json_end = strrchr(json_start, '}');
        if (json_end) json_end++;
    }

    if (!json_end) {
        result->message = strdup(response);
        return result;
    }

    /* Copy JSON for parsing */
    size_t json_len = json_end - json_start;
    char* json = malloc(json_len + 1);
    if (!json) {
        free(result);
        return NULL;
    }
    strncpy(json, json_start, json_len);
    json[json_len] = '\0';

    /* Extract message */
    result->message = extract_json_string(json, "message");

    /* Extract needs_confirmation */
    result->needs_confirmation = extract_json_bool(json, "needs_confirmation", true);

    /* Parse actions array */
    const char* actions_start = strstr(json, "\"actions\"");
    if (actions_start) {
        actions_start = strchr(actions_start, '[');
        if (actions_start) {
            actions_start++;

            AIAction* last_action = NULL;

            /* Parse each action object */
            const char* action_start = strchr(actions_start, '{');
            while (action_start) {
                /* Find end of this action object */
                const char* action_end = strchr(action_start, '}');
                if (!action_end) break;

                /* Extract action object substring */
                size_t action_len = action_end - action_start + 1;
                char* action_json = malloc(action_len + 1);
                if (!action_json) break;
                strncpy(action_json, action_start, action_len);
                action_json[action_len] = '\0';

                /* Parse action fields */
                char* action_type_str = extract_json_string(action_json, "action");
                AIActionType action_type = parse_action_type(action_type_str);
                free(action_type_str);

                if (action_type != AI_ACTION_NONE) {
                    AIAction* action = calloc(1, sizeof(AIAction));
                    if (action) {
                        action->type = action_type;
                        action->target = extract_json_string(action_json, "target");
                        action->content = extract_json_string(action_json, "content");
                        action->reason = extract_json_string(action_json, "reason");

                        /* Add to chain */
                        if (last_action) {
                            last_action->next = action;
                        } else {
                            result->actions = action;
                        }
                        last_action = action;
                    }
                }

                free(action_json);

                /* Find next action object */
                action_start = strchr(action_end + 1, '{');

                /* Check if we've passed the actions array */
                const char* array_end = strchr(actions_start, ']');
                if (array_end && action_start > array_end) break;
            }
        }
    }

    free(json);
    return result;
}

void ai_action_free(AIAction* action) {
    while (action) {
        AIAction* next = action->next;
        free(action->target);
        free(action->content);
        free(action->reason);
        free(action);
        action = next;
    }
}

void ai_agent_response_free(AIAgentResponse* response) {
    if (!response) return;
    free(response->message);
    ai_action_free(response->actions);
    free(response);
}

const char* ai_action_type_name(AIActionType type) {
    switch (type) {
        case AI_ACTION_READ_FILE:    return "Read file";
        case AI_ACTION_CREATE_FILE:  return "Create file";
        case AI_ACTION_DELETE_FILE:  return "Delete file";
        case AI_ACTION_DELETE_DIR:   return "Delete directory";
        case AI_ACTION_BUILD:        return "Build project";
        case AI_ACTION_CLEAN:        return "Clean build";
        case AI_ACTION_INSTALL:      return "Install package";
        case AI_ACTION_RUN_COMMAND:  return "Run command";
        case AI_ACTION_LIST_FILES:   return "List files";
        case AI_ACTION_MULTI:        return "Multiple actions";
        case AI_ACTION_NONE:
        default:                     return "No action";
    }
}

/* ========================================================================
 * Natural Language Command Parsing
 * ======================================================================== */

#ifdef _WIN32
    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
#endif

/* Helper to check if string contains word (case-insensitive) */
static bool contains_word(const char* str, const char* word) {
    if (!str || !word) return false;

    char* lower_str = strdup(str);
    char* lower_word = strdup(word);

    /* Convert to lowercase */
    for (char* p = lower_str; *p; p++) *p = (char)tolower(*p);
    for (char* p = lower_word; *p; p++) *p = (char)tolower(*p);

    bool found = strstr(lower_str, lower_word) != NULL;

    free(lower_str);
    free(lower_word);
    return found;
}

/* Helper to extract file path or target from input */
static char* extract_target(const char* input) {
    if (!input) return NULL;

    /* Look for common file extensions */
    const char* extensions[] = {
        ".c", ".cpp", ".h", ".hpp", ".md", ".txt", ".json", ".yaml", ".yml",
        ".cmake", ".py", ".rs", ".go", ".js", ".ts", NULL
    };

    /* Find any word that looks like a file */
    char* copy = strdup(input);
    char* token = strtok(copy, " \t\n");

    while (token) {
        /* Check if it has an extension */
        for (int i = 0; extensions[i]; i++) {
            if (strstr(token, extensions[i])) {
                char* result = strdup(token);
                free(copy);
                return result;
            }
        }

        /* Check if it looks like a path */
        if (strchr(token, '/') || strchr(token, '\\')) {
            char* result = strdup(token);
            free(copy);
            return result;
        }

        token = strtok(NULL, " \t\n");
    }

    free(copy);
    return NULL;
}

/* Helper to extract package name after "install" keyword */
static char* extract_package_name(const char* input) {
    if (!input) return NULL;

    /* Find "install" keyword and get the next word(s) */
    char* lower = strdup(input);
    for (char* p = lower; *p; p++) *p = (char)tolower(*p);

    const char* keywords[] = {"install", "add", "get", NULL};
    char* result = NULL;

    for (int i = 0; keywords[i]; i++) {
        char* pos = strstr(lower, keywords[i]);
        if (pos) {
            /* Skip past the keyword */
            pos += strlen(keywords[i]);

            /* Skip whitespace */
            while (*pos == ' ' || *pos == '\t') pos++;

            /* Skip common filler words */
            const char* fillers[] = {"package", "library", "lib", "the", "a", NULL};
            for (int j = 0; fillers[j]; j++) {
                size_t flen = strlen(fillers[j]);
                if (strncmp(pos, fillers[j], flen) == 0 && (pos[flen] == ' ' || pos[flen] == '\t')) {
                    pos += flen;
                    while (*pos == ' ' || *pos == '\t') pos++;
                }
            }

            /* Extract the package name (rest until end or common stopwords) */
            if (*pos) {
                /* Find end of package name */
                const char* end = pos;
                while (*end && *end != ' ' && *end != '\t' && *end != '\n') end++;

                size_t len = end - pos;
                if (len > 0) {
                    /* Calculate offset in original string */
                    size_t offset = pos - lower;
                    result = malloc(len + 1);
                    strncpy(result, input + offset, len);
                    result[len] = '\0';
                }
            }
            break;
        }
    }

    free(lower);
    return result;
}

/* Parse a natural language command locally (fast, no AI) */
ParsedCommand* parse_command_local(const char* input) {
    if (!input || strlen(input) == 0) return NULL;

    ParsedCommand* cmd = calloc(1, sizeof(ParsedCommand));
    if (!cmd) return NULL;

    cmd->intent = INTENT_UNKNOWN;
    cmd->confidence = 0.0;

    /* Clean keywords - check before build since "clean up the build" should match clean */
    if (contains_word(input, "clean") ||
        contains_word(input, "clear") ||
        contains_word(input, "remove build") ||
        contains_word(input, "delete build")) {
        cmd->intent = INTENT_CLEAN;
        cmd->confidence = 0.9;
    }
    /* Build keywords */
    else if (contains_word(input, "build") ||
             contains_word(input, "compile") ||
             contains_word(input, "make")) {
        cmd->intent = INTENT_BUILD;
        cmd->confidence = 0.9;
    }
    /* Initialize keywords */
    else if (contains_word(input, "init") ||
             contains_word(input, "analyze") ||
             contains_word(input, "scan") ||
             contains_word(input, "detect")) {
        cmd->intent = INTENT_INIT;
        cmd->confidence = 0.85;
    }
    /* Test keywords */
    else if (contains_word(input, "test") ||
             contains_word(input, "run test") ||
             contains_word(input, "check")) {
        cmd->intent = INTENT_TEST;
        cmd->confidence = 0.85;
    }
    /* Create file keywords */
    else if (contains_word(input, "create") ||
             contains_word(input, "new file") ||
             contains_word(input, "generate") ||
             contains_word(input, "make a") ||
             contains_word(input, "write")) {
        cmd->intent = INTENT_CREATE_FILE;
        cmd->confidence = 0.8;
        cmd->target = extract_target(input);
    }
    /* Read file keywords */
    else if (contains_word(input, "read") ||
             contains_word(input, "show") ||
             contains_word(input, "display") ||
             contains_word(input, "cat") ||
             contains_word(input, "view") ||
             contains_word(input, "open")) {
        cmd->intent = INTENT_READ_FILE;
        cmd->confidence = 0.8;
        cmd->target = extract_target(input);
    }
    /* Explain keywords */
    else if (contains_word(input, "explain") ||
             contains_word(input, "what is") ||
             contains_word(input, "what does") ||
             contains_word(input, "how does") ||
             contains_word(input, "why")) {
        cmd->intent = INTENT_EXPLAIN;
        cmd->confidence = 0.75;
    }
    /* Fix keywords */
    else if (contains_word(input, "fix") ||
             contains_word(input, "repair") ||
             contains_word(input, "solve") ||
             contains_word(input, "debug")) {
        cmd->intent = INTENT_FIX;
        cmd->confidence = 0.85;
    }
    /* Install keywords */
    else if (contains_word(input, "install") ||
             contains_word(input, "add package") ||
             contains_word(input, "get package") ||
             contains_word(input, "add dependency")) {
        cmd->intent = INTENT_INSTALL;
        cmd->confidence = 0.9;
        cmd->target = extract_package_name(input);  /* Use package-aware extraction */
    }
    /* Status keywords */
    else if (contains_word(input, "status") ||
             contains_word(input, "info") ||
             contains_word(input, "state")) {
        cmd->intent = INTENT_STATUS;
        cmd->confidence = 0.9;
    }
    /* Help keywords */
    else if (contains_word(input, "help") ||
             contains_word(input, "how to") ||
             contains_word(input, "usage")) {
        cmd->intent = INTENT_HELP;
        cmd->confidence = 0.9;
    }

    /* Store the full input as details */
    cmd->details = strdup(input);

    return cmd;
}

/* Generate prompt for AI command parsing */
char* prompt_parse_command(const char* user_input) {
    if (!user_input) return NULL;

    char* prompt = malloc(MAX_PROMPT_SIZE);
    if (!prompt) return NULL;

    snprintf(prompt, MAX_PROMPT_SIZE,
        "You are a build system assistant. Parse this user command and respond with ONLY a JSON object.\n\n"
        "User command: \"%s\"\n\n"
        "Respond with JSON in this exact format:\n"
        "{\n"
        "  \"intent\": \"<one of: build, init, clean, test, create_file, read_file, explain, fix, install, status, help, unknown>\",\n"
        "  \"target\": \"<file path, package name, or null>\",\n"
        "  \"details\": \"<brief description of what to do>\"\n"
        "}\n\n"
        "Examples:\n"
        "- \"build the project\" -> {\"intent\": \"build\", \"target\": null, \"details\": \"compile the project\"}\n"
        "- \"create readme.md\" -> {\"intent\": \"create_file\", \"target\": \"readme.md\", \"details\": \"create a new readme file\"}\n"
        "- \"install SDL2\" -> {\"intent\": \"install\", \"target\": \"SDL2\", \"details\": \"install SDL2 library\"}\n\n"
        "Respond with ONLY the JSON, no explanation.",
        user_input
    );

    return prompt;
}

/* Parse AI response to extract intent */
static ParsedCommand* parse_ai_response(const char* response) {
    if (!response) return NULL;

    ParsedCommand* cmd = calloc(1, sizeof(ParsedCommand));
    if (!cmd) return NULL;

    cmd->intent = INTENT_UNKNOWN;
    cmd->confidence = 0.7;  /* AI responses get moderate confidence */

    /* Simple JSON parsing - look for intent field */
    const char* intent_start = strstr(response, "\"intent\"");
    if (intent_start) {
        intent_start = strchr(intent_start, ':');
        if (intent_start) {
            intent_start = strchr(intent_start, '"');
            if (intent_start) {
                intent_start++;
                char intent[64] = {0};
                sscanf(intent_start, "%63[^\"]", intent);

                if (strcmp(intent, "build") == 0) cmd->intent = INTENT_BUILD;
                else if (strcmp(intent, "init") == 0) cmd->intent = INTENT_INIT;
                else if (strcmp(intent, "clean") == 0) cmd->intent = INTENT_CLEAN;
                else if (strcmp(intent, "test") == 0) cmd->intent = INTENT_TEST;
                else if (strcmp(intent, "create_file") == 0) cmd->intent = INTENT_CREATE_FILE;
                else if (strcmp(intent, "read_file") == 0) cmd->intent = INTENT_READ_FILE;
                else if (strcmp(intent, "explain") == 0) cmd->intent = INTENT_EXPLAIN;
                else if (strcmp(intent, "fix") == 0) cmd->intent = INTENT_FIX;
                else if (strcmp(intent, "install") == 0) cmd->intent = INTENT_INSTALL;
                else if (strcmp(intent, "status") == 0) cmd->intent = INTENT_STATUS;
                else if (strcmp(intent, "help") == 0) cmd->intent = INTENT_HELP;

                cmd->confidence = 0.85;
            }
        }
    }

    /* Extract target */
    const char* target_start = strstr(response, "\"target\"");
    if (target_start) {
        target_start = strchr(target_start, ':');
        if (target_start) {
            /* Skip whitespace */
            target_start++;
            while (*target_start == ' ' || *target_start == '\t') target_start++;

            if (*target_start == '"') {
                target_start++;
                char target[256] = {0};
                sscanf(target_start, "%255[^\"]", target);
                if (strlen(target) > 0 && strcmp(target, "null") != 0) {
                    cmd->target = strdup(target);
                }
            }
        }
    }

    /* Extract details */
    const char* details_start = strstr(response, "\"details\"");
    if (details_start) {
        details_start = strchr(details_start, ':');
        if (details_start) {
            details_start = strchr(details_start, '"');
            if (details_start) {
                details_start++;
                char details[512] = {0};
                sscanf(details_start, "%511[^\"]", details);
                if (strlen(details) > 0) {
                    cmd->details = strdup(details);
                }
            }
        }
    }

    return cmd;
}

/* Parse a natural language command using AI */
ParsedCommand* parse_command_with_ai(const char* input, LLMContext* llm) {
    if (!input || !llm || !llm_is_ready(llm)) {
        return parse_command_local(input);
    }

    log_debug("Parsing command with AI: %s", input);

    /* Generate prompt */
    char* prompt = prompt_parse_command(input);
    if (!prompt) {
        return parse_command_local(input);
    }

    /* Create request */
    LLMRequest* request = llm_request_create(prompt);
    if (!request) {
        free(prompt);
        return parse_command_local(input);
    }

    request->temperature = 0.1f;  /* Low temperature for consistent parsing */
    request->max_tokens = 256;

    /* Query LLM */
    LLMResponse* response = llm_query(llm, request);

    ParsedCommand* cmd = NULL;
    if (response && response->success && response->text) {
        cmd = parse_ai_response(response->text);
        if (cmd) {
            log_debug("AI parsed intent: %d, target: %s",
                     cmd->intent, cmd->target ? cmd->target : "none");
        }
    }

    /* Cleanup */
    llm_response_free(response);
    llm_request_free(request);
    free(prompt);

    /* Fallback to local parsing if AI failed */
    if (!cmd) {
        cmd = parse_command_local(input);
    }

    return cmd;
}

/* Free a parsed command */
void parsed_command_free(ParsedCommand* cmd) {
    if (!cmd) return;
    free(cmd->target);
    free(cmd->details);
    free(cmd);
}

/* Get intent name as string */
static const char* intent_to_string(CommandIntent intent) {
    switch (intent) {
        case INTENT_BUILD: return "build";
        case INTENT_INIT: return "init";
        case INTENT_CLEAN: return "clean";
        case INTENT_TEST: return "test";
        case INTENT_CREATE_FILE: return "create_file";
        case INTENT_READ_FILE: return "read_file";
        case INTENT_EXPLAIN: return "explain";
        case INTENT_FIX: return "fix";
        case INTENT_INSTALL: return "install";
        case INTENT_STATUS: return "status";
        case INTENT_HELP: return "help";
        default: return "unknown";
    }
}

/* Execute a natural language command */
char* execute_natural_command(const char* input,
                               LLMContext* llm,
                               const char* project_path) {
    if (!input) return NULL;

    (void)project_path;  /* Used in future for context */

    /* First try local parsing */
    ParsedCommand* cmd = parse_command_local(input);

    /* If confidence is low and AI is available, use AI parsing */
    if (cmd && cmd->confidence < 0.7 && llm && llm_is_ready(llm)) {
        parsed_command_free(cmd);
        cmd = parse_command_with_ai(input, llm);
    }

    if (!cmd) {
        return strdup("Failed to parse command");
    }

    char* result = malloc(1024);
    if (!result) {
        parsed_command_free(cmd);
        return NULL;
    }

    /* Generate result based on intent */
    snprintf(result, 1024,
             "Understood: %s%s%s\n"
             "Intent: %s (confidence: %.0f%%)",
             cmd->details ? cmd->details : input,
             cmd->target ? "\nTarget: " : "",
             cmd->target ? cmd->target : "",
             intent_to_string(cmd->intent),
             cmd->confidence * 100);

    parsed_command_free(cmd);
    return result;
}