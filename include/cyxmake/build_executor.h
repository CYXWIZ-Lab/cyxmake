/**
 * @file build_executor.h
 * @brief Build command execution system
 */

#ifndef CYXMAKE_BUILD_EXECUTOR_H
#define CYXMAKE_BUILD_EXECUTOR_H

#include "project_context.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Build result
 */
typedef struct {
    int exit_code;          /* Exit code from build command */
    char* stdout_output;    /* Captured stdout */
    char* stderr_output;    /* Captured stderr */
    double duration_sec;    /* Build duration in seconds */
    bool success;           /* True if build succeeded */
} BuildResult;

/**
 * Build options
 */
typedef struct {
    bool verbose;           /* Verbose output */
    bool clean_first;       /* Clean before building */
    int parallel_jobs;      /* Number of parallel jobs (0 = auto) */
    char* target;           /* Specific target to build (NULL = default) */
    char* build_dir;        /* Build directory (NULL = auto) */
} BuildOptions;

/**
 * Create default build options
 * @return Allocated BuildOptions (caller must free)
 */
BuildOptions* build_options_default(void);

/**
 * Free build options
 * @param opts Build options to free
 */
void build_options_free(BuildOptions* opts);

/**
 * Execute build command for a project
 * @param ctx Project context
 * @param opts Build options (NULL for defaults)
 * @return Build result (caller must free with build_result_free)
 */
BuildResult* build_execute(const ProjectContext* ctx, const BuildOptions* opts);

/**
 * Execute a specific build command
 * @param command Command to execute
 * @param working_dir Working directory (NULL for current)
 * @return Build result (caller must free with build_result_free)
 */
BuildResult* build_execute_command(const char* command, const char* working_dir);

/**
 * Get build command for a build system
 * @param build_system Build system type
 * @param opts Build options (NULL for defaults)
 * @return Allocated command string (caller must free)
 */
char* build_get_command(BuildSystem build_system, const BuildOptions* opts);

/**
 * Free build result
 * @param result Build result to free
 */
void build_result_free(BuildResult* result);

/**
 * Print build result summary
 * @param result Build result
 */
void build_result_print(const BuildResult* result);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_BUILD_EXECUTOR_H */
