/**
 * @file test_fix_validation.c
 * @brief Tests for Phase 3 Fix Validation System
 *
 * Phase 4: Testing & Quality
 */

#include "test_framework.h"
#include "cyxmake/fix_validation.h"
#include "cyxmake/error_recovery.h"
#include "cyxmake/tool_executor.h"
#include "cyxmake/logger.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
    #include <direct.h>
    #define mkdir(path, mode) _mkdir(path)
#else
    #include <sys/stat.h>
#endif

/* Helper to create mock project context */
static ProjectContext* create_mock_context(void) {
    ProjectContext* ctx = calloc(1, sizeof(ProjectContext));
    if (!ctx) return NULL;
    ctx->root_path = strdup(".");
    ctx->name = strdup("test_project");
    ctx->primary_language = LANG_C;
    ctx->build_system.type = BUILD_CMAKE;
    return ctx;
}

static void free_mock_context(ProjectContext* ctx) {
    if (!ctx) return;
    free(ctx->root_path);
    free(ctx->name);
    free(ctx);
}

/* ========================================================================
 * Fix Validator Tests
 * ======================================================================== */

static TestResult test_fix_validator_create(void) {
    FixValidator* validator = fix_validator_create(NULL);
    TEST_ASSERT_NOT_NULL(validator);

    fix_validator_free(validator);
    TEST_PASS_MSG("Created and freed fix validator (no registry)");

    /* With tool registry */
    ToolRegistry* registry = tool_registry_create();
    TEST_ASSERT_NOT_NULL(registry);

    validator = fix_validator_create(registry);
    TEST_ASSERT_NOT_NULL(validator);

    fix_validator_free(validator);
    tool_registry_free(registry);

    TEST_PASS_MSG("Created and freed fix validator (with registry)");
    return TEST_PASS;
}

static TestResult test_fix_validate_null_action(void) {
    FixValidator* validator = fix_validator_create(NULL);
    TEST_ASSERT_NOT_NULL(validator);

    ValidationResult* result = fix_validate(validator, NULL, NULL);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQ(VALIDATION_FAILED, result->status);
    TEST_ASSERT_FALSE(result->can_proceed);

    validation_result_free(result);
    fix_validator_free(validator);

    TEST_PASS_MSG("Null action validation fails correctly");
    return TEST_PASS;
}

static TestResult test_fix_validate_retry_action(void) {
    FixValidator* validator = fix_validator_create(NULL);
    TEST_ASSERT_NOT_NULL(validator);

    FixAction action = {
        .type = FIX_ACTION_RETRY,
        .description = "Retry build",
        .command = NULL,
        .target = NULL,
        .requires_confirmation = false
    };

    ValidationResult* result = fix_validate(validator, &action, NULL);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQ(VALIDATION_PASSED, result->status);
    TEST_ASSERT_TRUE(result->can_proceed);
    TEST_ASSERT_EQ(1.0, result->confidence);

    validation_result_free(result);
    fix_validator_free(validator);

    TEST_PASS_MSG("Retry action always passes validation");
    return TEST_PASS;
}

static TestResult test_fix_validate_file_action(void) {
    FixValidator* validator = fix_validator_create(NULL);
    TEST_ASSERT_NOT_NULL(validator);

    /* Valid file path (current directory exists) */
    FixAction action = {
        .type = FIX_ACTION_CREATE_FILE,
        .description = "Create file",
        .target = "new_file.txt",
        .requires_confirmation = false
    };

    ValidationResult* result = fix_validate(validator, &action, NULL);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQ(VALIDATION_PASSED, result->status);
    TEST_ASSERT_TRUE(result->can_proceed);

    validation_result_free(result);

    /* Invalid file path (non-existent directory) */
    action.target = "nonexistent_dir/subdir/file.txt";

    result = fix_validate(validator, &action, NULL);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQ(VALIDATION_FAILED, result->status);
    TEST_ASSERT_FALSE(result->can_proceed);

    validation_result_free(result);
    fix_validator_free(validator);

    TEST_PASS_MSG("File path validation works correctly");
    return TEST_PASS;
}

/* ========================================================================
 * Risk Assessment Tests
 * ======================================================================== */

static TestResult test_risk_assess_none(void) {
    FixAction action = {
        .type = FIX_ACTION_RETRY,
        .description = "Retry build"
    };

    RiskAssessment* risk = fix_assess_risk(&action, NULL);
    TEST_ASSERT_NOT_NULL(risk);
    TEST_ASSERT_EQ(RISK_NONE, risk->level);
    TEST_ASSERT_FALSE(risk->requires_confirmation);
    TEST_ASSERT_FALSE(risk->requires_backup);

    risk_assessment_free(risk);
    TEST_PASS_MSG("Retry action has no risk");
    return TEST_PASS;
}

static TestResult test_risk_assess_low(void) {
    FixAction action = {
        .type = FIX_ACTION_SET_ENV_VAR,
        .description = "Set environment variable",
        .target = "PATH",
        .value = "/usr/local/bin"
    };

    RiskAssessment* risk = fix_assess_risk(&action, NULL);
    TEST_ASSERT_NOT_NULL(risk);
    TEST_ASSERT_EQ(RISK_LOW, risk->level);
    TEST_ASSERT_TRUE(risk->is_reversible);

    risk_assessment_free(risk);
    TEST_PASS_MSG("Environment variable has low risk");
    return TEST_PASS;
}

static TestResult test_risk_assess_medium(void) {
    FixAction action = {
        .type = FIX_ACTION_MODIFY_FILE,
        .description = "Modify CMakeLists.txt",
        .target = "CMakeLists.txt"
    };

    RiskAssessment* risk = fix_assess_risk(&action, NULL);
    TEST_ASSERT_NOT_NULL(risk);
    TEST_ASSERT_EQ(RISK_MEDIUM, risk->level);
    TEST_ASSERT_TRUE(risk->requires_backup);
    TEST_ASSERT_TRUE(risk->requires_confirmation);
    TEST_ASSERT_TRUE(risk->is_reversible);
    TEST_ASSERT_EQ(1, risk->affected_count);

    risk_assessment_free(risk);
    TEST_PASS_MSG("File modification has medium risk");
    return TEST_PASS;
}

static TestResult test_risk_assess_high(void) {
    FixAction action = {
        .type = FIX_ACTION_INSTALL_PACKAGE,
        .description = "Install SDL2",
        .target = "sdl2"
    };

    RiskAssessment* risk = fix_assess_risk(&action, NULL);
    TEST_ASSERT_NOT_NULL(risk);
    TEST_ASSERT_EQ(RISK_HIGH, risk->level);
    TEST_ASSERT_TRUE(risk->requires_confirmation);

    risk_assessment_free(risk);
    TEST_PASS_MSG("Package installation has high risk");
    return TEST_PASS;
}

static TestResult test_risk_assess_critical(void) {
    FixAction action = {
        .type = FIX_ACTION_RUN_COMMAND,
        .description = "Run privileged command",
        .command = "sudo rm -rf /tmp/build"
    };

    RiskAssessment* risk = fix_assess_risk(&action, NULL);
    TEST_ASSERT_NOT_NULL(risk);
    TEST_ASSERT_EQ(RISK_CRITICAL, risk->level);
    TEST_ASSERT_TRUE(risk->requires_confirmation);

    risk_assessment_free(risk);
    TEST_PASS_MSG("Sudo command has critical risk");
    return TEST_PASS;
}

/* ========================================================================
 * Incremental Fix Session Tests
 * ======================================================================== */

static TestResult test_incremental_session_create(void) {
    ProjectContext* ctx = create_mock_context();
    TEST_ASSERT_NOT_NULL(ctx);

    IncrementalFixSession* session = incremental_fix_session_create(
        ctx, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(session);

    incremental_fix_session_free(session);
    free_mock_context(ctx);

    TEST_PASS_MSG("Created and freed incremental fix session");
    return TEST_PASS;
}

static TestResult test_incremental_session_results(void) {
    ProjectContext* ctx = create_mock_context();
    TEST_ASSERT_NOT_NULL(ctx);

    IncrementalFixSession* session = incremental_fix_session_create(
        ctx, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(session);

    /* Initially no results */
    size_t count = 0;
    const FixApplicationResult* results = incremental_fix_get_results(session, &count);
    TEST_ASSERT_EQ(0, count);

    incremental_fix_session_free(session);
    free_mock_context(ctx);

    TEST_PASS_MSG("Initial session has no results");
    return TEST_PASS;
}

/* ========================================================================
 * Fix History Tests
 * ======================================================================== */

static TestResult test_fix_history_create(void) {
    /* Use temp path for test history */
    FixHistory* history = fix_history_create("test_fix_history.json");
    TEST_ASSERT_NOT_NULL(history);

    fix_history_free(history);

    /* Cleanup */
    remove("test_fix_history.json");

    TEST_PASS_MSG("Created and freed fix history");
    return TEST_PASS;
}

static TestResult test_fix_history_record(void) {
    FixHistory* history = fix_history_create("test_fix_history2.json");
    TEST_ASSERT_NOT_NULL(history);

    /* Create mock diagnosis */
    ErrorDiagnosis diagnosis = {
        .pattern_type = ERROR_PATTERN_MISSING_LIBRARY,
        .error_message = "cannot find -lSDL2",
        .diagnosis = "Missing SDL2 library",
        .confidence = 0.9
    };

    /* Create mock fix action */
    FixAction action = {
        .type = FIX_ACTION_INSTALL_PACKAGE,
        .description = "Install SDL2",
        .target = "sdl2"
    };

    /* Record successful fix */
    fix_history_record(history, &diagnosis, &action, true, 1500.0);

    /* Verify stats */
    int total, successful, unique;
    fix_history_stats(history, &total, &successful, &unique);
    TEST_ASSERT_EQ(1, total);
    TEST_ASSERT_EQ(1, successful);
    TEST_ASSERT_EQ(1, unique);

    /* Record another fix for same error */
    fix_history_record(history, &diagnosis, &action, true, 1200.0);

    fix_history_stats(history, &total, &successful, &unique);
    TEST_ASSERT_EQ(2, total);
    TEST_ASSERT_EQ(2, successful);
    TEST_ASSERT_EQ(1, unique);  /* Same error type */

    fix_history_free(history);
    remove("test_fix_history2.json");

    TEST_PASS_MSG("Fix history recording works");
    return TEST_PASS;
}

static TestResult test_fix_history_save_load(void) {
    const char* test_path = "test_fix_history3.json";

    /* Create and populate history */
    FixHistory* history = fix_history_create(test_path);
    TEST_ASSERT_NOT_NULL(history);

    ErrorDiagnosis diagnosis = {
        .pattern_type = ERROR_PATTERN_CMAKE_PACKAGE,
        .error_message = "Could not find SDL2",
        .diagnosis = "CMake package not found"
    };

    FixAction action = {
        .type = FIX_ACTION_INSTALL_PACKAGE,
        .target = "sdl2"
    };

    fix_history_record(history, &diagnosis, &action, true, 1000.0);

    /* Save explicitly */
    bool saved = fix_history_save(history);
    TEST_ASSERT_TRUE(saved);

    fix_history_free(history);

    /* Reload and verify */
    history = fix_history_create(test_path);
    TEST_ASSERT_NOT_NULL(history);

    int total, successful, unique;
    fix_history_stats(history, &total, &successful, &unique);
    TEST_ASSERT_EQ(1, total);
    TEST_ASSERT_EQ(1, unique);

    fix_history_free(history);
    remove(test_path);

    TEST_PASS_MSG("Fix history save/load works");
    return TEST_PASS;
}

static TestResult test_fix_history_suggest(void) {
    FixHistory* history = fix_history_create("test_fix_history4.json");
    TEST_ASSERT_NOT_NULL(history);

    /* Record some successful fixes */
    ErrorDiagnosis diagnosis = {
        .pattern_type = ERROR_PATTERN_MISSING_LIBRARY,
        .error_message = "undefined reference to SDL_Init"
    };

    FixAction action = {
        .type = FIX_ACTION_INSTALL_PACKAGE,
        .description = "Install SDL2",
        .target = "sdl2"
    };

    /* Record multiple successes to build confidence */
    for (int i = 0; i < 5; i++) {
        fix_history_record(history, &diagnosis, &action, true, 1000.0);
    }

    /* Now request suggestion */
    FixAction* suggested = fix_history_suggest(history, &diagnosis);
    TEST_ASSERT_NOT_NULL(suggested);
    TEST_ASSERT_EQ(FIX_ACTION_INSTALL_PACKAGE, suggested->type);

    /* Free suggested action */
    free((void*)suggested->description);
    free(suggested->command);
    free(suggested->target);
    free(suggested);

    fix_history_free(history);
    remove("test_fix_history4.json");

    TEST_PASS_MSG("Fix history suggestion works");
    return TEST_PASS;
}

/* ========================================================================
 * Enhanced Recovery Options Tests
 * ======================================================================== */

static TestResult test_enhanced_recovery_defaults(void) {
    EnhancedRecoveryOptions opts = enhanced_recovery_defaults();

    TEST_ASSERT_TRUE(opts.validate_before_apply);
    TEST_ASSERT_TRUE(opts.verify_after_apply);
    TEST_ASSERT_TRUE(opts.incremental_apply);
    TEST_ASSERT_TRUE(opts.use_history);
    TEST_ASSERT_TRUE(opts.record_history);
    TEST_ASSERT_TRUE(opts.auto_rollback);
    TEST_ASSERT_EQ(RISK_LOW, opts.max_auto_risk);

    TEST_PASS_MSG("Enhanced recovery defaults are correct");
    return TEST_PASS;
}

/* ========================================================================
 * Benchmarks
 * ======================================================================== */

static FixValidator* g_bench_validator = NULL;
static FixAction g_bench_action;

static void bench_validate_setup(void) {
    g_bench_validator = fix_validator_create(NULL);
    g_bench_action.type = FIX_ACTION_RETRY;
    g_bench_action.description = "Test";
}

static void bench_validate(void) {
    ValidationResult* result = fix_validate(g_bench_validator, &g_bench_action, NULL);
    validation_result_free(result);
}

static TestResult test_benchmark_validation(void) {
    bench_validate_setup();

    BenchmarkResult br = test_benchmark("Fix Validation", bench_validate, 10000);
    test_benchmark_print(&br);

    TEST_ASSERT_TRUE(br.ops_per_sec > 1000);  /* Should be very fast */

    fix_validator_free(g_bench_validator);
    TEST_PASS_MSG("Validation benchmark: %.0f ops/sec", br.ops_per_sec);
    return TEST_PASS;
}

/* ========================================================================
 * Main Test Runner
 * ======================================================================== */

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    /* Initialize logger with defaults */
    log_init(NULL);

    /* Initialize error patterns (needed by some tests) */
    error_patterns_init();

    TestCase tests[] = {
        /* Fix Validator Tests */
        TEST_CASE(test_fix_validator_create),
        TEST_CASE(test_fix_validate_null_action),
        TEST_CASE(test_fix_validate_retry_action),
        TEST_CASE(test_fix_validate_file_action),

        /* Risk Assessment Tests */
        TEST_CASE(test_risk_assess_none),
        TEST_CASE(test_risk_assess_low),
        TEST_CASE(test_risk_assess_medium),
        TEST_CASE(test_risk_assess_high),
        TEST_CASE(test_risk_assess_critical),

        /* Incremental Fix Session Tests */
        TEST_CASE(test_incremental_session_create),
        TEST_CASE(test_incremental_session_results),

        /* Fix History Tests */
        TEST_CASE(test_fix_history_create),
        TEST_CASE(test_fix_history_record),
        TEST_CASE(test_fix_history_save_load),
        TEST_CASE(test_fix_history_suggest),

        /* Enhanced Recovery Tests */
        TEST_CASE(test_enhanced_recovery_defaults),

        /* Benchmarks */
        TEST_CASE(test_benchmark_validation),
    };

    test_suite_init("Fix Validation Test Suite (Phase 3)");
    int failures = test_suite_run(tests, sizeof(tests) / sizeof(tests[0]));

    test_memory_report();
    error_patterns_shutdown();
    log_shutdown();

    return failures;
}
