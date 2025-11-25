/**
 * @file tool_registry.c
 * @brief Tool registry implementation
 */

#include "cyxmake/tool_executor.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #define strdup _strdup
#endif

/* Tool registry structure */
struct ToolRegistry {
    ToolInfo** tools;
    size_t tool_count;
    size_t tool_capacity;
};

/* Create tool registry */
ToolRegistry* tool_registry_create(void) {
    ToolRegistry* registry = calloc(1, sizeof(ToolRegistry));
    if (!registry) {
        log_error("Failed to allocate tool registry");
        return NULL;
    }

    registry->tool_capacity = 32;  /* Initial capacity */
    registry->tools = calloc(registry->tool_capacity, sizeof(ToolInfo*));
    if (!registry->tools) {
        free(registry);
        return NULL;
    }

    registry->tool_count = 0;
    log_debug("Tool registry created");
    return registry;
}

/* Free tool registry */
void tool_registry_free(ToolRegistry* registry) {
    if (!registry) return;

    for (size_t i = 0; i < registry->tool_count; i++) {
        tool_info_free(registry->tools[i]);
    }

    free(registry->tools);
    free(registry);
    log_debug("Tool registry freed");
}

/* Register a tool */
bool tool_registry_add(ToolRegistry* registry, ToolInfo* tool) {
    if (!registry || !tool) return false;

    /* Grow array if needed */
    if (registry->tool_count >= registry->tool_capacity) {
        size_t new_capacity = registry->tool_capacity * 2;
        ToolInfo** new_tools = realloc(registry->tools,
                                       new_capacity * sizeof(ToolInfo*));
        if (!new_tools) {
            log_error("Failed to grow tool registry");
            return false;
        }
        registry->tools = new_tools;
        registry->tool_capacity = new_capacity;
    }

    registry->tools[registry->tool_count++] = tool;
    log_debug("Registered tool: %s (%s)", tool->name, tool->path);
    return true;
}

/* Find tool by name */
const ToolInfo* tool_registry_find(const ToolRegistry* registry, const char* name) {
    if (!registry || !name) return NULL;

    for (size_t i = 0; i < registry->tool_count; i++) {
        if (registry->tools[i]->name &&
            strcmp(registry->tools[i]->name, name) == 0) {
            return registry->tools[i];
        }
    }

    return NULL;
}

/* Find tools by type */
const ToolInfo** tool_registry_find_by_type(const ToolRegistry* registry,
                                             ToolType type,
                                             size_t* count) {
    if (!registry || !count) return NULL;

    /* Count matching tools */
    size_t match_count = 0;
    for (size_t i = 0; i < registry->tool_count; i++) {
        if (registry->tools[i]->type == type) {
            match_count++;
        }
    }

    if (match_count == 0) {
        *count = 0;
        return NULL;
    }

    /* Allocate result array */
    const ToolInfo** results = calloc(match_count, sizeof(ToolInfo*));
    if (!results) {
        *count = 0;
        return NULL;
    }

    /* Populate results */
    size_t idx = 0;
    for (size_t i = 0; i < registry->tool_count; i++) {
        if (registry->tools[i]->type == type) {
            results[idx++] = registry->tools[i];
        }
    }

    *count = match_count;
    return results;
}

/* Get all registered tools */
const ToolInfo** tool_registry_get_all(const ToolRegistry* registry, size_t* count) {
    if (!registry || !count) return NULL;

    if (registry->tool_count == 0) {
        *count = 0;
        return NULL;
    }

    /* Create array of pointers to tools (const cast for return) */
    *count = registry->tool_count;
    return (const ToolInfo**)registry->tools;
}

/* Convert tool type to string */
const char* tool_type_to_string(ToolType type) {
    switch (type) {
        case TOOL_TYPE_PACKAGE_MANAGER: return "Package Manager";
        case TOOL_TYPE_COMPILER:         return "Compiler";
        case TOOL_TYPE_BUILD_SYSTEM:     return "Build System";
        case TOOL_TYPE_VERSION_CONTROL:  return "Version Control";
        case TOOL_TYPE_LINTER:           return "Linter";
        case TOOL_TYPE_FORMATTER:        return "Formatter";
        case TOOL_TYPE_TEST_RUNNER:      return "Test Runner";
        case TOOL_TYPE_DEBUGGER:         return "Debugger";
        case TOOL_TYPE_PROFILER:         return "Profiler";
        default:                         return "Unknown";
    }
}

/* Convert package manager type to string */
const char* package_manager_to_string(PackageManagerType type) {
    switch (type) {
        case PKG_MGR_APT:     return "apt";
        case PKG_MGR_YUM:     return "yum";
        case PKG_MGR_DNF:     return "dnf";
        case PKG_MGR_PACMAN:  return "pacman";
        case PKG_MGR_BREW:    return "brew";
        case PKG_MGR_VCPKG:   return "vcpkg";
        case PKG_MGR_CONAN:   return "conan";
        case PKG_MGR_NPM:     return "npm";
        case PKG_MGR_YARN:    return "yarn";
        case PKG_MGR_PIP:     return "pip";
        case PKG_MGR_CARGO:   return "cargo";
        case PKG_MGR_CHOCO:   return "choco";
        case PKG_MGR_WINGET:  return "winget";
        default:              return "unknown";
    }
}

/* Free tool info */
void tool_info_free(ToolInfo* tool) {
    if (!tool) return;

    free(tool->name);
    free(tool->display_name);
    free(tool->path);
    free(tool->version);

    if (tool->capabilities) {
        for (size_t i = 0; i < tool->capability_count; i++) {
            free(tool->capabilities[i]);
        }
        free(tool->capabilities);
    }

    free(tool);
}
