/**
 * @file test_recovery_integration.c
 * @brief Integration tests for error recovery with REPL/cache
 */

#include "cyxmake/error_recovery.h"
#include "cyxmake/cache_manager.h"
#include "cyxmake/tool_executor.h"
#include "cyxmake/project_context.h"
#include "cyxmake/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define rmdir _rmdir
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

/* Test project directory */
static const char* TEST_PROJECT = "./test_recovery_project";

/* Create test directory structure */
static bool setup_test_project(void) {
    /* Create .cyxmake directory */
    char cyxmake_dir[256];
    snprintf(cyxmake_dir, sizeof(cyxmake_dir), "%s/.cyxmake", TEST_PROJECT);

    mkdir(TEST_PROJECT, 0755);
    mkdir(cyxmake_dir, 0755);

    return true;
}

/* Clean up test directory */
static void cleanup_test_project(void) {
    char cache_path[256];
    snprintf(cache_path, sizeof(cache_path), "%s/.cyxmake/cache.json", TEST_PROJECT);
    remove(cache_path);

    char cyxmake_dir[256];
    snprintf(cyxmake_dir, sizeof(cyxmake_dir), "%s/.cyxmake", TEST_PROJECT);
    rmdir(cyxmake_dir);
    rmdir(TEST_PROJECT);
}

/* Create mock project context */
static ProjectContext* create_mock_context(void) {
    ProjectContext* ctx = calloc(1, sizeof(ProjectContext));
    if (!ctx) return NULL;

    ctx->root_path = strdup(TEST_PROJECT);
    ctx->name = strdup("test_project");
    ctx->primary_language = LANG_C;
    ctx->build_system.type = BUILD_CMAKE;
    ctx->created_at = time(NULL);
    ctx->updated_at = time(NULL);
    ctx->cache_version = strdup("1.0");
    ctx->confidence = 0.9f;

    return ctx;
}

/* Test cache invalidation */
static void test_cache_invalidation(void) {
    log_info("Testing cache invalidation...");

    setup_test_project();

    /* Create a mock project context and save to cache */
    ProjectContext* ctx = create_mock_context();
    assert(ctx != NULL);

    /* Save cache */
    bool saved = cache_save(ctx, TEST_PROJECT);
    assert(saved == true);
    log_success("Cache saved successfully");

    /* Verify cache exists and is not stale */
    assert(cache_exists(TEST_PROJECT) == true);
    assert(cache_is_stale(ctx, TEST_PROJECT) == false);
    log_success("Cache exists and is fresh");

    project_context_free(ctx);

    /* Invalidate cache */
    bool invalidated = cache_invalidate(TEST_PROJECT);
    assert(invalidated == true);
    log_success("Cache invalidation called");

    /* Reload and verify stale */
    ProjectContext* reloaded = cache_load(TEST_PROJECT);
    assert(reloaded != NULL);
    assert(cache_is_stale(reloaded, TEST_PROJECT) == true);
    log_success("Cache is now stale after invalidation");

    project_context_free(reloaded);
    cleanup_test_project();

    log_success("Cache invalidation test passed!");
}

/* Test cache dependency marking */
static void test_cache_dependency_marking(void) {
    log_info("Testing cache dependency marking...");

    setup_test_project();

    /* Create context with dependencies */
    ProjectContext* ctx = create_mock_context();
    assert(ctx != NULL);

    /* Add a mock dependency */
    ctx->dependency_count = 1;
    ctx->dependencies = calloc(1, sizeof(Dependency*));
    ctx->dependencies[0] = calloc(1, sizeof(Dependency));
    ctx->dependencies[0]->name = strdup("libfoo");
    ctx->dependencies[0]->is_installed = false;

    /* Save cache */
    bool saved = cache_save(ctx, TEST_PROJECT);
    assert(saved == true);
    log_success("Cache with dependency saved");

    project_context_free(ctx);

    /* Mark dependency as installed */
    bool marked = cache_mark_dependency_installed(TEST_PROJECT, "libfoo");
    assert(marked == true);
    log_success("Dependency marked as installed");

    /* Reload and verify */
    ProjectContext* reloaded = cache_load(TEST_PROJECT);
    assert(reloaded != NULL);
    assert(reloaded->dependency_count == 1);
    assert(reloaded->dependencies[0]->is_installed == true);
    log_success("Dependency status updated correctly");

    project_context_free(reloaded);
    cleanup_test_project();

    log_success("Cache dependency marking test passed!");
}

/* Test fix action type mapping (placeholder for permission tests) */
static void test_fix_action_types_basic(void) {
    log_info("Testing fix action type mapping...");

    /* Test that FixActionType enum values are correct */
    assert(FIX_ACTION_INSTALL_PACKAGE == 0);
    log_success("FIX_ACTION_INSTALL_PACKAGE = 0");

    assert(FIX_ACTION_CREATE_FILE == 1);
    log_success("FIX_ACTION_CREATE_FILE = 1");

    assert(FIX_ACTION_MODIFY_FILE == 2);
    log_success("FIX_ACTION_MODIFY_FILE = 2");

    assert(FIX_ACTION_RUN_COMMAND == 4);
    log_success("FIX_ACTION_RUN_COMMAND = 4");

    assert(FIX_ACTION_FIX_CMAKE_VERSION == 6);
    log_success("FIX_ACTION_FIX_CMAKE_VERSION = 6");

    log_success("Fix action type mapping test passed!");
}

/* Test recovery context with tools */
static void test_recovery_with_tools(void) {
    log_info("Testing recovery context with tools...");

    /* Create tool registry */
    ToolRegistry* registry = tool_registry_create();
    assert(registry != NULL);
    log_success("Tool registry created");

    /* Discover available tools */
    int discovered = tool_discover_all(registry);
    log_info("Discovered %d tools", discovered);

    /* Create recovery context */
    RecoveryStrategy strategy = {
        .max_retries = 3,
        .retry_delay_ms = 1000,
        .backoff_multiplier = 2.0f,
        .max_delay_ms = 30000,
        .use_ai_analysis = false,
        .auto_apply_fixes = false
    };

    RecoveryContext* recovery = recovery_context_create(&strategy);
    assert(recovery != NULL);
    log_success("Recovery context created");

    /* Attach tools */
    recovery_set_tools(recovery, registry);
    log_success("Tools attached to recovery context");

    /* Clean up */
    recovery_context_free(recovery);
    tool_registry_free(registry);

    log_success("Recovery with tools test passed!");
}

/* Test fix action types */
static void test_fix_action_types(void) {
    log_info("Testing fix action types...");

    /* Initialize patterns */
    error_patterns_init();

    /* Create mock project context */
    ProjectContext* ctx = create_mock_context();
    assert(ctx != NULL);

    /* Test solution generation for different error types */
    size_t fix_count = 0;
    FixAction** fixes;

    /* Missing library */
    fixes = solution_generate(ERROR_PATTERN_MISSING_LIBRARY, "curl", ctx, &fix_count);
    assert(fixes != NULL && fix_count > 0);
    log_success("Generated %zu fixes for MISSING_LIBRARY", fix_count);

    /* Verify first fix is INSTALL_PACKAGE */
    assert(fixes[0]->type == FIX_ACTION_INSTALL_PACKAGE);
    log_success("First fix is INSTALL_PACKAGE (priority order correct)");

    /* Free fixes */
    fix_actions_free(fixes, fix_count);

    /* CMake version error */
    fixes = solution_generate(ERROR_PATTERN_CMAKE_VERSION, "3.10", ctx, &fix_count);
    assert(fixes != NULL && fix_count > 0);
    log_success("Generated %zu fixes for CMAKE_VERSION", fix_count);

    /* Verify fix is FIX_CMAKE_VERSION */
    bool has_cmake_fix = false;
    for (size_t i = 0; i < fix_count; i++) {
        if (fixes[i]->type == FIX_ACTION_FIX_CMAKE_VERSION) {
            has_cmake_fix = true;
            break;
        }
    }
    assert(has_cmake_fix == true);
    log_success("FIX_CMAKE_VERSION action generated");

    /* Free fixes */
    fix_actions_free(fixes, fix_count);

    project_context_free(ctx);
    error_patterns_shutdown();

    log_success("Fix action types test passed!");
}

/* Test error diagnosis flow */
static void test_error_diagnosis_flow(void) {
    log_info("Testing error diagnosis flow...");

    /* Initialize patterns */
    error_patterns_init();

    /* Create mock project context */
    ProjectContext* ctx = create_mock_context();
    assert(ctx != NULL);

    /* Create mock build result with error */
    BuildResult mock_result = {
        .success = false,
        .exit_code = 1,
        .stderr_output = "undefined reference to `curl_easy_init'",
        .stdout_output = NULL
    };

    /* Diagnose error */
    ErrorDiagnosis* diagnosis = error_diagnose(&mock_result, ctx);
    assert(diagnosis != NULL);
    log_success("Error diagnosed successfully");

    /* Verify diagnosis */
    assert(diagnosis->pattern_type == ERROR_PATTERN_MISSING_LIBRARY ||
           diagnosis->pattern_type == ERROR_PATTERN_UNDEFINED_REFERENCE);
    log_success("Correct error type identified: %d (MISSING_LIBRARY=%d, UNDEFINED_REF=%d)",
                diagnosis->pattern_type, ERROR_PATTERN_MISSING_LIBRARY,
                ERROR_PATTERN_UNDEFINED_REFERENCE);

    assert(diagnosis->confidence > 0.0f);
    log_success("Confidence: %.0f%%", diagnosis->confidence * 100);

    assert(diagnosis->fix_count > 0);
    log_success("Generated %zu suggested fixes", diagnosis->fix_count);

    error_diagnosis_free(diagnosis);
    project_context_free(ctx);
    error_patterns_shutdown();

    log_success("Error diagnosis flow test passed!");
}

int main(void) {
    log_init(LOG_LEVEL_DEBUG);
    log_set_colors(true);

    log_info("========================================");
    log_info("Recovery Integration Test Suite");
    log_info("Phase 2: Error Recovery Integration");
    log_info("========================================");
    log_plain("");

    test_cache_invalidation();
    log_plain("");

    test_cache_dependency_marking();
    log_plain("");

    test_fix_action_types_basic();
    log_plain("");

    test_recovery_with_tools();
    log_plain("");

    test_fix_action_types();
    log_plain("");

    test_error_diagnosis_flow();
    log_plain("");

    log_info("========================================");
    log_success("All integration tests passed!");
    log_info("========================================");

    log_shutdown();
    return 0;
}
