/**
 * @file error_analyzer.c
 * @brief AI-powered build error analysis
 */

#include "cyxmake/llm_interface.h"
#include "cyxmake/prompt_templates.h"
#include "cyxmake/build_executor.h"
#include "cyxmake/project_context.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Error analyzer context */
typedef struct {
    LLMContext* llm;
    ProjectContext* project;
    bool verbose;
} ErrorAnalyzer;

/**
 * Create error analyzer
 * @param llm_ctx LLM context (must be initialized)
 * @param project_ctx Project context
 * @return Error analyzer instance
 */
ErrorAnalyzer* error_analyzer_create(LLMContext* llm_ctx,
                                      ProjectContext* project_ctx) {
    if (!llm_ctx || !llm_is_ready(llm_ctx)) {
        log_error("LLM context is not ready");
        return NULL;
    }

    ErrorAnalyzer* analyzer = calloc(1, sizeof(ErrorAnalyzer));
    if (!analyzer) return NULL;

    analyzer->llm = llm_ctx;
    analyzer->project = project_ctx;
    analyzer->verbose = false;

    return analyzer;
}

/**
 * Free error analyzer
 */
void error_analyzer_free(ErrorAnalyzer* analyzer) {
    free(analyzer);
}

/**
 * Analyze build error and get suggestions
 * @param analyzer Error analyzer
 * @param build_result Build result with error output
 * @return Suggested fix (caller must free), or NULL on error
 */
char* error_analyzer_analyze(ErrorAnalyzer* analyzer,
                              const BuildResult* build_result) {
    if (!analyzer || !build_result) return NULL;

    /* Check if there's actually an error */
    if (build_result->success) {
        return strdup("Build succeeded - no errors to analyze.");
    }

    /* Get error output */
    const char* error_output = build_result->stderr_output;
    if (!error_output || strlen(error_output) == 0) {
        error_output = build_result->stdout_output;
    }
    if (!error_output || strlen(error_output) == 0) {
        return strdup("No error output available to analyze.");
    }

    /* Generate smart prompt based on error type */
    char* prompt = prompt_smart_error_analysis(error_output, analyzer->project);
    if (!prompt) {
        log_error("Failed to generate error analysis prompt");
        return NULL;
    }

    if (analyzer->verbose) {
        log_debug("Generated prompt:\n%s", prompt);
    }

    /* Create LLM request */
    LLMRequest* request = llm_request_create(prompt);
    if (!request) {
        free(prompt);
        return NULL;
    }

    /* Configure for error analysis */
    request->temperature = 0.3f;  /* Lower temperature for more focused responses */
    request->max_tokens = 512;    /* Reasonable length for fixes */
    request->top_p = 0.95f;

    /* Query LLM */
    log_info("Analyzing error with AI...");
    LLMResponse* response = llm_query(analyzer->llm, request);

    char* result = NULL;
    if (response && response->success) {
        result = format_llm_response(response->text);
        if (analyzer->verbose) {
            log_debug("Tokens used: %d (prompt) + %d (generated)",
                      response->tokens_prompt, response->tokens_generated);
            log_debug("Generation time: %.2f seconds", response->duration_sec);
        }
    } else {
        log_error("LLM query failed: %s",
                  response ? response->error_message : "Unknown error");
    }

    /* Cleanup */
    llm_response_free(response);
    llm_request_free(request);
    free(prompt);

    return result;
}

/**
 * Analyze build error and display suggestions interactively
 * @param analyzer Error analyzer
 * @param build_result Build result with error output
 * @return True if analysis succeeded
 */
bool error_analyzer_interactive(ErrorAnalyzer* analyzer,
                                 const BuildResult* build_result) {
    if (!analyzer || !build_result) return false;

    /* Analyze error */
    char* suggestion = error_analyzer_analyze(analyzer, build_result);
    if (!suggestion) {
        log_error("Failed to analyze build error");
        return false;
    }

    /* Display suggestion */
    log_plain("\n");
    log_info("AI Analysis:");
    log_plain("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    log_plain("%s\n", suggestion);
    log_plain("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    free(suggestion);
    return true;
}

/**
 * Get dependency installation command
 * @param analyzer Error analyzer
 * @param dependency_name Name of missing dependency
 * @return Installation command (caller must free), or NULL
 */
char* error_analyzer_get_install_cmd(ErrorAnalyzer* analyzer,
                                      const char* dependency_name) {
    if (!analyzer || !dependency_name) return NULL;

    /* Determine OS */
#ifdef _WIN32
    const char* os = "Windows";
#elif __APPLE__
    const char* os = "macOS";
#else
    const char* os = "Linux";
#endif

    /* Generate prompt */
    char* prompt = prompt_resolve_dependency(
        dependency_name,
        analyzer->project ? analyzer->project->build_system.type : BUILD_UNKNOWN,
        os
    );

    if (!prompt) return NULL;

    /* Query LLM */
    LLMRequest* request = llm_request_create(prompt);
    if (!request) {
        free(prompt);
        return NULL;
    }

    request->temperature = 0.2f;  /* Very low for factual responses */
    request->max_tokens = 256;

    log_info("Finding installation command for %s...", dependency_name);
    LLMResponse* response = llm_query(analyzer->llm, request);

    char* result = NULL;
    if (response && response->success) {
        result = strdup(response->text);
    }

    llm_response_free(response);
    llm_request_free(request);
    free(prompt);

    return result;
}

/**
 * Generate build configuration file
 * @param analyzer Error analyzer
 * @param project_type Type of project (library/application)
 * @param language Programming language
 * @param dependencies Comma-separated list of dependencies
 * @return Generated configuration (caller must free), or NULL
 */
char* error_analyzer_generate_config(ErrorAnalyzer* analyzer,
                                      const char* project_type,
                                      const char* language,
                                      const char* dependencies) {
    if (!analyzer || !language) return NULL;

    /* Generate prompt */
    char* prompt = prompt_create_build_config(project_type, language, dependencies);
    if (!prompt) return NULL;

    /* Query LLM */
    LLMRequest* request = llm_request_create(prompt);
    if (!request) {
        free(prompt);
        return NULL;
    }

    request->temperature = 0.3f;
    request->max_tokens = 1024;  /* Build configs can be longer */

    log_info("Generating build configuration...");
    LLMResponse* response = llm_query(analyzer->llm, request);

    char* result = NULL;
    if (response && response->success) {
        result = strdup(response->text);
    }

    llm_response_free(response);
    llm_request_free(request);
    free(prompt);

    return result;
}

/**
 * Suggest build optimizations
 * @param analyzer Error analyzer
 * @param build_time Build time in seconds
 * @return Optimization suggestions (caller must free), or NULL
 */
char* error_analyzer_optimize(ErrorAnalyzer* analyzer, double build_time) {
    if (!analyzer || !analyzer->project) return NULL;

    /* Generate prompt */
    char* prompt = prompt_optimize_build(analyzer->project, build_time);
    if (!prompt) return NULL;

    /* Query LLM */
    LLMRequest* request = llm_request_create(prompt);
    if (!request) {
        free(prompt);
        return NULL;
    }

    request->temperature = 0.5f;
    request->max_tokens = 512;

    log_info("Analyzing build performance...");
    LLMResponse* response = llm_query(analyzer->llm, request);

    char* result = NULL;
    if (response && response->success) {
        result = format_llm_response(response->text);
    }

    llm_response_free(response);
    llm_request_free(request);
    free(prompt);

    return result;
}