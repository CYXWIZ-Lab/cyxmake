/**
 * @file ai_build_agent.c
 * @brief AI-First Autonomous Build Agent Implementation
 */

#include "cyxmake/ai_build_agent.h"
#include "cyxmake/build_intelligence.h"
#include "cyxmake/logger.h"
#include "cyxmake/compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define getcwd _getcwd
    #define chdir _chdir
    #define popen _popen
    #define pclose _pclose
#else
    #include <unistd.h>
#endif

#define MAX_PROMPT_SIZE 16384
#define MAX_RESPONSE_SIZE 8192

/* ========================================================================
 * AI Build Agent Structure
 * ======================================================================== */

struct AIBuildAgent {
    AIProvider* ai;
    ToolRegistry* tools;
    AIBuildAgentConfig config;
    int total_attempts;
    int successful_builds;
    char* last_error;
    char* attempted_fixes;  /* Track what we've tried */
};

/* ========================================================================
 * Configuration
 * ======================================================================== */

AIBuildAgentConfig ai_build_agent_config_default(void) {
    return (AIBuildAgentConfig){
        .max_attempts = 5,
        .max_fix_attempts = 3,
        .verbose = true,
        .auto_install_deps = true,
        .allow_file_mods = false,      /* Safe by default */
        .allow_commands = true,
        .temperature = 0.2f
    };
}

/* ========================================================================
 * Agent Lifecycle
 * ======================================================================== */

AIBuildAgent* ai_build_agent_create(AIProvider* ai,
                                     ToolRegistry* tools,
                                     const AIBuildAgentConfig* config) {
    if (!ai || !ai_provider_is_ready(ai)) {
        log_error("AI Build Agent requires a working AI provider");
        return NULL;
    }

    AIBuildAgent* agent = calloc(1, sizeof(AIBuildAgent));
    if (!agent) return NULL;

    agent->ai = ai;
    agent->tools = tools;

    if (config) {
        agent->config = *config;
    } else {
        agent->config = ai_build_agent_config_default();
    }

    log_info("AI Build Agent created (max_attempts=%d)", agent->config.max_attempts);
    return agent;
}

void ai_build_agent_free(AIBuildAgent* agent) {
    if (!agent) return;
    free(agent->last_error);
    free(agent->attempted_fixes);
    free(agent);
}

/* ========================================================================
 * Build Step Management
 * ======================================================================== */

AIBuildStep* ai_build_step_create(BuildStepType type,
                                   const char* description,
                                   const char* command,
                                   const char* target) {
    AIBuildStep* step = calloc(1, sizeof(AIBuildStep));
    if (!step) return NULL;

    step->type = type;
    if (description) step->description = strdup(description);
    if (command) step->command = strdup(command);
    if (target) step->target = strdup(target);

    return step;
}

void ai_build_step_free(AIBuildStep* step) {
    if (!step) return;
    free(step->description);
    free(step->command);
    free(step->target);
    free(step->content);
    free(step->reason);
    free(step->error_output);
    free(step);
}

const char* build_step_type_name(BuildStepType type) {
    switch (type) {
        case BUILD_STEP_CONFIGURE:    return "Configure";
        case BUILD_STEP_BUILD:        return "Build";
        case BUILD_STEP_INSTALL_DEP:  return "Install Dependency";
        case BUILD_STEP_CREATE_DIR:   return "Create Directory";
        case BUILD_STEP_RUN_COMMAND:  return "Run Command";
        case BUILD_STEP_MODIFY_FILE:  return "Modify File";
        case BUILD_STEP_SET_ENV:      return "Set Environment";
        case BUILD_STEP_CLEAN:        return "Clean";
        case BUILD_STEP_DONE:         return "Done";
        case BUILD_STEP_FAILED:       return "Failed";
        default:                      return "Unknown";
    }
}

/* ========================================================================
 * Build Plan Management
 * ======================================================================== */

AIBuildPlan* ai_build_plan_create(const char* project_path) {
    AIBuildPlan* plan = calloc(1, sizeof(AIBuildPlan));
    if (!plan) return NULL;

    if (project_path) {
        plan->project_path = strdup(project_path);
    }

    return plan;
}

void ai_build_plan_add_step(AIBuildPlan* plan, AIBuildStep* step) {
    if (!plan || !step) return;

    if (!plan->steps) {
        plan->steps = step;
    } else {
        AIBuildStep* current = plan->steps;
        while (current->next) {
            current = current->next;
        }
        current->next = step;
    }
    plan->step_count++;
}

void ai_build_plan_free(AIBuildPlan* plan) {
    if (!plan) return;

    AIBuildStep* step = plan->steps;
    while (step) {
        AIBuildStep* next = step->next;
        ai_build_step_free(step);
        step = next;
    }

    free(plan->project_path);
    free(plan->summary);
    free(plan);
}

void ai_build_plan_print(const AIBuildPlan* plan) {
    if (!plan) return;

    log_info("=== Build Plan ===");
    if (plan->summary) {
        log_info("Summary: %s", plan->summary);
    }
    log_info("Steps: %d", plan->step_count);
    log_plain("");

    int i = 1;
    AIBuildStep* step = plan->steps;
    while (step) {
        const char* status = step->executed ?
            (step->success ? "[OK]" : "[FAIL]") : "[  ]";

        log_plain("  %s %d. [%s] %s",
                  status, i,
                  build_step_type_name(step->type),
                  step->description ? step->description : "");

        if (step->command && step->type != BUILD_STEP_BUILD) {
            log_plain("      Command: %s", step->command);
        }
        if (step->reason) {
            log_plain("      Reason: %s", step->reason);
        }

        step = step->next;
        i++;
    }
    log_plain("");
}

/* ========================================================================
 * Prompt Generation
 * ======================================================================== */

char* prompt_ai_build_plan(const ProjectContext* ctx,
                           const char* build_output,
                           const char* previous_errors) {
    char* prompt = malloc(MAX_PROMPT_SIZE);
    if (!prompt) return NULL;

    size_t offset = 0;

    /* System instruction */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "You are an expert build system AI. Your task is to analyze a project "
        "and create a step-by-step build plan. You must respond with ONLY valid JSON.\n\n");

    /* Project info */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "PROJECT INFORMATION:\n"
        "- Path: %s\n"
        "- Language: %s\n"
        "- Build System: %s\n"
        "- Source Files: %zu\n\n",
        ctx->root_path,
        language_to_string(ctx->primary_language),
        build_system_to_string(ctx->build_system.type),
        ctx->source_file_count);

    /* Previous build output if any */
    if (build_output && strlen(build_output) > 0) {
        offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
            "PREVIOUS BUILD OUTPUT:\n```\n%.4000s\n```\n\n",
            build_output);
    }

    /* Previous errors to avoid */
    if (previous_errors && strlen(previous_errors) > 0) {
        offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
            "PREVIOUS FAILED ATTEMPTS (do NOT repeat these):\n%s\n\n",
            previous_errors);
    }

    /* Available actions */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "AVAILABLE STEP TYPES:\n"
        "- configure: Run cmake/configure to set up build (command: cmake command)\n"
        "- build: Execute the build (command: build command)\n"
        "- install_dep: Install a dependency (target: package name, command: install command)\n"
        "- create_dir: Create a directory (target: directory path)\n"
        "- run_command: Run a shell command (command: the command)\n"
        "- clean: Clean build artifacts\n\n"
        "IMPORTANT CMAKE NOTES:\n"
        "- For CMake projects, always use: cmake -B build -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5\n"
        "- Build with: cmake --build build\n"
        "- Do NOT use 'cmake ..' - use the -B and -S flags for out-of-source builds\n"
        "- All commands are executed from the project root directory\n\n");

    /* Response format */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "Respond with JSON in this EXACT format:\n"
        "```json\n"
        "{\n"
        "  \"summary\": \"Brief description of what needs to be done\",\n"
        "  \"steps\": [\n"
        "    {\n"
        "      \"type\": \"configure|build|install_dep|create_dir|run_command|clean\",\n"
        "      \"description\": \"Human-readable description\",\n"
        "      \"command\": \"command to execute\",\n"
        "      \"target\": \"package/file/directory name if applicable\",\n"
        "      \"reason\": \"Why this step is needed\"\n"
        "    }\n"
        "  ]\n"
        "}\n"
        "```\n\n"
        "RULES:\n"
        "1. Always check if project needs configuration before building\n"
        "2. For CMake projects without build/, first run cmake to configure\n"
        "3. Include dependency installation if errors show missing packages\n"
        "4. Be specific with commands - use full paths when helpful\n"
        "5. Output ONLY the JSON, no explanation text\n");

    return prompt;
}

char* prompt_ai_error_fix(const char* error_output,
                          const ProjectContext* ctx,
                          const char* attempted_fixes) {
    char* prompt = malloc(MAX_PROMPT_SIZE);
    if (!prompt) return NULL;

    size_t offset = 0;

    /* System instruction */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "You are an expert build system debugger. A build has FAILED. "
        "Analyze the error and provide SPECIFIC fix steps. Respond with ONLY valid JSON.\n\n");

    /* Error output */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "BUILD ERROR OUTPUT:\n```\n%.6000s\n```\n\n",
        error_output);

    /* Project info */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "PROJECT:\n"
        "- Path: %s\n"
        "- Language: %s\n"
        "- Build System: %s\n\n",
        ctx->root_path,
        language_to_string(ctx->primary_language),
        build_system_to_string(ctx->build_system.type));

    /* What we've already tried */
    if (attempted_fixes && strlen(attempted_fixes) > 0) {
        offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
            "ALREADY ATTEMPTED (these did NOT work, try something different):\n%s\n\n",
            attempted_fixes);
    }

    /* Platform info */
#ifdef _WIN32
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "PLATFORM: Windows\n"
        "- Use winget, vcpkg, or choco for packages\n"
        "- Paths use backslashes\n\n");
#elif __APPLE__
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "PLATFORM: macOS\n"
        "- Use brew for packages\n\n");
#else
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "PLATFORM: Linux\n"
        "- Use apt, dnf, or pacman for packages\n\n");
#endif

    /* Response format */
    offset += snprintf(prompt + offset, MAX_PROMPT_SIZE - offset,
        "IMPORTANT CMAKE NOTES:\n"
        "- For CMake projects, always use: cmake -B build -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5\n"
        "- Build with: cmake --build build\n"
        "- Do NOT use 'cmake ..' - use the -B and -S flags for out-of-source builds\n"
        "- All commands are executed from the project root directory\n\n"
        "Respond with JSON fix steps:\n"
        "```json\n"
        "{\n"
        "  \"analysis\": \"What the error means\",\n"
        "  \"root_cause\": \"The actual problem\",\n"
        "  \"steps\": [\n"
        "    {\n"
        "      \"type\": \"install_dep|run_command|configure|clean\",\n"
        "      \"description\": \"What this fix does\",\n"
        "      \"command\": \"exact command to run\",\n"
        "      \"target\": \"package/file if applicable\",\n"
        "      \"reason\": \"Why this will fix the error\"\n"
        "    }\n"
        "  ]\n"
        "}\n"
        "```\n\n"
        "IMPORTANT:\n"
        "1. Analyze the ACTUAL error - don't guess\n"
        "2. Provide SPECIFIC commands for this platform\n"
        "3. If a package is missing, find the correct package name\n"
        "4. If configuration failed, fix the configure step\n"
        "5. Output ONLY the JSON\n");

    return prompt;
}

/* ========================================================================
 * JSON Parsing Helpers
 * ======================================================================== */

/* Extract string value from JSON */
static char* json_get_string(const char* json, const char* key) {
    if (!json || !key) return NULL;

    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char* key_pos = strstr(json, pattern);
    if (!key_pos) return NULL;

    /* Find the colon */
    const char* colon = strchr(key_pos + strlen(pattern), ':');
    if (!colon) return NULL;

    /* Skip whitespace */
    const char* start = colon + 1;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;

    /* Check for null */
    if (strncmp(start, "null", 4) == 0) return NULL;

    /* Must be a string */
    if (*start != '"') return NULL;
    start++;

    /* Find end quote (handle escapes) */
    const char* end = start;
    while (*end && !(*end == '"' && *(end-1) != '\\')) end++;

    if (!*end) return NULL;

    size_t len = end - start;
    char* result = malloc(len + 1);
    if (!result) return NULL;

    /* Copy and unescape */
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

/* Parse step type from string */
static BuildStepType parse_step_type(const char* type_str) {
    if (!type_str) return BUILD_STEP_RUN_COMMAND;

    if (strcmp(type_str, "configure") == 0) return BUILD_STEP_CONFIGURE;
    if (strcmp(type_str, "build") == 0) return BUILD_STEP_BUILD;
    if (strcmp(type_str, "install_dep") == 0) return BUILD_STEP_INSTALL_DEP;
    if (strcmp(type_str, "create_dir") == 0) return BUILD_STEP_CREATE_DIR;
    if (strcmp(type_str, "run_command") == 0) return BUILD_STEP_RUN_COMMAND;
    if (strcmp(type_str, "clean") == 0) return BUILD_STEP_CLEAN;

    return BUILD_STEP_RUN_COMMAND;
}

/* Parse build plan from AI response */
AIBuildPlan* parse_ai_build_plan_response(const char* response,
                                           const char* project_path) {
    if (!response) return NULL;

    AIBuildPlan* plan = ai_build_plan_create(project_path);
    if (!plan) return NULL;

    /* Find JSON block */
    const char* json_start = strstr(response, "```json");
    if (json_start) {
        json_start += 7;
        while (*json_start == '\n' || *json_start == '\r') json_start++;
    } else {
        json_start = strchr(response, '{');
    }

    if (!json_start) {
        log_warning("No JSON found in AI response");
        ai_build_plan_free(plan);
        return NULL;
    }

    /* Extract summary */
    plan->summary = json_get_string(json_start, "summary");
    if (!plan->summary) {
        plan->summary = json_get_string(json_start, "analysis");
    }

    /* Find steps array */
    const char* steps_start = strstr(json_start, "\"steps\"");
    if (!steps_start) {
        log_warning("No steps array in AI response");
        ai_build_plan_free(plan);
        return NULL;
    }

    steps_start = strchr(steps_start, '[');
    if (!steps_start) {
        ai_build_plan_free(plan);
        return NULL;
    }

    /* Parse each step object */
    const char* step_obj = strchr(steps_start, '{');
    while (step_obj) {
        /* Find end of this step object */
        const char* step_end = strchr(step_obj, '}');
        if (!step_end) break;

        /* Check if we've left the steps array */
        const char* array_end = strchr(steps_start, ']');
        if (array_end && step_obj > array_end) break;

        /* Extract step fields from this object */
        size_t step_len = step_end - step_obj + 1;
        char* step_json = malloc(step_len + 1);
        if (!step_json) break;
        strncpy(step_json, step_obj, step_len);
        step_json[step_len] = '\0';

        /* Parse step fields */
        char* type_str = json_get_string(step_json, "type");
        char* description = json_get_string(step_json, "description");
        char* command = json_get_string(step_json, "command");
        char* target = json_get_string(step_json, "target");
        char* reason = json_get_string(step_json, "reason");

        BuildStepType type = parse_step_type(type_str);

        /* Create and add step */
        AIBuildStep* step = ai_build_step_create(type, description, command, target);
        if (step) {
            step->reason = reason;
            reason = NULL;  /* Ownership transferred */
            ai_build_plan_add_step(plan, step);
        }

        /* Cleanup */
        free(type_str);
        free(description);
        free(command);
        free(target);
        free(reason);
        free(step_json);

        /* Find next step object */
        step_obj = strchr(step_end + 1, '{');
    }

    if (plan->step_count == 0) {
        log_warning("No steps parsed from AI response");
        ai_build_plan_free(plan);
        return NULL;
    }

    return plan;
}

/* ========================================================================
 * Step Execution
 * ======================================================================== */

/* Execute a command and capture output */
static char* execute_command(const char* command, const char* working_dir, int* exit_code) {
    if (!command) return NULL;

    char* old_dir = NULL;
    if (working_dir) {
        old_dir = getcwd(NULL, 0);
        if (chdir(working_dir) != 0) {
            log_error("Failed to change to directory: %s", working_dir);
            free(old_dir);
            return NULL;
        }
    }

    /* Build command with stderr redirect */
    char full_cmd[4096];
    snprintf(full_cmd, sizeof(full_cmd), "%s 2>&1", command);

    FILE* pipe = popen(full_cmd, "r");
    if (!pipe) {
        log_error("Failed to execute: %s", command);
        if (old_dir) {
            chdir(old_dir);
            free(old_dir);
        }
        return NULL;
    }

    /* Read output */
    char* output = malloc(MAX_RESPONSE_SIZE);
    if (!output) {
        pclose(pipe);
        if (old_dir) {
            chdir(old_dir);
            free(old_dir);
        }
        return NULL;
    }

    size_t offset = 0;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) && offset < MAX_RESPONSE_SIZE - 1) {
        size_t len = strlen(buffer);
        if (offset + len < MAX_RESPONSE_SIZE) {
            memcpy(output + offset, buffer, len);
            offset += len;
        }
    }
    output[offset] = '\0';

    int status = pclose(pipe);
    if (exit_code) {
#ifdef _WIN32
        *exit_code = status;
#else
        *exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
    }

    if (old_dir) {
        chdir(old_dir);
        free(old_dir);
    }

    return output;
}

bool ai_build_agent_execute_step(AIBuildAgent* agent,
                                  AIBuildStep* step,
                                  const ProjectContext* ctx) {
    if (!agent || !step || !ctx) return false;

    step->executed = true;

    if (agent->config.verbose) {
        log_info("[%s] %s",
                 build_step_type_name(step->type),
                 step->description ? step->description : "");
    }

    int exit_code = 0;
    char* output = NULL;

    switch (step->type) {
        case BUILD_STEP_CONFIGURE:
        case BUILD_STEP_BUILD:
        case BUILD_STEP_RUN_COMMAND:
        case BUILD_STEP_CLEAN:
            if (!step->command) {
                log_error("No command specified for step");
                step->success = false;
                return false;
            }

            if (agent->config.verbose) {
                log_debug("Executing: %s", step->command);
            }

            output = execute_command(step->command, ctx->root_path, &exit_code);
            step->error_output = output;
            step->success = (exit_code == 0);

            if (!step->success && agent->config.verbose) {
                log_warning("Command failed (exit %d)", exit_code);
                if (output && strlen(output) > 0) {
                    /* Show last few lines of error */
                    const char* last_lines = output;
                    int newlines = 0;
                    for (const char* p = output + strlen(output) - 1; p > output && newlines < 10; p--) {
                        if (*p == '\n') {
                            newlines++;
                            last_lines = p + 1;
                        }
                    }
                    log_plain("  ...%s", last_lines);
                }
            }
            break;

        case BUILD_STEP_INSTALL_DEP:
            if (!step->command) {
                log_error("No install command specified");
                step->success = false;
                return false;
            }

            log_info("Installing: %s", step->target ? step->target : "dependency");

            output = execute_command(step->command, NULL, &exit_code);
            step->error_output = output;
            step->success = (exit_code == 0);
            break;

        case BUILD_STEP_CREATE_DIR:
            if (!step->target) {
                step->success = false;
                return false;
            }

            {
                char mkdir_cmd[512];
#ifdef _WIN32
                snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir \"%s\" 2>nul", step->target);
#else
                snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", step->target);
#endif
                output = execute_command(mkdir_cmd, ctx->root_path, &exit_code);
                free(output);
                step->success = true;  /* mkdir usually succeeds or dir exists */
            }
            break;

        case BUILD_STEP_DONE:
            step->success = true;
            break;

        case BUILD_STEP_FAILED:
            step->success = false;
            break;

        default:
            log_warning("Unknown step type: %d", step->type);
            step->success = false;
            break;
    }

    return step->success;
}

/* ========================================================================
 * AI Planning
 * ======================================================================== */

/* Create a fallback plan using build intelligence when AI fails */
static AIBuildPlan* create_fallback_plan(const ProjectContext* ctx) {
    log_info("Using intelligent fallback build plan...");

    BuildIntelligencePlan* intel_plan = build_intelligence_fallback_plan(ctx);
    if (!intel_plan) return NULL;

    AIBuildPlan* plan = ai_build_plan_create(ctx->root_path);
    if (!plan) {
        build_intelligence_plan_free(intel_plan);
        return NULL;
    }

    plan->summary = strdup("Fallback build plan using known patterns");

    for (int i = 0; i < intel_plan->command_count; i++) {
        BuildStepType type = BUILD_STEP_RUN_COMMAND;

        /* Detect step type from command */
        if (strstr(intel_plan->commands[i], "cmake -B") ||
            strstr(intel_plan->commands[i], "cmake -S") ||
            strstr(intel_plan->commands[i], "configure") ||
            strstr(intel_plan->commands[i], "meson setup")) {
            type = BUILD_STEP_CONFIGURE;
        } else if (strstr(intel_plan->commands[i], "--build") ||
                   strstr(intel_plan->commands[i], "make") ||
                   strstr(intel_plan->commands[i], "cargo build") ||
                   strstr(intel_plan->commands[i], "gradle build") ||
                   strstr(intel_plan->commands[i], "npm run build")) {
            type = BUILD_STEP_BUILD;
        } else if (strstr(intel_plan->commands[i], "clean")) {
            type = BUILD_STEP_CLEAN;
        }

        AIBuildStep* step = ai_build_step_create(
            type,
            intel_plan->descriptions[i],
            intel_plan->commands[i],
            NULL
        );
        if (step) {
            step->reason = strdup("Standard build pattern for this project type");
            ai_build_plan_add_step(plan, step);
        }
    }

    build_intelligence_plan_free(intel_plan);
    return plan;
}

AIBuildPlan* ai_build_agent_plan(AIBuildAgent* agent,
                                const ProjectContext* ctx) {
    if (!agent || !ctx) return NULL;

    log_info("AI analyzing project and creating build plan...");

    /* Generate prompt */
    char* prompt = prompt_ai_build_plan(ctx, NULL, agent->attempted_fixes);
    if (!prompt) return NULL;

    /* Query AI provider */
    char* response_text = ai_provider_query(agent->ai, prompt, 2048);

    AIBuildPlan* plan = NULL;
    if (response_text) {
        if (agent->config.verbose) {
            log_debug("AI response received");
        }
        plan = parse_ai_build_plan_response(response_text, ctx->root_path);
        free(response_text);
    }

    free(prompt);

    /* If AI failed, use intelligent fallback */
    if (!plan) {
        const char* error = ai_provider_error(agent->ai);
        log_warning("AI planning failed: %s", error ? error : "Unknown error");
        log_info("Falling back to rule-based build intelligence...");
        plan = create_fallback_plan(ctx);
    }

    return plan;
}

/* Create a fallback fix plan using build intelligence error patterns */
static AIBuildPlan* create_fallback_error_fix(const char* error_output,
                                               const ProjectContext* ctx) {
    log_info("Using rule-based error analysis...");

    DetectedBuildError** errors = build_intelligence_analyze_error(error_output, ctx);
    if (!errors) return NULL;

    AIBuildPlan* plan = ai_build_plan_create(ctx->root_path);
    if (!plan) {
        detected_build_errors_free(errors);
        return NULL;
    }

    /* Build summary from detected errors */
    char summary[512] = "Detected issues: ";
    bool first = true;

    for (int i = 0; errors[i]; i++) {
        DetectedBuildError* err = errors[i];

        if (!first) strcat(summary, ", ");
        first = false;
        strncat(summary, err->description, sizeof(summary) - strlen(summary) - 1);

        if (err->fix_command) {
            BuildStepType type = BUILD_STEP_RUN_COMMAND;

            /* Detect step type */
            if (strstr(err->fix_command, "cmake")) {
                type = BUILD_STEP_CONFIGURE;
            } else if (strstr(err->fix_command, "install") ||
                       strstr(err->fix_command, "apt") ||
                       strstr(err->fix_command, "brew") ||
                       strstr(err->fix_command, "vcpkg")) {
                type = BUILD_STEP_INSTALL_DEP;
            }

            AIBuildStep* step = ai_build_step_create(
                type,
                err->fix_description,
                err->fix_command,
                NULL
            );
            if (step) {
                step->reason = strdup(err->description);
                ai_build_plan_add_step(plan, step);
            }
        }
    }

    plan->summary = strdup(summary);

    detected_build_errors_free(errors);

    /* If no fix steps were generated, add a retry configure step */
    if (plan->step_count == 0) {
        BuildCommandSet cmds = build_intelligence_get_commands(ctx->build_system.type);
        if (cmds.configure_cmd) {
            AIBuildStep* step = ai_build_step_create(
                BUILD_STEP_CONFIGURE,
                "Re-run configuration with correct flags",
                cmds.configure_cmd,
                NULL
            );
            if (step) {
                step->reason = strdup("Attempting fresh configure with known-good settings");
                ai_build_plan_add_step(plan, step);
            }
        }
    }

    return plan;
}

AIBuildPlan* ai_build_agent_analyze_error(AIBuildAgent* agent,
                                         const char* error_output,
                                         const ProjectContext* ctx) {
    if (!agent || !error_output || !ctx) return NULL;

    log_info("AI analyzing build error...");

    /* Generate prompt */
    char* prompt = prompt_ai_error_fix(error_output, ctx, agent->attempted_fixes);
    if (!prompt) return NULL;

    /* Query AI provider */
    char* response_text = ai_provider_query(agent->ai, prompt, 2048);

    AIBuildPlan* plan = NULL;
    if (response_text) {
        if (agent->config.verbose) {
            log_debug("AI error analysis complete");
        }
        plan = parse_ai_build_plan_response(response_text, ctx->root_path);

        if (plan && plan->summary) {
            log_info("AI Analysis: %s", plan->summary);
        }
        free(response_text);
    }

    free(prompt);

    /* If AI failed, use rule-based error analysis */
    if (!plan) {
        const char* error = ai_provider_error(agent->ai);
        log_warning("AI error analysis failed: %s", error ? error : "Unknown error");
        log_info("Falling back to rule-based error pattern matching...");
        plan = create_fallback_error_fix(error_output, ctx);
    }

    return plan;
}

/* ========================================================================
 * Main Build Function
 * ======================================================================== */

/* Track attempted fix */
static void track_attempted_fix(AIBuildAgent* agent, const char* fix_desc) {
    if (!agent || !fix_desc) return;

    size_t new_len = (agent->attempted_fixes ? strlen(agent->attempted_fixes) : 0) +
                     strlen(fix_desc) + 10;
    char* new_fixes = malloc(new_len);
    if (!new_fixes) return;

    if (agent->attempted_fixes) {
        snprintf(new_fixes, new_len, "%s\n- %s", agent->attempted_fixes, fix_desc);
        free(agent->attempted_fixes);
    } else {
        snprintf(new_fixes, new_len, "- %s", fix_desc);
    }
    agent->attempted_fixes = new_fixes;
}

BuildResult* ai_build_agent_build(AIBuildAgent* agent,
                                   const char* project_path) {
    if (!agent || !project_path) return NULL;

    log_info("=== AI Build Agent Starting ===");
    log_info("Project: %s", project_path);
    log_plain("");

    /* Analyze project */
    ProjectContext* ctx = project_analyze(project_path, NULL);
    if (!ctx) {
        log_error("Failed to analyze project");
        return NULL;
    }

    log_info("Detected: %s project with %s",
             language_to_string(ctx->primary_language),
             build_system_to_string(ctx->build_system.type));

    /* Reset state */
    agent->total_attempts = 0;
    free(agent->attempted_fixes);
    agent->attempted_fixes = NULL;
    free(agent->last_error);
    agent->last_error = NULL;

    BuildResult* final_result = NULL;
    bool success = false;

    /* Main build loop */
    while (agent->total_attempts < agent->config.max_attempts && !success) {
        agent->total_attempts++;
        log_info("\n=== Build Attempt %d/%d ===",
                 agent->total_attempts, agent->config.max_attempts);

        /* Get AI to create build plan */
        AIBuildPlan* plan = ai_build_agent_plan(agent, ctx);

        if (!plan || plan->step_count == 0) {
            log_warning("AI could not create a build plan, using default");

            /* Create simple default plan */
            plan = ai_build_plan_create(project_path);

            if (ctx->build_system.type == BUILD_CMAKE) {
                /* CMake: configure then build with proper out-of-source build */
                AIBuildStep* config = ai_build_step_create(
                    BUILD_STEP_CONFIGURE,
                    "Configure CMake project (out-of-source)",
                    "cmake -B build -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release",
                    NULL);
                config->reason = strdup("CMake requires policy 3.5+ and out-of-source builds are recommended");
                ai_build_plan_add_step(plan, config);

                AIBuildStep* build = ai_build_step_create(
                    BUILD_STEP_BUILD,
                    "Build the project",
                    "cmake --build build --config Release",
                    NULL);
                build->reason = strdup("Build the project after configuration");
                ai_build_plan_add_step(plan, build);
            } else {
                /* Generic build command */
                AIBuildStep* build = ai_build_step_create(
                    BUILD_STEP_BUILD,
                    "Build the project",
                    "make",
                    NULL);
                ai_build_plan_add_step(plan, build);
            }
        }

        /* Show plan */
        ai_build_plan_print(plan);

        /* Execute plan */
        bool plan_success = true;
        AIBuildStep* step = plan->steps;
        char* last_error_output = NULL;

        while (step && plan_success) {
            bool step_ok = ai_build_agent_execute_step(agent, step, ctx);

            if (!step_ok) {
                plan_success = false;
                last_error_output = step->error_output ? strdup(step->error_output) : NULL;

                /* Track what we tried */
                if (step->description) {
                    track_attempted_fix(agent, step->description);
                }
            }

            if (step_ok) {
                log_success("  Step completed successfully");
            }

            step = step->next;
        }

        if (plan_success) {
            log_success("\n=== Build Successful! ===");
            success = true;

            /* Create success result */
            final_result = calloc(1, sizeof(BuildResult));
            if (final_result) {
                final_result->success = true;
                final_result->exit_code = 0;
                final_result->stdout_output = strdup("Build completed successfully");
                final_result->stderr_output = strdup("");
            }
        } else {
            log_warning("\nBuild failed, analyzing error...");

            /* Get AI to analyze error and suggest fixes */
            if (last_error_output && agent->total_attempts < agent->config.max_attempts) {
                AIBuildPlan* fix_plan = ai_build_agent_analyze_error(agent, last_error_output, ctx);

                if (fix_plan && fix_plan->step_count > 0) {
                    log_info("AI suggests %d fix(es):", fix_plan->step_count);
                    ai_build_plan_print(fix_plan);

                    /* Execute fix steps */
                    AIBuildStep* fix_step = fix_plan->steps;
                    int fixes_applied = 0;

                    while (fix_step && fixes_applied < agent->config.max_fix_attempts) {
                        log_info("Applying fix: %s",
                                 fix_step->description ? fix_step->description : "");

                        bool fix_ok = ai_build_agent_execute_step(agent, fix_step, ctx);

                        if (fix_ok) {
                            log_success("  Fix applied successfully");
                            fixes_applied++;
                        } else {
                            log_warning("  Fix failed");
                        }

                        /* Track attempt */
                        if (fix_step->description) {
                            track_attempted_fix(agent, fix_step->description);
                        }

                        fix_step = fix_step->next;
                    }

                    ai_build_plan_free(fix_plan);
                } else {
                    log_warning("AI could not suggest fixes");
                }
            }

            free(last_error_output);
        }

        ai_build_plan_free(plan);
    }

    if (!success) {
        log_error("\n=== Build Failed After %d Attempts ===", agent->total_attempts);

        /* Create failure result */
        final_result = calloc(1, sizeof(BuildResult));
        if (final_result) {
            final_result->success = false;
            final_result->exit_code = 1;
            final_result->stdout_output = strdup("");
            final_result->stderr_output = agent->last_error ?
                strdup(agent->last_error) : strdup("Build failed");
        }
    }

    project_context_free(ctx);
    return final_result;
}
