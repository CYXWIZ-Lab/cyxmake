/**
 * @file build_intelligence.h
 * @brief Rule-based build intelligence system
 *
 * This module provides intelligent build defaults and error recovery
 * that work even without AI. It captures common build patterns and
 * solutions to complement the AI system.
 */

#ifndef CYXMAKE_BUILD_INTELLIGENCE_H
#define CYXMAKE_BUILD_INTELLIGENCE_H

#include "cyxmake/project_context.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Build Command Templates
 * ======================================================================== */

/**
 * Build command set for a specific build system
 */
typedef struct {
    const char* configure_cmd;     /* Command to configure/generate */
    const char* build_cmd;         /* Command to build */
    const char* clean_cmd;         /* Command to clean */
    const char* test_cmd;          /* Command to run tests */
    bool needs_build_dir;          /* Needs separate build directory */
    const char* build_dir_name;    /* Name of build directory */
} BuildCommandSet;

/**
 * Get standard build commands for a project
 * Returns commands that are known to work for this build system type
 */
BuildCommandSet build_intelligence_get_commands(BuildSystem type);

/**
 * Get the correct cmake configure command with proper flags
 * Handles the cmake version policy issue automatically
 */
char* build_intelligence_cmake_configure(const char* source_dir,
                                          const char* build_dir,
                                          const char* extra_args);

/**
 * Get the correct cmake build command
 */
char* build_intelligence_cmake_build(const char* build_dir,
                                      const char* config);

/* ========================================================================
 * Error Pattern Recognition
 * ======================================================================== */

typedef enum {
    BUILD_ERROR_NONE,
    BUILD_ERROR_CMAKE_VERSION,         /* CMake version/policy issue */
    BUILD_ERROR_MISSING_DEPENDENCY,    /* Missing library or package */
    BUILD_ERROR_MISSING_INCLUDE,       /* Missing header file */
    BUILD_ERROR_COMPILER_NOT_FOUND,    /* Compiler not in PATH */
    BUILD_ERROR_SYNTAX_ERROR,          /* Code syntax error */
    BUILD_ERROR_LINK_ERROR,            /* Linker error */
    BUILD_ERROR_PERMISSION_DENIED,     /* Permission issue */
    BUILD_ERROR_OUT_OF_MEMORY,         /* Memory exhausted */
    BUILD_ERROR_NO_CMAKE_LISTS,        /* No CMakeLists.txt found */
    BUILD_ERROR_WRONG_DIRECTORY,       /* Running from wrong directory */
    BUILD_ERROR_UNKNOWN               /* Unknown error */
} BuildErrorType;

/**
 * Detected error with fix suggestion
 */
typedef struct {
    BuildErrorType type;
    char* pattern_matched;      /* The pattern that matched */
    char* description;          /* Human-readable description */
    char* fix_command;          /* Suggested fix command */
    char* fix_description;      /* What the fix does */
    float confidence;           /* How confident we are (0-1) */
    bool requires_admin;        /* Needs elevated privileges */
} DetectedBuildError;

/**
 * Analyze error output and detect known patterns
 * Returns an array of detected errors (NULL terminated)
 */
DetectedBuildError** build_intelligence_analyze_error(const char* error_output,
                                                       const ProjectContext* ctx);

/**
 * Free detected error
 */
void detected_build_error_free(DetectedBuildError* error);

/**
 * Free array of detected errors
 */
void detected_build_errors_free(DetectedBuildError** errors);

/* ========================================================================
 * Smart Fix Generation
 * ======================================================================== */

/**
 * Generate a fix for a known error type
 * Returns the command to run, or NULL if no automatic fix is available
 */
char* build_intelligence_generate_fix(BuildErrorType error_type,
                                       const char* error_details,
                                       const ProjectContext* ctx);

/**
 * Get a fallback build plan when AI fails
 * Uses rule-based logic to generate a sensible build plan
 */
typedef struct BuildIntelligencePlan {
    char** commands;            /* Array of commands to run */
    char** descriptions;        /* Description for each command */
    int command_count;
} BuildIntelligencePlan;

BuildIntelligencePlan* build_intelligence_fallback_plan(const ProjectContext* ctx);
void build_intelligence_plan_free(BuildIntelligencePlan* plan);

/* ========================================================================
 * Platform-Specific Knowledge
 * ======================================================================== */

/**
 * Get the correct package install command for this platform
 */
char* build_intelligence_package_install_cmd(const char* package_name);

/**
 * Check if a command exists on this platform
 */
bool build_intelligence_command_exists(const char* command);

/**
 * Get cmake with proper version handling
 */
const char* build_intelligence_get_cmake_path(void);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_BUILD_INTELLIGENCE_H */
