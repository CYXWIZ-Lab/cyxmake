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
#include "cyxmake/config.h"
#include "cyxmake/project_generator.h"
#include "cyxmake/logger.h"
#include "cyxmake/threading.h"
#include "cyxmake/agent_registry.h"
#include "cyxmake/agent_coordinator.h"
#include "cyxmake/agent_comm.h"
#include "cyxmake/task_queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* AI-enabled orchestrator structure */
struct Orchestrator {
    Config* config;              /* Loaded configuration */
    LLMContext* llm;
    AIProviderRegistry* ai_registry;
    AIProvider* default_ai;
    ToolRegistry* tool_registry;
    ProjectContext* current_project;
    RecoveryStrategy recovery_strategy;
    bool ai_enabled;
    bool tools_enabled;

    /* Multi-agent system */
    ThreadPool* thread_pool;         /* Async task execution */
    AgentRegistry* agent_registry;   /* Named agent management */
    MessageBus* message_bus;         /* Agent communication */
    SharedState* shared_state;       /* Shared context */
    AgentCoordinator* coordinator;   /* Task distribution & conflict resolution */
    TaskQueue* task_queue;           /* Priority-based task scheduling */
    bool multi_agent_enabled;
};

const char* cyxmake_version(void) {
    return CYXMAKE_VERSION;
}

Orchestrator* cyxmake_init(const char* config_path) {
    Orchestrator* orch = calloc(1, sizeof(Orchestrator));
    if (!orch) {
        return NULL;
    }

    /* Load configuration from file */
    orch->config = config_load(config_path);
    if (orch->config) {
        config_apply_logging(orch->config);
        if (orch->config->loaded) {
            log_info("Loaded configuration from: %s", orch->config->config_path);
        }
    }

    /* Set default recovery strategy */
    orch->recovery_strategy = (RecoveryStrategy){
        .max_retries = 3,
        .retry_delay_ms = 1000,
        .backoff_multiplier = 2.0f,
        .max_delay_ms = 30000,
        .use_ai_analysis = true,
        .auto_apply_fixes = true  /* Auto-apply safe fixes like CMake version updates */
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

    /* Initialize multi-agent system */
    log_info("Initializing multi-agent system...");

    /* Create thread pool (auto-size based on CPU cores) */
    orch->thread_pool = thread_pool_create(0);
    if (orch->thread_pool) {
        log_debug("Thread pool created");
    }

    /* Create shared state for agent context */
    orch->shared_state = shared_state_create();
    if (orch->shared_state) {
        /* Set persistence path */
        char state_path[1024];
        snprintf(state_path, sizeof(state_path), ".cyxmake/agent_state.json");
        shared_state_set_persistence(orch->shared_state, state_path);
        shared_state_load(orch->shared_state);
    }

    /* Create message bus for agent communication */
    orch->message_bus = message_bus_create();

    /* Create agent registry */
    orch->agent_registry = agent_registry_create(
        orch->default_ai,
        orch->tool_registry,
        orch->thread_pool
    );
    if (orch->agent_registry) {
        log_debug("Agent registry created");
        /* Connect shared state to registry for auto-updating during tasks */
        if (orch->shared_state) {
            agent_registry_set_shared_state(orch->agent_registry, orch->shared_state);
        }
    }

    /* Create task queue with priority scheduling */
    orch->task_queue = task_queue_create();

    /* Create agent coordinator */
    if (orch->agent_registry && orch->message_bus && orch->shared_state) {
        CoordinatorConfig coord_config = coordinator_config_defaults();
        coord_config.verbose = false;
        coord_config.max_concurrent_agents = 4;

        orch->coordinator = coordinator_create(
            orch->agent_registry,
            orch->message_bus,
            orch->shared_state,
            &coord_config
        );

        if (orch->coordinator && orch->task_queue) {
            coordinator_set_task_queue(orch->coordinator, orch->task_queue);
        }

        if (orch->coordinator) {
            orch->multi_agent_enabled = true;
            log_success("Multi-agent system ready");
        }
    }

    if (!orch->multi_agent_enabled) {
        log_debug("Multi-agent system not fully initialized");
    }

    log_plain("\n");
    return orch;
}

void cyxmake_shutdown(Orchestrator* orch) {
    if (!orch) {
        return;
    }

    /* Shutdown multi-agent system first */
    if (orch->multi_agent_enabled) {
        log_debug("Shutting down multi-agent system...");

        /* Free coordinator (manages agents, doesn't own registry) */
        if (orch->coordinator) {
            coordinator_free(orch->coordinator);
            orch->coordinator = NULL;
        }

        /* Free task queue */
        if (orch->task_queue) {
            task_queue_free(orch->task_queue);
            orch->task_queue = NULL;
        }

        /* Free agent registry (will terminate all agents) */
        if (orch->agent_registry) {
            agent_registry_free(orch->agent_registry);
            orch->agent_registry = NULL;
        }

        /* Save and free shared state */
        if (orch->shared_state) {
            shared_state_save(orch->shared_state);
            shared_state_free(orch->shared_state);
            orch->shared_state = NULL;
        }

        /* Free message bus */
        if (orch->message_bus) {
            message_bus_free(orch->message_bus);
            orch->message_bus = NULL;
        }

        /* Shutdown thread pool last */
        if (orch->thread_pool) {
            thread_pool_free(orch->thread_pool);
            orch->thread_pool = NULL;
        }
    }

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

    /* Free config */
    if (orch->config) {
        config_free(orch->config);
        orch->config = NULL;
    }

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

                /* Invalidate cache if fixes were applied */
                if (successful_recoveries > 0) {
                    cache_invalidate(project_path);
                    log_debug("Cache invalidated after successful recovery");
                }
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

    log_info("Creating project from description: %s", description);
    log_info("Output path: %s", output_path);

    /* Parse the natural language description */
    ProjectSpec* spec = project_spec_parse(description);
    if (!spec) {
        log_error("Failed to parse project description");
        return CYXMAKE_ERROR_INTERNAL;
    }

    /* Log what we detected */
    log_info("Detected language: %s", language_to_string(spec->language));
    log_info("Project type: %s",
             spec->type == PROJECT_GAME ? "Game" :
             spec->type == PROJECT_LIBRARY ? "Library" :
             spec->type == PROJECT_CLI ? "CLI" :
             spec->type == PROJECT_WEB ? "Web" :
             spec->type == PROJECT_GUI ? "GUI" : "Executable");

    if (spec->dependency_count > 0) {
        log_info("Dependencies detected: %d", spec->dependency_count);
        for (int i = 0; i < spec->dependency_count; i++) {
            log_debug("  - %s", spec->dependencies[i]);
        }
    }

    /* Generate the project */
    GenerationResult* result = project_generate(spec, output_path);

    /* Cleanup spec */
    project_spec_free(spec);

    if (!result) {
        log_error("Failed to generate project");
        return CYXMAKE_ERROR_INTERNAL;
    }

    CyxMakeError err = result->success ? CYXMAKE_SUCCESS : CYXMAKE_ERROR_INTERNAL;

    if (!result->success && result->error_message) {
        log_error("Generation error: %s", result->error_message);
    }

    generation_result_free(result);
    return err;
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

/* ========================================================================
 * Multi-Agent System Accessors
 * ======================================================================== */

/* Get the agent registry from orchestrator */
AgentRegistry* cyxmake_get_agent_registry(Orchestrator* orch) {
    return orch ? orch->agent_registry : NULL;
}

/* Get the agent coordinator from orchestrator */
AgentCoordinator* cyxmake_get_coordinator(Orchestrator* orch) {
    return orch ? orch->coordinator : NULL;
}

/* Get the message bus from orchestrator */
MessageBus* cyxmake_get_message_bus(Orchestrator* orch) {
    return orch ? orch->message_bus : NULL;
}

/* Get the shared state from orchestrator */
SharedState* cyxmake_get_shared_state(Orchestrator* orch) {
    return orch ? orch->shared_state : NULL;
}

/* Get the task queue from orchestrator */
TaskQueue* cyxmake_get_task_queue(Orchestrator* orch) {
    return orch ? orch->task_queue : NULL;
}

/* Get the thread pool from orchestrator */
ThreadPool* cyxmake_get_thread_pool(Orchestrator* orch) {
    return orch ? orch->thread_pool : NULL;
}

/* Check if multi-agent system is enabled */
bool cyxmake_multi_agent_enabled(Orchestrator* orch) {
    return orch ? orch->multi_agent_enabled : false;
}
