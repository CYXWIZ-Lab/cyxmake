/**
 * @file error_analyzer.h
 * @brief AI-powered build error analysis
 */

#ifndef CYXMAKE_ERROR_ANALYZER_H
#define CYXMAKE_ERROR_ANALYZER_H

#include "llm_interface.h"
#include "build_executor.h"
#include "project_context.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct ErrorAnalyzer ErrorAnalyzer;

/**
 * Create error analyzer
 * @param llm_ctx LLM context (must be initialized)
 * @param project_ctx Project context
 * @return Error analyzer instance, or NULL on error
 */
ErrorAnalyzer* error_analyzer_create(LLMContext* llm_ctx,
                                      ProjectContext* project_ctx);

/**
 * Free error analyzer
 * @param analyzer Error analyzer to free
 */
void error_analyzer_free(ErrorAnalyzer* analyzer);

/**
 * Analyze build error and get suggestions
 * @param analyzer Error analyzer
 * @param build_result Build result with error output
 * @return Suggested fix (caller must free), or NULL on error
 */
char* error_analyzer_analyze(ErrorAnalyzer* analyzer,
                              const BuildResult* build_result);

/**
 * Analyze build error and display suggestions interactively
 * @param analyzer Error analyzer
 * @param build_result Build result with error output
 * @return True if analysis succeeded
 */
bool error_analyzer_interactive(ErrorAnalyzer* analyzer,
                                 const BuildResult* build_result);

/**
 * Get dependency installation command
 * @param analyzer Error analyzer
 * @param dependency_name Name of missing dependency
 * @return Installation command (caller must free), or NULL
 */
char* error_analyzer_get_install_cmd(ErrorAnalyzer* analyzer,
                                      const char* dependency_name);

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
                                      const char* dependencies);

/**
 * Suggest build optimizations
 * @param analyzer Error analyzer
 * @param build_time Build time in seconds
 * @return Optimization suggestions (caller must free), or NULL
 */
char* error_analyzer_optimize(ErrorAnalyzer* analyzer, double build_time);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_ERROR_ANALYZER_H */