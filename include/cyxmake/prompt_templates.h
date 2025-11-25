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

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_PROMPT_TEMPLATES_H */