/**
 * @file prompt_templates.c
 * @brief LLM prompt templates for build error analysis
 */

#include "cyxmake/llm_interface.h"
#include "cyxmake/project_context.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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