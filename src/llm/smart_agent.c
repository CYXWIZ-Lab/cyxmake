/**
 * @file smart_agent.c
 * @brief Smart Agent implementation - Chain-of-thought reasoning for builds
 */

#include "cyxmake/smart_agent.h"
#include "cyxmake/ai_provider.h"
#include "cyxmake/tool_executor.h"
#include "cyxmake/project_context.h"
#include "cyxmake/conversation_context.h"
#include "cyxmake/logger.h"
#include "cJSON/cJSON.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir_p(path) _mkdir(path)
#else
#include <sys/types.h>
#define mkdir_p(path) mkdir(path, 0755)
#endif

/* ============================================================================
 * Constants and Prompts
 * ============================================================================ */

static const char* REASONING_SYSTEM_PROMPT =
"You are CyxMake, an expert build system agent. Think step-by-step about problems.\n"
"\n"
"For each problem, follow this reasoning pattern:\n"
"1. OBSERVE: What do I see? What are the facts?\n"
"2. INTERPRET: What does this mean? What's the root cause?\n"
"3. HYPOTHESIZE: What should happen? What's the solution?\n"
"4. PLAN: What specific action should I take?\n"
"5. PREDICT: What result do I expect?\n"
"\n"
"Always explain your thinking clearly. Be specific and actionable.\n";

static const char* INTENT_SYSTEM_PROMPT =
"You are analyzing user input for a build system. Extract:\n"
"1. Primary intent (build, clean, test, run, fix, install, configure, explain, create, read, help)\n"
"2. Any files, packages, or targets mentioned\n"
"3. Modifiers (verbose, quiet, fast, force, dry-run)\n"
"4. If the user references something from context (\"that error\", \"this file\")\n"
"\n"
"Respond in this exact JSON format:\n"
"{\n"
"  \"intent\": \"build\",\n"
"  \"confidence\": 0.95,\n"
"  \"files\": [\"main.c\"],\n"
"  \"packages\": [],\n"
"  \"targets\": [],\n"
"  \"modifiers\": {\"verbose\": false, \"force\": false},\n"
"  \"references_context\": false,\n"
"  \"interpretation\": \"User wants to build the project\"\n"
"}\n";

static const char* DECISION_SYSTEM_PROMPT =
"You are making a decision for a build system. Analyze the options carefully.\n"
"\n"
"For each option, consider:\n"
"- Will it solve the problem?\n"
"- Is it safe? Can it be undone?\n"
"- What are the risks?\n"
"- How confident are you?\n"
"\n"
"Respond in this exact JSON format:\n"
"{\n"
"  \"selected_option\": 0,\n"
"  \"reasoning\": \"Step-by-step explanation of why this option is best\",\n"
"  \"confidence\": 0.85,\n"
"  \"risks\": [\"potential risk 1\"],\n"
"  \"alternatives_if_fails\": [\"backup plan\"]\n"
"}\n";

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static char* strdup_safe(const char* s) {
    return s ? _strdup(s) : NULL;
}

/* Windows doesn't have strndup */
static char* strndup_safe(const char* s, size_t n) {
    if (!s) return NULL;
    size_t len = strlen(s);
    if (len > n) len = n;
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, s, len);
    result[len] = '\0';
    return result;
}

static char* build_context_string(SmartAgent* agent) {
    /* Build a context string with project and conversation info */
    char* ctx = calloc(4096, 1);
    if (!ctx) return NULL;

    strcat(ctx, "=== Current Context ===\n");

    if (agent->project) {
        char temp[512];
        snprintf(temp, sizeof(temp),
            "Project: %s (%s)\n"
            "Build System: %s\n"
            "Source Files: %d\n",
            agent->project->root_path ? agent->project->root_path : "unknown",
            language_to_string(agent->project->primary_language),
            build_system_to_string(agent->project->build_system.type),
            agent->project->source_file_count);
        strcat(ctx, temp);
    }

    if (agent->conversation) {
        const char* last_error = conversation_get_last_error(agent->conversation);
        const char* current_file = conversation_get_current_file(agent->conversation);

        if (last_error) {
            strcat(ctx, "\nLast Error:\n");
            /* Truncate long errors */
            if (strlen(last_error) > 500) {
                strncat(ctx, last_error, 500);
                strcat(ctx, "...(truncated)");
            } else {
                strcat(ctx, last_error);
            }
            strcat(ctx, "\n");
        }

        if (current_file) {
            strcat(ctx, "Current File: ");
            strcat(ctx, current_file);
            strcat(ctx, "\n");
        }
    }

    return ctx;
}

/* Simple JSON string extraction */
static char* json_get_string(const char* json, const char* key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);

    const char* pos = strstr(json, search);
    if (!pos) return NULL;

    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;

    if (*pos != '"') return NULL;
    pos++;

    const char* end = strchr(pos, '"');
    if (!end) return NULL;

    size_t len = end - pos;
    char* result = malloc(len + 1);
    if (!result) return NULL;

    strncpy(result, pos, len);
    result[len] = '\0';

    return result;
}

static float json_get_float(const char* json, const char* key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);

    const char* pos = strstr(json, search);
    if (!pos) return 0.0f;

    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;

    return (float)atof(pos);
}

static int json_get_int(const char* json, const char* key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);

    const char* pos = strstr(json, search);
    if (!pos) return 0;

    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;

    return atoi(pos);
}

static bool json_get_bool(const char* json, const char* key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);

    const char* pos = strstr(json, search);
    if (!pos) return false;

    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;

    return strncmp(pos, "true", 4) == 0;
}

/* ============================================================================
 * Reasoning Chain
 * ============================================================================ */

static ReasoningChain* reasoning_chain_create(void) {
    ReasoningChain* chain = calloc(1, sizeof(ReasoningChain));
    if (!chain) return NULL;

    chain->step_capacity = 10;
    chain->steps = calloc(chain->step_capacity, sizeof(ThoughtStep*));
    if (!chain->steps) {
        free(chain);
        return NULL;
    }

    return chain;
}

static void thought_step_free(ThoughtStep* step) {
    if (!step) return;
    free(step->observation);
    free(step->interpretation);
    free(step->hypothesis);
    free(step->action);
    free(step->expected_result);
    free(step);
}

void reasoning_chain_free(ReasoningChain* chain) {
    if (!chain) return;

    for (int i = 0; i < chain->step_count; i++) {
        thought_step_free(chain->steps[i]);
    }
    free(chain->steps);
    free(chain->conclusion);
    free(chain->reasoning_summary);
    free(chain);
}

static bool reasoning_chain_add_step(ReasoningChain* chain, ThoughtStep* step) {
    if (chain->step_count >= chain->step_capacity) {
        int new_cap = chain->step_capacity * 2;
        ThoughtStep** new_steps = realloc(chain->steps, new_cap * sizeof(ThoughtStep*));
        if (!new_steps) return false;
        chain->steps = new_steps;
        chain->step_capacity = new_cap;
    }

    chain->steps[chain->step_count++] = step;
    return true;
}

/* ============================================================================
 * Smart Agent Lifecycle
 * ============================================================================ */

SmartAgent* smart_agent_create(AIProvider* ai, ToolRegistry* tools) {
    SmartAgent* agent = calloc(1, sizeof(SmartAgent));
    if (!agent) return NULL;

    agent->ai = ai;
    agent->tools = tools;

    /* Default settings */
    agent->verbose = true;
    agent->explain_actions = true;
    agent->auto_fix = false;
    agent->confirm_destructive = true;
    agent->max_reasoning_steps = 10;

    /* Create memory */
    agent->memory = agent_memory_create();

    return agent;
}

void smart_agent_free(SmartAgent* agent) {
    if (!agent) return;

    reasoning_chain_free(agent->current_reasoning);

    for (int i = 0; i < agent->decision_count; i++) {
        decision_free(agent->pending_decisions + i);
    }
    free(agent->pending_decisions);

    agent_memory_free(agent->memory);
    free(agent);
}

void smart_agent_set_project(SmartAgent* agent, ProjectContext* project) {
    if (agent) agent->project = project;
}

void smart_agent_set_conversation(SmartAgent* agent, ConversationContext* conv) {
    if (agent) agent->conversation = conv;
}

/* ============================================================================
 * Core Reasoning - Chain of Thought
 * ============================================================================ */

ReasoningChain* smart_agent_reason(SmartAgent* agent, const char* problem) {
    if (!agent || !agent->ai || !problem) return NULL;

    ReasoningChain* chain = reasoning_chain_create();
    if (!chain) return NULL;

    /* Build the reasoning prompt */
    char* context = build_context_string(agent);
    char* prompt = malloc(8192);
    if (!prompt) {
        free(context);
        reasoning_chain_free(chain);
        return NULL;
    }

    snprintf(prompt, 8192,
        "%s\n"
        "%s\n"
        "=== Problem ===\n"
        "%s\n"
        "\n"
        "Think through this step-by-step. For each step, provide:\n"
        "OBSERVE: [what you notice]\n"
        "INTERPRET: [what it means]\n"
        "HYPOTHESIS: [what you think]\n"
        "ACTION: [what to do]\n"
        "EXPECT: [expected result]\n"
        "\n"
        "After your reasoning steps, provide:\n"
        "CONCLUSION: [final decision]\n"
        "CONFIDENCE: [0-100]%%\n",
        REASONING_SYSTEM_PROMPT,
        context ? context : "",
        problem);

    free(context);

    if (agent->verbose) {
        log_info("Reasoning about: %s", problem);
    }

    /* Query AI */
    char* response = ai_provider_query(agent->ai, prompt, 2048);
    free(prompt);

    if (!response) {
        log_warning("AI reasoning failed");
        reasoning_chain_free(chain);
        return NULL;
    }

    /* Parse the response into thought steps */
    /* Look for OBSERVE:, INTERPRET:, etc. patterns */
    char* current = response;
    ThoughtStep* step = NULL;

    while (*current) {
        if (strncmp(current, "OBSERVE:", 8) == 0) {
            if (step) {
                reasoning_chain_add_step(chain, step);
            }
            step = calloc(1, sizeof(ThoughtStep));
            current += 8;
            while (*current == ' ') current++;

            /* Extract until next keyword or newline */
            char* end = strstr(current, "\nINTERPRET:");
            if (!end) end = strstr(current, "\n");
            if (end) {
                step->observation = strndup_safe(current, end - current);
                current = end;
            }
        }
        else if (strncmp(current, "INTERPRET:", 10) == 0 && step) {
            current += 10;
            while (*current == ' ') current++;
            char* end = strstr(current, "\nHYPOTHESIS:");
            if (!end) end = strstr(current, "\n");
            if (end) {
                step->interpretation = strndup_safe(current, end - current);
                current = end;
            }
        }
        else if (strncmp(current, "HYPOTHESIS:", 11) == 0 && step) {
            current += 11;
            while (*current == ' ') current++;
            char* end = strstr(current, "\nACTION:");
            if (!end) end = strstr(current, "\n");
            if (end) {
                step->hypothesis = strndup_safe(current, end - current);
                current = end;
            }
        }
        else if (strncmp(current, "ACTION:", 7) == 0 && step) {
            current += 7;
            while (*current == ' ') current++;
            char* end = strstr(current, "\nEXPECT:");
            if (!end) end = strstr(current, "\n");
            if (end) {
                step->action = strndup_safe(current, end - current);
                current = end;
            }
        }
        else if (strncmp(current, "EXPECT:", 7) == 0 && step) {
            current += 7;
            while (*current == ' ') current++;
            char* end = strstr(current, "\nOBSERVE:");
            if (!end) end = strstr(current, "\nCONCLUSION:");
            if (!end) end = strstr(current, "\n");
            if (end) {
                step->expected_result = strndup_safe(current, end - current);
                current = end;
            }
        }
        else if (strncmp(current, "CONCLUSION:", 11) == 0) {
            if (step) {
                reasoning_chain_add_step(chain, step);
                step = NULL;
            }
            current += 11;
            while (*current == ' ') current++;
            char* end = strstr(current, "\nCONFIDENCE:");
            if (!end) end = strstr(current, "\n");
            if (end) {
                chain->conclusion = strndup_safe(current, end - current);
                current = end;
            }
        }
        else if (strncmp(current, "CONFIDENCE:", 11) == 0) {
            current += 11;
            while (*current == ' ') current++;
            chain->confidence = (float)atof(current) / 100.0f;
            break;
        }
        else {
            current++;
        }
    }

    /* Add last step if any */
    if (step) {
        reasoning_chain_add_step(chain, step);
    }

    /* Store raw response as summary if no structured parsing worked */
    if (chain->step_count == 0) {
        chain->reasoning_summary = strdup(response);
        chain->confidence = 0.5f;  /* Low confidence for unstructured */
    }

    free(response);

    /* Log reasoning if verbose */
    if (agent->verbose && agent->explain_actions) {
        log_plain("");
        log_info("=== Reasoning Process ===");
        for (int i = 0; i < chain->step_count; i++) {
            ThoughtStep* s = chain->steps[i];
            log_plain("Step %d:", i + 1);
            if (s->observation) log_plain("  Observe: %s", s->observation);
            if (s->interpretation) log_plain("  Interpret: %s", s->interpretation);
            if (s->hypothesis) log_plain("  Hypothesis: %s", s->hypothesis);
            if (s->action) log_plain("  Action: %s", s->action);
            if (s->expected_result) log_plain("  Expected: %s", s->expected_result);
            log_plain("");
        }
        if (chain->conclusion) {
            log_info("Conclusion: %s", chain->conclusion);
            log_info("Confidence: %.0f%%", chain->confidence * 100);
        }
    }

    /* Store as current reasoning */
    reasoning_chain_free(agent->current_reasoning);
    agent->current_reasoning = chain;

    return chain;
}

/* ============================================================================
 * Intent Understanding
 * ============================================================================ */

SmartIntent* smart_agent_understand(SmartAgent* agent, const char* input) {
    if (!agent || !input) return NULL;

    SmartIntent* intent = calloc(1, sizeof(SmartIntent));
    if (!intent) return NULL;

    /* First, try pattern-based detection for common cases */
    const char* lower = input;
    char lower_buf[512];
    strncpy(lower_buf, input, sizeof(lower_buf) - 1);
    lower_buf[sizeof(lower_buf) - 1] = '\0';
    for (char* p = lower_buf; *p; p++) *p = tolower(*p);
    lower = lower_buf;

    /* Quick pattern matching */
    if (strstr(lower, "build") || strstr(lower, "compile") || strstr(lower, "make")) {
        intent->primary_intent = SMART_INTENT_BUILD;
        intent->pattern_confidence = 0.9f;
    }
    else if (strstr(lower, "clean") || strstr(lower, "clear") || strstr(lower, "remove build")) {
        intent->primary_intent = SMART_INTENT_CLEAN;
        intent->pattern_confidence = 0.9f;
    }
    else if (strstr(lower, "test") || strstr(lower, "run test")) {
        intent->primary_intent = SMART_INTENT_TEST;
        intent->pattern_confidence = 0.9f;
    }
    else if (strstr(lower, "run") || strstr(lower, "execute") || strstr(lower, "start")) {
        intent->primary_intent = SMART_INTENT_RUN;
        intent->pattern_confidence = 0.85f;
    }
    else if (strstr(lower, "fix") || strstr(lower, "solve") || strstr(lower, "repair")) {
        intent->primary_intent = SMART_INTENT_FIX;
        intent->pattern_confidence = 0.9f;
        intent->references_last_error = true;
    }
    else if (strstr(lower, "install") || strstr(lower, "add package") || strstr(lower, "get ")) {
        intent->primary_intent = SMART_INTENT_INSTALL;
        intent->pattern_confidence = 0.85f;
    }
    else if (strstr(lower, "config") || strstr(lower, "setup") || strstr(lower, "init")) {
        intent->primary_intent = SMART_INTENT_CONFIGURE;
        intent->pattern_confidence = 0.85f;
    }
    else if (strstr(lower, "explain") || strstr(lower, "what") || strstr(lower, "why") || strstr(lower, "how")) {
        intent->primary_intent = SMART_INTENT_EXPLAIN;
        intent->pattern_confidence = 0.8f;
    }
    else if (strstr(lower, "create") || strstr(lower, "new") || strstr(lower, "generate")) {
        intent->primary_intent = SMART_INTENT_CREATE;
        intent->pattern_confidence = 0.85f;
    }
    else if (strstr(lower, "read") || strstr(lower, "show") || strstr(lower, "cat ") || strstr(lower, "view")) {
        intent->primary_intent = SMART_INTENT_READ;
        intent->pattern_confidence = 0.85f;
    }
    else if (strstr(lower, "help") || strstr(lower, "?")) {
        intent->primary_intent = SMART_INTENT_HELP;
        intent->pattern_confidence = 0.95f;
    }
    else {
        intent->primary_intent = SMART_INTENT_UNKNOWN;
        intent->pattern_confidence = 0.0f;
    }

    /* Detect modifiers */
    intent->wants_verbose = strstr(lower, "verbose") != NULL || strstr(lower, "-v") != NULL;
    intent->wants_quiet = strstr(lower, "quiet") != NULL || strstr(lower, "silent") != NULL;
    intent->wants_fast = strstr(lower, "fast") != NULL || strstr(lower, "quick") != NULL;
    intent->wants_thorough = strstr(lower, "thorough") != NULL || strstr(lower, "full") != NULL;
    intent->wants_force = strstr(lower, "force") != NULL || strstr(lower, "-f") != NULL;
    intent->wants_dry_run = strstr(lower, "dry") != NULL || strstr(lower, "preview") != NULL;

    /* Detect context references */
    intent->references_last_error = strstr(lower, "that error") != NULL ||
                                    strstr(lower, "the error") != NULL ||
                                    strstr(lower, "this error") != NULL ||
                                    strstr(lower, "fix it") != NULL;
    intent->references_last_file = strstr(lower, "that file") != NULL ||
                                   strstr(lower, "this file") != NULL ||
                                   strstr(lower, "the file") != NULL;

    /* If pattern confidence is low, use AI for semantic understanding */
    if (intent->pattern_confidence < 0.7f && agent->ai) {
        char* context = build_context_string(agent);
        char prompt[4096];
        snprintf(prompt, sizeof(prompt),
            "%s\n"
            "Context:\n%s\n"
            "User input: \"%s\"\n"
            "\n"
            "Analyze this input and respond with JSON.",
            INTENT_SYSTEM_PROMPT,
            context ? context : "",
            input);
        free(context);

        char* response = ai_provider_query(agent->ai, prompt, 1024);
        if (response) {
            /* Parse JSON response */
            char* intent_str = json_get_string(response, "intent");
            if (intent_str) {
                if (strcmp(intent_str, "build") == 0) intent->primary_intent = SMART_INTENT_BUILD;
                else if (strcmp(intent_str, "clean") == 0) intent->primary_intent = SMART_INTENT_CLEAN;
                else if (strcmp(intent_str, "test") == 0) intent->primary_intent = SMART_INTENT_TEST;
                else if (strcmp(intent_str, "run") == 0) intent->primary_intent = SMART_INTENT_RUN;
                else if (strcmp(intent_str, "fix") == 0) intent->primary_intent = SMART_INTENT_FIX;
                else if (strcmp(intent_str, "install") == 0) intent->primary_intent = SMART_INTENT_INSTALL;
                else if (strcmp(intent_str, "configure") == 0) intent->primary_intent = SMART_INTENT_CONFIGURE;
                else if (strcmp(intent_str, "explain") == 0) intent->primary_intent = SMART_INTENT_EXPLAIN;
                else if (strcmp(intent_str, "create") == 0) intent->primary_intent = SMART_INTENT_CREATE;
                else if (strcmp(intent_str, "read") == 0) intent->primary_intent = SMART_INTENT_READ;
                else if (strcmp(intent_str, "help") == 0) intent->primary_intent = SMART_INTENT_HELP;
                free(intent_str);
            }

            intent->semantic_confidence = json_get_float(response, "confidence");
            intent->ai_interpretation = json_get_string(response, "interpretation");
            intent->references_last_error = json_get_bool(response, "references_context");

            free(response);
        }
    }

    /* Calculate overall confidence */
    intent->overall_confidence = intent->pattern_confidence;
    if (intent->semantic_confidence > 0) {
        intent->overall_confidence = (intent->pattern_confidence + intent->semantic_confidence) / 2.0f;
    }

    return intent;
}

void smart_intent_free(SmartIntent* intent) {
    if (!intent) return;

    free(intent->secondary_intents);

    for (int i = 0; i < intent->file_ref_count; i++) {
        free(intent->file_references[i]);
    }
    free(intent->file_references);

    for (int i = 0; i < intent->package_ref_count; i++) {
        free(intent->package_references[i]);
    }
    free(intent->package_references);

    for (int i = 0; i < intent->target_ref_count; i++) {
        free(intent->target_references[i]);
    }
    free(intent->target_references);

    free(intent->clarification_question);
    free(intent->ai_interpretation);
    free(intent);
}

/* ============================================================================
 * Decision Making
 * ============================================================================ */

static DecisionOption* decision_option_create(const char* id, const char* desc, float score) {
    DecisionOption* opt = calloc(1, sizeof(DecisionOption));
    if (!opt) return NULL;

    opt->id = strdup_safe(id);
    opt->description = strdup_safe(desc);
    opt->score = score;
    opt->is_safe = true;

    return opt;
}

static void decision_option_free(DecisionOption* opt) {
    if (!opt) return;
    free(opt->id);
    free(opt->description);
    for (int i = 0; i < opt->pros_count; i++) free(opt->pros[i]);
    free(opt->pros);
    for (int i = 0; i < opt->cons_count; i++) free(opt->cons[i]);
    free(opt->cons);
    free(opt->implementation);
    free(opt);
}

Decision* smart_agent_decide(SmartAgent* agent, DecisionType type, const char* context) {
    if (!agent || !context) return NULL;

    Decision* decision = calloc(1, sizeof(Decision));
    if (!decision) return NULL;

    decision->type = type;
    decision->context = strdup_safe(context);
    decision->selected_option = -1;

    decision->option_capacity = 5;
    decision->options = calloc(decision->option_capacity, sizeof(DecisionOption*));

    /* Generate options based on decision type */
    switch (type) {
        case DECISION_BUILD_STRATEGY:
            decision->question = strdup("How should we build this project?");
            /* Add common build strategy options */
            decision->options[decision->option_count++] =
                decision_option_create("incremental", "Incremental build (only changed files)", 0.9f);
            decision->options[decision->option_count++] =
                decision_option_create("clean", "Clean build (rebuild everything)", 0.7f);
            decision->options[decision->option_count++] =
                decision_option_create("parallel", "Parallel build (use all CPU cores)", 0.85f);
            break;

        case DECISION_ERROR_FIX:
            decision->question = strdup("How should we fix this error?");
            /* Options will be generated by AI based on error context */
            break;

        case DECISION_DEPENDENCY:
            decision->question = strdup("How should we handle this dependency?");
            decision->options[decision->option_count++] =
                decision_option_create("install", "Install using package manager", 0.9f);
            decision->options[decision->option_count++] =
                decision_option_create("manual", "Manual installation", 0.5f);
            decision->options[decision->option_count++] =
                decision_option_create("skip", "Skip this dependency", 0.3f);
            break;

        case DECISION_TOOL_SELECTION:
            decision->question = strdup("Which tool should we use?");
            break;

        case DECISION_CONFIGURATION:
            decision->question = strdup("What configuration should we use?");
            decision->options[decision->option_count++] =
                decision_option_create("debug", "Debug configuration", 0.8f);
            decision->options[decision->option_count++] =
                decision_option_create("release", "Release configuration", 0.7f);
            decision->options[decision->option_count++] =
                decision_option_create("relwithdebinfo", "Release with debug info", 0.75f);
            break;

        case DECISION_RECOVERY:
            decision->question = strdup("How should we recover from this failure?");
            decision->options[decision->option_count++] =
                decision_option_create("retry", "Retry the operation", 0.8f);
            decision->options[decision->option_count++] =
                decision_option_create("clean_retry", "Clean and retry", 0.7f);
            decision->options[decision->option_count++] =
                decision_option_create("abort", "Abort and report", 0.4f);
            break;
    }

    /* Use AI to evaluate options and select best one */
    if (agent->ai && decision->option_count > 0) {
        char prompt[4096];
        char* ctx = build_context_string(agent);

        snprintf(prompt, sizeof(prompt),
            "%s\n"
            "Context:\n%s\n"
            "Decision: %s\n"
            "Situation: %s\n"
            "\n"
            "Options:\n",
            DECISION_SYSTEM_PROMPT,
            ctx ? ctx : "",
            decision->question,
            context);

        for (int i = 0; i < decision->option_count; i++) {
            char opt_line[256];
            snprintf(opt_line, sizeof(opt_line), "%d. %s: %s\n",
                     i, decision->options[i]->id, decision->options[i]->description);
            strcat(prompt, opt_line);
        }

        strcat(prompt, "\nSelect the best option and explain why.");

        free(ctx);

        char* response = ai_provider_query(agent->ai, prompt, 1024);
        if (response) {
            decision->selected_option = json_get_int(response, "selected_option");
            decision->selection_reasoning = json_get_string(response, "reasoning");

            /* Update confidence of selected option */
            float conf = json_get_float(response, "confidence");
            if (decision->selected_option >= 0 && decision->selected_option < decision->option_count) {
                decision->options[decision->selected_option]->score = conf;
            }

            free(response);
        }
    }

    /* Default to highest scored option if AI didn't select */
    if (decision->selected_option < 0 && decision->option_count > 0) {
        float best_score = 0;
        for (int i = 0; i < decision->option_count; i++) {
            if (decision->options[i]->score > best_score) {
                best_score = decision->options[i]->score;
                decision->selected_option = i;
            }
        }
    }

    return decision;
}

void decision_free(Decision* decision) {
    if (!decision) return;

    free(decision->context);
    free(decision->question);
    free(decision->selection_reasoning);

    for (int i = 0; i < decision->option_count; i++) {
        decision_option_free(decision->options[i]);
    }
    free(decision->options);

    reasoning_chain_free(decision->reasoning);
}

/* ============================================================================
 * Smart Execution
 * ============================================================================ */

SmartResult* smart_agent_build(SmartAgent* agent) {
    if (!agent) return NULL;

    SmartResult* result = calloc(1, sizeof(SmartResult));
    if (!result) return NULL;

    /* Step 1: Reason about the build */
    ReasoningChain* reasoning = smart_agent_reason(agent,
        "How should I build this project? Consider the build system, dependencies, and any previous errors.");

    if (!reasoning) {
        result->success = false;
        result->error = strdup("Failed to reason about build");
        return result;
    }

    /* Step 2: Make build strategy decision */
    Decision* strategy = smart_agent_decide(agent, DECISION_BUILD_STRATEGY,
        reasoning->conclusion ? reasoning->conclusion : "Standard build");

    if (agent->verbose && strategy && strategy->selected_option >= 0) {
        log_info("Build strategy: %s", strategy->options[strategy->selected_option]->description);
        if (strategy->selection_reasoning) {
            log_plain("Reasoning: %s", strategy->selection_reasoning);
        }
    }

    /* Step 3: Execute the build */
    /* This would integrate with the existing build system... */
    /* For now, provide a summary */

    result->success = true;
    result->explanation = strdup(reasoning->conclusion ? reasoning->conclusion : "Build plan created");

    decision_free(strategy);

    return result;
}

SmartResult* smart_agent_fix_error(SmartAgent* agent, const char* error) {
    if (!agent || !error) return NULL;

    SmartResult* result = calloc(1, sizeof(SmartResult));
    if (!result) return NULL;

    /* Reason about the error */
    char problem[2048];
    snprintf(problem, sizeof(problem),
        "Build error occurred:\n%s\n\nHow should I fix this?", error);

    ReasoningChain* reasoning = smart_agent_reason(agent, problem);

    if (!reasoning || !reasoning->conclusion) {
        result->success = false;
        result->error = strdup("Could not determine fix");
        return result;
    }

    result->explanation = strdup(reasoning->conclusion);

    /* Extract suggestions from reasoning */
    if (reasoning->step_count > 0) {
        result->suggestions = calloc(reasoning->step_count, sizeof(char*));
        for (int i = 0; i < reasoning->step_count; i++) {
            if (reasoning->steps[i]->action) {
                result->suggestions[result->suggestion_count++] =
                    strdup(reasoning->steps[i]->action);
            }
        }
    }

    result->success = true;

    /* Learn from this */
    if (result->suggestion_count > 0) {
        smart_agent_learn_success(agent, result->suggestions[0], error);
    }

    return result;
}

SmartResult* smart_agent_execute(SmartAgent* agent, const char* command) {
    if (!agent || !command) return NULL;

    SmartResult* result = calloc(1, sizeof(SmartResult));
    if (!result) return NULL;

    /* Reason about how to execute this command */
    char problem[2048];
    snprintf(problem, sizeof(problem),
        "I need to execute this task: %s\n\nHow should I approach this?", command);

    ReasoningChain* reasoning = smart_agent_reason(agent, problem);

    if (!reasoning) {
        result->success = false;
        result->error = strdup("Failed to reason about task");
        return result;
    }

    /* Build result from reasoning */
    if (reasoning->conclusion) {
        result->output = strdup(reasoning->conclusion);
    }

    /* Extract suggestions from reasoning steps */
    if (reasoning->step_count > 0) {
        result->suggestions = calloc((size_t)reasoning->step_count, sizeof(char*));
        result->suggestion_count = 0;

        for (int i = 0; i < reasoning->step_count && result->suggestion_count < 10; i++) {
            if (reasoning->steps[i] && reasoning->steps[i]->action) {
                result->suggestions[result->suggestion_count++] =
                    strdup(reasoning->steps[i]->action);
            }
        }
    }

    result->success = true;
    result->explanation = strdup(reasoning->conclusion ?
        reasoning->conclusion : "Task analyzed");

    reasoning_chain_free(reasoning);

    return result;
}

void smart_result_free(SmartResult* result) {
    if (!result) return;
    free(result->output);
    free(result->error);
    free(result->explanation);
    for (int i = 0; i < result->suggestion_count; i++) {
        free(result->suggestions[i]);
    }
    free(result->suggestions);
    free(result);
}

/* ============================================================================
 * Learning
 * ============================================================================ */

AgentMemory* agent_memory_create(void) {
    return calloc(1, sizeof(AgentMemory));
}

void agent_memory_free(AgentMemory* memory) {
    if (!memory) return;

    for (int i = 0; i < memory->command_count; i++) {
        free(memory->recent_commands[i]);
    }
    free(memory->recent_commands);
    free(memory->command_successes);

    for (int i = 0; i < memory->fix_count; i++) {
        free(memory->error_signatures[i]);
        free(memory->successful_fixes[i]);
    }
    free(memory->error_signatures);
    free(memory->successful_fixes);

    free(memory->preferred_config);
    free(memory);
}

void smart_agent_learn_success(SmartAgent* agent, const char* action, const char* context) {
    if (!agent || !agent->memory || !action) return;

    AgentMemory* mem = agent->memory;

    /* Add to recent commands */
    if (mem->command_count < 100) {  /* Max 100 recent commands */
        char** new_cmds = realloc(mem->recent_commands,
                                  (mem->command_count + 1) * sizeof(char*));
        bool* new_success = realloc(mem->command_successes,
                                    (mem->command_count + 1) * sizeof(bool));
        if (new_cmds && new_success) {
            mem->recent_commands = new_cmds;
            mem->command_successes = new_success;
            mem->recent_commands[mem->command_count] = strdup_safe(action);
            mem->command_successes[mem->command_count] = true;
            mem->command_count++;
        }
    }

    log_debug("Learned success: '%s' for context '%s'", action,
              context ? context : "unknown");
}

void smart_agent_learn_failure(SmartAgent* agent, const char* action, const char* error) {
    if (!agent || !agent->memory || !action) return;

    AgentMemory* mem = agent->memory;

    /* Record failed action with error signature */
    if (mem->fix_count < 50 && error) {  /* Max 50 error fixes */
        char** new_sigs = realloc(mem->error_signatures,
                                  (mem->fix_count + 1) * sizeof(char*));
        char** new_fixes = realloc(mem->successful_fixes,
                                   (mem->fix_count + 1) * sizeof(char*));
        if (new_sigs && new_fixes) {
            mem->error_signatures = new_sigs;
            mem->successful_fixes = new_fixes;
            mem->error_signatures[mem->fix_count] = strdup_safe(error);
            mem->successful_fixes[mem->fix_count] = strdup_safe(action);
            mem->fix_count++;
        }
    }

    log_debug("Learned failure: '%s' with error '%s'", action,
              error ? error : "unknown");
}

/* Save agent memory to JSON file */
bool agent_memory_save(AgentMemory* memory, const char* path) {
    if (!memory || !path) return false;

    cJSON* root = cJSON_CreateObject();
    if (!root) return false;

    /* Version for future compatibility */
    cJSON_AddNumberToObject(root, "version", 1);

    /* Recent commands */
    cJSON* commands = cJSON_CreateArray();
    for (int i = 0; i < memory->command_count; i++) {
        cJSON* cmd = cJSON_CreateObject();
        cJSON_AddStringToObject(cmd, "command", memory->recent_commands[i] ? memory->recent_commands[i] : "");
        cJSON_AddBoolToObject(cmd, "success", memory->command_successes[i]);
        cJSON_AddItemToArray(commands, cmd);
    }
    cJSON_AddItemToObject(root, "recent_commands", commands);

    /* Error fixes */
    cJSON* fixes = cJSON_CreateArray();
    for (int i = 0; i < memory->fix_count; i++) {
        cJSON* fix = cJSON_CreateObject();
        cJSON_AddStringToObject(fix, "error", memory->error_signatures[i] ? memory->error_signatures[i] : "");
        cJSON_AddStringToObject(fix, "fix", memory->successful_fixes[i] ? memory->successful_fixes[i] : "");
        cJSON_AddItemToArray(fixes, fix);
    }
    cJSON_AddItemToObject(root, "error_fixes", fixes);

    /* Preferences */
    cJSON* prefs = cJSON_CreateObject();
    cJSON_AddBoolToObject(prefs, "verbose", memory->prefers_verbose);
    cJSON_AddBoolToObject(prefs, "parallel", memory->prefers_parallel);
    if (memory->preferred_config) {
        cJSON_AddStringToObject(prefs, "config", memory->preferred_config);
    }
    cJSON_AddItemToObject(root, "preferences", prefs);

    /* Write to file */
    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    if (!json_str) return false;

    /* Ensure directory exists */
    char dir[1024];
    strncpy(dir, path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char* last_sep = strrchr(dir, '/');
    if (!last_sep) last_sep = strrchr(dir, '\\');
    if (last_sep) {
        *last_sep = '\0';
        mkdir_p(dir);
    }

    FILE* f = fopen(path, "w");
    if (!f) {
        free(json_str);
        return false;
    }

    fputs(json_str, f);
    fclose(f);
    free(json_str);

    log_debug("Saved agent memory to: %s", path);
    return true;
}

/* Load agent memory from JSON file */
AgentMemory* agent_memory_load(const char* path) {
    if (!path) return NULL;

    FILE* f = fopen(path, "r");
    if (!f) {
        log_debug("No existing memory file at: %s", path);
        return NULL;
    }

    /* Read file */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 1024 * 1024) {  /* Max 1MB */
        fclose(f);
        return NULL;
    }

    char* json_str = malloc(size + 1);
    if (!json_str) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(json_str, 1, size, f);
    fclose(f);
    json_str[read] = '\0';

    cJSON* root = cJSON_Parse(json_str);
    free(json_str);

    if (!root) {
        log_warning("Failed to parse memory file: %s", path);
        return NULL;
    }

    AgentMemory* memory = agent_memory_create();
    if (!memory) {
        cJSON_Delete(root);
        return NULL;
    }

    /* Load recent commands */
    cJSON* commands = cJSON_GetObjectItem(root, "recent_commands");
    if (cJSON_IsArray(commands)) {
        int count = cJSON_GetArraySize(commands);
        if (count > 0) {
            memory->recent_commands = calloc(count, sizeof(char*));
            memory->command_successes = calloc(count, sizeof(bool));
            if (memory->recent_commands && memory->command_successes) {
                for (int i = 0; i < count; i++) {
                    cJSON* cmd = cJSON_GetArrayItem(commands, i);
                    cJSON* cmd_str = cJSON_GetObjectItem(cmd, "command");
                    cJSON* success = cJSON_GetObjectItem(cmd, "success");
                    if (cJSON_IsString(cmd_str)) {
                        memory->recent_commands[i] = strdup_safe(cmd_str->valuestring);
                    }
                    memory->command_successes[i] = cJSON_IsTrue(success);
                    memory->command_count++;
                }
            }
        }
    }

    /* Load error fixes */
    cJSON* fixes = cJSON_GetObjectItem(root, "error_fixes");
    if (cJSON_IsArray(fixes)) {
        int count = cJSON_GetArraySize(fixes);
        if (count > 0) {
            memory->error_signatures = calloc(count, sizeof(char*));
            memory->successful_fixes = calloc(count, sizeof(char*));
            if (memory->error_signatures && memory->successful_fixes) {
                for (int i = 0; i < count; i++) {
                    cJSON* fix = cJSON_GetArrayItem(fixes, i);
                    cJSON* error = cJSON_GetObjectItem(fix, "error");
                    cJSON* fix_str = cJSON_GetObjectItem(fix, "fix");
                    if (cJSON_IsString(error)) {
                        memory->error_signatures[i] = strdup_safe(error->valuestring);
                    }
                    if (cJSON_IsString(fix_str)) {
                        memory->successful_fixes[i] = strdup_safe(fix_str->valuestring);
                    }
                    memory->fix_count++;
                }
            }
        }
    }

    /* Load preferences */
    cJSON* prefs = cJSON_GetObjectItem(root, "preferences");
    if (cJSON_IsObject(prefs)) {
        cJSON* verbose = cJSON_GetObjectItem(prefs, "verbose");
        cJSON* parallel = cJSON_GetObjectItem(prefs, "parallel");
        cJSON* config = cJSON_GetObjectItem(prefs, "config");

        memory->prefers_verbose = cJSON_IsTrue(verbose);
        memory->prefers_parallel = cJSON_IsTrue(parallel);
        if (cJSON_IsString(config)) {
            memory->preferred_config = strdup_safe(config->valuestring);
        }
    }

    cJSON_Delete(root);

    log_debug("Loaded agent memory from: %s (%d commands, %d fixes)",
              path, memory->command_count, memory->fix_count);
    return memory;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

const char* smart_intent_type_to_string(SmartIntentType type) {
    switch (type) {
        case SMART_INTENT_BUILD: return "build";
        case SMART_INTENT_CLEAN: return "clean";
        case SMART_INTENT_TEST: return "test";
        case SMART_INTENT_RUN: return "run";
        case SMART_INTENT_FIX: return "fix";
        case SMART_INTENT_INSTALL: return "install";
        case SMART_INTENT_CONFIGURE: return "configure";
        case SMART_INTENT_EXPLAIN: return "explain";
        case SMART_INTENT_CREATE: return "create";
        case SMART_INTENT_READ: return "read";
        case SMART_INTENT_HELP: return "help";
        default: return "unknown";
    }
}

const char* decision_type_to_string(DecisionType type) {
    switch (type) {
        case DECISION_BUILD_STRATEGY: return "build_strategy";
        case DECISION_ERROR_FIX: return "error_fix";
        case DECISION_DEPENDENCY: return "dependency";
        case DECISION_TOOL_SELECTION: return "tool_selection";
        case DECISION_CONFIGURATION: return "configuration";
        case DECISION_RECOVERY: return "recovery";
        default: return "unknown";
    }
}
