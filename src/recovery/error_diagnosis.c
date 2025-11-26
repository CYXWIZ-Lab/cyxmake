/**
 * @file error_diagnosis.c
 * @brief Main error diagnosis and recovery coordinator
 */

#include "cyxmake/error_recovery.h"
#include "cyxmake/logger.h"
#include "cyxmake/prompt_templates.h"
#include "cyxmake/tool_executor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #define sleep(x) Sleep((x) * 1000)
#else
    #include <unistd.h>
#endif

/* Recovery context structure */
struct RecoveryContext {
    RecoveryStrategy strategy;
    int total_attempts;
    int successful_recoveries;
    time_t last_attempt;
    LLMContext* llm_ctx;         /* Optional LLM for AI analysis */
    ToolRegistry* tool_registry; /* Optional tool registry for smart installs */
};

/* Create default recovery strategy */
static RecoveryStrategy default_strategy(void) {
    RecoveryStrategy strategy = {
        .max_retries = 3,
        .retry_delay_ms = 1000,
        .backoff_multiplier = 2.0f,
        .max_delay_ms = 30000,
        .use_ai_analysis = true,
        .auto_apply_fixes = false
    };
    return strategy;
}

/* External functions from error_patterns.c */
extern char* extract_error_detail(const char* error_output, ErrorPatternType type);

/* Create error diagnosis */
ErrorDiagnosis* error_diagnose(const BuildResult* build_result,
                               const ProjectContext* ctx) {
    if (!build_result || build_result->success) {
        return NULL;
    }

    log_info("Diagnosing build error...");

    /* Create diagnosis result */
    ErrorDiagnosis* diagnosis = calloc(1, sizeof(ErrorDiagnosis));
    if (!diagnosis) {
        log_error("Failed to allocate diagnosis");
        return NULL;
    }

    /* Combine stdout and stderr for analysis */
    size_t error_len = 0;
    if (build_result->stdout_output) {
        error_len += strlen(build_result->stdout_output);
    }
    if (build_result->stderr_output) {
        error_len += strlen(build_result->stderr_output);
    }

    if (error_len == 0) {
        diagnosis->pattern_type = ERROR_PATTERN_UNKNOWN;
        diagnosis->diagnosis = strdup("Build failed with no error output");
        diagnosis->confidence = 0.0;
        return diagnosis;
    }

    /* Combine outputs */
    char* combined_output = calloc(error_len + 2, sizeof(char));
    if (!combined_output) {
        free(diagnosis);
        return NULL;
    }

    if (build_result->stdout_output) {
        strcat(combined_output, build_result->stdout_output);
    }
    if (build_result->stderr_output) {
        strcat(combined_output, build_result->stderr_output);
    }

    /* Store original error message */
    diagnosis->error_message = strdup(combined_output);

    /* Match against error patterns */
    ErrorPatternType pattern_type = error_patterns_match(combined_output);
    diagnosis->pattern_type = pattern_type;

    /* Get pattern details */
    const ErrorPattern* pattern = error_patterns_get(pattern_type);

    if (pattern) {
        /* Extract error details (e.g., missing library name) */
        char* error_detail = extract_error_detail(combined_output, pattern_type);

        /* Generate human-readable diagnosis */
        char diag_buffer[512];
        snprintf(diag_buffer, sizeof(diag_buffer),
                 "%s: %s",
                 pattern->description,
                 error_detail ? error_detail : "See error output for details");
        diagnosis->diagnosis = strdup(diag_buffer);

        /* Generate fix suggestions */
        size_t fix_count = 0;
        diagnosis->suggested_fixes = solution_generate(pattern_type, error_detail,
                                                       ctx, &fix_count);
        diagnosis->fix_count = fix_count;

        /* Set confidence based on pattern priority */
        diagnosis->confidence = pattern->priority / 10.0;
        if (diagnosis->confidence > 1.0) {
            diagnosis->confidence = 1.0;
        }

        if (error_detail) {
            free(error_detail);
        }

        log_info("Diagnosis: %s (confidence: %.2f)",
                 pattern->name, diagnosis->confidence);
    } else {
        /* Unknown error pattern */
        diagnosis->diagnosis = strdup("Unknown error type - manual investigation required");
        diagnosis->confidence = 0.0;

        /* Generate generic fixes */
        size_t fix_count = 0;
        diagnosis->suggested_fixes = solution_generate(ERROR_PATTERN_UNKNOWN, NULL,
                                                       ctx, &fix_count);
        diagnosis->fix_count = fix_count;

        log_warning("Could not identify error pattern");
    }

    free(combined_output);
    return diagnosis;
}

/* Free error diagnosis */
void error_diagnosis_free(ErrorDiagnosis* diagnosis) {
    if (!diagnosis) return;

    free(diagnosis->error_message);
    free(diagnosis->diagnosis);

    if (diagnosis->suggested_fixes) {
        fix_actions_free(diagnosis->suggested_fixes, diagnosis->fix_count);
    }

    free(diagnosis);
}

/* Create recovery context */
RecoveryContext* recovery_context_create(const RecoveryStrategy* strategy) {
    RecoveryContext* ctx = calloc(1, sizeof(RecoveryContext));
    if (!ctx) return NULL;

    if (strategy) {
        ctx->strategy = *strategy;
    } else {
        ctx->strategy = default_strategy();
    }

    ctx->total_attempts = 0;
    ctx->successful_recoveries = 0;
    ctx->last_attempt = 0;

    log_debug("Recovery context created (max_retries=%d, delay=%dms)",
              ctx->strategy.max_retries, ctx->strategy.retry_delay_ms);

    return ctx;
}

/* Free recovery context */
void recovery_context_free(RecoveryContext* ctx) {
    free(ctx);
}

/* Get recovery statistics */
void recovery_get_stats(const RecoveryContext* ctx,
                        int* total_attempts,
                        int* successful_recoveries) {
    if (!ctx) return;

    if (total_attempts) {
        *total_attempts = ctx->total_attempts;
    }
    if (successful_recoveries) {
        *successful_recoveries = ctx->successful_recoveries;
    }
}

/* Calculate backoff delay */
int calculate_backoff_delay(int attempt, int base_delay_ms,
                            float multiplier, int max_delay_ms) {
    if (attempt <= 0) return base_delay_ms;

    int delay = base_delay_ms;
    for (int i = 0; i < attempt; i++) {
        delay = (int)(delay * multiplier);
        if (delay > max_delay_ms) {
            delay = max_delay_ms;
            break;
        }
    }

    return delay;
}

/* Sleep for milliseconds (cross-platform) */
static void sleep_ms(int milliseconds) {
    if (milliseconds <= 0) return;

#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

/* Attempt to recover from build failure */
BuildResult* recovery_attempt(RecoveryContext* ctx,
                              const BuildResult* build_result,
                              ProjectContext* project_ctx) {
    if (!ctx || !build_result || !project_ctx) return NULL;

    ctx->total_attempts++;
    ctx->last_attempt = time(NULL);

    log_info("Recovery attempt %d", ctx->total_attempts);

    /* Diagnose the error - use LLM if available and AI analysis is enabled */
    ErrorDiagnosis* diagnosis = NULL;
    if (ctx->strategy.use_ai_analysis && ctx->llm_ctx) {
        diagnosis = error_diagnose_with_llm(build_result, project_ctx, ctx->llm_ctx);
    } else {
        diagnosis = error_diagnose(build_result, project_ctx);
    }
    if (!diagnosis) {
        log_error("Failed to diagnose error");
        return NULL;
    }

    /* Display diagnosis */
    log_info("Error diagnosis: %s", diagnosis->diagnosis);
    log_info("Found %zu potential fix(es)", diagnosis->fix_count);

    /* Apply fixes if configured */
    int fixes_applied = 0;
    if (diagnosis->fix_count > 0) {
        if (ctx->strategy.auto_apply_fixes) {
            log_info("Auto-applying fixes...");
            /* Use tool registry if available for smarter package installation */
            if (ctx->tool_registry) {
                fixes_applied = fix_execute_all_with_tools(diagnosis->suggested_fixes,
                                                           diagnosis->fix_count,
                                                           project_ctx,
                                                           ctx->tool_registry);
            } else {
                fixes_applied = fix_execute_all(diagnosis->suggested_fixes,
                                               diagnosis->fix_count,
                                               project_ctx);
            }
        } else {
            /* Interactive mode - ask user */
            log_plain("\nSuggested fixes:");
            for (size_t i = 0; i < diagnosis->fix_count; i++) {
                if (diagnosis->suggested_fixes[i]) {
                    log_plain("  %zu. %s", i + 1,
                             diagnosis->suggested_fixes[i]->description);
                }
            }

            log_plain("\nApply fixes? (y/n): ");
            char response[10];
            if (fgets(response, sizeof(response), stdin) != NULL &&
                (response[0] == 'y' || response[0] == 'Y')) {
                /* Use tool registry if available */
                if (ctx->tool_registry) {
                    fixes_applied = fix_execute_all_with_tools(diagnosis->suggested_fixes,
                                                               diagnosis->fix_count,
                                                               project_ctx,
                                                               ctx->tool_registry);
                } else {
                    fixes_applied = fix_execute_all(diagnosis->suggested_fixes,
                                                   diagnosis->fix_count,
                                                   project_ctx);
                }
            }
        }
    }

    /* Free diagnosis */
    error_diagnosis_free(diagnosis);

    /* If fixes were applied, retry the build */
    BuildResult* new_result = NULL;
    if (fixes_applied > 0) {
        log_info("Retrying build after applying %d fix(es)...", fixes_applied);

        /* Small delay to let fixes take effect */
        sleep_ms(500);

        /* Retry build */
        new_result = build_execute(project_ctx, NULL);

        if (new_result && new_result->success) {
            ctx->successful_recoveries++;
            log_success("Build successful after recovery!");
        }
    } else {
        log_warning("No fixes applied, build not retried");
    }

    return new_result;
}

/* Execute build with retry logic */
BuildResult* build_with_retry(ProjectContext* project_ctx,
                              const BuildOptions* build_opts,
                              const RecoveryStrategy* strategy) {
    if (!project_ctx) return NULL;

    /* Create recovery context */
    RecoveryContext* recovery_ctx = recovery_context_create(strategy);
    if (!recovery_ctx) {
        return build_execute(project_ctx, build_opts);
    }

    BuildResult* result = NULL;
    BuildResult* last_result = NULL;
    int attempt = 0;

    /* Initial build attempt */
    log_info("Starting build with recovery enabled");
    result = build_execute(project_ctx, build_opts);

    /* If initial build succeeds, we're done */
    if (result && result->success) {
        recovery_context_free(recovery_ctx);
        return result;
    }

    /* Save initial failure */
    last_result = result;

    /* Retry loop with recovery */
    while (attempt < recovery_ctx->strategy.max_retries &&
           (!result || !result->success)) {

        attempt++;
        log_warning("Build failed, attempting recovery (attempt %d/%d)",
                   attempt, recovery_ctx->strategy.max_retries);

        /* Calculate delay with exponential backoff */
        int delay = calculate_backoff_delay(attempt - 1,
                                           recovery_ctx->strategy.retry_delay_ms,
                                           recovery_ctx->strategy.backoff_multiplier,
                                           recovery_ctx->strategy.max_delay_ms);

        if (delay > 0) {
            log_info("Waiting %d ms before retry...", delay);
            sleep_ms(delay);
        }

        /* Attempt recovery */
        BuildResult* new_result = recovery_attempt(recovery_ctx, last_result, project_ctx);

        /* Free previous result if we got a new one */
        if (new_result && new_result != last_result) {
            if (last_result && last_result != result) {
                build_result_free(last_result);
            }
            last_result = new_result;
        }

        /* Check if recovery succeeded */
        if (new_result && new_result->success) {
            /* Success! Free original result and return new one */
            if (result != new_result) {
                build_result_free(result);
            }
            result = new_result;
            break;
        }

        /* If no new result from recovery, just retry the build */
        if (!new_result) {
            log_info("Retrying build (attempt %d/%d)...",
                    attempt + 1, recovery_ctx->strategy.max_retries);

            new_result = build_execute(project_ctx, build_opts);

            if (new_result) {
                if (last_result && last_result != result) {
                    build_result_free(last_result);
                }
                last_result = new_result;

                if (new_result->success) {
                    if (result != new_result) {
                        build_result_free(result);
                    }
                    result = new_result;
                    break;
                }
            }
        }
    }

    /* Clean up */
    if (last_result && last_result != result) {
        build_result_free(last_result);
    }

    /* Log final statistics */
    int total, successful;
    recovery_get_stats(recovery_ctx, &total, &successful);
    log_info("Recovery statistics: %d attempts, %d successful",
             total, successful);

    recovery_context_free(recovery_ctx);
    return result;
}

/* ========================================================================
 * LLM-Enhanced Diagnosis Functions
 * ======================================================================== */

/* Set LLM context for recovery */
void recovery_set_llm(RecoveryContext* recovery_ctx, LLMContext* llm_ctx) {
    if (recovery_ctx) {
        recovery_ctx->llm_ctx = llm_ctx;
        if (llm_ctx && llm_is_ready(llm_ctx)) {
            log_info("LLM enabled for error recovery");
        }
    }
}

/* Set tool registry for recovery */
void recovery_set_tools(RecoveryContext* recovery_ctx, ToolRegistry* registry) {
    if (recovery_ctx) {
        recovery_ctx->tool_registry = registry;
        if (registry) {
            log_info("Tool registry enabled for error recovery");
        }
    }
}


/* Get LLM suggestion for error */
char* error_get_llm_suggestion(const char* error_output,
                                const ProjectContext* ctx,
                                LLMContext* llm_ctx) {
    if (!error_output || !llm_ctx || !llm_is_ready(llm_ctx)) {
        return NULL;
    }

    log_info("Consulting AI for error analysis...");

    /* Generate smart prompt */
    char* prompt = prompt_smart_error_analysis(error_output, ctx);
    if (!prompt) {
        log_error("Failed to generate analysis prompt");
        return NULL;
    }

    /* Create LLM request with low temperature for focused response */
    LLMRequest* request = llm_request_create(prompt);
    if (!request) {
        free(prompt);
        return NULL;
    }

    request->temperature = 0.3f;
    request->max_tokens = 512;
    request->top_p = 0.95f;

    /* Query LLM */
    LLMResponse* response = llm_query(llm_ctx, request);

    char* result = NULL;
    if (response && response->success && response->text) {
        result = strdup(response->text);
        log_debug("LLM analysis completed (%.2fs, %d tokens)",
                  response->duration_sec, response->tokens_generated);
    } else {
        log_warning("LLM analysis failed: %s",
                    response ? response->error_message : "Unknown error");
    }

    /* Cleanup */
    llm_response_free(response);
    llm_request_free(request);
    free(prompt);

    return result;
}

/* Diagnose with LLM enhancement */
ErrorDiagnosis* error_diagnose_with_llm(const BuildResult* build_result,
                                         const ProjectContext* ctx,
                                         LLMContext* llm_ctx) {
    /* First, get local diagnosis */
    ErrorDiagnosis* diagnosis = error_diagnose(build_result, ctx);
    if (!diagnosis) {
        return NULL;
    }

    /* Check if we should consult LLM */
    bool should_use_llm = false;

    /* Use LLM if:
     * 1. LLM is available and ready
     * 2. Confidence is low (< 0.6) OR pattern is unknown
     * 3. We have error output to analyze
     */
    if (llm_ctx && llm_is_ready(llm_ctx)) {
        if (diagnosis->confidence < 0.6 ||
            diagnosis->pattern_type == ERROR_PATTERN_UNKNOWN) {
            should_use_llm = true;
        }
    }

    if (!should_use_llm) {
        return diagnosis;
    }

    log_info("Low confidence (%.2f), consulting AI for deeper analysis...",
             diagnosis->confidence);

    /* Get LLM suggestion */
    char* llm_suggestion = error_get_llm_suggestion(
        diagnosis->error_message, ctx, llm_ctx);

    if (llm_suggestion) {
        /* Enhance diagnosis with LLM analysis */
        size_t new_diag_len = strlen(diagnosis->diagnosis) +
                              strlen(llm_suggestion) + 100;
        char* enhanced_diag = malloc(new_diag_len);

        if (enhanced_diag) {
            snprintf(enhanced_diag, new_diag_len,
                     "%s\n\nAI Analysis:\n%s",
                     diagnosis->diagnosis, llm_suggestion);

            free(diagnosis->diagnosis);
            diagnosis->diagnosis = enhanced_diag;

            /* Boost confidence with LLM input */
            diagnosis->confidence = diagnosis->confidence + 0.3;
            if (diagnosis->confidence > 1.0) {
                diagnosis->confidence = 1.0;
            }
        }

        free(llm_suggestion);
    }

    return diagnosis;
}
