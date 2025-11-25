/**
 * @file main.c
 * @brief CyxMake CLI entry point
 */

#include <stdlib.h>
#include <string.h>
#include "cyxmake/cyxmake.h"
#include "cyxmake/logger.h"
#include "cyxmake/llm_interface.h"

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
    log_plain("  build             Build the project\n");
    log_plain("  create            Create new project from natural language\n");
    log_plain("  doctor            Check project health\n");
    log_plain("  status            Show project status\n");
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
    log_plain("  --version         Show version and exit\n");
    log_plain("  --help            Show help and exit\n");
    log_plain("\n");
    log_plain("Examples:\n");
    log_plain("  %s init                    # Analyze current directory\n", program_name);
    log_plain("  %s build                   # Build project\n", program_name);
    log_plain("  %s create \"C++ game engine\" # Create new project\n", program_name);
    log_plain("\n");
    log_plain("Documentation: https://docs.cyxmake.com\n");
    log_plain("Report issues: https://github.com/cyxmake/cyxmake/issues\n");
}

int main(int argc, char** argv) {
    // Initialize logger
    log_init(NULL);  /* Use default configuration */

    // Handle version flag
    if (argc == 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "version") == 0)) {
        print_version();
        log_shutdown();
        return 0;
    }

    // Handle help flag
    if (argc == 1 || strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0) {
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
        log_info("Checking project status...");
        log_info("(Not yet implemented)");
    }
    else {
        log_error("Unknown command '%s'", command);
        log_info("Run '%s help' for usage information", argv[0]);
        err = CYXMAKE_ERROR_INVALID_ARG;
    }

    cyxmake_shutdown(orch);
    log_shutdown();
    return err;
}
