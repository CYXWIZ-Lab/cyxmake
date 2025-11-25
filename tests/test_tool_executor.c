/**
 * @file test_tool_executor.c
 * @brief Test tool executor system
 */

#include "cyxmake/tool_executor.h"
#include "cyxmake/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test tool discovery */
static void test_tool_discovery(void) {
    log_info("Testing tool discovery...");

    /* Create registry */
    ToolRegistry* registry = tool_registry_create();
    assert(registry != NULL);
    log_success("Tool registry created");

    /* Discover all tools */
    int count = tool_discover_all(registry);
    log_success("Discovered %d tools", count);
    assert(count > 0);  /* Should find at least some tools */

    /* Get all tools */
    size_t total_count = 0;
    const ToolInfo** tools = tool_registry_get_all(registry, &total_count);
    assert(tools != NULL || total_count == 0);
    log_success("Registry contains %zu tools", total_count);

    /* Display discovered tools */
    log_info("\nDiscovered Tools:");
    log_info("================");
    for (size_t i = 0; i < total_count; i++) {
        if (tools[i]->is_available) {
            log_info("%s (%s)",
                    tools[i]->display_name,
                    tool_type_to_string(tools[i]->type));
            log_info("  Path: %s", tools[i]->path);
            if (tools[i]->version) {
                log_info("  Version: %s", tools[i]->version);
            }
        }
    }

    tool_registry_free(registry);
    log_success("Tool discovery tests passed!\n");
}

/* Test finding specific tools */
static void test_find_tools(void) {
    log_info("Testing tool finding...");

    ToolRegistry* registry = tool_registry_create();
    assert(registry != NULL);

    tool_discover_all(registry);

    /* Test finding by name */
    const ToolInfo* cmake = tool_registry_find(registry, "cmake");
    if (cmake) {
        log_success("Found CMake: %s", cmake->path);
        assert(cmake->type == TOOL_TYPE_BUILD_SYSTEM);
    } else {
        log_warning("CMake not found on system");
    }

    const ToolInfo* git = tool_registry_find(registry, "git");
    if (git) {
        log_success("Found Git: %s", git->path);
        assert(git->type == TOOL_TYPE_VERSION_CONTROL);
    } else {
        log_warning("Git not found on system");
    }

    /* Test finding by type */
    size_t compiler_count = 0;
    const ToolInfo** compilers = tool_registry_find_by_type(
        registry, TOOL_TYPE_COMPILER, &compiler_count);

    if (compilers && compiler_count > 0) {
        log_success("Found %zu compiler(s):", compiler_count);
        for (size_t i = 0; i < compiler_count; i++) {
            if (compilers[i]->is_available) {
                log_info("  - %s", compilers[i]->name);
            }
        }
        free(compilers);
    }

    size_t pkg_mgr_count = 0;
    const ToolInfo** pkg_mgrs = tool_registry_find_by_type(
        registry, TOOL_TYPE_PACKAGE_MANAGER, &pkg_mgr_count);

    if (pkg_mgrs && pkg_mgr_count > 0) {
        log_success("Found %zu package manager(s):", pkg_mgr_count);
        for (size_t i = 0; i < pkg_mgr_count; i++) {
            if (pkg_mgrs[i]->is_available) {
                log_info("  - %s", pkg_mgrs[i]->display_name);
            }
        }
        free(pkg_mgrs);
    }

    tool_registry_free(registry);
    log_success("Tool finding tests passed!\n");
}

/* Test tool execution */
static void test_tool_execution(void) {
    log_info("Testing tool execution...");

    ToolRegistry* registry = tool_registry_create();
    assert(registry != NULL);

    tool_discover_all(registry);

    /* Test executing git --version */
    const ToolInfo* git = tool_registry_find(registry, "git");
    if (git && git->is_available) {
        log_info("Testing git execution...");

        ToolExecOptions* options = tool_exec_options_create();
        options->args = calloc(2, sizeof(char*));
        options->args[0] = strdup("--version");
        options->arg_count = 1;
        options->show_output = false;

        ToolExecResult* result = tool_execute(git, options);

        assert(result != NULL);
        assert(result->success);
        assert(result->exit_code == 0);
        assert(result->stdout_output != NULL);

        log_success("Git execution successful");
        log_info("Output: %s", result->stdout_output);

        tool_exec_result_free(result);
        tool_exec_options_free(options);
    } else {
        log_warning("Git not available, skipping execution test");
    }

    tool_registry_free(registry);
    log_success("Tool execution tests passed!\n");
}

/* Test package manager detection */
static void test_package_manager(void) {
    log_info("Testing package manager detection...");

    ToolRegistry* registry = tool_registry_create();
    assert(registry != NULL);

    tool_discover_all(registry);

    const ToolInfo* pkg_mgr = package_get_default_manager(registry);
    if (pkg_mgr) {
        log_success("Default package manager: %s", pkg_mgr->display_name);
        log_info("  Path: %s", pkg_mgr->path);
        log_info("  Type: %s",
                package_manager_to_string((PackageManagerType)pkg_mgr->subtype));
    } else {
        log_warning("No package manager found on system");
    }

    tool_registry_free(registry);
    log_success("Package manager tests passed!\n");
}

/* Test direct command execution */
static void test_direct_execution(void) {
    log_info("Testing direct command execution...");

    /* Test executing a simple command */
    char* args[] = {"--version", NULL};
    ToolExecResult* result = tool_execute_command("git", args, NULL);

    if (result) {
        if (result->success) {
            log_success("Direct execution successful");
            log_info("Exit code: %d", result->exit_code);
            if (result->stdout_output) {
                log_info("Output: %s", result->stdout_output);
            }
        } else {
            log_warning("Command failed (might not be installed)");
        }
        tool_exec_result_free(result);
    } else {
        log_warning("Direct execution test skipped (command not available)");
    }

    log_success("Direct execution tests passed!\n");
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    /* Initialize logger */
    log_init(LOG_LEVEL_DEBUG);
    log_set_colors(true);

    log_info("========================================");
    log_info("Tool Executor System Test Suite");
    log_info("========================================\n");

    /* Run tests */
    test_tool_discovery();
    test_find_tools();
    test_tool_execution();
    test_package_manager();
    test_direct_execution();

    log_info("========================================");
    log_success("All tool executor tests passed!");
    log_info("========================================");

    log_shutdown();
    return 0;
}
