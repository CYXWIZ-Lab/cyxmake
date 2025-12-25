/**
 * @file test_framework.h
 * @brief Lightweight test framework with assertions, benchmarking, and reporting
 *
 * Phase 4: Testing & Quality
 */

#ifndef CYXMAKE_TEST_FRAMEWORK_H
#define CYXMAKE_TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #define TEST_SLEEP_MS(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define TEST_SLEEP_MS(ms) usleep((ms) * 1000)
#endif

/* ========================================================================
 * Test Framework Configuration
 * ======================================================================== */

/* ANSI color codes */
#define TEST_COLOR_RESET   "\033[0m"
#define TEST_COLOR_RED     "\033[31m"
#define TEST_COLOR_GREEN   "\033[32m"
#define TEST_COLOR_YELLOW  "\033[33m"
#define TEST_COLOR_BLUE    "\033[34m"
#define TEST_COLOR_CYAN    "\033[36m"

/* Test result codes */
typedef enum {
    TEST_PASS = 0,
    TEST_FAIL = 1,
    TEST_SKIP = 2,
    TEST_ERROR = 3
} TestResult;

/* Test statistics */
typedef struct {
    int total;
    int passed;
    int failed;
    int skipped;
    int errors;
    double total_time_ms;
} TestStats;

/* Benchmark result */
typedef struct {
    const char* name;
    int iterations;
    double total_ms;
    double avg_ms;
    double min_ms;
    double max_ms;
    double ops_per_sec;
} BenchmarkResult;

/* Global test state */
static TestStats g_test_stats = {0};
static bool g_test_colors = true;
static int g_test_verbose = 1;

/* ========================================================================
 * Timer Utilities
 * ======================================================================== */

#ifdef _WIN32
static double test_get_time_ms(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart * 1000.0 / (double)freq.QuadPart;
}
#else
static double test_get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}
#endif

/* ========================================================================
 * Output Helpers
 * ======================================================================== */

static void test_print_color(const char* color, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    if (g_test_colors) {
        printf("%s", color);
    }
    vprintf(fmt, args);
    if (g_test_colors) {
        printf("%s", TEST_COLOR_RESET);
    }

    va_end(args);
}

#define TEST_LOG(fmt, ...) \
    do { if (g_test_verbose >= 1) printf(fmt "\n", ##__VA_ARGS__); } while(0)

#define TEST_DEBUG(fmt, ...) \
    do { if (g_test_verbose >= 2) printf("  [DEBUG] " fmt "\n", ##__VA_ARGS__); } while(0)

#define TEST_PASS_MSG(fmt, ...) \
    test_print_color(TEST_COLOR_GREEN, "  [PASS] " fmt "\n", ##__VA_ARGS__)

#define TEST_FAIL_MSG(fmt, ...) \
    test_print_color(TEST_COLOR_RED, "  [FAIL] " fmt "\n", ##__VA_ARGS__)

#define TEST_SKIP_MSG(fmt, ...) \
    test_print_color(TEST_COLOR_YELLOW, "  [SKIP] " fmt "\n", ##__VA_ARGS__)

#define TEST_INFO(fmt, ...) \
    test_print_color(TEST_COLOR_CYAN, "  [INFO] " fmt "\n", ##__VA_ARGS__)

/* ========================================================================
 * Assertion Macros
 * ======================================================================== */

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            TEST_FAIL_MSG("Assertion failed: %s (line %d)", #condition, __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

#define TEST_ASSERT_MSG(condition, msg) \
    do { \
        if (!(condition)) { \
            TEST_FAIL_MSG("%s: %s (line %d)", msg, #condition, __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            TEST_FAIL_MSG("Expected %d, got %d (line %d)", \
                          (int)(expected), (int)(actual), __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

#define TEST_ASSERT_STR_EQ(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            TEST_FAIL_MSG("Expected \"%s\", got \"%s\" (line %d)", \
                          (expected), (actual), __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

#define TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            TEST_FAIL_MSG("Expected non-NULL: %s (line %d)", #ptr, __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

#define TEST_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            TEST_FAIL_MSG("Expected NULL: %s (line %d)", #ptr, __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

#define TEST_ASSERT_TRUE(condition) TEST_ASSERT(condition)
#define TEST_ASSERT_FALSE(condition) TEST_ASSERT(!(condition))

/* ========================================================================
 * Test Suite Management
 * ======================================================================== */

typedef TestResult (*TestFunc)(void);

typedef struct {
    const char* name;
    TestFunc func;
    bool skip;
    const char* skip_reason;
} TestCase;

#define TEST_CASE(name) {#name, name, false, NULL}
#define TEST_CASE_SKIP(name, reason) {#name, name, true, reason}

static void test_suite_init(const char* suite_name) {
    memset(&g_test_stats, 0, sizeof(g_test_stats));
    printf("\n");
    test_print_color(TEST_COLOR_BLUE, "========================================\n");
    test_print_color(TEST_COLOR_BLUE, " %s\n", suite_name);
    test_print_color(TEST_COLOR_BLUE, "========================================\n");
}

static TestResult test_run_single(const TestCase* tc) {
    g_test_stats.total++;

    if (tc->skip) {
        g_test_stats.skipped++;
        TEST_SKIP_MSG("%s: %s", tc->name, tc->skip_reason ? tc->skip_reason : "skipped");
        return TEST_SKIP;
    }

    printf("\n");
    test_print_color(TEST_COLOR_CYAN, "Running: %s\n", tc->name);

    double start = test_get_time_ms();
    TestResult result = tc->func();
    double elapsed = test_get_time_ms() - start;

    g_test_stats.total_time_ms += elapsed;

    switch (result) {
        case TEST_PASS:
            g_test_stats.passed++;
            TEST_PASS_MSG("%s (%.2f ms)", tc->name, elapsed);
            break;
        case TEST_FAIL:
            g_test_stats.failed++;
            TEST_FAIL_MSG("%s (%.2f ms)", tc->name, elapsed);
            break;
        case TEST_SKIP:
            g_test_stats.skipped++;
            TEST_SKIP_MSG("%s", tc->name);
            break;
        case TEST_ERROR:
            g_test_stats.errors++;
            TEST_FAIL_MSG("%s - ERROR (%.2f ms)", tc->name, elapsed);
            break;
    }

    return result;
}

static int test_suite_run(TestCase* tests, size_t count) {
    for (size_t i = 0; i < count; i++) {
        test_run_single(&tests[i]);
    }

    printf("\n");
    test_print_color(TEST_COLOR_BLUE, "========================================\n");
    test_print_color(TEST_COLOR_BLUE, " Results: ");

    if (g_test_stats.failed == 0 && g_test_stats.errors == 0) {
        test_print_color(TEST_COLOR_GREEN, "ALL PASSED");
    } else {
        test_print_color(TEST_COLOR_RED, "FAILURES");
    }
    printf("\n");

    printf(" Total:   %d\n", g_test_stats.total);
    test_print_color(TEST_COLOR_GREEN, " Passed:  %d\n", g_test_stats.passed);
    if (g_test_stats.failed > 0) {
        test_print_color(TEST_COLOR_RED, " Failed:  %d\n", g_test_stats.failed);
    }
    if (g_test_stats.skipped > 0) {
        test_print_color(TEST_COLOR_YELLOW, " Skipped: %d\n", g_test_stats.skipped);
    }
    if (g_test_stats.errors > 0) {
        test_print_color(TEST_COLOR_RED, " Errors:  %d\n", g_test_stats.errors);
    }
    printf(" Time:    %.2f ms\n", g_test_stats.total_time_ms);
    test_print_color(TEST_COLOR_BLUE, "========================================\n");

    return (g_test_stats.failed + g_test_stats.errors);
}

/* ========================================================================
 * Benchmarking
 * ======================================================================== */

typedef void (*BenchmarkFunc)(void);

static BenchmarkResult test_benchmark(const char* name, BenchmarkFunc func, int iterations) {
    BenchmarkResult result = {0};
    result.name = name;
    result.iterations = iterations;
    result.min_ms = 1e9;
    result.max_ms = 0;

    /* Warm-up run */
    func();

    double total = 0;
    for (int i = 0; i < iterations; i++) {
        double start = test_get_time_ms();
        func();
        double elapsed = test_get_time_ms() - start;

        total += elapsed;
        if (elapsed < result.min_ms) result.min_ms = elapsed;
        if (elapsed > result.max_ms) result.max_ms = elapsed;
    }

    result.total_ms = total;
    result.avg_ms = total / iterations;
    result.ops_per_sec = (iterations * 1000.0) / total;

    return result;
}

static void test_benchmark_print(const BenchmarkResult* result) {
    test_print_color(TEST_COLOR_CYAN, "\nBenchmark: %s\n", result->name);
    printf("  Iterations: %d\n", result->iterations);
    printf("  Total time: %.2f ms\n", result->total_ms);
    printf("  Average:    %.4f ms\n", result->avg_ms);
    printf("  Min:        %.4f ms\n", result->min_ms);
    printf("  Max:        %.4f ms\n", result->max_ms);
    printf("  Ops/sec:    %.2f\n", result->ops_per_sec);
}

#define BENCHMARK(name, func, iters) \
    do { \
        BenchmarkResult br = test_benchmark(name, func, iters); \
        test_benchmark_print(&br); \
    } while(0)

/* ========================================================================
 * Memory Tracking (for leak detection)
 * ======================================================================== */

#ifdef CYXMAKE_TEST_TRACK_MEMORY

static size_t g_alloc_count = 0;
static size_t g_free_count = 0;
static size_t g_total_allocated = 0;

static void* test_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr) {
        g_alloc_count++;
        g_total_allocated += size;
    }
    return ptr;
}

static void test_free(void* ptr) {
    if (ptr) {
        g_free_count++;
    }
    free(ptr);
}

#define malloc test_malloc
#define free test_free

static void test_memory_report(void) {
    printf("\n");
    test_print_color(TEST_COLOR_BLUE, "Memory Report:\n");
    printf("  Allocations: %zu\n", g_alloc_count);
    printf("  Frees:       %zu\n", g_free_count);
    printf("  Total:       %zu bytes\n", g_total_allocated);

    if (g_alloc_count != g_free_count) {
        test_print_color(TEST_COLOR_RED, "  WARNING: Potential memory leak! (%zu unfreed)\n",
                         g_alloc_count - g_free_count);
    } else {
        test_print_color(TEST_COLOR_GREEN, "  No leaks detected\n");
    }
}

#else

static void test_memory_report(void) {
    /* No-op when memory tracking is disabled */
}

#endif /* CYXMAKE_TEST_TRACK_MEMORY */

#endif /* CYXMAKE_TEST_FRAMEWORK_H */
