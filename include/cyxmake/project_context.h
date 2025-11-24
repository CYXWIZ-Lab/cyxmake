/**
 * @file project_context.h
 * @brief Project Context Manager - Maintains semantic understanding of projects
 * @version 0.1.0
 */

#ifndef CYXMAKE_PROJECT_CONTEXT_H
#define CYXMAKE_PROJECT_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <time.h>

/* Language enumeration */
typedef enum {
    LANG_UNKNOWN = 0,
    LANG_C,
    LANG_CPP,
    LANG_PYTHON,
    LANG_JAVASCRIPT,
    LANG_TYPESCRIPT,
    LANG_RUST,
    LANG_GO,
    LANG_JAVA,
    LANG_CSHARP,
    LANG_RUBY,
    LANG_PHP,
    LANG_SHELL
} Language;

/* Build system enumeration */
typedef enum {
    BUILD_UNKNOWN = 0,
    BUILD_CMAKE,
    BUILD_MAKE,
    BUILD_MESON,
    BUILD_CARGO,
    BUILD_NPM,
    BUILD_GRADLE,
    BUILD_MAVEN,
    BUILD_BAZEL,
    BUILD_SETUPTOOLS,
    BUILD_POETRY,
    BUILD_CUSTOM
} BuildSystem;

/* Language statistics */
typedef struct {
    Language language;
    size_t file_count;
    size_t line_count;
    float percentage;
} LanguageStats;

/* Dependency information */
typedef struct {
    char* name;
    char* version_spec;
    bool is_installed;
    char* installed_version;
    bool is_dev_dependency;
    char* source;  /* "npm", "pip", "cargo", "vcpkg", etc. */
} Dependency;

/* Source file information */
typedef struct {
    char* path;
    Language language;
    size_t line_count;
    time_t last_modified;
    bool is_generated;
} SourceFile;

/* Build target information */
typedef struct {
    char* name;
    char* type;  /* "executable", "library", "test" */
    char** sources;
    size_t source_count;
} BuildTarget;

/* Build step */
typedef struct {
    int number;
    char* description;
    char* command;
} BuildStep;

/* Build system info */
typedef struct {
    BuildSystem type;
    char** config_files;
    size_t config_file_count;
    BuildStep** steps;
    size_t step_count;
    BuildTarget** targets;
    size_t target_count;
} BuildSystemInfo;

/* README information */
typedef struct {
    char* path;
    bool has_build_instructions;
    BuildStep** steps;
    size_t step_count;
    char** prerequisites;
    size_t prerequisite_count;
} ReadmeInfo;

/* Git repository information */
typedef struct {
    bool is_repo;
    char* remote;
    char* branch;
    bool has_uncommitted_changes;
} GitInfo;

/* Main project context */
typedef struct ProjectContext {
    char* name;
    char* root_path;
    char* type;

    /* Language information */
    Language primary_language;
    LanguageStats** language_stats;
    size_t language_count;

    /* Build system */
    BuildSystemInfo build_system;

    /* Dependencies */
    Dependency** dependencies;
    size_t dependency_count;
    size_t dependencies_missing;

    /* Source files */
    SourceFile** source_files;
    size_t source_file_count;

    /* README */
    ReadmeInfo readme;

    /* Git */
    GitInfo git;

    /* Metadata */
    time_t created_at;
    time_t updated_at;
    char* cache_version;
    float confidence;
    char* content_hash;
} ProjectContext;

/* Analysis options */
typedef struct {
    bool analyze_dependencies;
    bool parse_readme;
    bool scan_git;
    bool deep_analysis;
    int max_files;
    char** ignore_patterns;
    size_t ignore_pattern_count;
} AnalysisOptions;

/* API Functions */

/**
 * Create default analysis options
 * @return Default options structure
 */
AnalysisOptions* analysis_options_default(void);

/**
 * Free analysis options
 * @param options Options to free
 */
void analysis_options_free(AnalysisOptions* options);

/**
 * Analyze project and create context
 * @param root_path Path to project root
 * @param options Analysis options (or NULL for defaults)
 * @return ProjectContext or NULL on failure
 */
ProjectContext* project_analyze(const char* root_path, AnalysisOptions* options);

/**
 * Free project context
 * @param ctx Context to free
 */
void project_context_free(ProjectContext* ctx);

/**
 * Convert language enum to string
 * @param lang Language enum
 * @return Language name
 */
const char* language_to_string(Language lang);

/**
 * Convert build system enum to string
 * @param build Build system enum
 * @return Build system name
 */
const char* build_system_to_string(BuildSystem build);

/**
 * Parse language from string
 * @param str Language name
 * @return Language enum
 */
Language language_from_string(const char* str);

/**
 * Parse build system from string
 * @param str Build system name
 * @return BuildSystem enum
 */
BuildSystem build_system_from_string(const char* str);

/**
 * Detect primary language from file extensions
 * @param root_path Project root path
 * @return Detected language
 */
Language detect_primary_language(const char* root_path);

/**
 * Detect build system from config files
 * @param root_path Project root path
 * @return Detected build system
 */
BuildSystem detect_build_system(const char* root_path);

/**
 * Calculate language statistics
 * @param files Array of source files
 * @param file_count Number of files
 * @param stats_count Output: number of language stats
 * @return Array of language statistics
 */
LanguageStats** calculate_language_stats(SourceFile** files,
                                          size_t file_count,
                                          size_t* stats_count);

/**
 * Scan source files in directory
 * @param root_path Project root path
 * @param primary_lang Primary language hint
 * @param file_count Output: number of files found
 * @return Array of source files
 */
SourceFile** scan_source_files(const char* root_path,
                                Language primary_lang,
                                size_t* file_count);

/**
 * Calculate content hash for change detection
 * @param ctx Project context
 * @return SHA-256 hash string (caller must free)
 */
char* calculate_content_hash(ProjectContext* ctx);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_PROJECT_CONTEXT_H */
