/**
 * @file main.c
 * @brief CyxMake CLI entry point
 */

#include <stdlib.h>
#include <string.h>
#include "cyxmake/cyxmake.h"
#include "cyxmake/logger.h"
#include "cyxmake/llm_interface.h"
#include "cyxmake/tool_executor.h"
#include "cyxmake/prompt_templates.h"
#include "cyxmake/file_ops.h"
#include "cyxmake/repl.h"

static void print_version(void) {
    log_plain("CyxMake version %s\n", cyxmake_version());
    log_plain("AI-Powered Build Automation System\n");
    log_plain("\n");
    log_plain("Copyright (C) 2025 CyxMake Team\n");
    log_plain("Licensed under Apache License 2.0\n");
}

static void print_help(const char* program_name) {
    log_plain("Usage: %s [command] [options]\n", program_name);
    log_plain("\n");
    log_plain("AI-Powered Build Automation System\n");
    log_plain("\n");
    log_plain("Commands:\n");
    log_plain("  init              Initialize project (analyze and create cache)\n");
    log_plain("  build             Build the project with AI error recovery\n");
    log_plain("  create            Create new project from natural language\n");
    log_plain("  doctor            Check project health\n");
    log_plain("  status            Show project and AI status\n");
    log_plain("  clean             Clean build artifacts\n");
    log_plain("  cache             Manage project cache\n");
    log_plain("  config            Manage configuration\n");
    log_plain("  test-llm          Test LLM integration (requires model)\n");
    log_plain("  help              Show this help message\n");
    log_plain("  version           Show version information\n");
    log_plain("\n");
    log_plain("Options:\n");
    log_plain("  -v, --verbose     Enable verbose output\n");
    log_plain("  -q, --quiet       Suppress output\n");
    log_plain("  --no-ai           Disable AI features (faster startup)\n");
    log_plain("  --auto-fix        Auto-apply suggested fixes without prompting\n");
    log_plain("  --version         Show version and exit\n");
    log_plain("  --help            Show help and exit\n");
    log_plain("\n");
    log_plain("AI Features:\n");
    log_plain("  - Automatic error diagnosis using local LLM\n");
    log_plain("  - Smart package installation via system package manager\n");
    log_plain("  - Build retry with exponential backoff\n");
    log_plain("\n");
    log_plain("AI Setup:\n");
    log_plain("  1. Download model: huggingface.co/Qwen/Qwen2.5-Coder-3B-Instruct-GGUF\n");
    log_plain("  2. Place at: ~/.cyxmake/models/qwen2.5-coder-3b-q4_k_m.gguf\n");
    log_plain("  3. Run: %s test-llm (to verify)\n", program_name);
    log_plain("\n");
    log_plain("Examples:\n");
    log_plain("  %s init                    # Analyze current directory\n", program_name);
    log_plain("  %s build                   # Build with AI recovery\n", program_name);
    log_plain("  %s build --no-ai           # Build without AI\n", program_name);
    log_plain("  %s build --auto-fix        # Build with auto-fix enabled\n", program_name);
    log_plain("  %s create \"C++ game engine\" # Create new project\n", program_name);
    log_plain("\n");
    log_plain("Natural Language:\n");
    log_plain("  You can also use plain English commands:\n");
    log_plain("  %s \"build the project\"     # Same as 'build'\n", program_name);
    log_plain("  %s \"show readme.md\"        # Read a file\n", program_name);
    log_plain("  %s \"install SDL2\"          # Install a package\n", program_name);
    log_plain("  %s \"clean up\"              # Clean build artifacts\n", program_name);
    log_plain("\n");
    log_plain("Documentation: https://docs.cyxmake.com\n");
    log_plain("Report issues: https://github.com/cyxmake/cyxmake/issues\n");
}

int main(int argc, char** argv) {
    // Initialize logger
    log_init(NULL);  /* Use default configuration */

    // Start interactive REPL if no arguments provided
    if (argc == 1) {
        ReplConfig config = repl_config_default();
        ReplSession* session = repl_session_create(&config, NULL);
        if (!session) {
            log_error("Failed to create REPL session");
            log_shutdown();
            return CYXMAKE_ERROR_INTERNAL;
        }

        int result = repl_run(session);
        repl_session_free(session);
        log_shutdown();
        return result;
    }

    // Handle version flag
    if (argc == 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "version") == 0)) {
        print_version();
        log_shutdown();
        return 0;
    }

    // Handle help flag
    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0) {
        print_help(argv[0]);
        log_shutdown();
        return 0;
    }

    // Initialize CyxMake
    log_info("CyxMake v%s - AI-Powered Build Automation", cyxmake_version());
    log_info("Initializing...");
    log_plain("\n");

    Orchestrator* orch = cyxmake_init(NULL);
    if (!orch) {
        log_error("Failed to initialize CyxMake");
        log_shutdown();
        return CYXMAKE_ERROR_INTERNAL;
    }

    CyxMakeError err = CYXMAKE_SUCCESS;
    const char* command = argv[1];

    // Handle commands
    if (strcmp(command, "init") == 0) {
        log_info("Analyzing project...");
        err = cyxmake_analyze_project(orch, ".");
        if (err == CYXMAKE_SUCCESS) {
            log_plain("\n");
            log_success("Project analysis complete");
            log_info("Cache saved to .cyxmake/cache.json");
            log_plain("\nNext steps:\n");
            log_info("  • Run 'cyxmake build' to build the project");
            log_info("  • Run 'cyxmake doctor' to check for issues");
        } else {
            log_plain("\n");
            log_error("Project analysis failed");
        }
    }
    else if (strcmp(command, "build") == 0) {
        log_info("Building project...");
        err = cyxmake_build(orch, ".");
        if (err == CYXMAKE_SUCCESS) {
            log_plain("\n");
            log_success("Build successful");
        } else {
            log_plain("\n");
            log_error("Build failed");
        }
    }
    else if (strcmp(command, "create") == 0) {
        if (argc < 3) {
            log_error("'create' command requires a description");
            log_info("Example: cyxmake create \"C++ project with SDL2\"");
            err = CYXMAKE_ERROR_INVALID_ARG;
        } else {
            log_info("Creating project from description...");
            err = cyxmake_create_project(orch, argv[2], ".");
            if (err == CYXMAKE_SUCCESS) {
                log_plain("\n");
                log_success("Project created successfully");
            } else {
                log_plain("\n");
                log_error("Project creation failed");
            }
        }
    }
    else if (strcmp(command, "test-llm") == 0) {
        log_info("Testing LLM integration...");
        log_plain("\n");

        /* Get model path from argument or use default */
        char* model_path_allocated = NULL;
        const char* model_path = argc >= 3 ? argv[2] : NULL;
        if (!model_path) {
            model_path_allocated = llm_get_default_model_path();
            model_path = model_path_allocated;
            log_info("Using default model path: %s", model_path);
        }

        /* Validate model file */
        log_info("Validating model file...");
        if (!llm_validate_model_file(model_path)) {
            log_error("Model validation failed");
            log_plain("\n");
            log_info("Please provide a valid GGUF model file:");
            log_info("  1. Download from: https://huggingface.co/Qwen/Qwen2.5-Coder-3B-Instruct-GGUF");
            log_info("  2. Place at: %s", model_path);
            log_info("  3. Or specify path: cyxmake test-llm /path/to/model.gguf");

            /* Free allocated path before returning */
            if (model_path_allocated) {
                free(model_path_allocated);
            }

            err = CYXMAKE_ERROR_INTERNAL;
        } else {
            log_success("Model file valid");
            log_plain("\n");

            /* Configure LLM */
            LLMConfig* config = llm_config_default();
            config->model_path = model_path;
            config->n_ctx = 2048;  /* Smaller context for testing */
            config->verbose = false;

            /* Initialize LLM */
            log_info("Loading model (this may take a few seconds)...");
            LLMContext* llm = llm_init(config);

            if (!llm) {
                log_error("Failed to initialize LLM");
                err = CYXMAKE_ERROR_INTERNAL;
            } else {
                log_plain("\n");

                /* Get model info */
                LLMModelInfo* info = llm_get_model_info(llm);
                if (info) {
                    log_info("Model Information:");
                    log_info("  Name: %s", info->model_name);
                    log_info("  Type: %s", info->model_type);
                    log_info("  Vocabulary: %d tokens", info->vocab_size);
                    log_info("  Context length: %d tokens", info->context_length);
                    log_info("  File size: %.2f GB", (double)info->model_size_bytes / (1024*1024*1024));
                    llm_model_info_free(info);
                    log_plain("\n");
                }

                /* Test query */
                const char* test_prompt = "What is the main purpose of a build system? Answer in one sentence.";
                log_info("Testing inference with prompt:");
                log_plain("  \"%s\"", test_prompt);
                log_plain("\n");

                log_info("Generating response (this may take 1-2 seconds)...");
                LLMRequest* req = llm_request_create(test_prompt);
                req->max_tokens = 64;
                req->temperature = 0.3;

                LLMResponse* resp = llm_query(llm, req);

                if (resp && resp->success) {
                    log_plain("\n");
                    log_success("Inference successful!");
                    log_plain("\n");
                    log_info("Response:");
                    log_plain("  %s", resp->text);
                    log_plain("\n");
                    log_info("Statistics:");
                    log_info("  Tokens (prompt): %d", resp->tokens_prompt);
                    log_info("  Tokens (generated): %d", resp->tokens_generated);
                    log_info("  Duration: %.2f seconds", resp->duration_sec);
                    log_info("  Speed: %.1f tokens/sec", resp->tokens_generated / resp->duration_sec);
                } else {
                    log_plain("\n");
                    log_error("Inference failed: %s",
                             resp ? resp->error_message : "Unknown error");
                    err = CYXMAKE_ERROR_INTERNAL;
                }

                llm_response_free(resp);
                llm_request_free(req);
                llm_shutdown(llm);
            }

            llm_config_free(config);
        }

        /* Free allocated path if we allocated it */
        if (model_path_allocated) {
            free(model_path_allocated);
        }
    }
    else if (strcmp(command, "doctor") == 0) {
        log_info("Running health check...");
        log_info("(Not yet implemented)");
    }
    else if (strcmp(command, "status") == 0) {
        log_info("CyxMake Status");
        log_plain("\n");

        /* Check AI model */
        char* model_path = llm_get_default_model_path();
        log_info("AI Configuration:");
        log_info("  Model path: %s", model_path ? model_path : "N/A");

        if (model_path && llm_validate_model_file(model_path)) {
            log_success("  Model status: Available");

            /* Quick model info */
            LLMConfig* config = llm_config_default();
            config->model_path = model_path;
            LLMContext* llm = llm_init(config);
            if (llm) {
                LLMModelInfo* info = llm_get_model_info(llm);
                if (info) {
                    log_info("  Model name: %s", info->model_name);
                    log_info("  Model type: %s", info->model_type);
                    log_info("  Context: %d tokens", info->context_length);

                    /* Check GPU */
                    LLMGpuBackend gpu = llm_detect_gpu();
                    if (gpu != LLM_GPU_NONE) {
                        log_info("  GPU: %s", llm_gpu_backend_name(gpu));
                    } else {
                        log_info("  GPU: None (CPU mode)");
                    }

                    llm_model_info_free(info);
                }
                llm_shutdown(llm);
            }
            llm_config_free(config);
        } else {
            log_warning("  Model status: Not found");
            log_info("  To enable AI, download a GGUF model to:");
            log_info("    %s", model_path ? model_path : "~/.cyxmake/models/");
        }

        if (model_path) {
            free(model_path);
        }

        log_plain("\n");

        /* Check tools */
        log_info("Tool Discovery:");
        ToolRegistry* temp_registry = tool_registry_create();
        if (temp_registry) {
            int tools = tool_discover_all(temp_registry);
            log_info("  Tools found: %d", tools);

            const ToolInfo* pkg_mgr = package_get_default_manager(temp_registry);
            if (pkg_mgr) {
                log_info("  Package manager: %s", pkg_mgr->display_name);
            } else {
                log_warning("  Package manager: None found");
            }

            tool_registry_free(temp_registry);
        }

        log_plain("\n");
        log_info("Run 'cyxmake test-llm' to test AI inference");
    }
    else {
        /* Treat unknown commands as natural language input */
        log_info("Processing natural language command...");
        log_plain("\n");

        /* Combine all arguments into one string */
        size_t total_len = 0;
        for (int i = 1; i < argc; i++) {
            total_len += strlen(argv[i]) + 1;  /* +1 for space */
        }

        char* nl_command = malloc(total_len + 1);
        if (nl_command) {
            nl_command[0] = '\0';
            for (int i = 1; i < argc; i++) {
                if (i > 1) strcat(nl_command, " ");
                strcat(nl_command, argv[i]);
            }

            /* Parse the command */
            ParsedCommand* parsed = parse_command_local(nl_command);

            if (parsed) {
                log_info("Detected intent: %s (confidence: %.0f%%)",
                        parsed->intent == INTENT_BUILD ? "build" :
                        parsed->intent == INTENT_INIT ? "init" :
                        parsed->intent == INTENT_CLEAN ? "clean" :
                        parsed->intent == INTENT_TEST ? "test" :
                        parsed->intent == INTENT_CREATE_FILE ? "create_file" :
                        parsed->intent == INTENT_READ_FILE ? "read_file" :
                        parsed->intent == INTENT_EXPLAIN ? "explain" :
                        parsed->intent == INTENT_FIX ? "fix" :
                        parsed->intent == INTENT_INSTALL ? "install" :
                        parsed->intent == INTENT_STATUS ? "status" :
                        parsed->intent == INTENT_HELP ? "help" : "unknown",
                        parsed->confidence * 100);

                if (parsed->target) {
                    log_info("Target: %s", parsed->target);
                }
                log_plain("\n");

                /* Execute based on intent */
                switch (parsed->intent) {
                    case INTENT_BUILD:
                        log_info("Executing: build");
                        err = cyxmake_build(orch, ".");
                        if (err == CYXMAKE_SUCCESS) {
                            log_plain("\n");
                            log_success("Build successful");
                        } else {
                            log_plain("\n");
                            log_error("Build failed");
                        }
                        break;

                    case INTENT_INIT:
                        log_info("Executing: init");
                        err = cyxmake_analyze_project(orch, ".");
                        if (err == CYXMAKE_SUCCESS) {
                            log_plain("\n");
                            log_success("Project analysis complete");
                        }
                        break;

                    case INTENT_CLEAN:
                        log_info("Executing: clean");
                        log_plain("\n");

                        /* Clean common build directories */
                        {
                            const char* build_dirs[] = {"build", "cmake-build-debug", "cmake-build-release", "out", NULL};
                            int cleaned = 0;

                            for (int i = 0; build_dirs[i]; i++) {
                                if (file_exists(build_dirs[i])) {
                                    log_info("Removing: %s/", build_dirs[i]);
                                    if (dir_delete_recursive(build_dirs[i])) {
                                        cleaned++;
                                    } else {
                                        log_warning("Could not fully remove %s", build_dirs[i]);
                                    }
                                }
                            }

                            /* Also clean .cyxmake cache if it exists */
                            if (file_exists(".cyxmake")) {
                                log_info("Removing: .cyxmake/");
                                if (dir_delete_recursive(".cyxmake")) {
                                    cleaned++;
                                }
                            }

                            if (cleaned > 0) {
                                log_success("Cleaned %d build director%s", cleaned, cleaned == 1 ? "y" : "ies");
                            } else {
                                log_info("No build directories found to clean");
                            }
                        }
                        break;

                    case INTENT_TEST:
                        log_info("Executing: test");
                        log_warning("Test not yet implemented");
                        break;

                    case INTENT_READ_FILE:
                        if (parsed->target) {
                            log_info("Reading file: %s", parsed->target);
                            log_plain("\n");
                            if (file_exists(parsed->target)) {
                                if (!file_read_display(parsed->target, 100)) {
                                    log_error("Failed to read file");
                                    err = CYXMAKE_ERROR_INTERNAL;
                                }
                            } else {
                                log_error("File not found: %s", parsed->target);
                                err = CYXMAKE_ERROR_INVALID_ARG;
                            }
                        } else {
                            log_warning("No file specified to read");
                            log_info("Example: cyxmake \"show readme.md\"");
                        }
                        break;

                    case INTENT_CREATE_FILE:
                        if (parsed->target) {
                            log_info("Creating file: %s", parsed->target);
                            if (file_exists(parsed->target)) {
                                log_warning("File already exists: %s", parsed->target);
                                log_info("Use 'cyxmake \"overwrite %s\"' to replace", parsed->target);
                            } else {
                                /* Create empty file or with template based on extension */
                                const char* content = "";

                                /* Check file extension for templates */
                                if (strstr(parsed->target, ".c")) {
                                    content = "/**\n * @file \n * @brief \n */\n\n#include <stdio.h>\n\nint main(void) {\n    return 0;\n}\n";
                                } else if (strstr(parsed->target, ".h")) {
                                    content = "/**\n * @file \n * @brief \n */\n\n#ifndef _H\n#define _H\n\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n\n\n#ifdef __cplusplus\n}\n#endif\n\n#endif\n";
                                } else if (strstr(parsed->target, ".md")) {
                                    content = "# Title\n\n## Description\n\n";
                                } else if (strstr(parsed->target, ".py")) {
                                    content = "#!/usr/bin/env python3\n\"\"\"\nDescription\n\"\"\"\n\ndef main():\n    pass\n\nif __name__ == \"__main__\":\n    main()\n";
                                }

                                if (file_write(parsed->target, content)) {
                                    log_success("Created: %s", parsed->target);
                                } else {
                                    log_error("Failed to create file");
                                    err = CYXMAKE_ERROR_INTERNAL;
                                }
                            }
                        } else {
                            log_warning("No file specified to create");
                            log_info("Example: cyxmake \"create main.c\"");
                        }
                        break;

                    case INTENT_INSTALL:
                        if (parsed->target) {
                            log_info("Installing package: %s", parsed->target);
                            log_plain("\n");

                            /* Get tool registry from orchestrator or create temp */
                            ToolRegistry* install_registry = tool_registry_create();
                            if (install_registry) {
                                tool_discover_all(install_registry);
                                const ToolInfo* pkg_mgr = package_get_default_manager(install_registry);

                                if (pkg_mgr) {
                                    log_info("Using package manager: %s", pkg_mgr->display_name);

                                    /* Execute package install */
                                    ToolExecResult* install_result = package_install(install_registry,
                                                                                     parsed->target,
                                                                                     NULL);
                                    if (install_result && install_result->success) {
                                        log_success("Package '%s' installed successfully", parsed->target);
                                    } else {
                                        log_error("Failed to install '%s'", parsed->target);
                                        if (install_result && install_result->stderr_output) {
                                            log_plain("%s\n", install_result->stderr_output);
                                        }
                                        log_info("Try manually: %s install %s",
                                                pkg_mgr->display_name, parsed->target);
                                        err = CYXMAKE_ERROR_INTERNAL;
                                    }
                                    tool_exec_result_free(install_result);
                                } else {
                                    log_error("No package manager found on this system");
                                    log_info("Install a package manager like winget, apt, or brew");
                                    err = CYXMAKE_ERROR_INTERNAL;
                                }

                                tool_registry_free(install_registry);
                            }
                        } else {
                            log_warning("No package specified to install");
                            log_info("Example: cyxmake \"install SDL2\"");
                        }
                        break;

                    case INTENT_STATUS:
                        log_info("Executing: status");
                        /* Run status command logic here */
                        {
                            char* model_path = llm_get_default_model_path();
                            log_info("AI Model: %s", model_path ? model_path : "Not found");
                            if (model_path) free(model_path);
                        }
                        break;

                    case INTENT_HELP:
                        print_help(argv[0]);
                        break;

                    case INTENT_EXPLAIN:
                    case INTENT_FIX:
                        log_info("This feature requires AI. Checking availability...");
                        log_warning("AI-powered %s not yet implemented",
                                   parsed->intent == INTENT_EXPLAIN ? "explain" : "fix");
                        break;

                    default:
                        log_warning("Could not understand: '%s'", nl_command);
                        log_info("Try 'cyxmake help' for available commands");
                        err = CYXMAKE_ERROR_INVALID_ARG;
                        break;
                }

                parsed_command_free(parsed);
            } else {
                log_error("Failed to parse command");
                err = CYXMAKE_ERROR_INVALID_ARG;
            }

            free(nl_command);
        } else {
            log_error("Memory allocation failed");
            err = CYXMAKE_ERROR_INTERNAL;
        }
    }

    cyxmake_shutdown(orch);
    log_shutdown();
    return err;
}
