/**
 * @file orchestrator.c
 * @brief Core orchestrator implementation with AI integration
 */

#include "cyxmake/cyxmake.h"
#include "cyxmake/project_context.h"
#include "cyxmake/cache_manager.h"
#include "cyxmake/build_executor.h"
#include "cyxmake/error_recovery.h"
#include "cyxmake/llm_interface.h"
#include "cyxmake/ai_provider.h"
#include "cyxmake/tool_executor.h"
#include "cyxmake/ai_build_agent.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* AI-enabled orchestrator structure */
struct Orchestrator {
    Config* config;
    LLMContext* llm;
    AIProviderRegistry* ai_registry;
    AIProvider* default_ai;
    ToolRegistry* tool_registry;
    ProjectContext* current_project;
    RecoveryStrategy recovery_strategy;
    bool ai_enabled;
    bool tools_enabled;
};

const char* cyxmake_version(void) {
    return CYXMAKE_VERSION;
}

Orchestrator* cyxmake_init(const char* config_path) {
    Orchestrator* orch = calloc(1, sizeof(Orchestrator));
    if (!orch) {
        return NULL;
    }

    (void)config_path;  /* TODO: Load configuration from file */

    /* Set default recovery strategy */
    orch->recovery_strategy = (RecoveryStrategy){
        .max_retries = 3,
        .retry_delay_ms = 1000,
        .backoff_multiplier = 2.0f,
        .max_delay_ms = 30000,
        .use_ai_analysis = true,
        .auto_apply_fixes = false
    };

    /* Initialize tool registry and discover tools */
    log_info("Discovering available tools...");
    orch->tool_registry = tool_registry_create();
    if (orch->tool_registry) {
        int discovered = tool_discover_all(orch->tool_registry);
        if (discovered > 0) {
            log_info("Discovered %d tools", discovered);
            orch->tools_enabled = true;

            /* Show default package manager */
            const ToolInfo* pkg_mgr = package_get_default_manager(orch->tool_registry);
            if (pkg_mgr) {
                log_debug("Default package manager: %s", pkg_mgr->display_name);
            }
        } else {
            log_warning("No tools discovered");
        }
    }

    /* Initialize AI provider from config */
    log_info("Initializing AI engine...");

    /* Try to load AI providers from config file (cyxmake.toml) */
    orch->ai_registry = ai_registry_create();
    if (orch->ai_registry) {
        int providers_loaded = ai_registry_load_config(orch->ai_registry, config_path);
        if (providers_loaded > 0) {
            log_info("Loaded %d AI provider(s) from config", providers_loaded);

            /* Get the default provider */
            orch->default_ai = ai_registry_get_default(orch->ai_registry);
            if (orch->default_ai) {
                /* Initialize the provider */
                if (ai_provider_init(orch->default_ai)) {
                    if (ai_provider_is_ready(orch->default_ai)) {
                        log_success("AI engine ready (provider: %s)",
                                   orch->default_ai->config.name ? orch->default_ai->config.name : "default");
                        orch->ai_enabled = true;
                    }
                }

                if (!orch->ai_enabled) {
                    log_warning("Default AI provider not ready: %s",
                               ai_provider_error(orch->default_ai) ? ai_provider_error(orch->default_ai) : "unknown error");
                }
            }
        }
    }

    /* Fallback to local llama.cpp if no provider configured */
    if (!orch->ai_enabled) {
        char* model_path = llm_get_default_model_path();

        if (model_path && llm_validate_model_file(model_path)) {
            LLMConfig* llm_config = llm_config_default();
            llm_config->model_path = model_path;
            llm_config->n_ctx = 4096;       /* Context for error analysis */
            llm_config->verbose = false;

            orch->llm = llm_init(llm_config);

            if (orch->llm && llm_is_ready(orch->llm)) {
                log_success("AI engine ready (local llama.cpp)");
                orch->ai_enabled = true;

                /* Get model info */
                LLMModelInfo* info = llm_get_model_info(orch->llm);
                if (info) {
                    log_debug("Model: %s (%s)", info->model_name, info->model_type);
                    llm_model_info_free(info);
                }
            } else {
                log_warning("AI engine failed to initialize - continuing without AI");
                orch->ai_enabled = false;
                orch->recovery_strategy.use_ai_analysis = false;
            }

            llm_config_free(llm_config);
        } else {
            log_info("No AI model found - running in tool-only mode");
            log_info("To enable AI: configure in cyxmake.toml or download model to ~/.cyxmake/models/");
            orch->ai_enabled = false;
            orch->recovery_strategy.use_ai_analysis = false;
        }

        if (model_path) {
            free(model_path);
        }
    }

    log_plain("\n");
    return orch;
}

void cyxmake_shutdown(Orchestrator* orch) {
    if (!orch) {
        return;
    }

    log_debug("Shutting down CyxMake...");

    /* Free project context */
    if (orch->current_project) {
        project_context_free(orch->current_project);
        orch->current_project = NULL;
    }

    /* Shutdown LLM */
    if (orch->llm) {
        log_debug("Shutting down AI engine...");
        llm_shutdown(orch->llm);
        orch->llm = NULL;
    }

    /* Free AI provider registry */
    if (orch->ai_registry) {
        log_debug("Freeing AI provider registry...");
        ai_registry_free(orch->ai_registry);
        orch->ai_registry = NULL;
        orch->default_ai = NULL;  /* Freed by registry */
    }

    /* Free tool registry */
    if (orch->tool_registry) {
        log_debug("Freeing tool registry...");
        tool_registry_free(orch->tool_registry);
        orch->tool_registry = NULL;
    }

    /* TODO: Free config when implemented */

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

    /* Show AI status */
    if (orch->ai_enabled) {
        log_info("AI-powered error recovery: enabled");
    }
    if (orch->tools_enabled) {
        log_info("Smart package installation: enabled");
    }

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

    log_plain("\n");

    /* Use AI-powered recovery if enabled */
    BuildResult* result = NULL;

    if (orch->ai_enabled || orch->tools_enabled) {
        /* Create recovery context with AI and tools */
        RecoveryContext* recovery_ctx = recovery_context_create(&orch->recovery_strategy);

        if (recovery_ctx) {
            /* Attach LLM for AI analysis */
            if (orch->ai_enabled && orch->llm) {
                recovery_set_llm(recovery_ctx, orch->llm);
            }

            /* Attach tool registry for smart package installation */
            if (orch->tools_enabled && orch->tool_registry) {
                recovery_set_tools(recovery_ctx, orch->tool_registry);
            }

            /* Build with automatic retry and recovery */
            log_info("Starting build with recovery enabled (max %d retries)",
                    orch->recovery_strategy.max_retries);

            result = build_with_retry(orch->current_project, NULL, &orch->recovery_strategy);

            /* Get recovery stats */
            int total_attempts, successful_recoveries;
            recovery_get_stats(recovery_ctx, &total_attempts, &successful_recoveries);

            if (total_attempts > 0) {
                log_info("Recovery stats: %d attempt(s), %d successful",
                        total_attempts, successful_recoveries);
            }

            recovery_context_free(recovery_ctx);
        } else {
            /* Fallback to simple build */
            result = build_execute(orch->current_project, NULL);
        }
    } else {
        /* Simple build without recovery */
        result = build_execute(orch->current_project, NULL);
    }

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

/* ========================================================================
 * AI-First Autonomous Build
 * ======================================================================== */

CyxMakeError cyxmake_build_autonomous(Orchestrator* orch, const char* project_path) {
    if (!orch || !project_path) {
        return CYXMAKE_ERROR_INVALID_ARG;
    }

    /* Check if AI is available */
    if (!orch->ai_enabled) {
        log_error("Autonomous build requires AI engine");
        log_info("Please configure AI provider in cyxmake.toml or install model at ~/.cyxmake/models/");
        return CYXMAKE_ERROR_INTERNAL;
    }

    /* Get the AI provider to use */
    AIProvider* ai = orch->default_ai;
    if (!ai && orch->llm) {
        /* Create a wrapper provider for llama.cpp if no custom provider */
        log_warning("No AIProvider configured, falling back to local llama.cpp");
        log_error("Autonomous build requires AIProvider (custom providers in cyxmake.toml)");
        return CYXMAKE_ERROR_INTERNAL;
    }

    if (!ai) {
        log_error("No AI provider available");
        return CYXMAKE_ERROR_INTERNAL;
    }

    log_info("Starting AI-powered autonomous build...");
    log_info("Project: %s", project_path);
    log_info("Using AI provider: %s", ai->config.name ? ai->config.name : "default");
    log_plain("");

    /* Create AI Build Agent */
    AIBuildAgentConfig config = ai_build_agent_config_default();
    config.verbose = true;
    config.auto_install_deps = true;
    config.allow_commands = true;

    AIBuildAgent* agent = ai_build_agent_create(ai, orch->tool_registry, &config);
    if (!agent) {
        log_error("Failed to create AI Build Agent");
        return CYXMAKE_ERROR_INTERNAL;
    }

    /* Run autonomous build */
    BuildResult* result = ai_build_agent_build(agent, project_path);

    /* Cleanup agent */
    ai_build_agent_free(agent);

    if (!result) {
        log_error("Autonomous build returned no result");
        return CYXMAKE_ERROR_BUILD;
    }

    /* Show result */
    log_plain("");
    if (result->success) {
        log_success("Autonomous build completed successfully!");
    } else {
        log_error("Autonomous build failed");
        if (result->stderr_output && strlen(result->stderr_output) > 0) {
            log_plain("Last error: %s", result->stderr_output);
        }
    }

    CyxMakeError err = result->success ? CYXMAKE_SUCCESS : CYXMAKE_ERROR_BUILD;

    build_result_free(result);
    return err;
}

/* Get the LLM context from orchestrator (for REPL/CLI) */
LLMContext* cyxmake_get_llm(Orchestrator* orch) {
    return orch ? orch->llm : NULL;
}

/* Get the tool registry from orchestrator */
ToolRegistry* cyxmake_get_tools(Orchestrator* orch) {
    return orch ? orch->tool_registry : NULL;
}

/* Check if AI is enabled */
bool cyxmake_ai_enabled(Orchestrator* orch) {
    return orch ? orch->ai_enabled : false;
}
