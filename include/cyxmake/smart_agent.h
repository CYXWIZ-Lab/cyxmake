/**
 * @file smart_agent.h
 * @brief Smart Agent - Intelligent reasoning and planning for builds
 *
 * This module implements the core intelligence of CyxMake:
 * - Chain-of-thought reasoning
 * - Step-by-step problem solving
 * - Context-aware decision making
 * - Learning from outcomes
 */

#ifndef CYXMAKE_SMART_AGENT_H
#define CYXMAKE_SMART_AGENT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations - only typedef if not already declared */
#ifndef CYXMAKE_SMART_AGENT_FWD
#define CYXMAKE_SMART_AGENT_FWD
typedef struct SmartAgent SmartAgent;
#endif

#ifndef CYXMAKE_AI_PROVIDER_FWD
#define CYXMAKE_AI_PROVIDER_FWD
typedef struct AIProvider AIProvider;
#endif

#ifndef CYXMAKE_TOOL_REGISTRY_FWD
#define CYXMAKE_TOOL_REGISTRY_FWD
typedef struct ToolRegistry ToolRegistry;
#endif

#ifndef CYXMAKE_PROJECT_CONTEXT_FWD
#define CYXMAKE_PROJECT_CONTEXT_FWD
typedef struct ProjectContext ProjectContext;
#endif

#ifndef CYXMAKE_CONVERSATION_CONTEXT_FWD
#define CYXMAKE_CONVERSATION_CONTEXT_FWD
typedef struct ConversationContext ConversationContext;
#endif

/* ============================================================================
 * Thought Process - Chain of Thought Reasoning
 * ============================================================================ */

/**
 * A single step in the reasoning chain
 */
typedef struct {
    char* observation;      /* What I see/notice */
    char* interpretation;   /* What it means */
    char* hypothesis;       /* What I think should happen */
    char* action;           /* What I'll do about it */
    char* expected_result;  /* What I expect to happen */
} ThoughtStep;

/**
 * Complete reasoning chain for a decision
 */
typedef struct {
    ThoughtStep** steps;
    int step_count;
    int step_capacity;

    char* conclusion;       /* Final decision/action */
    float confidence;       /* How confident (0-1) */
    char* reasoning_summary; /* Human-readable summary */
} ReasoningChain;

/* ============================================================================
 * Decision Making
 * ============================================================================ */

typedef enum {
    DECISION_BUILD_STRATEGY,    /* How to build the project */
    DECISION_ERROR_FIX,         /* How to fix a build error */
    DECISION_DEPENDENCY,        /* Which dependency version/source */
    DECISION_TOOL_SELECTION,    /* Which tool to use */
    DECISION_CONFIGURATION,     /* Build configuration choices */
    DECISION_RECOVERY,          /* How to recover from failure */
} DecisionType;

/**
 * A possible option for a decision
 */
typedef struct {
    char* id;               /* Unique identifier */
    char* description;      /* What this option does */
    float score;            /* Confidence/priority score (0-1) */
    char** pros;            /* Advantages */
    int pros_count;
    char** cons;            /* Disadvantages */
    int cons_count;
    char* implementation;   /* How to implement this option */
    bool is_safe;           /* Is this a safe/reversible option */
} DecisionOption;

/**
 * A decision point with multiple options
 */
typedef struct {
    DecisionType type;
    char* context;              /* What triggered this decision */
    char* question;             /* The decision question */

    DecisionOption** options;
    int option_count;
    int option_capacity;

    int selected_option;        /* Index of chosen option (-1 if none) */
    char* selection_reasoning;  /* Why this option was chosen */
    ReasoningChain* reasoning;  /* Full reasoning chain */
} Decision;

/* ============================================================================
 * Smart Intent - Enhanced Intent Detection
 * ============================================================================ */

typedef enum {
    SMART_INTENT_BUILD,
    SMART_INTENT_CLEAN,
    SMART_INTENT_TEST,
    SMART_INTENT_RUN,
    SMART_INTENT_FIX,
    SMART_INTENT_INSTALL,
    SMART_INTENT_CONFIGURE,
    SMART_INTENT_EXPLAIN,
    SMART_INTENT_CREATE,
    SMART_INTENT_READ,
    SMART_INTENT_HELP,
    SMART_INTENT_UNKNOWN,
} SmartIntentType;

/**
 * Enhanced intent with extracted entities and context
 */
typedef struct {
    SmartIntentType primary_intent;
    SmartIntentType* secondary_intents;
    int secondary_count;

    /* Extracted entities */
    char** file_references;     /* Files mentioned */
    int file_ref_count;
    char** package_references;  /* Packages mentioned */
    int package_ref_count;
    char** target_references;   /* Build targets mentioned */
    int target_ref_count;

    /* Modifiers */
    bool wants_verbose;
    bool wants_quiet;
    bool wants_fast;
    bool wants_thorough;
    bool wants_force;
    bool wants_dry_run;

    /* Context references */
    bool references_last_error;     /* "fix that error" */
    bool references_last_file;      /* "in that file" */
    bool references_last_output;    /* "show me more" */

    /* Clarification */
    bool needs_clarification;
    char* clarification_question;

    /* Confidence breakdown */
    float semantic_confidence;      /* AI understanding */
    float pattern_confidence;       /* Pattern matching */
    float context_confidence;       /* From conversation */
    float overall_confidence;       /* Combined score */

    /* Raw analysis */
    char* ai_interpretation;        /* How AI understood this */
} SmartIntent;

/* ============================================================================
 * Smart Agent Context
 * ============================================================================ */

/**
 * Memory of past interactions and outcomes
 */
typedef struct {
    /* Recent commands and results */
    char** recent_commands;
    bool* command_successes;
    int command_count;

    /* Learned fixes for this project */
    char** error_signatures;
    char** successful_fixes;
    int fix_count;

    /* User preferences observed */
    bool prefers_verbose;
    bool prefers_parallel;
    char* preferred_config;     /* Debug, Release, etc. */
} AgentMemory;

/**
 * The Smart Agent - main intelligence controller
 */
struct SmartAgent {
    AIProvider* ai;
    ToolRegistry* tools;
    ProjectContext* project;
    ConversationContext* conversation;

    /* Memory and learning */
    AgentMemory* memory;

    /* Current state */
    ReasoningChain* current_reasoning;
    Decision* pending_decisions;
    int decision_count;

    /* Configuration */
    bool verbose;
    bool explain_actions;       /* Show reasoning to user */
    bool auto_fix;              /* Automatically apply safe fixes */
    bool confirm_destructive;   /* Ask before destructive actions */
    int max_reasoning_steps;
};

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* Agent lifecycle */
SmartAgent* smart_agent_create(AIProvider* ai, ToolRegistry* tools);
void smart_agent_free(SmartAgent* agent);

/* Set context */
void smart_agent_set_project(SmartAgent* agent, ProjectContext* project);
void smart_agent_set_conversation(SmartAgent* agent, ConversationContext* conv);

/* Core reasoning */
ReasoningChain* smart_agent_reason(SmartAgent* agent, const char* problem);
void reasoning_chain_free(ReasoningChain* chain);

/* Intent understanding */
SmartIntent* smart_agent_understand(SmartAgent* agent, const char* input);
void smart_intent_free(SmartIntent* intent);

/* Decision making */
Decision* smart_agent_decide(SmartAgent* agent, DecisionType type, const char* context);
void decision_free(Decision* decision);

/* Execute with intelligence */
typedef struct {
    bool success;
    char* output;
    char* error;
    char* explanation;          /* What happened and why */
    char** suggestions;         /* What to do next */
    int suggestion_count;
} SmartResult;

SmartResult* smart_agent_execute(SmartAgent* agent, const char* command);
SmartResult* smart_agent_build(SmartAgent* agent);
SmartResult* smart_agent_fix_error(SmartAgent* agent, const char* error);
void smart_result_free(SmartResult* result);

/* Learning */
void smart_agent_learn_success(SmartAgent* agent, const char* action, const char* context);
void smart_agent_learn_failure(SmartAgent* agent, const char* action, const char* error);

/* Explanation */
char* smart_agent_explain(SmartAgent* agent, const char* topic);
char* smart_agent_summarize_project(SmartAgent* agent);

/* Memory management */
AgentMemory* agent_memory_create(void);
void agent_memory_free(AgentMemory* memory);
bool agent_memory_save(AgentMemory* memory, const char* path);
AgentMemory* agent_memory_load(const char* path);

/* Utility */
const char* smart_intent_type_to_string(SmartIntentType type);
const char* decision_type_to_string(DecisionType type);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_SMART_AGENT_H */
