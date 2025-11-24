/**
 * @file project_analyzer.c
 * @brief Project analysis implementation
 */

#include "cyxmake/project_context.h"
#include "cyxmake/compat.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

/* Cross-platform directory scanning */
#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #define PATH_SEPARATOR '\\'
    #define PATH_SEPARATOR_STR "\\"
#else
    #include <dirent.h>
    #define PATH_SEPARATOR '/'
    #define PATH_SEPARATOR_STR "/"
#endif

/* Maximum path length */
#define MAX_PATH_LEN 4096

/* Helper: Check if file exists */
static bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* Helper: Check if directory exists */
static bool dir_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* Helper: Get file extension */
static const char* get_extension(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
}

/* Helper: Check if path should be ignored */
static bool should_ignore(const char* name) {
    /* Common directories to ignore */
    const char* ignored_dirs[] = {
        ".", "..", ".git", ".svn", ".hg",
        "node_modules", "venv", "env", "__pycache__",
        "build", "dist", "target", ".cache",
        NULL
    };

    for (int i = 0; ignored_dirs[i] != NULL; i++) {
        if (strcmp(name, ignored_dirs[i]) == 0) {
            return true;
        }
    }

    return false;
}

/* Map extension to language */
static Language extension_to_language(const char* ext) {
    if (strcmp(ext, "c") == 0) return LANG_C;
    if (strcmp(ext, "h") == 0) return LANG_C;

    if (strcmp(ext, "cpp") == 0 || strcmp(ext, "cc") == 0 ||
        strcmp(ext, "cxx") == 0 || strcmp(ext, "hpp") == 0 ||
        strcmp(ext, "hxx") == 0) return LANG_CPP;

    if (strcmp(ext, "py") == 0) return LANG_PYTHON;

    if (strcmp(ext, "js") == 0 || strcmp(ext, "mjs") == 0) return LANG_JAVASCRIPT;
    if (strcmp(ext, "ts") == 0) return LANG_TYPESCRIPT;
    if (strcmp(ext, "tsx") == 0) return LANG_TYPESCRIPT;
    if (strcmp(ext, "jsx") == 0) return LANG_JAVASCRIPT;

    if (strcmp(ext, "rs") == 0) return LANG_RUST;
    if (strcmp(ext, "go") == 0) return LANG_GO;
    if (strcmp(ext, "java") == 0) return LANG_JAVA;
    if (strcmp(ext, "cs") == 0) return LANG_CSHARP;
    if (strcmp(ext, "rb") == 0) return LANG_RUBY;
    if (strcmp(ext, "php") == 0) return LANG_PHP;

    if (strcmp(ext, "sh") == 0 || strcmp(ext, "bash") == 0) return LANG_SHELL;

    return LANG_UNKNOWN;
}

/* Cross-platform directory scanner helper */
static void scan_directory_for_languages(const char* dir_path, size_t* lang_counts, int depth) {
    if (depth > 2) return;  /* Limit recursion depth */

#ifdef _WIN32
    /* Windows implementation using FindFirstFile/FindNextFile */
    WIN32_FIND_DATAA find_data;
    char search_path[MAX_PATH_LEN];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (should_ignore(find_data.cFileName)) continue;

        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            /* Directory - recurse */
            scan_directory_for_languages(full_path, lang_counts, depth + 1);
        } else {
            /* File - count by extension */
            const char* ext = get_extension(find_data.cFileName);
            Language lang = extension_to_language(ext);
            if (lang != LANG_UNKNOWN) {
                lang_counts[lang]++;
            }
        }
    } while (FindNextFileA(hFind, &find_data) != 0);

    FindClose(hFind);

#else
    /* POSIX implementation using opendir/readdir */
    DIR* dir = opendir(dir_path);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (should_ignore(entry->d_name)) continue;

        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                /* Directory - recurse */
                scan_directory_for_languages(full_path, lang_counts, depth + 1);
            } else if (S_ISREG(st.st_mode)) {
                /* File - count by extension */
                const char* ext = get_extension(entry->d_name);
                Language lang = extension_to_language(ext);
                if (lang != LANG_UNKNOWN) {
                    lang_counts[lang]++;
                }
            }
        }
    }

    closedir(dir);
#endif
}

/* Detect primary language from file counts */
Language detect_primary_language(const char* root_path) {
    /* Count files by language */
    size_t lang_counts[20] = {0};  /* Max languages */

    /* Scan directory tree */
    scan_directory_for_languages(root_path, lang_counts, 0);

    /* Find language with most files */
    Language primary = LANG_UNKNOWN;
    size_t max_count = 0;

    for (int i = 1; i < 20; i++) {  /* Skip LANG_UNKNOWN (0) */
        if (lang_counts[i] > max_count) {
            max_count = lang_counts[i];
            primary = (Language)i;
        }
    }

    return primary;
}

/* Detect build system from config files */
BuildSystem detect_build_system(const char* root_path) {
    char path[MAX_PATH_LEN];

    /* Check for CMake */
    snprintf(path, sizeof(path), "%s/CMakeLists.txt", root_path);
    if (file_exists(path)) return BUILD_CMAKE;

    /* Check for Cargo (Rust) */
    snprintf(path, sizeof(path), "%s/Cargo.toml", root_path);
    if (file_exists(path)) return BUILD_CARGO;

    /* Check for npm (JavaScript/TypeScript) */
    snprintf(path, sizeof(path), "%s/package.json", root_path);
    if (file_exists(path)) return BUILD_NPM;

    /* Check for Python build systems */
    snprintf(path, sizeof(path), "%s/pyproject.toml", root_path);
    if (file_exists(path)) {
        /* Could be Poetry or modern setuptools */
        return BUILD_POETRY;  /* Assume Poetry for now */
    }

    snprintf(path, sizeof(path), "%s/setup.py", root_path);
    if (file_exists(path)) return BUILD_SETUPTOOLS;

    /* Check for Make */
    snprintf(path, sizeof(path), "%s/Makefile", root_path);
    if (file_exists(path)) return BUILD_MAKE;

    snprintf(path, sizeof(path), "%s/makefile", root_path);
    if (file_exists(path)) return BUILD_MAKE;

    /* Check for Gradle (Java/Kotlin) */
    snprintf(path, sizeof(path), "%s/build.gradle", root_path);
    if (file_exists(path)) return BUILD_GRADLE;

    snprintf(path, sizeof(path), "%s/build.gradle.kts", root_path);
    if (file_exists(path)) return BUILD_GRADLE;

    /* Check for Maven (Java) */
    snprintf(path, sizeof(path), "%s/pom.xml", root_path);
    if (file_exists(path)) return BUILD_MAVEN;

    /* Check for Meson */
    snprintf(path, sizeof(path), "%s/meson.build", root_path);
    if (file_exists(path)) return BUILD_MESON;

    /* Check for Bazel */
    snprintf(path, sizeof(path), "%s/BUILD", root_path);
    if (file_exists(path)) return BUILD_BAZEL;

    snprintf(path, sizeof(path), "%s/WORKSPACE", root_path);
    if (file_exists(path)) return BUILD_BAZEL;

    return BUILD_UNKNOWN;
}

/* Dynamic array for collecting source files */
typedef struct {
    SourceFile** files;
    size_t count;
    size_t capacity;
} FileList;

/* Initialize file list */
static FileList* file_list_create(void) {
    FileList* list = malloc(sizeof(FileList));
    if (!list) return NULL;

    list->capacity = 64;
    list->count = 0;
    list->files = malloc(list->capacity * sizeof(SourceFile*));
    if (!list->files) {
        free(list);
        return NULL;
    }

    return list;
}

/* Add file to list */
static bool file_list_add(FileList* list, SourceFile* file) {
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        SourceFile** new_files = realloc(list->files, new_capacity * sizeof(SourceFile*));
        if (!new_files) return false;

        list->files = new_files;
        list->capacity = new_capacity;
    }

    list->files[list->count++] = file;
    return true;
}

/* Recursively scan directory for source files */
static void scan_directory_recursive(const char* dir_path, FileList* list, int depth) {
    if (depth > 10) return;  /* Limit recursion depth */

#ifdef _WIN32
    /* Windows implementation */
    WIN32_FIND_DATAA find_data;
    char search_path[MAX_PATH_LEN];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (should_ignore(find_data.cFileName)) continue;

        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            /* Recurse into subdirectory */
            scan_directory_recursive(full_path, list, depth + 1);
        } else {
            /* Check if it's a source file */
            const char* ext = get_extension(find_data.cFileName);
            Language lang = extension_to_language(ext);

            if (lang != LANG_UNKNOWN) {
                /* Create source file entry */
                SourceFile* sf = calloc(1, sizeof(SourceFile));
                if (sf) {
                    sf->path = strdup(full_path);
                    sf->language = lang;
                    sf->line_count = 0;  /* TODO: Count lines */
                    sf->is_generated = false;

                    /* Get last modified time */
                    FILETIME ft = find_data.ftLastWriteTime;
                    ULARGE_INTEGER ull;
                    ull.LowPart = ft.dwLowDateTime;
                    ull.HighPart = ft.dwHighDateTime;
                    sf->last_modified = (time_t)((ull.QuadPart / 10000000ULL) - 11644473600ULL);

                    file_list_add(list, sf);
                }
            }
        }
    } while (FindNextFileA(hFind, &find_data) != 0);

    FindClose(hFind);

#else
    /* POSIX implementation */
    DIR* dir = opendir(dir_path);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (should_ignore(entry->d_name)) continue;

        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                /* Recurse into subdirectory */
                scan_directory_recursive(full_path, list, depth + 1);
            } else if (S_ISREG(st.st_mode)) {
                /* Check if it's a source file */
                const char* ext = get_extension(entry->d_name);
                Language lang = extension_to_language(ext);

                if (lang != LANG_UNKNOWN) {
                    /* Create source file entry */
                    SourceFile* sf = calloc(1, sizeof(SourceFile));
                    if (sf) {
                        sf->path = strdup(full_path);
                        sf->language = lang;
                        sf->line_count = 0;  /* TODO: Count lines */
                        sf->last_modified = st.st_mtime;
                        sf->is_generated = false;

                        file_list_add(list, sf);
                    }
                }
            }
        }
    }

    closedir(dir);
#endif
}

/* Scan source files */
SourceFile** scan_source_files(const char* root_path,
                                Language primary_lang,
                                size_t* file_count) {
    (void)primary_lang;  /* Unused for now */

    FileList* list = file_list_create();
    if (!list) {
        *file_count = 0;
        return NULL;
    }

    /* Recursively scan directory tree */
    scan_directory_recursive(root_path, list, 0);

    *file_count = list->count;

    /* Transfer ownership of files array */
    SourceFile** files = list->files;
    free(list);

    return files;
}

/* Calculate language statistics */
LanguageStats** calculate_language_stats(SourceFile** files,
                                          size_t file_count,
                                          size_t* stats_count) {
    if (!files || file_count == 0) {
        *stats_count = 0;
        return NULL;
    }

    /* Count files by language */
    size_t lang_file_counts[20] = {0};  /* Max 20 languages */
    size_t lang_line_counts[20] = {0};

    for (size_t i = 0; i < file_count; i++) {
        if (files[i] && files[i]->language != LANG_UNKNOWN) {
            lang_file_counts[files[i]->language]++;
            lang_line_counts[files[i]->language] += files[i]->line_count;
        }
    }

    /* Count how many languages we have */
    size_t num_langs = 0;
    for (int i = 1; i < 20; i++) {
        if (lang_file_counts[i] > 0) {
            num_langs++;
        }
    }

    if (num_langs == 0) {
        *stats_count = 0;
        return NULL;
    }

    /* Allocate stats array */
    LanguageStats** stats = calloc(num_langs, sizeof(LanguageStats*));
    if (!stats) {
        *stats_count = 0;
        return NULL;
    }

    /* Fill in statistics */
    size_t index = 0;
    for (int i = 1; i < 20; i++) {
        if (lang_file_counts[i] > 0) {
            LanguageStats* ls = calloc(1, sizeof(LanguageStats));
            if (ls) {
                ls->language = (Language)i;
                ls->file_count = lang_file_counts[i];
                ls->line_count = lang_line_counts[i];
                ls->percentage = (float)lang_file_counts[i] / (float)file_count * 100.0f;

                stats[index++] = ls;
            }
        }
    }

    *stats_count = num_langs;
    return stats;
}

/* Main project analysis function */
ProjectContext* project_analyze(const char* root_path, AnalysisOptions* options) {
    if (!root_path) return NULL;

    /* Use default options if none provided */
    AnalysisOptions* opts = options;
    bool free_opts = false;
    if (!opts) {
        opts = analysis_options_default();
        free_opts = true;
    }

    log_info("Analyzing project at: %s", root_path);

    /* Create project context */
    ProjectContext* ctx = calloc(1, sizeof(ProjectContext));
    if (!ctx) {
        if (free_opts) analysis_options_free(opts);
        return NULL;
    }

    /* Set metadata */
    ctx->root_path = strdup(root_path);
    ctx->created_at = time(NULL);
    ctx->updated_at = time(NULL);
    ctx->cache_version = strdup("1.0");

    /* Extract project name from path */
    const char* last_slash = strrchr(root_path, '/');
    if (!last_slash) last_slash = strrchr(root_path, '\\');
    ctx->name = strdup(last_slash ? last_slash + 1 : root_path);

    /* Step 1: Detect primary language */
    log_step(1, 5, "Detecting primary language...");
    ctx->primary_language = detect_primary_language(root_path);
    log_with_prefix("       ", "Primary language: %s", language_to_string(ctx->primary_language));

    /* Step 2: Detect build system */
    log_step(2, 5, "Detecting build system...");
    ctx->build_system.type = detect_build_system(root_path);
    log_with_prefix("       ", "Build system: %s", build_system_to_string(ctx->build_system.type));

    /* Step 3: Scan source files */
    log_step(3, 5, "Scanning source files...");
    ctx->source_files = scan_source_files(root_path, ctx->primary_language,
                                           &ctx->source_file_count);
    log_with_prefix("       ", "Source files: %zu", ctx->source_file_count);

    /* Step 4: Calculate language statistics */
    log_step(4, 5, "Calculating language statistics...");
    ctx->language_stats = calculate_language_stats(ctx->source_files,
                                                     ctx->source_file_count,
                                                     &ctx->language_count);
    log_with_prefix("       ", "Languages detected: %zu", ctx->language_count);

    /* Step 5: Calculate content hash */
    log_step(5, 5, "Calculating content hash...");
    ctx->content_hash = calculate_content_hash(ctx);

    /* Set confidence (stub) */
    ctx->confidence = 0.85f;

    /* Set project type based on build system */
    switch (ctx->build_system.type) {
        case BUILD_CMAKE:
        case BUILD_MAKE:
            ctx->type = strdup("application");
            break;
        case BUILD_NPM:
            ctx->type = strdup("web_application");
            break;
        case BUILD_CARGO:
            ctx->type = strdup("application");
            break;
        case BUILD_SETUPTOOLS:
        case BUILD_POETRY:
            ctx->type = strdup("package");
            break;
        default:
            ctx->type = strdup("unknown");
    }

    if (free_opts) analysis_options_free(opts);

    log_plain("\n");
    log_success("Project analysis complete!");
    log_with_prefix("       ", "Confidence: %.0f%%", ctx->confidence * 100);

    return ctx;
}
