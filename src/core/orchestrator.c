/**
 * @file orchestrator.c
 * @brief Core orchestrator implementation
 */

#include "cyxmake/cyxmake.h"
#include "cyxmake/project_context.h"
#include "cyxmake/cache_manager.h"
#include "cyxmake/build_executor.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct Orchestrator {
    Config* config;
    LLMContext* llm;
    ToolRegistry* tool_registry;
    ProjectContext* current_project;
};

const char* cyxmake_version(void) {
    return CYXMAKE_VERSION;
}

Orchestrator* cyxmake_init(const char* config_path) {
    Orchestrator* orch = calloc(1, sizeof(Orchestrator));
    if (!orch) {
        return NULL;
    }

    // TODO: Load configuration
    // TODO: Initialize LLM
    // TODO: Initialize tool registry
    // TODO: Discover tools

    printf("CyxMake initialized (stub implementation)\n");
    printf("Phase 0: Basic structure established\n");
    printf("TODO: Implement full initialization in Phase 1\n");

    return orch;
}

void cyxmake_shutdown(Orchestrator* orch) {
    if (!orch) {
        return;
    }

    /* Free project context */
    if (orch->current_project) {
        project_context_free(orch->current_project);
    }

    // TODO: Cleanup LLM context
    // TODO: Cleanup tool registry
    // TODO: Cleanup config

    free(orch);
}

CyxMakeError cyxmake_analyze_project(Orchestrator* orch, const char* project_path) {
    if (!orch || !project_path) {
        return CYXMAKE_ERROR_INVALID_ARG;
    }

    /* Free existing project context if any */
    if (orch->current_project) {
        project_context_free(orch->current_project);
        orch->current_project = NULL;
    }

    /* Analyze the project */
    orch->current_project = project_analyze(project_path, NULL);
    if (!orch->current_project) {
        fprintf(stderr, "Error: Failed to analyze project\n");
        return CYXMAKE_ERROR_INTERNAL;
    }

    /* Save cache to .cyxmake/cache.json */
    if (!cache_save(orch->current_project, project_path)) {
        fprintf(stderr, "Warning: Failed to save cache\n");
        /* Don't fail the operation if cache save fails */
    }

    return CYXMAKE_SUCCESS;
}

CyxMakeError cyxmake_build(Orchestrator* orch, const char* project_path) {
    if (!orch || !project_path) {
        return CYXMAKE_ERROR_INVALID_ARG;
    }

    log_info("Building project at: %s", project_path);

    /* Load or analyze project if not already loaded */
    if (!orch->current_project) {
        /* Try to load from cache first */
        orch->current_project = cache_load(project_path);

        if (!orch->current_project) {
            /* No cache, analyze project */
            log_info("No cache found, analyzing project...");
            orch->current_project = project_analyze(project_path, NULL);

            if (!orch->current_project) {
                log_error("Failed to analyze project");
                return CYXMAKE_ERROR_INTERNAL;
            }

            /* Save cache for next time */
            cache_save(orch->current_project, project_path);
        }
    }

    /* Check if build system is detected */
    if (orch->current_project->build_system.type == BUILD_UNKNOWN) {
        log_error("Unknown build system - cannot build");
        return CYXMAKE_ERROR_BUILD;
    }

    /* Execute build */
    log_plain("\n");
    BuildResult* result = build_execute(orch->current_project, NULL);

    if (!result) {
        log_error("Failed to execute build");
        return CYXMAKE_ERROR_BUILD;
    }

    /* Print result */
    log_plain("\n");
    build_result_print(result);

    /* Check if successful */
    CyxMakeError err = result->success ? CYXMAKE_SUCCESS : CYXMAKE_ERROR_BUILD;

    build_result_free(result);
    return err;
}

CyxMakeError cyxmake_create_project(Orchestrator* orch,
                                     const char* description,
                                     const char* output_path) {
    if (!orch || !description || !output_path) {
        return CYXMAKE_ERROR_INVALID_ARG;
    }

    printf("Creating project from description: %s\n", description);
    printf("Output path: %s\n", output_path);
    printf("TODO: Implement project generation\n");
    printf("- Parse natural language description\n");
    printf("- Generate project structure\n");
    printf("- Create build files\n");
    printf("- Generate README\n");

    return CYXMAKE_SUCCESS;
}

void cyxmake_set_log_level(LogLevel level) {
    // TODO: Implement logging
    printf("Setting log level to: %d\n", level);
}
