# CyxMake: Core Components Design

## Overview

This document provides detailed designs for three critical core components of CyxMake:

1. **Project Context Manager** - Maintains semantic understanding of projects
2. **Error Recovery System** - Autonomous error diagnosis and fixing
3. **LLM Integration Layer** - Hybrid local/cloud AI orchestration

These components form the "intelligence layer" that enables CyxMake's autonomous capabilities.

---

# Part 1: Project Context Manager

## Purpose

The Project Context Manager maintains a live, semantic representation of the project being built. Unlike traditional build tools that parse files on-demand, it creates a persistent, queryable model of the project's structure, dependencies, and state.

## Key Responsibilities

1. **Project Analysis**: Deep scanning and understanding of project structure
2. **Cache Management**: Persistent storage and retrieval of project knowledge
3. **Change Detection**: Identify modifications that invalidate cached knowledge
4. **Semantic Querying**: Answer questions about the project (e.g., "What are the dependencies?")
5. **Incremental Updates**: Efficiently update cache when files change

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│           Project Context Manager                        │
├─────────────────────────────────────────────────────────┤
│                                                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   Project    │  │    Cache     │  │   Change     │  │
│  │   Analyzer   │  │   Manager    │  │   Detector   │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│                                                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  Dependency  │  │  Semantic    │  │    Query     │  │
│  │   Scanner    │  │    Graph     │  │   Engine     │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│                                                           │
└─────────────────────────────────────────────────────────┘
                         │
                         ▼
          ┌─────────────────────────────┐
          │   .cyxmake/cache.json        │
          │   (Persistent Storage)       │
          └─────────────────────────────┘
```

---

## Data Structures

### Core Context Structure

```c
// project_context.h

typedef enum {
    LANG_C,
    LANG_CPP,
    LANG_PYTHON,
    LANG_JAVASCRIPT,
    LANG_TYPESCRIPT,
    LANG_RUST,
    LANG_GO,
    LANG_JAVA,
    LANG_CSHARP,
    LANG_UNKNOWN
} Language;

typedef enum {
    BUILD_CMAKE,
    BUILD_MAKE,
    BUILD_MESON,
    BUILD_CARGO,
    BUILD_NPM,
    BUILD_GRADLE,
    BUILD_MAVEN,
    BUILD_BAZEL,
    BUILD_SETUPTOOLS,
    BUILD_CUSTOM,
    BUILD_UNKNOWN
} BuildSystem;

typedef struct {
    Language language;
    size_t file_count;
    size_t line_count;
    float percentage;
} LanguageStats;

typedef struct {
    char* name;
    char* version_spec;      // e.g., ">=1.21.0", "^2.0.0"
    bool is_installed;
    char* installed_version;
    bool is_dev_dependency;
    char* source;            // "npm", "pip", "cargo", "vcpkg"
} Dependency;

typedef struct {
    char* path;              // Relative to project root
    Language language;
    size_t line_count;
    time_t last_modified;
    bool is_generated;       // e.g., protobuf output
} SourceFile;

typedef struct {
    char* name;              // Target name (e.g., "myapp")
    char* type;              // "executable", "shared_library", "static_library"
    SourceFile** sources;
    size_t source_count;
    Dependency** dependencies;
    size_t dependency_count;
} BuildTarget;

typedef struct {
    char* description;
    char* command;
    int step_number;
} BuildStep;

typedef struct {
    BuildSystem type;
    char** config_files;     // ["CMakeLists.txt", "cmake/Modules.cmake"]
    size_t config_file_count;
    BuildStep** steps;
    size_t step_count;
    BuildTarget** targets;
    size_t target_count;
} BuildSystemInfo;

typedef struct {
    char* readme_path;
    bool has_build_instructions;
    BuildStep** extracted_steps;
    size_t step_count;
    char** prerequisites;
    size_t prerequisite_count;
} ReadmeInfo;

typedef struct {
    char* name;
    char* root_path;
    char* type;              // "executable", "library", "package", "application"

    // Language information
    Language primary_language;
    LanguageStats** language_stats;
    size_t language_count;

    // Build system
    BuildSystemInfo build_system;

    // Dependencies
    Dependency** dependencies;
    size_t dependency_count;
    size_t dependencies_missing;

    // Source files
    SourceFile** source_files;
    size_t source_file_count;

    // README
    ReadmeInfo readme;

    // Git information
    bool is_git_repo;
    char* git_remote;
    char* git_branch;
    bool has_uncommitted_changes;

    // Metadata
    time_t created_at;
    time_t updated_at;
    char* cache_version;
    float confidence;        // 0.0-1.0, how confident we are about this analysis

    // Hash for change detection
    char* content_hash;      // SHA-256 of all analyzed files

    // Semantic graph (for complex queries)
    void* semantic_graph;    // Opaque pointer to graph structure
} ProjectContext;
```

---

## Implementation: Project Analyzer

### Analysis Pipeline

```c
// project_analyzer.h

typedef struct {
    bool analyze_dependencies;
    bool parse_readme;
    bool scan_git;
    bool deep_analysis;      // Includes semantic graph construction
    int max_files;           // Limit for large projects
    char** ignore_patterns;  // Additional patterns to ignore
} AnalysisOptions;

// Main entry point
ProjectContext* project_analyze(const char* root_path,
                                AnalysisOptions* options,
                                ToolRegistry* tools);

// Individual analysis steps
Language detect_primary_language(const char* root_path);
BuildSystem detect_build_system(const char* root_path);
Dependency** scan_dependencies(ProjectContext* ctx, ToolRegistry* tools);
SourceFile** scan_source_files(const char* root_path, Language primary_lang);
ReadmeInfo parse_readme(const char* readme_path, ToolRegistry* tools);
char* calculate_content_hash(ProjectContext* ctx);
```

### Analysis Implementation

```c
// project_analyzer.c

ProjectContext* project_analyze(const char* root_path,
                                AnalysisOptions* options,
                                ToolRegistry* tools) {
    ProjectContext* ctx = calloc(1, sizeof(ProjectContext));
    ctx->root_path = strdup(root_path);
    ctx->created_at = time(NULL);
    ctx->updated_at = time(NULL);
    ctx->cache_version = strdup("1.0");

    // Step 1: Detect primary language
    log_info("Detecting primary language...");
    ctx->primary_language = detect_primary_language(root_path);

    // Step 2: Scan all source files
    log_info("Scanning source files...");
    ctx->source_files = scan_source_files(root_path, ctx->primary_language);
    ctx->source_file_count = count_source_files(ctx->source_files);

    // Step 3: Calculate language statistics
    log_info("Calculating language statistics...");
    ctx->language_stats = calculate_language_stats(ctx->source_files,
                                                    ctx->source_file_count,
                                                    &ctx->language_count);

    // Step 4: Detect build system
    log_info("Detecting build system...");
    ctx->build_system.type = detect_build_system(root_path);
    ctx->build_system.config_files = find_build_config_files(root_path,
                                                              ctx->build_system.type);

    // Step 5: Scan dependencies (if enabled)
    if (options->analyze_dependencies) {
        log_info("Scanning dependencies...");
        ctx->dependencies = scan_dependencies(ctx, tools);
        ctx->dependency_count = count_dependencies(ctx->dependencies);
        ctx->dependencies_missing = count_missing_dependencies(ctx->dependencies);
    }

    // Step 6: Parse README (if enabled)
    if (options->parse_readme) {
        log_info("Parsing README...");
        char* readme_path = find_readme(root_path);
        if (readme_path) {
            ctx->readme = parse_readme(readme_path, tools);
            free(readme_path);
        }
    }

    // Step 7: Git information (if enabled)
    if (options->scan_git) {
        log_info("Scanning git repository...");
        ctx->is_git_repo = check_git_repo(root_path);
        if (ctx->is_git_repo) {
            ctx->git_remote = get_git_remote(root_path);
            ctx->git_branch = get_git_branch(root_path);
            ctx->has_uncommitted_changes = check_git_status(root_path);
        }
    }

    // Step 8: Build semantic graph (if deep analysis)
    if (options->deep_analysis) {
        log_info("Building semantic graph...");
        ctx->semantic_graph = build_semantic_graph(ctx);
    }

    // Step 9: Calculate content hash for change detection
    log_info("Calculating content hash...");
    ctx->content_hash = calculate_content_hash(ctx);

    // Step 10: Estimate confidence
    ctx->confidence = estimate_analysis_confidence(ctx);

    log_info("Project analysis complete. Confidence: %.2f", ctx->confidence);

    return ctx;
}
```

### Language Detection

```c
// Language detection using file extensions and heuristics

Language detect_primary_language(const char* root_path) {
    HashMap* extension_counts = hashmap_create();

    // Scan for file extensions
    scan_directory_recursive(root_path, extension_counts);

    // Count by language
    size_t c_count = hashmap_get_count(extension_counts, ".c") +
                     hashmap_get_count(extension_counts, ".h");
    size_t cpp_count = hashmap_get_count(extension_counts, ".cpp") +
                       hashmap_get_count(extension_counts, ".hpp") +
                       hashmap_get_count(extension_counts, ".cc");
    size_t py_count = hashmap_get_count(extension_counts, ".py");
    size_t js_count = hashmap_get_count(extension_counts, ".js");
    size_t ts_count = hashmap_get_count(extension_counts, ".ts");
    size_t rs_count = hashmap_get_count(extension_counts, ".rs");

    // Find maximum
    Language detected = LANG_UNKNOWN;
    size_t max_count = 0;

    if (cpp_count > max_count) { detected = LANG_CPP; max_count = cpp_count; }
    if (c_count > max_count) { detected = LANG_C; max_count = c_count; }
    if (py_count > max_count) { detected = LANG_PYTHON; max_count = py_count; }
    if (ts_count > max_count) { detected = LANG_TYPESCRIPT; max_count = ts_count; }
    if (js_count > max_count) { detected = LANG_JAVASCRIPT; max_count = js_count; }
    if (rs_count > max_count) { detected = LANG_RUST; max_count = rs_count; }

    hashmap_destroy(extension_counts);
    return detected;
}
```

### Build System Detection

```c
// Build system detection based on config files

BuildSystem detect_build_system(const char* root_path) {
    char path_buffer[4096];

    // Check for CMakeLists.txt
    snprintf(path_buffer, sizeof(path_buffer), "%s/CMakeLists.txt", root_path);
    if (file_exists(path_buffer)) return BUILD_CMAKE;

    // Check for Cargo.toml
    snprintf(path_buffer, sizeof(path_buffer), "%s/Cargo.toml", root_path);
    if (file_exists(path_buffer)) return BUILD_CARGO;

    // Check for package.json (npm)
    snprintf(path_buffer, sizeof(path_buffer), "%s/package.json", root_path);
    if (file_exists(path_buffer)) return BUILD_NPM;

    // Check for setup.py or pyproject.toml (Python)
    snprintf(path_buffer, sizeof(path_buffer), "%s/setup.py", root_path);
    if (file_exists(path_buffer)) return BUILD_SETUPTOOLS;

    snprintf(path_buffer, sizeof(path_buffer), "%s/pyproject.toml", root_path);
    if (file_exists(path_buffer)) return BUILD_SETUPTOOLS;

    // Check for Makefile
    snprintf(path_buffer, sizeof(path_buffer), "%s/Makefile", root_path);
    if (file_exists(path_buffer)) return BUILD_MAKE;

    // Check for build.gradle
    snprintf(path_buffer, sizeof(path_buffer), "%s/build.gradle", root_path);
    if (file_exists(path_buffer)) return BUILD_GRADLE;

    // Check for pom.xml (Maven)
    snprintf(path_buffer, sizeof(path_buffer), "%s/pom.xml", root_path);
    if (file_exists(path_buffer)) return BUILD_MAVEN;

    return BUILD_UNKNOWN;
}
```

---

## Implementation: Cache Manager

### Cache Operations

```c
// cache_manager.h

typedef struct {
    char* cache_dir;         // Usually ".cyxmake"
    bool compress;           // Compress cache with gzip
    int ttl_days;            // Time-to-live (0 = infinite)
} CacheConfig;

// Cache operations
bool cache_exists(const char* project_root);
ProjectContext* cache_load(const char* project_root, CacheConfig* config);
bool cache_save(ProjectContext* ctx, CacheConfig* config);
bool cache_is_valid(const char* project_root, CacheConfig* config);
bool cache_clear(const char* project_root);
char* cache_export_json(ProjectContext* ctx);
char* cache_export_yaml(ProjectContext* ctx);
```

### Cache Serialization

```c
// cache_manager.c

#include "cJSON.h"

char* cache_export_json(ProjectContext* ctx) {
    cJSON* root = cJSON_CreateObject();

    // Metadata
    cJSON_AddStringToObject(root, "version", ctx->cache_version);
    cJSON_AddNumberToObject(root, "created_at", ctx->created_at);
    cJSON_AddNumberToObject(root, "updated_at", ctx->updated_at);

    // Project info
    cJSON* project = cJSON_CreateObject();
    cJSON_AddStringToObject(project, "name", ctx->name);
    cJSON_AddStringToObject(project, "root", ctx->root_path);
    cJSON_AddStringToObject(project, "type", ctx->type);
    cJSON_AddStringToObject(project, "primary_language",
                            language_to_string(ctx->primary_language));
    cJSON_AddItemToObject(root, "project", project);

    // Language stats
    cJSON* languages = cJSON_CreateObject();
    for (size_t i = 0; i < ctx->language_count; i++) {
        cJSON* lang_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(lang_obj, "files", ctx->language_stats[i]->file_count);
        cJSON_AddNumberToObject(lang_obj, "lines", ctx->language_stats[i]->line_count);
        cJSON_AddNumberToObject(lang_obj, "percentage", ctx->language_stats[i]->percentage);
        cJSON_AddItemToObject(languages,
                              language_to_string(ctx->language_stats[i]->language),
                              lang_obj);
    }
    cJSON_AddItemToObject(root, "languages", languages);

    // Build system
    cJSON* build_system = cJSON_CreateObject();
    cJSON_AddStringToObject(build_system, "type",
                            build_system_to_string(ctx->build_system.type));
    cJSON* config_files = cJSON_CreateArray();
    for (size_t i = 0; i < ctx->build_system.config_file_count; i++) {
        cJSON_AddItemToArray(config_files,
                            cJSON_CreateString(ctx->build_system.config_files[i]));
    }
    cJSON_AddItemToObject(build_system, "config_files", config_files);
    cJSON_AddItemToObject(root, "build_system", build_system);

    // Dependencies
    cJSON* dependencies = cJSON_CreateArray();
    for (size_t i = 0; i < ctx->dependency_count; i++) {
        cJSON* dep = cJSON_CreateObject();
        cJSON_AddStringToObject(dep, "name", ctx->dependencies[i]->name);
        cJSON_AddStringToObject(dep, "version_spec", ctx->dependencies[i]->version_spec);
        cJSON_AddBoolToObject(dep, "installed", ctx->dependencies[i]->is_installed);
        if (ctx->dependencies[i]->installed_version) {
            cJSON_AddStringToObject(dep, "installed_version",
                                   ctx->dependencies[i]->installed_version);
        }
        cJSON_AddStringToObject(dep, "source", ctx->dependencies[i]->source);
        cJSON_AddItemToArray(dependencies, dep);
    }
    cJSON_AddItemToObject(root, "dependencies", dependencies);

    // Content hash
    cJSON_AddStringToObject(root, "content_hash", ctx->content_hash);
    cJSON_AddNumberToObject(root, "confidence", ctx->confidence);

    // Convert to string
    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    return json_str;
}

bool cache_save(ProjectContext* ctx, CacheConfig* config) {
    // Create cache directory
    char cache_dir[4096];
    snprintf(cache_dir, sizeof(cache_dir), "%s/%s", ctx->root_path, config->cache_dir);
    mkdir_recursive(cache_dir);

    // Generate JSON
    char* json = cache_export_json(ctx);

    // Write to file
    char cache_path[4096];
    snprintf(cache_path, sizeof(cache_path), "%s/cache.json", cache_dir);

    FILE* f = fopen(cache_path, "w");
    if (!f) {
        free(json);
        return false;
    }

    fputs(json, f);
    fclose(f);
    free(json);

    log_info("Cache saved to %s", cache_path);
    return true;
}
```

---

## Implementation: Change Detector

### Change Detection Strategy

```c
// change_detector.h

typedef enum {
    CHANGE_NONE,
    CHANGE_SOURCE_FILES,     // .c, .cpp, .py files modified
    CHANGE_CONFIG_FILES,     // CMakeLists.txt, package.json modified
    CHANGE_DEPENDENCIES,     // requirements.txt, package-lock.json modified
    CHANGE_BUILD_SYSTEM,     // Build system type changed
    CHANGE_MAJOR             // Major structural change (many files)
} ChangeType;

typedef struct {
    ChangeType type;
    char** modified_files;
    size_t modified_count;
    char** added_files;
    size_t added_count;
    char** deleted_files;
    size_t deleted_count;
    bool cache_invalid;
} ChangeReport;

ChangeReport* detect_changes(ProjectContext* cached_ctx, const char* project_root);
bool should_invalidate_cache(ChangeReport* report);
ProjectContext* update_cache_incremental(ProjectContext* cached_ctx,
                                         ChangeReport* report,
                                         ToolRegistry* tools);
```

### Implementation

```c
// change_detector.c

ChangeReport* detect_changes(ProjectContext* cached_ctx, const char* project_root) {
    ChangeReport* report = calloc(1, sizeof(ChangeReport));
    report->type = CHANGE_NONE;

    // Calculate current content hash
    char* current_hash = calculate_directory_hash(project_root);

    // Quick check: if hashes match, no changes
    if (strcmp(current_hash, cached_ctx->content_hash) == 0) {
        free(current_hash);
        return report;  // No changes
    }

    free(current_hash);

    // Detailed analysis: identify what changed
    ArrayList* modified = arraylist_create();
    ArrayList* added = arraylist_create();
    ArrayList* deleted = arraylist_create();

    // Compare current files with cached list
    char** current_files = list_all_files(project_root);
    size_t current_count = count_files(current_files);

    // Build hash map of cached files for O(1) lookup
    HashMap* cached_files = hashmap_create();
    for (size_t i = 0; i < cached_ctx->source_file_count; i++) {
        hashmap_put(cached_files, cached_ctx->source_files[i]->path,
                    (void*)(intptr_t)cached_ctx->source_files[i]->last_modified);
    }

    // Check for modifications and additions
    for (size_t i = 0; i < current_count; i++) {
        void* cached_mtime = hashmap_get(cached_files, current_files[i]);

        if (!cached_mtime) {
            // New file
            arraylist_add(added, strdup(current_files[i]));
        } else {
            // Check if modified
            time_t current_mtime = get_file_mtime(current_files[i]);
            if (current_mtime != (time_t)(intptr_t)cached_mtime) {
                arraylist_add(modified, strdup(current_files[i]));
            }
        }
    }

    // Check for deletions
    for (size_t i = 0; i < cached_ctx->source_file_count; i++) {
        bool found = false;
        for (size_t j = 0; j < current_count; j++) {
            if (strcmp(cached_ctx->source_files[i]->path, current_files[j]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            arraylist_add(deleted, strdup(cached_ctx->source_files[i]->path));
        }
    }

    // Categorize change type
    bool config_changed = false;
    bool deps_changed = false;

    for (size_t i = 0; i < modified->size; i++) {
        char* file = arraylist_get(modified, i);
        if (is_config_file(file, cached_ctx->build_system.type)) {
            config_changed = true;
        }
        if (is_dependency_file(file)) {
            deps_changed = true;
        }
    }

    if (config_changed) report->type = CHANGE_CONFIG_FILES;
    else if (deps_changed) report->type = CHANGE_DEPENDENCIES;
    else if (modified->size > 10 || added->size > 10 || deleted->size > 10) {
        report->type = CHANGE_MAJOR;
    } else if (modified->size > 0 || added->size > 0 || deleted->size > 0) {
        report->type = CHANGE_SOURCE_FILES;
    }

    // Convert ArrayLists to arrays
    report->modified_files = arraylist_to_array(modified, &report->modified_count);
    report->added_files = arraylist_to_array(added, &report->added_count);
    report->deleted_files = arraylist_to_array(deleted, &report->deleted_count);

    // Determine if cache should be invalidated
    report->cache_invalid = should_invalidate_cache(report);

    // Cleanup
    arraylist_destroy(modified);
    arraylist_destroy(added);
    arraylist_destroy(deleted);
    hashmap_destroy(cached_files);
    free_file_list(current_files);

    return report;
}

bool should_invalidate_cache(ChangeReport* report) {
    return (report->type == CHANGE_CONFIG_FILES ||
            report->type == CHANGE_DEPENDENCIES ||
            report->type == CHANGE_BUILD_SYSTEM ||
            report->type == CHANGE_MAJOR);
}
```

---

## Query Engine

### Semantic Queries

```c
// query_engine.h

// Query interface for LLM or tools to ask about project
typedef struct {
    char* query;             // Natural language query
    cJSON* result;           // Structured result
} QueryResult;

QueryResult* project_query(ProjectContext* ctx, const char* query);

// Predefined query types
cJSON* query_dependencies(ProjectContext* ctx);
cJSON* query_build_targets(ProjectContext* ctx);
cJSON* query_source_files(ProjectContext* ctx, const char* pattern);
cJSON* query_build_steps(ProjectContext* ctx);
cJSON* query_missing_dependencies(ProjectContext* ctx);
```

### Example Queries

```c
// Example: "What dependencies are missing?"
cJSON* query_missing_dependencies(ProjectContext* ctx) {
    cJSON* result = cJSON_CreateArray();

    for (size_t i = 0; i < ctx->dependency_count; i++) {
        if (!ctx->dependencies[i]->is_installed) {
            cJSON* dep = cJSON_CreateObject();
            cJSON_AddStringToObject(dep, "name", ctx->dependencies[i]->name);
            cJSON_AddStringToObject(dep, "version", ctx->dependencies[i]->version_spec);
            cJSON_AddStringToObject(dep, "source", ctx->dependencies[i]->source);
            cJSON_AddItemToArray(result, dep);
        }
    }

    return result;
}

// Example: "What are the build steps?"
cJSON* query_build_steps(ProjectContext* ctx) {
    cJSON* result = cJSON_CreateArray();

    if (ctx->readme.has_build_instructions) {
        for (size_t i = 0; i < ctx->readme.step_count; i++) {
            cJSON* step = cJSON_CreateObject();
            cJSON_AddNumberToObject(step, "number", ctx->readme.extracted_steps[i]->step_number);
            cJSON_AddStringToObject(step, "description", ctx->readme.extracted_steps[i]->description);
            cJSON_AddStringToObject(step, "command", ctx->readme.extracted_steps[i]->command);
            cJSON_AddItemToArray(result, step);
        }
    }

    return result;
}
```

---

# Part 2: Error Recovery System

## Purpose

The Error Recovery System is the "autonomous intelligence" that enables CyxMake to diagnose build failures and automatically apply fixes without human intervention.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│             Error Recovery System                        │
├─────────────────────────────────────────────────────────┤
│                                                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │    Error     │  │  Diagnosis   │  │   Solution   │  │
│  │   Capture    │  │    Engine    │  │  Generator   │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│                                                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   Pattern    │  │     Fix      │  │  Verification│  │
│  │   Database   │  │   Executor   │  │    Engine    │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│                                                           │
└─────────────────────────────────────────────────────────┘
```

---

## Error Categorization

```c
// error_recovery.h

typedef enum {
    ERR_MISSING_DEPENDENCY,        // Package/library not found
    ERR_COMPILER_ERROR,            // Syntax errors, type errors
    ERR_LINKER_ERROR,              // Undefined references, missing libs
    ERR_PERMISSION_DENIED,         // File/directory access denied
    ERR_ENV_MISCONFIGURATION,      // Wrong PATH, missing env vars
    ERR_NETWORK_FAILURE,           // Download timeout, DNS failure
    ERR_DISK_SPACE,                // Out of disk space
    ERR_VERSION_MISMATCH,          // Tool version incompatibility
    ERR_MISSING_TOOL,              // Required command not found
    ERR_SYNTAX_ERROR,              // Config file syntax error
    ERR_UNKNOWN                    // Unrecognized error
} ErrorCategory;

typedef struct {
    ErrorCategory category;
    char* raw_message;             // Full stderr output
    char* parsed_summary;          // Human-readable summary
    char** key_details;            // Extracted key information
    size_t detail_count;
    float confidence;              // 0.0-1.0 confidence in diagnosis
    int occurrence_count;          // How many times seen
    time_t first_seen;
    time_t last_seen;
} ErrorDiagnosis;

typedef struct {
    char* description;             // Human-readable action
    char** commands;               // Shell commands to execute
    size_t command_count;
    char** file_modifications;     // JSON patches for files
    size_t modification_count;
    int priority;                  // Higher = try first
    float success_probability;     // ML/heuristic-based probability
    bool requires_sudo;            // Needs elevated permissions
    bool is_destructive;           // Could break things
} RecoveryAction;
```

---

## Error Pattern Database

### Pattern Matching

```c
// error_patterns.h

typedef struct {
    char* regex_pattern;           // Regex to match error
    ErrorCategory category;
    char* description;
    float confidence_boost;        // Increase confidence if matched
} ErrorPattern;

// Example patterns
static ErrorPattern ERROR_PATTERNS[] = {
    {
        .regex_pattern = "fatal error: (.+\\.h): No such file or directory",
        .category = ERR_MISSING_DEPENDENCY,
        .description = "Missing header file",
        .confidence_boost = 0.9
    },
    {
        .regex_pattern = "undefined reference to `(.+)'",
        .category = ERR_LINKER_ERROR,
        .description = "Undefined symbol during linking",
        .confidence_boost = 0.95
    },
    {
        .regex_pattern = "Could not find a version that satisfies the requirement (.+)",
        .category = ERR_MISSING_DEPENDENCY,
        .description = "Python package not found or version unavailable",
        .confidence_boost = 0.95
    },
    {
        .regex_pattern = "bash: (.+): command not found",
        .category = ERR_MISSING_TOOL,
        .description = "Required command-line tool not installed",
        .confidence_boost = 1.0
    },
    {
        .regex_pattern = "Permission denied",
        .category = ERR_PERMISSION_DENIED,
        .description = "Insufficient permissions",
        .confidence_boost = 0.85
    },
    // ... hundreds more patterns
};
```

### Pattern Matching Implementation

```c
// error_patterns.c

#include <regex.h>

ErrorDiagnosis* diagnose_with_patterns(const char* error_output) {
    ErrorDiagnosis* diagnosis = calloc(1, sizeof(ErrorDiagnosis));
    diagnosis->raw_message = strdup(error_output);
    diagnosis->confidence = 0.0;
    diagnosis->category = ERR_UNKNOWN;

    // Try each pattern
    for (size_t i = 0; i < sizeof(ERROR_PATTERNS) / sizeof(ErrorPattern); i++) {
        regex_t regex;
        regmatch_t matches[10];

        if (regcomp(&regex, ERROR_PATTERNS[i].regex_pattern, REG_EXTENDED) != 0) {
            continue;
        }

        if (regexec(&regex, error_output, 10, matches, 0) == 0) {
            // Pattern matched!
            diagnosis->category = ERROR_PATTERNS[i].category;
            diagnosis->parsed_summary = strdup(ERROR_PATTERNS[i].description);
            diagnosis->confidence = ERROR_PATTERNS[i].confidence_boost;

            // Extract captured groups
            for (int j = 1; j < 10 && matches[j].rm_so != -1; j++) {
                int start = matches[j].rm_so;
                int end = matches[j].rm_eo;
                char* captured = strndup(error_output + start, end - start);
                arraylist_add(diagnosis->key_details, captured);
            }

            regfree(&regex);
            break;  // Use first matching pattern
        }

        regfree(&regex);
    }

    return diagnosis;
}
```

---

## Diagnosis Engine

### Multi-Stage Diagnosis

```c
// diagnosis_engine.h

ErrorDiagnosis* error_diagnose(const char* error_output,
                               ProjectContext* ctx,
                               LLMContext* llm);

// Diagnosis stages
ErrorDiagnosis* stage1_pattern_matching(const char* error_output);
ErrorDiagnosis* stage2_heuristic_analysis(ErrorDiagnosis* initial, ProjectContext* ctx);
ErrorDiagnosis* stage3_llm_reasoning(ErrorDiagnosis* initial,
                                      const char* error_output,
                                      ProjectContext* ctx,
                                      LLMContext* llm);
```

### Implementation

```c
// diagnosis_engine.c

ErrorDiagnosis* error_diagnose(const char* error_output,
                               ProjectContext* ctx,
                               LLMContext* llm) {
    // Stage 1: Pattern matching (fast, high precision)
    ErrorDiagnosis* diagnosis = stage1_pattern_matching(error_output);

    // If high confidence, return immediately
    if (diagnosis->confidence >= 0.9) {
        log_info("High confidence diagnosis: %s", diagnosis->parsed_summary);
        return diagnosis;
    }

    // Stage 2: Heuristic analysis with project context
    diagnosis = stage2_heuristic_analysis(diagnosis, ctx);

    if (diagnosis->confidence >= 0.7) {
        log_info("Medium confidence diagnosis: %s", diagnosis->parsed_summary);
        return diagnosis;
    }

    // Stage 3: LLM-powered deep reasoning (expensive, use sparingly)
    log_info("Low confidence, escalating to LLM...");
    diagnosis = stage3_llm_reasoning(diagnosis, error_output, ctx, llm);

    return diagnosis;
}

ErrorDiagnosis* stage2_heuristic_analysis(ErrorDiagnosis* initial, ProjectContext* ctx) {
    // Use project context to refine diagnosis

    if (initial->category == ERR_MISSING_DEPENDENCY) {
        // Check if it's a known dependency
        for (size_t i = 0; i < initial->detail_count; i++) {
            char* dep_name = initial->key_details[i];

            // Is this in our dependency list?
            for (size_t j = 0; j < ctx->dependency_count; j++) {
                if (strstr(ctx->dependencies[j]->name, dep_name) != NULL) {
                    if (!ctx->dependencies[j]->is_installed) {
                        initial->confidence = 0.95;
                        snprintf(initial->parsed_summary, 256,
                                "Missing dependency: %s (declared in project but not installed)",
                                ctx->dependencies[j]->name);
                        return initial;
                    }
                }
            }
        }
    }

    if (initial->category == ERR_MISSING_TOOL) {
        // Check what build system we're using
        if (ctx->build_system.type == BUILD_CMAKE &&
            strstr(initial->raw_message, "cmake") != NULL) {
            initial->confidence = 1.0;
            initial->parsed_summary = strdup("CMake not installed (required by build system)");
        }
    }

    return initial;
}
```

---

## Solution Generator

### Solution Generation

```c
// solution_generator.h

RecoveryAction** generate_solutions(ErrorDiagnosis* diagnosis,
                                     ProjectContext* ctx,
                                     LLMContext* llm,
                                     size_t* action_count);

// Category-specific generators
RecoveryAction** solve_missing_dependency(ErrorDiagnosis* diag, ProjectContext* ctx);
RecoveryAction** solve_missing_tool(ErrorDiagnosis* diag, ProjectContext* ctx);
RecoveryAction** solve_permission_error(ErrorDiagnosis* diag, ProjectContext* ctx);
RecoveryAction** solve_linker_error(ErrorDiagnosis* diag, ProjectContext* ctx);
```

### Example: Missing Dependency Solutions

```c
RecoveryAction** solve_missing_dependency(ErrorDiagnosis* diag, ProjectContext* ctx) {
    ArrayList* actions = arraylist_create();

    char* dep_name = diag->key_details[0];  // Extracted dependency name

    // Generate actions based on build system
    switch (ctx->build_system.type) {
        case BUILD_SETUPTOOLS:  // Python
            {
                RecoveryAction* action = calloc(1, sizeof(RecoveryAction));
                action->description = malloc(256);
                snprintf(action->description, 256, "Install Python package: %s", dep_name);

                action->commands = malloc(sizeof(char*) * 2);
                action->commands[0] = malloc(256);
                snprintf(action->commands[0], 256, "pip install %s", dep_name);
                action->commands[1] = NULL;
                action->command_count = 1;

                action->priority = 10;
                action->success_probability = 0.9;
                action->requires_sudo = false;
                action->is_destructive = false;

                arraylist_add(actions, action);
            }
            break;

        case BUILD_CMAKE:  // C/C++
            {
                // Try vcpkg first (Windows/cross-platform)
                RecoveryAction* vcpkg_action = calloc(1, sizeof(RecoveryAction));
                vcpkg_action->description = malloc(256);
                snprintf(vcpkg_action->description, 256,
                        "Install via vcpkg: %s", dep_name);
                vcpkg_action->commands = malloc(sizeof(char*) * 2);
                vcpkg_action->commands[0] = malloc(256);
                snprintf(vcpkg_action->commands[0], 256, "vcpkg install %s", dep_name);
                vcpkg_action->command_count = 1;
                vcpkg_action->priority = 10;
                vcpkg_action->success_probability = 0.8;
                arraylist_add(actions, vcpkg_action);

                // Try system package manager (Linux/macOS)
                if (is_linux() || is_macos()) {
                    RecoveryAction* pkg_action = calloc(1, sizeof(RecoveryAction));
                    pkg_action->description = malloc(256);

                    if (is_linux()) {
                        // Detect distro
                        if (is_debian_based()) {
                            snprintf(pkg_action->description, 256,
                                    "Install via apt: lib%s-dev", dep_name);
                            pkg_action->commands = malloc(sizeof(char*) * 2);
                            pkg_action->commands[0] = malloc(256);
                            snprintf(pkg_action->commands[0], 256,
                                    "sudo apt install lib%s-dev", dep_name);
                        } else if (is_fedora_based()) {
                            snprintf(pkg_action->description, 256,
                                    "Install via dnf: %s-devel", dep_name);
                            pkg_action->commands = malloc(sizeof(char*) * 2);
                            pkg_action->commands[0] = malloc(256);
                            snprintf(pkg_action->commands[0], 256,
                                    "sudo dnf install %s-devel", dep_name);
                        }
                    } else if (is_macos()) {
                        snprintf(pkg_action->description, 256,
                                "Install via brew: %s", dep_name);
                        pkg_action->commands = malloc(sizeof(char*) * 2);
                        pkg_action->commands[0] = malloc(256);
                        snprintf(pkg_action->commands[0], 256,
                                "brew install %s", dep_name);
                    }

                    pkg_action->command_count = 1;
                    pkg_action->priority = 9;
                    pkg_action->success_probability = 0.7;
                    pkg_action->requires_sudo = is_linux();
                    arraylist_add(actions, pkg_action);
                }
            }
            break;

        // ... other build systems
    }

    size_t count;
    RecoveryAction** action_array = arraylist_to_array(actions, &count);
    arraylist_destroy(actions);

    return action_array;
}
```

---

## Fix Executor

### Execution with Verification

```c
// fix_executor.h

typedef struct {
    bool success;
    int exit_code;
    char* output;
    char* error_message;
    double execution_time_ms;
} ExecutionResult;

ExecutionResult* execute_recovery_action(RecoveryAction* action,
                                          ProjectContext* ctx,
                                          bool dry_run);

bool verify_fix(ErrorDiagnosis* original_error,
                RecoveryAction* applied_action,
                ProjectContext* ctx);
```

### Retry Loop

```c
// fix_executor.c

bool attempt_recovery(ErrorDiagnosis* diagnosis,
                      ProjectContext* ctx,
                      LLMContext* llm,
                      int max_attempts) {
    size_t action_count;
    RecoveryAction** actions = generate_solutions(diagnosis, ctx, llm, &action_count);

    // Sort by priority
    qsort(actions, action_count, sizeof(RecoveryAction*), compare_priority);

    for (int attempt = 0; attempt < max_attempts; attempt++) {
        if (attempt >= action_count) {
            log_error("Exhausted all recovery actions");
            return false;
        }

        RecoveryAction* action = actions[attempt];

        log_info("Attempting fix [%d/%d]: %s", attempt + 1, max_attempts,
                action->description);

        // Ask user permission if required
        if (action->requires_sudo || action->is_destructive) {
            if (!prompt_user_permission(action)) {
                log_info("User declined action, trying next...");
                continue;
            }
        }

        // Execute action
        ExecutionResult* result = execute_recovery_action(action, ctx, false);

        if (result->success) {
            log_info("Fix applied successfully");

            // Verify fix
            if (verify_fix(diagnosis, action, ctx)) {
                log_info("Fix verified, recovery successful!");
                free_execution_result(result);
                return true;
            } else {
                log_warn("Fix applied but verification failed, trying next...");
            }
        } else {
            log_warn("Fix failed: %s", result->error_message);
        }

        free_execution_result(result);
    }

    return false;
}
```

---

# Part 3: LLM Integration Layer

## Purpose

The LLM Integration Layer provides a unified interface for interacting with AI models (both local and cloud), handling prompting, response parsing, and intelligent model selection.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│             LLM Integration Layer                        │
├─────────────────────────────────────────────────────────┤
│                                                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │    Local     │  │    Cloud     │  │   Model      │  │
│  │  SLM Engine  │  │  API Client  │  │  Selector    │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│                                                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   Prompt     │  │   Response   │  │   Token      │  │
│  │  Templates   │  │    Parser    │  │  Manager     │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│                                                           │
└─────────────────────────────────────────────────────────┘
```

---

## LLM Context Structure

```c
// llm_interface.h

typedef enum {
    LLM_BACKEND_LOCAL,
    LLM_BACKEND_CLOUD_ANTHROPIC,
    LLM_BACKEND_CLOUD_OPENAI,
    LLM_BACKEND_AUTO              // Automatic selection
} LLMBackend;

typedef struct {
    // Local model (llama.cpp)
    void* llama_model;            // llama_model*
    void* llama_context;          // llama_context*
    char* model_path;
    int context_size;
    int n_threads;

    // Cloud API
    char* api_key;
    char* api_endpoint;
    LLMBackend cloud_provider;

    // Configuration
    float confidence_threshold;   // When to escalate to cloud
    int max_tokens;
    float temperature;
    bool use_local;
    bool use_cloud;

    // Usage tracking
    size_t local_queries;
    size_t cloud_queries;
    size_t total_tokens;
    double total_cost_usd;
} LLMContext;

typedef struct {
    char* prompt;
    LLMBackend backend_preference;
    float temperature;
    int max_tokens;
    char** stop_sequences;
    bool json_mode;              // Request JSON output
} LLMRequest;

typedef struct {
    char* response;
    LLMBackend used_backend;
    float confidence;
    int tokens_used;
    double latency_ms;
    bool is_json;
    cJSON* parsed_json;          // If json_mode enabled
} LLMResponse;

// API
LLMContext* llm_init(const char* config_path);
LLMResponse* llm_query(LLMContext* ctx, LLMRequest* request);
void llm_free_response(LLMResponse* response);
void llm_shutdown(LLMContext* ctx);
```

---

## Local SLM Integration (llama.cpp)

```c
// llm_local.c

#include "llama.h"

LLMContext* llm_init_local(const char* model_path, int n_threads) {
    LLMContext* ctx = calloc(1, sizeof(LLMContext));
    ctx->model_path = strdup(model_path);
    ctx->n_threads = n_threads;
    ctx->context_size = 4096;

    // Initialize llama.cpp
    llama_backend_init(false);

    // Load model
    llama_model_params model_params = llama_model_default_params();
    ctx->llama_model = llama_load_model_from_file(model_path, model_params);

    if (!ctx->llama_model) {
        log_error("Failed to load model: %s", model_path);
        free(ctx);
        return NULL;
    }

    // Create context
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = ctx->context_size;
    ctx_params.n_threads = n_threads;
    ctx->llama_context = llama_new_context_with_model(ctx->llama_model, ctx_params);

    if (!ctx->llama_context) {
        log_error("Failed to create llama context");
        llama_free_model(ctx->llama_model);
        free(ctx);
        return NULL;
    }

    log_info("Local LLM initialized: %s", model_path);
    return ctx;
}

LLMResponse* llm_query_local(LLMContext* ctx, LLMRequest* request) {
    clock_t start = clock();
    LLMResponse* response = calloc(1, sizeof(LLMResponse));
    response->used_backend = LLM_BACKEND_LOCAL;

    // Tokenize prompt
    int* tokens = malloc(sizeof(int) * ctx->context_size);
    int n_tokens = llama_tokenize(ctx->llama_context, request->prompt,
                                   ctx->context_size, tokens, true, true);

    if (n_tokens < 0) {
        log_error("Failed to tokenize prompt");
        response->response = strdup("[ERROR: Tokenization failed]");
        free(tokens);
        return response;
    }

    // Batch evaluation
    llama_batch batch = llama_batch_init(n_tokens, 0, 1);
    for (int i = 0; i < n_tokens; i++) {
        llama_batch_add(&batch, tokens[i], i, {0}, false);
    }
    batch.logits[batch.n_tokens - 1] = true;  // Last token generates logits

    if (llama_decode(ctx->llama_context, batch) != 0) {
        log_error("Failed to decode batch");
        response->response = strdup("[ERROR: Decode failed]");
        llama_batch_free(batch);
        free(tokens);
        return response;
    }

    // Generate response
    char* output_buffer = malloc(request->max_tokens * 10);  // Rough estimate
    size_t output_pos = 0;
    int n_generated = 0;

    while (n_generated < request->max_tokens) {
        // Sample next token
        llama_token new_token = llama_sample_token_greedy(ctx->llama_context, NULL);

        // Check for EOS or stop sequences
        if (new_token == llama_token_eos(ctx->llama_model)) {
            break;
        }

        // Convert token to text
        char piece[32];
        int n_chars = llama_token_to_piece(ctx->llama_model, new_token,
                                            piece, sizeof(piece));
        if (n_chars > 0) {
            memcpy(output_buffer + output_pos, piece, n_chars);
            output_pos += n_chars;
        }

        // Prepare next iteration
        llama_batch_clear(&batch);
        llama_batch_add(&batch, new_token, n_tokens + n_generated, {0}, true);

        if (llama_decode(ctx->llama_context, batch) != 0) {
            break;
        }

        n_generated++;
    }

    output_buffer[output_pos] = '\0';
    response->response = output_buffer;
    response->tokens_used = n_tokens + n_generated;

    clock_t end = clock();
    response->latency_ms = ((double)(end - start)) / CLOCKS_PER_SEC * 1000;

    // Estimate confidence (heuristic: longer, coherent response = higher confidence)
    response->confidence = estimate_response_confidence(response->response);

    llama_batch_free(batch);
    free(tokens);

    ctx->local_queries++;
    ctx->total_tokens += response->tokens_used;

    return response;
}
```

---

## Cloud API Integration

```c
// llm_cloud.c

#include <curl/curl.h>
#include "cJSON.h"

// Callback for libcurl
struct MemoryStruct {
    char* memory;
    size_t size;
};

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct* mem = (struct MemoryStruct*)userp;

    char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

LLMResponse* llm_query_cloud_anthropic(LLMContext* ctx, LLMRequest* request) {
    clock_t start = clock();
    CURL* curl = curl_easy_init();
    LLMResponse* response = calloc(1, sizeof(LLMResponse));
    response->used_backend = LLM_BACKEND_CLOUD_ANTHROPIC;

    if (!curl) {
        response->response = strdup("[ERROR: Failed to initialize CURL]");
        return response;
    }

    // Prepare request body
    cJSON* body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "model", "claude-3-5-sonnet-20241022");
    cJSON_AddNumberToObject(body, "max_tokens", request->max_tokens);
    cJSON_AddNumberToObject(body, "temperature", request->temperature);

    cJSON* messages = cJSON_CreateArray();
    cJSON* message = cJSON_CreateObject();
    cJSON_AddStringToObject(message, "role", "user");
    cJSON_AddStringToObject(message, "content", request->prompt);
    cJSON_AddItemToArray(messages, message);
    cJSON_AddItemToObject(body, "messages", messages);

    char* body_str = cJSON_PrintUnformatted(body);

    // Set up HTTP headers
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "x-api-key: %s", ctx->api_key);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");

    // Set up response buffer
    struct MemoryStruct chunk = {malloc(1), 0};

    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.anthropic.com/v1/messages");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        response->response = malloc(256);
        snprintf(response->response, 256, "[ERROR: %s]", curl_easy_strerror(res));
    } else {
        // Parse response
        cJSON* resp_json = cJSON_Parse(chunk.memory);
        if (resp_json) {
            cJSON* content = cJSON_GetObjectItem(resp_json, "content");
            if (content && cJSON_IsArray(content) && cJSON_GetArraySize(content) > 0) {
                cJSON* first_content = cJSON_GetArrayItem(content, 0);
                cJSON* text = cJSON_GetObjectItem(first_content, "text");
                if (text && cJSON_IsString(text)) {
                    response->response = strdup(text->valuestring);
                }
            }

            cJSON* usage = cJSON_GetObjectItem(resp_json, "usage");
            if (usage) {
                cJSON* input_tokens = cJSON_GetObjectItem(usage, "input_tokens");
                cJSON* output_tokens = cJSON_GetObjectItem(usage, "output_tokens");
                response->tokens_used = (input_tokens ? input_tokens->valueint : 0) +
                                        (output_tokens ? output_tokens->valueint : 0);
            }

            cJSON_Delete(resp_json);
        }
    }

    clock_t end = clock();
    response->latency_ms = ((double)(end - start)) / CLOCKS_PER_SEC * 1000;
    response->confidence = 0.95;  // Cloud models generally high confidence

    // Cleanup
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    free(chunk.memory);
    free(body_str);
    cJSON_Delete(body);

    ctx->cloud_queries++;
    ctx->total_tokens += response->tokens_used;
    ctx->total_cost_usd += (response->tokens_used / 1000000.0) * 3.0;  // Rough estimate

    return response;
}
```

---

## Intelligent Model Selection

```c
// llm_selector.c

LLMResponse* llm_query(LLMContext* ctx, LLMRequest* request) {
    LLMBackend selected_backend;

    // Explicit backend preference
    if (request->backend_preference != LLM_BACKEND_AUTO) {
        selected_backend = request->backend_preference;
    } else {
        // Automatic selection heuristics
        size_t prompt_length = strlen(request->prompt);

        if (ctx->use_local && prompt_length < 2000) {
            // Short prompts: use local model
            selected_backend = LLM_BACKEND_LOCAL;
        } else if (ctx->use_local && ctx->local_queries < ctx->cloud_queries * 10) {
            // Prefer local if we haven't used it much
            selected_backend = LLM_BACKEND_LOCAL;
        } else if (ctx->use_cloud && prompt_length > 4000) {
            // Very long prompts: use cloud (larger context)
            selected_backend = LLM_BACKEND_CLOUD_ANTHROPIC;
        } else if (ctx->use_local) {
            // Default to local if available
            selected_backend = LLM_BACKEND_LOCAL;
        } else {
            // Fallback to cloud
            selected_backend = LLM_BACKEND_CLOUD_ANTHROPIC;
        }
    }

    LLMResponse* response = NULL;

    // Execute query
    switch (selected_backend) {
        case LLM_BACKEND_LOCAL:
            log_debug("Using local LLM");
            response = llm_query_local(ctx, request);

            // If local failed or low confidence, retry with cloud
            if (ctx->use_cloud &&
                (response->confidence < ctx->confidence_threshold ||
                 strstr(response->response, "[ERROR") != NULL)) {
                log_info("Local LLM low confidence (%.2f), escalating to cloud",
                        response->confidence);
                llm_free_response(response);
                response = llm_query_cloud_anthropic(ctx, request);
            }
            break;

        case LLM_BACKEND_CLOUD_ANTHROPIC:
            log_debug("Using cloud LLM (Anthropic)");
            response = llm_query_cloud_anthropic(ctx, request);
            break;

        case LLM_BACKEND_CLOUD_OPENAI:
            log_debug("Using cloud LLM (OpenAI)");
            response = llm_query_cloud_openai(ctx, request);
            break;

        default:
            log_error("Invalid backend selected");
            response = calloc(1, sizeof(LLMResponse));
            response->response = strdup("[ERROR: Invalid backend]");
            break;
    }

    return response;
}
```

---

## Prompt Templates

### Tool Selection Prompt

```c
const char* PROMPT_TOOL_SELECTION =
"You are CyxMake, an AI build assistant. Based on the project information and "
"current goal, select the most appropriate tool to use.\n\n"
"Project Information:\n"
"%s\n\n"
"Goal: %s\n\n"
"Available Tools:\n"
"%s\n\n"
"Respond with JSON:\n"
"{\n"
"  \"tool\": \"tool_name\",\n"
"  \"reason\": \"why this tool\",\n"
"  \"parameters\": {\"param1\": \"value1\"}\n"
"}\n";
```

### Error Diagnosis Prompt

```c
const char* PROMPT_ERROR_DIAGNOSIS =
"You are CyxMake error diagnosis system. Analyze the following build error "
"and provide a diagnosis.\n\n"
"Project Type: %s\n"
"Build System: %s\n"
"Error Output:\n"
"```\n%s\n```\n\n"
"Respond with JSON:\n"
"{\n"
"  \"category\": \"error category\",\n"
"  \"summary\": \"brief description\",\n"
"  \"root_cause\": \"likely root cause\",\n"
"  \"confidence\": 0.0-1.0\n"
"}\n";
```

---

## Conclusion

These three core components work together:

1. **Project Context Manager** - Knows what the project is
2. **Error Recovery System** - Fixes problems autonomously
3. **LLM Integration Layer** - Provides AI reasoning when needed

Together, they enable CyxMake's autonomous build capabilities.

---

**Document Version**: 1.0
**Last Updated**: 2025-11-24
**Status**: Ready for Implementation
