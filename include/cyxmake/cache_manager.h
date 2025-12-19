/**
 * @file cache_manager.h
 * @brief Project context cache management
 */

#ifndef CYXMAKE_CACHE_MANAGER_H
#define CYXMAKE_CACHE_MANAGER_H

#include "project_context.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Save project context to cache file
 * Creates .cyxmake directory if it doesn't exist
 *
 * @param ctx Project context to save
 * @param project_root Root directory of the project
 * @return true on success, false on failure
 */
bool cache_save(const ProjectContext* ctx, const char* project_root);

/**
 * Load project context from cache file
 *
 * @param project_root Root directory of the project
 * @return ProjectContext on success, NULL if cache doesn't exist or is invalid
 */
ProjectContext* cache_load(const char* project_root);

/**
 * Check if cache exists and is valid
 *
 * @param project_root Root directory of the project
 * @return true if valid cache exists, false otherwise
 */
bool cache_exists(const char* project_root);

/**
 * Delete cache file
 *
 * @param project_root Root directory of the project
 * @return true on success, false on failure
 */
bool cache_delete(const char* project_root);

/**
 * Get cache file path
 * Caller must free the returned string
 *
 * @param project_root Root directory of the project
 * @return Path to cache file, or NULL on error
 */
char* cache_get_path(const char* project_root);

/**
 * Check if cache is stale (older than project files)
 *
 * @param ctx Project context with cache metadata
 * @param project_root Root directory of the project
 * @return true if cache is stale and needs refresh
 */
bool cache_is_stale(const ProjectContext* ctx, const char* project_root);

/**
 * Invalidate cache after successful fix
 *
 * Marks cache as stale so the next operation will re-analyze
 * the project. Use this after applying fixes that change
 * project structure or dependencies.
 *
 * @param project_root Root directory of the project
 * @return true on success, false on failure
 */
bool cache_invalidate(const char* project_root);

/**
 * Update dependency installation status in cache
 *
 * Updates a specific dependency's is_installed flag without
 * invalidating the entire cache. Use after successfully
 * installing a package.
 *
 * @param project_root Root directory of the project
 * @param dep_name Dependency name that was installed
 * @return true on success, false on failure
 */
bool cache_mark_dependency_installed(const char* project_root, const char* dep_name);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_CACHE_MANAGER_H */
