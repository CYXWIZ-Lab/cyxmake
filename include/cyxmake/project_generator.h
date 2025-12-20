/**
 * @file project_generator.h
 * @brief Project scaffolding and generation from natural language
 */

#ifndef CYXMAKE_PROJECT_GENERATOR_H
#define CYXMAKE_PROJECT_GENERATOR_H

#include <stdbool.h>
#include "cyxmake/project_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Project Type Enumeration
 * ======================================================================== */

typedef enum {
    PROJECT_EXECUTABLE = 0,   /* Standalone executable */
    PROJECT_LIBRARY,          /* Static/shared library */
    PROJECT_GAME,             /* Game project (SDL2, OpenGL, etc.) */
    PROJECT_CLI,              /* Command-line tool */
    PROJECT_WEB,              /* Web application/API */
    PROJECT_GUI,              /* Desktop GUI application */
    PROJECT_TEST              /* Test project */
} ProjectType;

/* ========================================================================
 * Project Specification
 * ======================================================================== */

/**
 * Project specification parsed from natural language description
 */
typedef struct {
    char* name;               /* Project name */
    Language language;        /* Primary language */
    BuildSystem build_system; /* Build system to use */
    ProjectType type;         /* Project type */

    char** dependencies;      /* Dependency names */
    int dependency_count;

    bool with_tests;          /* Generate test scaffold */
    bool with_docs;           /* Generate documentation */
    bool with_git;            /* Initialize git repository */

    char* cpp_standard;       /* C++ standard (e.g., "17", "20") */
    char* c_standard;         /* C standard (e.g., "11", "17") */
    char* license;            /* License type (MIT, Apache-2.0, etc.) */

    char* description;        /* Project description */
} ProjectSpec;

/* ========================================================================
 * Generation Result
 * ======================================================================== */

/**
 * Result of project generation
 */
typedef struct {
    bool success;
    char* output_path;        /* Path to generated project */
    char** files_created;     /* List of created files */
    int file_count;
    char* error_message;      /* Error message if failed */
} GenerationResult;

/* ========================================================================
 * Project Specification API
 * ======================================================================== */

/**
 * Create default project specification
 * @return New ProjectSpec with defaults (caller must free)
 */
ProjectSpec* project_spec_create(void);

/**
 * Parse project specification from natural language description
 * Uses keyword matching for language, type, and dependencies
 * @param description Natural language description
 * @return Parsed ProjectSpec or NULL on failure
 */
ProjectSpec* project_spec_parse(const char* description);

/**
 * Free project specification
 * @param spec Specification to free
 */
void project_spec_free(ProjectSpec* spec);

/* ========================================================================
 * Project Generation API
 * ======================================================================== */

/**
 * Generate project from specification
 * @param spec Project specification
 * @param output_path Directory to create project in
 * @return Generation result (caller must free)
 */
GenerationResult* project_generate(const ProjectSpec* spec, const char* output_path);

/**
 * Free generation result
 * @param result Result to free
 */
void generation_result_free(GenerationResult* result);

/* ========================================================================
 * Template Generation Functions
 * ======================================================================== */

/**
 * Generate CMakeLists.txt content
 * @param spec Project specification
 * @return Allocated string with CMake content (caller must free)
 */
char* generate_cmake_content(const ProjectSpec* spec);

/**
 * Generate Cargo.toml content (for Rust projects)
 * @param spec Project specification
 * @return Allocated string with Cargo content (caller must free)
 */
char* generate_cargo_content(const ProjectSpec* spec);

/**
 * Generate package.json content (for Node.js projects)
 * @param spec Project specification
 * @return Allocated string with package.json content (caller must free)
 */
char* generate_package_json_content(const ProjectSpec* spec);

/**
 * Generate main source file content
 * @param spec Project specification
 * @return Allocated string with source content (caller must free)
 */
char* generate_main_source(const ProjectSpec* spec);

/**
 * Generate README.md content
 * @param spec Project specification
 * @return Allocated string with README content (caller must free)
 */
char* generate_readme(const ProjectSpec* spec);

/**
 * Generate .gitignore content
 * @param language Primary language
 * @param build_system Build system
 * @return Allocated string with gitignore content (caller must free)
 */
char* generate_gitignore(Language language, BuildSystem build_system);

/* ========================================================================
 * Utility Functions
 * Note: language_from_string, language_to_string, and build_system_from_string
 * are declared in project_context.h
 * ======================================================================== */

/**
 * Get file extension for language
 * @param language Language enum
 * @param is_header True for header files
 * @return Static file extension string
 */
const char* language_extension(Language language, bool is_header);

/**
 * Get default build system for language
 * @param language Language enum
 * @return Default build system for the language
 */
BuildSystem default_build_system(Language language);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_PROJECT_GENERATOR_H */
