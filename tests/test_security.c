/**
 * @file test_security.c
 * @brief Tests for Phase 2 Security System (audit, dry-run, rollback)
 *
 * Phase 4: Testing & Quality
 */

#include "test_framework.h"
#include "cyxmake/security.h"
#include "cyxmake/permission.h"
#include "cyxmake/logger.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
    #include <direct.h>
    #include <io.h>
    #define mkdir(path, mode) _mkdir(path)
    #define access _access
    #define F_OK 0
    #define unlink _unlink
    #define rmdir _rmdir
#else
    #include <unistd.h>
    #include <sys/stat.h>
#endif

/* ========================================================================
 * Audit Logger Tests
 * ======================================================================== */

static TestResult test_audit_logger_create(void) {
    /* Test default creation */
    AuditLogger* logger = audit_logger_create_default();
    TEST_ASSERT_NOT_NULL(logger);

    audit_logger_free(logger);
    TEST_PASS_MSG("Created and freed default audit logger");

    /* Test custom config */
    AuditConfig config = {
        .enabled = true,
        .log_file = NULL,
        .log_to_console = false,
        .min_severity = AUDIT_INFO,
        .include_timestamps = true,
        .include_user = false,
        .max_entries = 100,
        .rotation_size_mb = 0
    };

    logger = audit_logger_create(&config);
    TEST_ASSERT_NOT_NULL(logger);

    audit_logger_free(logger);
    TEST_PASS_MSG("Created and freed custom audit logger");

    return TEST_PASS;
}

static TestResult test_audit_log_action(void) {
    AuditLogger* logger = audit_logger_create_default();
    TEST_ASSERT_NOT_NULL(logger);

    /* Log various actions */
    audit_log_action(logger, AUDIT_INFO, ACTION_RUN_COMMAND,
                     "test_target", "Test command execution", true);

    audit_log_action(logger, AUDIT_WARNING, ACTION_MODIFY_FILE,
                     "test.txt", "Test file modification", true);

    audit_log_action(logger, AUDIT_DENIED, ACTION_INSTALL_PKG,
                     "test-package", "Package installation denied", false);

    TEST_PASS_MSG("Logged multiple actions");

    audit_logger_free(logger);
    return TEST_PASS;
}

static TestResult test_audit_log_command(void) {
    AuditLogger* logger = audit_logger_create_default();
    TEST_ASSERT_NOT_NULL(logger);

    audit_log_command(logger, "cmake", "--build .", 0, true);
    audit_log_command(logger, "make", "-j4", 2, false);

    TEST_PASS_MSG("Logged command executions");

    audit_logger_free(logger);
    return TEST_PASS;
}

static TestResult test_audit_severity_name(void) {
    TEST_ASSERT_STR_EQ("DEBUG", audit_severity_name(AUDIT_DEBUG));
    TEST_ASSERT_STR_EQ("INFO", audit_severity_name(AUDIT_INFO));
    TEST_ASSERT_STR_EQ("WARNING", audit_severity_name(AUDIT_WARNING));
    TEST_ASSERT_STR_EQ("ACTION", audit_severity_name(AUDIT_ACTION));
    TEST_ASSERT_STR_EQ("DENIED", audit_severity_name(AUDIT_DENIED));
    TEST_ASSERT_STR_EQ("ERROR", audit_severity_name(AUDIT_ERROR));
    TEST_ASSERT_STR_EQ("SECURITY", audit_severity_name(AUDIT_SECURITY));

    TEST_PASS_MSG("All severity names correct");
    return TEST_PASS;
}

/* ========================================================================
 * Dry-Run Tests
 * ======================================================================== */

static TestResult test_dry_run_create(void) {
    DryRunContext* ctx = dry_run_create();
    TEST_ASSERT_NOT_NULL(ctx);

    dry_run_free(ctx);
    TEST_PASS_MSG("Created and freed dry-run context");
    return TEST_PASS;
}

static TestResult test_dry_run_enable_disable(void) {
    DryRunContext* ctx = dry_run_create();
    TEST_ASSERT_NOT_NULL(ctx);

    /* Initially disabled */
    TEST_ASSERT_FALSE(dry_run_is_enabled(ctx));

    /* Enable */
    dry_run_set_enabled(ctx, true);
    TEST_ASSERT_TRUE(dry_run_is_enabled(ctx));

    /* Disable */
    dry_run_set_enabled(ctx, false);
    TEST_ASSERT_FALSE(dry_run_is_enabled(ctx));

    TEST_PASS_MSG("Enable/disable works correctly");

    dry_run_free(ctx);
    return TEST_PASS;
}

static TestResult test_dry_run_record(void) {
    DryRunContext* ctx = dry_run_create();
    TEST_ASSERT_NOT_NULL(ctx);

    dry_run_set_enabled(ctx, true);

    /* Record file operations */
    dry_run_record_file(ctx, ACTION_CREATE_FILE, "test_file.txt",
                        "Create test file");
    dry_run_record_file(ctx, ACTION_MODIFY_FILE, "config.toml",
                        "Modify configuration");

    /* Record command */
    dry_run_record_command(ctx, "cmake --build .", "Build project");

    /* Get recorded actions */
    int count = 0;
    const DryRunAction** actions = dry_run_get_actions(ctx, &count);
    TEST_ASSERT_EQ(3, count);
    TEST_ASSERT_NOT_NULL(actions);

    TEST_PASS_MSG("Recorded %d dry-run actions", count);

    dry_run_free(ctx);
    return TEST_PASS;
}

static TestResult test_dry_run_clear(void) {
    DryRunContext* ctx = dry_run_create();
    TEST_ASSERT_NOT_NULL(ctx);

    dry_run_set_enabled(ctx, true);
    dry_run_record_file(ctx, ACTION_CREATE_FILE, "test.txt", "Test");

    int count = 0;
    dry_run_get_actions(ctx, &count);
    TEST_ASSERT_EQ(1, count);

    /* Clear actions */
    dry_run_clear(ctx);

    dry_run_get_actions(ctx, &count);
    TEST_ASSERT_EQ(0, count);

    TEST_PASS_MSG("Clear works correctly");

    dry_run_free(ctx);
    return TEST_PASS;
}

/* ========================================================================
 * Rollback Tests
 * ======================================================================== */

static TestResult test_rollback_create(void) {
    RollbackManager* mgr = rollback_create_default();
    TEST_ASSERT_NOT_NULL(mgr);

    rollback_free(mgr);
    TEST_PASS_MSG("Created and freed default rollback manager");

    /* Test custom config */
    RollbackConfig config = {
        .enabled = true,
        .backup_dir = NULL,
        .max_entries = 50,
        .max_file_size = 512 * 1024,  /* 512KB */
        .backup_large_files = true,
        .retention_hours = 24
    };

    mgr = rollback_create(&config);
    TEST_ASSERT_NOT_NULL(mgr);

    rollback_free(mgr);
    TEST_PASS_MSG("Created and freed custom rollback manager");

    return TEST_PASS;
}

static TestResult test_rollback_is_enabled(void) {
    RollbackConfig config = {
        .enabled = true,
        .backup_dir = NULL,
        .max_entries = 10,
        .max_file_size = 100 * 1024,
        .backup_large_files = false,
        .retention_hours = 0
    };

    RollbackManager* mgr = rollback_create(&config);
    TEST_ASSERT_NOT_NULL(mgr);

    TEST_ASSERT_TRUE(rollback_is_enabled(mgr));

    rollback_free(mgr);
    return TEST_PASS;
}

static TestResult test_rollback_backup_file(void) {
    RollbackManager* mgr = rollback_create_default();
    TEST_ASSERT_NOT_NULL(mgr);

    /* Create a test file */
    const char* test_file = "test_rollback_file.txt";
    FILE* f = fopen(test_file, "w");
    if (f) {
        fprintf(f, "Original content\n");
        fclose(f);
    } else {
        rollback_free(mgr);
        TEST_SKIP_MSG("Could not create test file");
        return TEST_SKIP;
    }

    /* Backup the file */
    bool backed_up = rollback_backup_file(mgr, test_file, ROLLBACK_FILE_MODIFY);
    TEST_ASSERT_TRUE(backed_up);
    TEST_PASS_MSG("Backed up test file");

    /* Modify the file */
    f = fopen(test_file, "w");
    if (f) {
        fprintf(f, "Modified content\n");
        fclose(f);
    }

    /* Rollback */
    int rolled_back = rollback_last(mgr, 1);
    TEST_ASSERT_EQ(1, rolled_back);
    TEST_PASS_MSG("Rolled back 1 file");

    /* Verify content is restored */
    f = fopen(test_file, "r");
    if (f) {
        char buf[64];
        if (fgets(buf, sizeof(buf), f)) {
            TEST_ASSERT_STR_EQ("Original content\n", buf);
        }
        fclose(f);
    }

    /* Cleanup */
    unlink(test_file);
    rollback_free(mgr);

    TEST_PASS_MSG("Rollback restored original content");
    return TEST_PASS;
}

static TestResult test_rollback_record_create(void) {
    RollbackManager* mgr = rollback_create_default();
    TEST_ASSERT_NOT_NULL(mgr);

    /* Record a file creation (for rollback = delete) */
    const char* test_file = "test_rollback_create.txt";

    bool recorded = rollback_record_create(mgr, test_file);
    TEST_ASSERT_TRUE(recorded);

    /* Create the file */
    FILE* f = fopen(test_file, "w");
    if (f) {
        fprintf(f, "New file content\n");
        fclose(f);
    }

    /* Verify file exists */
    TEST_ASSERT_EQ(0, access(test_file, F_OK));

    /* Rollback should delete the file */
    int rolled_back = rollback_last(mgr, 1);
    TEST_ASSERT_EQ(1, rolled_back);

    /* Verify file is deleted */
    TEST_ASSERT_EQ(-1, access(test_file, F_OK));

    rollback_free(mgr);
    TEST_PASS_MSG("Record create and rollback (delete) works");
    return TEST_PASS;
}

/* ========================================================================
 * Security Context Tests
 * ======================================================================== */

static TestResult test_security_context_create(void) {
    SecurityConfig config = {
        .enable_permissions = true,
        .enable_audit = true,
        .enable_dry_run = false,
        .enable_rollback = true,
        .audit_config = {
            .enabled = true,
            .log_file = NULL,
            .log_to_console = false,
            .min_severity = AUDIT_INFO
        },
        .rollback_config = {
            .enabled = true,
            .max_entries = 100
        }
    };

    SecurityContext* ctx = security_context_create(&config);
    TEST_ASSERT_NOT_NULL(ctx);

    /* Verify enabled components exist */
    TEST_ASSERT_NOT_NULL(ctx->audit);       /* enable_audit = true */
    TEST_ASSERT_NOT_NULL(ctx->rollback);    /* enable_rollback = true */
    /* dry_run is NULL when enable_dry_run = false */

    security_context_free(ctx);
    TEST_PASS_MSG("Created and freed security context with all components");
    return TEST_PASS;
}

static TestResult test_security_context_default(void) {
    SecurityContext* ctx = security_context_create_default();
    TEST_ASSERT_NOT_NULL(ctx);

    security_context_free(ctx);
    TEST_PASS_MSG("Created and freed default security context");
    return TEST_PASS;
}

/* ========================================================================
 * Permission Tests
 * ======================================================================== */

static TestResult test_permission_level(void) {
    /* Test permission level for different actions */
    PermissionLevel level;

    level = permission_get_level(ACTION_RUN_COMMAND);
    TEST_ASSERT_EQ(PERM_ASK, level);

    level = permission_get_level(ACTION_CREATE_FILE);
    TEST_ASSERT_EQ(PERM_ASK, level);

    level = permission_get_level(ACTION_INSTALL_PKG);
    TEST_ASSERT_EQ(PERM_ASK, level);

    level = permission_get_level(ACTION_DELETE_DIR);
    TEST_ASSERT_EQ(PERM_DANGEROUS, level);

    TEST_PASS_MSG("Permission levels correct for all action types");
    return TEST_PASS;
}

static TestResult test_permission_context_create(void) {
    PermissionContext* ctx = permission_context_create();
    TEST_ASSERT_NOT_NULL(ctx);

    permission_context_free(ctx);
    TEST_PASS_MSG("Created and freed permission context");
    return TEST_PASS;
}

static TestResult test_blocked_path_check(void) {
    PermissionContext* ctx = permission_context_create();
    TEST_ASSERT_NOT_NULL(ctx);

    /* Block some paths */
    permission_block_path(ctx, "/etc");
    permission_block_path(ctx, "C:\\Windows");

    /* Test that blocked paths are detected */
#ifdef _WIN32
    TEST_ASSERT_TRUE(permission_is_blocked(ctx, "C:\\Windows\\System32\\file.exe"));
    TEST_ASSERT_FALSE(permission_is_blocked(ctx, "C:\\Users\\test\\project\\file.c"));
#else
    TEST_ASSERT_TRUE(permission_is_blocked(ctx, "/etc/passwd"));
    TEST_ASSERT_FALSE(permission_is_blocked(ctx, "/home/user/project/file.c"));
#endif

    permission_context_free(ctx);
    TEST_PASS_MSG("Blocked path detection works");
    return TEST_PASS;
}

/* ========================================================================
 * Main Test Runner
 * ======================================================================== */

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    /* Initialize logger with defaults */
    log_init(NULL);  /* Use default config */

    TestCase tests[] = {
        /* Audit Logger Tests */
        TEST_CASE(test_audit_logger_create),
        TEST_CASE(test_audit_log_action),
        TEST_CASE(test_audit_log_command),
        TEST_CASE(test_audit_severity_name),

        /* Dry-Run Tests */
        TEST_CASE(test_dry_run_create),
        TEST_CASE(test_dry_run_enable_disable),
        TEST_CASE(test_dry_run_record),
        TEST_CASE(test_dry_run_clear),

        /* Rollback Tests */
        TEST_CASE(test_rollback_create),
        TEST_CASE(test_rollback_is_enabled),
        TEST_CASE(test_rollback_backup_file),
        TEST_CASE(test_rollback_record_create),

        /* Security Context Tests */
        TEST_CASE(test_security_context_create),
        TEST_CASE(test_security_context_default),

        /* Permission Tests */
        TEST_CASE(test_permission_level),
        TEST_CASE(test_permission_context_create),
        TEST_CASE(test_blocked_path_check),
    };

    test_suite_init("Security System Test Suite (Phase 2)");
    int failures = test_suite_run(tests, sizeof(tests) / sizeof(tests[0]));

    test_memory_report();
    log_shutdown();

    return failures;
}
