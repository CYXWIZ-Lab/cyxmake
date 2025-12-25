/**
 * @file hello_plugin.c
 * @brief Example CyxMake plugin
 *
 * This demonstrates how to create a CyxMake plugin that:
 * - Provides a custom REPL command
 * - Registers lifecycle hooks
 *
 * Build:
 *   gcc -shared -fPIC -o hello_plugin.so hello_plugin.c
 *   cl /LD hello_plugin.c /Fe:hello_plugin.dll  (Windows)
 */

#include "cyxmake/plugin.h"
#include <stdio.h>
#include <string.h>

/* ========================================================================
 * Plugin Information
 * ======================================================================== */

static const PluginInfo plugin_info = {
    .name = "hello_plugin",
    .display_name = "Hello Plugin",
    .version = "1.0.0",
    .author = "CyxMake Team",
    .description = "Example plugin demonstrating the CyxMake plugin API",
    .url = "https://github.com/CYXWIZ-Lab/cyxmake",
    .license = "Apache-2.0",
    .types = PLUGIN_TYPE_COMMAND | PLUGIN_TYPE_HOOK,
    .api_version = CYXMAKE_PLUGIN_API_VERSION,
    .priority = PLUGIN_PRIORITY_NORMAL,
};

PLUGIN_EXPORT const PluginInfo* cyxmake_plugin_info(void) {
    return &plugin_info;
}

/* ========================================================================
 * Custom Command: /hello
 * ======================================================================== */

static int hello_execute(const char* args, char* output_buf, size_t output_size) {
    if (args && strlen(args) > 0) {
        snprintf(output_buf, output_size, "Hello, %s! Welcome to CyxMake.\n", args);
    } else {
        snprintf(output_buf, output_size, "Hello, World! Welcome to CyxMake.\n");
    }
    return 0;
}

static int hello_complete(const char* partial, char** completions, int max_completions) {
    /* No completions for this simple command */
    (void)partial;
    (void)completions;
    (void)max_completions;
    return 0;
}

static const PluginCommand hello_command = {
    .name = "hello",
    .alias = "hi",
    .description = "Say hello",
    .usage = "/hello [name]",
    .execute = hello_execute,
    .complete = hello_complete,
};

static const PluginCommand* commands[] = {
    &hello_command,
    NULL
};

/* ========================================================================
 * Lifecycle Hooks
 * ======================================================================== */

static bool on_pre_build(HookEvent event, const char* data) {
    (void)event;
    (void)data;
    /* Log message before build */
    printf("[HelloPlugin] Build starting...\n");
    return true;  /* Continue with build */
}

static bool on_post_build(HookEvent event, const char* data) {
    (void)event;
    (void)data;
    printf("[HelloPlugin] Build completed!\n");
    return true;
}

/* ========================================================================
 * Plugin Lifecycle
 * ======================================================================== */

static PluginContext* g_ctx = NULL;

PLUGIN_EXPORT bool cyxmake_plugin_init(PluginContext* ctx) {
    g_ctx = ctx;

    plugin_log_info(ctx, "Hello Plugin initialized");

    /* Register commands */
    /* Note: In a real plugin, you would call the registration function */

    return true;
}

PLUGIN_EXPORT void cyxmake_plugin_shutdown(PluginContext* ctx) {
    plugin_log_info(ctx, "Hello Plugin shutting down");
    g_ctx = NULL;
}

PLUGIN_EXPORT int cyxmake_plugin_register_commands(PluginContext* ctx,
                                                    const PluginCommand** out_commands) {
    (void)ctx;
    *out_commands = commands[0];  /* Return first command */
    return 1;  /* Number of commands */
}

/* ========================================================================
 * Hook Registration (called by plugin manager)
 * ======================================================================== */

/* In a real implementation, hooks would be registered via plugin_register_hook() */
