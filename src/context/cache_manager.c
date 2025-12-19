/**
 * @file cache_manager.c
 * @brief Project context cache management implementation
 */

#include "cyxmake/cache_manager.h"
#include "cyxmake/project_context.h"
#include "cyxmake/compat.h"
#include "cyxmake/logger.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
    #include <direct.h>
    #define mkdir(path, mode) _mkdir(path)
#else
    #include <sys/types.h>
#endif

#define CACHE_DIR ".cyxmake"
#define CACHE_FILE "cache.json"
#define CACHE_VERSION "1.0"

/* Get cache directory path */
static char* get_cache_dir(const char* project_root) {
    size_t len = strlen(project_root) + strlen(CACHE_DIR) + 2;
    char* path = malloc(len);
    if (!path) return NULL;

    snprintf(path, len, "%s%c%s", project_root, DIR_SEP, CACHE_DIR);
    return path;
}

/* Get cache file path */
char* cache_get_path(const char* project_root) {
    size_t len = strlen(project_root) + strlen(CACHE_DIR) + strlen(CACHE_FILE) + 3;
    char* path = malloc(len);
    if (!path) return NULL;

    snprintf(path, len, "%s%c%s%c%s", project_root, DIR_SEP, CACHE_DIR, DIR_SEP, CACHE_FILE);
    return path;
}

/* Ensure cache directory exists */
static bool ensure_cache_dir(const char* project_root) {
    char* cache_dir = get_cache_dir(project_root);
    if (!cache_dir) return false;

    /* Check if directory exists */
    struct stat st;
    if (stat(cache_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
        free(cache_dir);
        return true;
    }

    /* Create directory */
    int result = mkdir(cache_dir, 0755);
    free(cache_dir);
    return result == 0;
}

/* Serialize ProjectContext to JSON */
static cJSON* project_context_to_json(const ProjectContext* ctx) {
    if (!ctx) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    /* Metadata */
    cJSON_AddStringToObject(root, "cache_version", ctx->cache_version ? ctx->cache_version : CACHE_VERSION);
    cJSON_AddStringToObject(root, "name", ctx->name ? ctx->name : "unknown");
    cJSON_AddStringToObject(root, "root_path", ctx->root_path ? ctx->root_path : "");
    cJSON_AddStringToObject(root, "type", ctx->type ? ctx->type : "unknown");

    /* Timestamps */
    cJSON_AddNumberToObject(root, "created_at", (double)ctx->created_at);
    cJSON_AddNumberToObject(root, "updated_at", (double)ctx->updated_at);

    /* Language */
    cJSON_AddStringToObject(root, "primary_language", language_to_string(ctx->primary_language));

    /* Build system */
    cJSON* build_system = cJSON_CreateObject();
    cJSON_AddStringToObject(build_system, "type", build_system_to_string(ctx->build_system.type));

    /* Config files array */
    if (ctx->build_system.config_files && ctx->build_system.config_file_count > 0) {
        cJSON* config_files = cJSON_CreateArray();
        for (size_t i = 0; i < ctx->build_system.config_file_count; i++) {
            if (ctx->build_system.config_files[i]) {
                cJSON_AddItemToArray(config_files, cJSON_CreateString(ctx->build_system.config_files[i]));
            }
        }
        cJSON_AddItemToObject(build_system, "config_files", config_files);
    }

    cJSON_AddItemToObject(root, "build_system", build_system);

    /* Source files (if available) */
    if (ctx->source_files && ctx->source_file_count > 0) {
        cJSON* files = cJSON_CreateArray();
        for (size_t i = 0; i < ctx->source_file_count; i++) {
            if (ctx->source_files[i]) {
                cJSON* file = cJSON_CreateObject();
                cJSON_AddStringToObject(file, "path", ctx->source_files[i]->path ? ctx->source_files[i]->path : "");
                cJSON_AddStringToObject(file, "language", language_to_string(ctx->source_files[i]->language));
                cJSON_AddNumberToObject(file, "line_count", (double)ctx->source_files[i]->line_count);
                cJSON_AddNumberToObject(file, "last_modified", (double)ctx->source_files[i]->last_modified);
                cJSON_AddBoolToObject(file, "is_generated", ctx->source_files[i]->is_generated);
                cJSON_AddItemToArray(files, file);
            }
        }
        cJSON_AddItemToObject(root, "source_files", files);
    }

    /* Language statistics (if available) */
    if (ctx->language_stats && ctx->language_count > 0) {
        cJSON* stats = cJSON_CreateArray();
        for (size_t i = 0; i < ctx->language_count; i++) {
            if (ctx->language_stats[i]) {
                cJSON* stat = cJSON_CreateObject();
                cJSON_AddStringToObject(stat, "language", language_to_string(ctx->language_stats[i]->language));
                cJSON_AddNumberToObject(stat, "file_count", (double)ctx->language_stats[i]->file_count);
                cJSON_AddNumberToObject(stat, "line_count", (double)ctx->language_stats[i]->line_count);
                cJSON_AddNumberToObject(stat, "percentage", ctx->language_stats[i]->percentage);
                cJSON_AddItemToArray(stats, stat);
            }
        }
        cJSON_AddItemToObject(root, "language_stats", stats);
    }

    /* Dependencies (if available) */
    if (ctx->dependencies && ctx->dependency_count > 0) {
        cJSON* deps = cJSON_CreateArray();
        for (size_t i = 0; i < ctx->dependency_count; i++) {
            if (ctx->dependencies[i]) {
                cJSON* dep = cJSON_CreateObject();
                cJSON_AddStringToObject(dep, "name", ctx->dependencies[i]->name ? ctx->dependencies[i]->name : "");
                cJSON_AddStringToObject(dep, "version_spec", ctx->dependencies[i]->version_spec ? ctx->dependencies[i]->version_spec : "");
                cJSON_AddStringToObject(dep, "installed_version", ctx->dependencies[i]->installed_version ? ctx->dependencies[i]->installed_version : "");
                cJSON_AddBoolToObject(dep, "is_installed", ctx->dependencies[i]->is_installed);
                cJSON_AddBoolToObject(dep, "is_dev_dependency", ctx->dependencies[i]->is_dev_dependency);
                cJSON_AddStringToObject(dep, "source", ctx->dependencies[i]->source ? ctx->dependencies[i]->source : "");
                cJSON_AddItemToArray(deps, dep);
            }
        }
        cJSON_AddItemToObject(root, "dependencies", deps);
    }

    /* Content hash and confidence */
    cJSON_AddStringToObject(root, "content_hash", ctx->content_hash ? ctx->content_hash : "");
    cJSON_AddNumberToObject(root, "confidence", ctx->confidence);

    return root;
}

/* Save project context to cache */
bool cache_save(const ProjectContext* ctx, const char* project_root) {
    if (!ctx || !project_root) return false;

    /* Ensure cache directory exists */
    if (!ensure_cache_dir(project_root)) {
        log_error("Failed to create cache directory");
        return false;
    }

    /* Serialize to JSON */
    cJSON* json = project_context_to_json(ctx);
    if (!json) {
        log_error("Failed to serialize project context");
        return false;
    }

    /* Convert to string */
    char* json_str = cJSON_Print(json);
    cJSON_Delete(json);

    if (!json_str) {
        log_error("Failed to convert JSON to string");
        return false;
    }

    /* Get cache file path */
    char* cache_path = cache_get_path(project_root);
    if (!cache_path) {
        free(json_str);
        return false;
    }

    /* Write to file */
    FILE* fp = fopen(cache_path, "w");
    if (!fp) {
        log_error("Failed to open cache file for writing: %s", cache_path);
        free(cache_path);
        free(json_str);
        return false;
    }

    size_t written = fwrite(json_str, 1, strlen(json_str), fp);
    fclose(fp);

    bool success = (written == strlen(json_str));

    if (success) {
        log_info("Cache saved to %s", cache_path);
    } else {
        log_error("Failed to write cache file");
    }

    free(cache_path);
    free(json_str);
    return success;
}

/* Deserialize JSON to ProjectContext */
static ProjectContext* json_to_project_context(cJSON* root) {
    if (!root) return NULL;

    ProjectContext* ctx = calloc(1, sizeof(ProjectContext));
    if (!ctx) return NULL;

    /* Metadata */
    cJSON* item;

    item = cJSON_GetObjectItem(root, "cache_version");
    if (item) ctx->cache_version = strdup(cJSON_GetStringValue(item));

    item = cJSON_GetObjectItem(root, "name");
    if (item) ctx->name = strdup(cJSON_GetStringValue(item));

    item = cJSON_GetObjectItem(root, "root_path");
    if (item) ctx->root_path = strdup(cJSON_GetStringValue(item));

    item = cJSON_GetObjectItem(root, "type");
    if (item) ctx->type = strdup(cJSON_GetStringValue(item));

    /* Timestamps */
    item = cJSON_GetObjectItem(root, "created_at");
    if (item) ctx->created_at = (time_t)cJSON_GetNumberValue(item);

    item = cJSON_GetObjectItem(root, "updated_at");
    if (item) ctx->updated_at = (time_t)cJSON_GetNumberValue(item);

    /* Language */
    item = cJSON_GetObjectItem(root, "primary_language");
    if (item) ctx->primary_language = language_from_string(cJSON_GetStringValue(item));

    /* Build system */
    cJSON* build_system = cJSON_GetObjectItem(root, "build_system");
    if (build_system) {
        item = cJSON_GetObjectItem(build_system, "type");
        if (item) ctx->build_system.type = build_system_from_string(cJSON_GetStringValue(item));

        cJSON* config_files = cJSON_GetObjectItem(build_system, "config_files");
        if (config_files && cJSON_IsArray(config_files)) {
            ctx->build_system.config_file_count = cJSON_GetArraySize(config_files);
            if (ctx->build_system.config_file_count > 0) {
                ctx->build_system.config_files = calloc(ctx->build_system.config_file_count, sizeof(char*));

                cJSON* config_file;
                int index = 0;
                cJSON_ArrayForEach(config_file, config_files) {
                    ctx->build_system.config_files[index++] = strdup(cJSON_GetStringValue(config_file));
                }
            }
        }
    }

    /* Source files */
    cJSON* files = cJSON_GetObjectItem(root, "source_files");
    if (files && cJSON_IsArray(files)) {
        ctx->source_file_count = cJSON_GetArraySize(files);
        if (ctx->source_file_count > 0) {
            ctx->source_files = calloc(ctx->source_file_count, sizeof(SourceFile*));

            cJSON* file;
            int index = 0;
            cJSON_ArrayForEach(file, files) {
                SourceFile* sf = calloc(1, sizeof(SourceFile));

                item = cJSON_GetObjectItem(file, "path");
                if (item) sf->path = strdup(cJSON_GetStringValue(item));

                item = cJSON_GetObjectItem(file, "language");
                if (item) sf->language = language_from_string(cJSON_GetStringValue(item));

                item = cJSON_GetObjectItem(file, "line_count");
                if (item) sf->line_count = (size_t)cJSON_GetNumberValue(item);

                item = cJSON_GetObjectItem(file, "last_modified");
                if (item) sf->last_modified = (time_t)cJSON_GetNumberValue(item);

                item = cJSON_GetObjectItem(file, "is_generated");
                if (item) sf->is_generated = cJSON_IsTrue(item);

                ctx->source_files[index++] = sf;
            }
        }
    }

    /* Language statistics */
    cJSON* stats = cJSON_GetObjectItem(root, "language_stats");
    if (stats && cJSON_IsArray(stats)) {
        ctx->language_count = cJSON_GetArraySize(stats);
        if (ctx->language_count > 0) {
            ctx->language_stats = calloc(ctx->language_count, sizeof(LanguageStats*));

            cJSON* stat;
            int index = 0;
            cJSON_ArrayForEach(stat, stats) {
                LanguageStats* ls = calloc(1, sizeof(LanguageStats));

                item = cJSON_GetObjectItem(stat, "language");
                if (item) ls->language = language_from_string(cJSON_GetStringValue(item));

                item = cJSON_GetObjectItem(stat, "file_count");
                if (item) ls->file_count = (size_t)cJSON_GetNumberValue(item);

                item = cJSON_GetObjectItem(stat, "line_count");
                if (item) ls->line_count = (size_t)cJSON_GetNumberValue(item);

                item = cJSON_GetObjectItem(stat, "percentage");
                if (item) ls->percentage = (float)cJSON_GetNumberValue(item);

                ctx->language_stats[index++] = ls;
            }
        }
    }

    /* Dependencies */
    cJSON* deps = cJSON_GetObjectItem(root, "dependencies");
    if (deps && cJSON_IsArray(deps)) {
        ctx->dependency_count = cJSON_GetArraySize(deps);
        if (ctx->dependency_count > 0) {
            ctx->dependencies = calloc(ctx->dependency_count, sizeof(Dependency*));

            cJSON* dep;
            int index = 0;
            cJSON_ArrayForEach(dep, deps) {
                Dependency* d = calloc(1, sizeof(Dependency));

                item = cJSON_GetObjectItem(dep, "name");
                if (item) d->name = strdup(cJSON_GetStringValue(item));

                item = cJSON_GetObjectItem(dep, "version_spec");
                if (item) d->version_spec = strdup(cJSON_GetStringValue(item));

                item = cJSON_GetObjectItem(dep, "installed_version");
                if (item) d->installed_version = strdup(cJSON_GetStringValue(item));

                item = cJSON_GetObjectItem(dep, "is_installed");
                if (item) d->is_installed = cJSON_IsTrue(item);

                item = cJSON_GetObjectItem(dep, "is_dev_dependency");
                if (item) d->is_dev_dependency = cJSON_IsTrue(item);

                item = cJSON_GetObjectItem(dep, "source");
                if (item) d->source = strdup(cJSON_GetStringValue(item));

                ctx->dependencies[index++] = d;
            }
        }
    }

    /* Content hash and confidence */
    item = cJSON_GetObjectItem(root, "content_hash");
    if (item) ctx->content_hash = strdup(cJSON_GetStringValue(item));

    item = cJSON_GetObjectItem(root, "confidence");
    if (item) ctx->confidence = (float)cJSON_GetNumberValue(item);

    return ctx;
}

/* Load project context from cache */
ProjectContext* cache_load(const char* project_root) {
    if (!project_root) return NULL;

    /* Get cache file path */
    char* cache_path = cache_get_path(project_root);
    if (!cache_path) return NULL;

    /* Check if file exists */
    FILE* fp = fopen(cache_path, "r");
    if (!fp) {
        free(cache_path);
        return NULL;
    }

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* Read file */
    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(fp);
        free(cache_path);
        return NULL;
    }

    size_t read = fread(buffer, 1, size, fp);
    fclose(fp);
    buffer[read] = '\0';

    /* Parse JSON */
    cJSON* json = cJSON_Parse(buffer);
    free(buffer);

    if (!json) {
        log_error("Failed to parse cache file: %s", cache_path);
        free(cache_path);
        return NULL;
    }

    /* Deserialize */
    ProjectContext* ctx = json_to_project_context(json);
    cJSON_Delete(json);

    if (ctx) {
        log_info("Cache loaded from %s", cache_path);
    }

    free(cache_path);
    return ctx;
}

/* Check if cache exists */
bool cache_exists(const char* project_root) {
    if (!project_root) return false;

    char* cache_path = cache_get_path(project_root);
    if (!cache_path) return false;

    struct stat st;
    bool exists = (stat(cache_path, &st) == 0 && S_ISREG(st.st_mode));

    free(cache_path);
    return exists;
}

/* Delete cache file */
bool cache_delete(const char* project_root) {
    if (!project_root) return false;

    char* cache_path = cache_get_path(project_root);
    if (!cache_path) return false;

    int result = remove(cache_path);
    free(cache_path);

    return result == 0;
}

/* Check if cache is stale */
bool cache_is_stale(const ProjectContext* ctx, const char* project_root) {
    if (!ctx || !project_root) return true;

    /* For now, consider cache valid for 24 hours */
    time_t now = time(NULL);
    double age_seconds = difftime(now, ctx->updated_at);

    /* Cache older than 24 hours is stale */
    return age_seconds > (24 * 60 * 60);
}

/* Invalidate cache after successful fix */
bool cache_invalidate(const char* project_root) {
    if (!project_root) return false;

    /* Load existing cache */
    ProjectContext* ctx = cache_load(project_root);
    if (!ctx) {
        /* No cache to invalidate - that's fine */
        log_debug("No cache to invalidate for: %s", project_root);
        return true;
    }

    /* Set updated_at to 0 to force staleness check to fail */
    ctx->updated_at = 0;

    bool result = cache_save(ctx, project_root);
    project_context_free(ctx);

    if (result) {
        log_info("Cache invalidated for: %s", project_root);
    } else {
        log_error("Failed to invalidate cache for: %s", project_root);
    }

    return result;
}

/* Update dependency installation status in cache */
bool cache_mark_dependency_installed(const char* project_root, const char* dep_name) {
    if (!project_root || !dep_name) return false;

    ProjectContext* ctx = cache_load(project_root);
    if (!ctx) {
        log_warning("No cache found to update dependency: %s", dep_name);
        return false;
    }

    /* Find and update dependency status */
    bool found = false;
    for (size_t i = 0; i < ctx->dependency_count; i++) {
        if (ctx->dependencies[i] && ctx->dependencies[i]->name &&
            strcmp(ctx->dependencies[i]->name, dep_name) == 0) {
            ctx->dependencies[i]->is_installed = true;
            log_debug("Marked dependency as installed: %s", dep_name);
            found = true;
            break;
        }
    }

    if (!found) {
        log_debug("Dependency not found in cache: %s (invalidating cache)", dep_name);
        /* Dependency not tracked - invalidate entire cache */
        ctx->updated_at = 0;
    }

    /* Update timestamp */
    ctx->updated_at = time(NULL);

    bool result = cache_save(ctx, project_root);
    project_context_free(ctx);

    return result;
}
