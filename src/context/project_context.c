/**
 * @file project_context.c
 * @brief Project Context data structure implementations
 */

#include "cyxmake/project_context.h"
#include "cyxmake/compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Language to string mapping */
const char* language_to_string(Language lang) {
    switch (lang) {
        case LANG_C: return "C";
        case LANG_CPP: return "C++";
        case LANG_PYTHON: return "Python";
        case LANG_JAVASCRIPT: return "JavaScript";
        case LANG_TYPESCRIPT: return "TypeScript";
        case LANG_RUST: return "Rust";
        case LANG_GO: return "Go";
        case LANG_JAVA: return "Java";
        case LANG_CSHARP: return "C#";
        case LANG_RUBY: return "Ruby";
        case LANG_PHP: return "PHP";
        case LANG_SHELL: return "Shell";
        default: return "Unknown";
    }
}

/* Build system to string mapping */
const char* build_system_to_string(BuildSystem build) {
    switch (build) {
        case BUILD_CMAKE: return "CMake";
        case BUILD_MAKE: return "Make";
        case BUILD_MESON: return "Meson";
        case BUILD_CARGO: return "Cargo";
        case BUILD_NPM: return "npm";
        case BUILD_GRADLE: return "Gradle";
        case BUILD_MAVEN: return "Maven";
        case BUILD_BAZEL: return "Bazel";
        case BUILD_SETUPTOOLS: return "setuptools";
        case BUILD_POETRY: return "Poetry";
        case BUILD_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

/* Parse language from string */
Language language_from_string(const char* str) {
    if (!str) return LANG_UNKNOWN;

    if (strcasecmp(str, "c") == 0) return LANG_C;
    if (strcasecmp(str, "c++") == 0 || strcasecmp(str, "cpp") == 0) return LANG_CPP;
    if (strcasecmp(str, "python") == 0 || strcasecmp(str, "py") == 0) return LANG_PYTHON;
    if (strcasecmp(str, "javascript") == 0 || strcasecmp(str, "js") == 0) return LANG_JAVASCRIPT;
    if (strcasecmp(str, "typescript") == 0 || strcasecmp(str, "ts") == 0) return LANG_TYPESCRIPT;
    if (strcasecmp(str, "rust") == 0 || strcasecmp(str, "rs") == 0) return LANG_RUST;
    if (strcasecmp(str, "go") == 0) return LANG_GO;
    if (strcasecmp(str, "java") == 0) return LANG_JAVA;
    if (strcasecmp(str, "c#") == 0 || strcasecmp(str, "csharp") == 0) return LANG_CSHARP;
    if (strcasecmp(str, "ruby") == 0 || strcasecmp(str, "rb") == 0) return LANG_RUBY;
    if (strcasecmp(str, "php") == 0) return LANG_PHP;
    if (strcasecmp(str, "shell") == 0 || strcasecmp(str, "sh") == 0) return LANG_SHELL;

    return LANG_UNKNOWN;
}

/* Parse build system from string */
BuildSystem build_system_from_string(const char* str) {
    if (!str) return BUILD_UNKNOWN;

    if (strcasecmp(str, "cmake") == 0) return BUILD_CMAKE;
    if (strcasecmp(str, "make") == 0) return BUILD_MAKE;
    if (strcasecmp(str, "meson") == 0) return BUILD_MESON;
    if (strcasecmp(str, "cargo") == 0) return BUILD_CARGO;
    if (strcasecmp(str, "npm") == 0) return BUILD_NPM;
    if (strcasecmp(str, "gradle") == 0) return BUILD_GRADLE;
    if (strcasecmp(str, "maven") == 0) return BUILD_MAVEN;
    if (strcasecmp(str, "bazel") == 0) return BUILD_BAZEL;
    if (strcasecmp(str, "setuptools") == 0) return BUILD_SETUPTOOLS;
    if (strcasecmp(str, "poetry") == 0) return BUILD_POETRY;
    if (strcasecmp(str, "custom") == 0) return BUILD_CUSTOM;

    return BUILD_UNKNOWN;
}

/* Create default analysis options */
AnalysisOptions* analysis_options_default(void) {
    AnalysisOptions* opts = calloc(1, sizeof(AnalysisOptions));
    if (!opts) return NULL;

    opts->analyze_dependencies = true;
    opts->parse_readme = true;
    opts->scan_git = true;
    opts->deep_analysis = false;
    opts->max_files = 10000;
    opts->ignore_patterns = NULL;
    opts->ignore_pattern_count = 0;

    return opts;
}

/* Free analysis options */
void analysis_options_free(AnalysisOptions* options) {
    if (!options) return;

    if (options->ignore_patterns) {
        for (size_t i = 0; i < options->ignore_pattern_count; i++) {
            free(options->ignore_patterns[i]);
        }
        free(options->ignore_patterns);
    }

    free(options);
}

/* Free dependency */
static void dependency_free(Dependency* dep) {
    if (!dep) return;
    free(dep->name);
    free(dep->version_spec);
    free(dep->installed_version);
    free(dep->source);
    free(dep);
}

/* Free source file */
static void source_file_free(SourceFile* file) {
    if (!file) return;
    free(file->path);
    free(file);
}

/* Free language stats */
static void language_stats_free(LanguageStats* stats) {
    if (!stats) return;
    free(stats);
}

/* Free build step */
static void build_step_free(BuildStep* step) {
    if (!step) return;
    free(step->description);
    free(step->command);
    free(step);
}

/* Free build target */
static void build_target_free(BuildTarget* target) {
    if (!target) return;
    free(target->name);
    free(target->type);
    if (target->sources) {
        for (size_t i = 0; i < target->source_count; i++) {
            free(target->sources[i]);
        }
        free(target->sources);
    }
    free(target);
}

/* Free project context */
void project_context_free(ProjectContext* ctx) {
    if (!ctx) return;

    free(ctx->name);
    free(ctx->root_path);
    free(ctx->type);

    /* Free language stats */
    if (ctx->language_stats) {
        for (size_t i = 0; i < ctx->language_count; i++) {
            language_stats_free(ctx->language_stats[i]);
        }
        free(ctx->language_stats);
    }

    /* Free build system info */
    if (ctx->build_system.config_files) {
        for (size_t i = 0; i < ctx->build_system.config_file_count; i++) {
            free(ctx->build_system.config_files[i]);
        }
        free(ctx->build_system.config_files);
    }
    if (ctx->build_system.steps) {
        for (size_t i = 0; i < ctx->build_system.step_count; i++) {
            build_step_free(ctx->build_system.steps[i]);
        }
        free(ctx->build_system.steps);
    }
    if (ctx->build_system.targets) {
        for (size_t i = 0; i < ctx->build_system.target_count; i++) {
            build_target_free(ctx->build_system.targets[i]);
        }
        free(ctx->build_system.targets);
    }

    /* Free dependencies */
    if (ctx->dependencies) {
        for (size_t i = 0; i < ctx->dependency_count; i++) {
            dependency_free(ctx->dependencies[i]);
        }
        free(ctx->dependencies);
    }

    /* Free source files */
    if (ctx->source_files) {
        for (size_t i = 0; i < ctx->source_file_count; i++) {
            source_file_free(ctx->source_files[i]);
        }
        free(ctx->source_files);
    }

    /* Free README info */
    free(ctx->readme.path);
    if (ctx->readme.steps) {
        for (size_t i = 0; i < ctx->readme.step_count; i++) {
            build_step_free(ctx->readme.steps[i]);
        }
        free(ctx->readme.steps);
    }
    if (ctx->readme.prerequisites) {
        for (size_t i = 0; i < ctx->readme.prerequisite_count; i++) {
            free(ctx->readme.prerequisites[i]);
        }
        free(ctx->readme.prerequisites);
    }

    /* Free git info */
    free(ctx->git.remote);
    free(ctx->git.branch);

    /* Free metadata */
    free(ctx->cache_version);
    free(ctx->content_hash);

    free(ctx);
}

/* Calculate content hash (stub for now) */
char* calculate_content_hash(ProjectContext* ctx) {
    if (!ctx) return NULL;

    /* TODO: Implement proper SHA-256 hashing */
    /* For now, use a simple timestamp-based hash */
    char* hash = malloc(65);  /* 64 hex chars + null */
    if (!hash) return NULL;

    snprintf(hash, 65, "%016lx%016lx%016lx%016lx",
             (unsigned long)ctx->created_at,
             (unsigned long)ctx->source_file_count,
             (unsigned long)ctx->dependency_count,
             (unsigned long)ctx->build_system.type);

    return hash;
}
