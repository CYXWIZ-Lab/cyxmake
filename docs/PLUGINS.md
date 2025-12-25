# CyxMake Plugin Development Guide

> Extend CyxMake with custom tools, error patterns, and AI providers

## Table of Contents

1. [Overview](#overview)
2. [Plugin Types](#plugin-types)
3. [Creating a Plugin](#creating-a-plugin)
4. [Plugin API Reference](#plugin-api-reference)
5. [Examples](#examples)
6. [Best Practices](#best-practices)

---

## Overview

CyxMake supports plugins as shared libraries that extend functionality:

- **Custom Tools** - Add new build tools, linters, formatters
- **Error Patterns** - Add custom error detection and fixes
- **AI Providers** - Integrate custom AI backends
- **REPL Commands** - Add new interactive commands
- **Lifecycle Hooks** - React to build events

### Plugin Loading

Plugins are loaded from:
1. `~/.cyxmake/plugins/`
2. `/usr/local/lib/cyxmake/plugins/` (Linux/macOS)
3. `%LOCALAPPDATA%\cyxmake\plugins\` (Windows)
4. `./plugins/` (project-local)

### Plugin Files

| Platform | Extension |
|----------|-----------|
| Linux | `.so` |
| macOS | `.dylib` |
| Windows | `.dll` |

---

## Plugin Types

### Tool Plugin

Provides custom build tools or utilities.

```c
static const PluginTool my_tool = {
    .name = "my-tool",
    .description = "My custom tool",
    .capabilities = TOOL_CAP_BUILD | TOOL_CAP_LINT,
    .is_available = my_tool_available,
    .execute = my_tool_execute,
    .get_help = my_tool_help,
};
```

### Pattern Plugin

Adds error detection patterns.

```c
static const PluginErrorPattern my_pattern = {
    .name = "my-error",
    .patterns = (const char*[]){"error: my custom error", NULL},
    .priority = 100,
    .suggest_fixes = my_suggest_fixes,
};
```

### Provider Plugin

Integrates custom AI provider.

```c
static const PluginAIProvider my_provider = {
    .name = "my-ai",
    .init = my_ai_init,
    .complete = my_ai_complete,
    .is_available = my_ai_available,
};
```

### Command Plugin

Adds REPL commands.

```c
static const PluginCommand my_command = {
    .name = "mycommand",
    .alias = "mc",
    .description = "My custom command",
    .execute = my_command_execute,
};
```

### Hook Plugin

Reacts to lifecycle events.

```c
bool my_hook(HookEvent event, const char* data) {
    if (event == HOOK_PRE_BUILD) {
        /* Do something before build */
    }
    return true;  /* Continue */
}
```

---

## Creating a Plugin

### Step 1: Create Plugin Structure

```c
#include "cyxmake/plugin.h"

/* Plugin info (required) */
static const PluginInfo plugin_info = {
    .name = "my_plugin",
    .display_name = "My Plugin",
    .version = "1.0.0",
    .author = "Your Name",
    .description = "What my plugin does",
    .types = PLUGIN_TYPE_COMMAND,
    .api_version = CYXMAKE_PLUGIN_API_VERSION,
    .priority = PLUGIN_PRIORITY_NORMAL,
};

/* Required export */
PLUGIN_EXPORT const PluginInfo* cyxmake_plugin_info(void) {
    return &plugin_info;
}
```

### Step 2: Implement Lifecycle Functions

```c
PLUGIN_EXPORT bool cyxmake_plugin_init(PluginContext* ctx) {
    plugin_log_info(ctx, "My plugin initializing");

    /* Setup code here */

    return true;
}

PLUGIN_EXPORT void cyxmake_plugin_shutdown(PluginContext* ctx) {
    plugin_log_info(ctx, "My plugin shutting down");

    /* Cleanup code here */
}
```

### Step 3: Register Components

```c
/* For commands */
PLUGIN_EXPORT int cyxmake_plugin_register_commands(
    PluginContext* ctx,
    const PluginCommand** commands
) {
    static const PluginCommand* cmds[] = { &my_command, NULL };
    *commands = cmds[0];
    return 1;
}

/* For tools */
PLUGIN_EXPORT int cyxmake_plugin_register_tools(
    PluginContext* ctx,
    const PluginTool** tools
) {
    static const PluginTool* t[] = { &my_tool, NULL };
    *tools = t[0];
    return 1;
}
```

### Step 4: Build Plugin

**CMake:**
```cmake
add_library(my_plugin SHARED my_plugin.c)
target_include_directories(my_plugin PRIVATE /path/to/cyxmake/include)
set_target_properties(my_plugin PROPERTIES PREFIX "")
```

**GCC:**
```bash
gcc -shared -fPIC -o my_plugin.so my_plugin.c -I/path/to/cyxmake/include
```

**MSVC:**
```cmd
cl /LD my_plugin.c /I C:\cyxmake\include /Fe:my_plugin.dll
```

### Step 5: Install Plugin

```bash
# User plugins
cp my_plugin.so ~/.cyxmake/plugins/

# System plugins (requires sudo)
sudo cp my_plugin.so /usr/local/lib/cyxmake/plugins/
```

---

## Plugin API Reference

### Context Functions

```c
/* Logging */
void plugin_log_debug(PluginContext* ctx, const char* format, ...);
void plugin_log_info(PluginContext* ctx, const char* format, ...);
void plugin_log_warning(PluginContext* ctx, const char* format, ...);
void plugin_log_error(PluginContext* ctx, const char* format, ...);

/* Configuration */
char* plugin_get_config(PluginContext* ctx, const char* key);
bool plugin_set_config(PluginContext* ctx, const char* key, const char* value);

/* Project */
const char* plugin_get_project_path(PluginContext* ctx);

/* Version */
const char* plugin_get_cyxmake_version(void);
```

### Plugin Info Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| name | string | Yes | Unique identifier |
| display_name | string | Yes | Human-readable name |
| version | string | Yes | Semver version |
| author | string | No | Author name |
| description | string | Yes | Short description |
| url | string | No | Homepage URL |
| license | string | No | License identifier |
| types | flags | Yes | Plugin type flags |
| api_version | int | Yes | Must be `CYXMAKE_PLUGIN_API_VERSION` |
| priority | enum | No | Load priority |

### Hook Events

| Event | Description | Data |
|-------|-------------|------|
| HOOK_PRE_BUILD | Before build starts | Build options JSON |
| HOOK_POST_BUILD | After build completes | Build result JSON |
| HOOK_PRE_FIX | Before fix applied | Fix action JSON |
| HOOK_POST_FIX | After fix applied | Fix result JSON |
| HOOK_ERROR_DETECTED | Error detected | Error details JSON |
| HOOK_PROJECT_LOADED | Project analyzed | Project info JSON |

---

## Examples

### Custom Command Plugin

```c
#include "cyxmake/plugin.h"
#include <stdio.h>

static int stats_execute(const char* args, char* buf, size_t size) {
    snprintf(buf, size,
        "Build Statistics:\n"
        "  Builds today: 42\n"
        "  Success rate: 87%%\n"
    );
    return 0;
}

static const PluginCommand stats_cmd = {
    .name = "stats",
    .description = "Show build statistics",
    .execute = stats_execute,
};

static const PluginInfo info = {
    .name = "stats_plugin",
    .display_name = "Build Stats",
    .version = "1.0.0",
    .types = PLUGIN_TYPE_COMMAND,
    .api_version = CYXMAKE_PLUGIN_API_VERSION,
};

PLUGIN_EXPORT const PluginInfo* cyxmake_plugin_info(void) { return &info; }
PLUGIN_EXPORT bool cyxmake_plugin_init(PluginContext* ctx) { return true; }
PLUGIN_EXPORT void cyxmake_plugin_shutdown(PluginContext* ctx) {}
```

### Custom Error Pattern Plugin

```c
#include "cyxmake/plugin.h"
#include <string.h>

static const char* npm_patterns[] = {
    "npm ERR!",
    "ENOENT",
    NULL
};

static int npm_suggest(const char* error, char** suggestions, int max) {
    if (strstr(error, "ENOENT")) {
        suggestions[0] = "Run 'npm install' to install dependencies";
        return 1;
    }
    return 0;
}

static const PluginErrorPattern npm_pattern = {
    .name = "npm-errors",
    .patterns = npm_patterns,
    .priority = 50,
    .suggest_fixes = npm_suggest,
};
```

### Build Hook Plugin

```c
#include "cyxmake/plugin.h"
#include <time.h>

static time_t build_start;

static bool on_pre_build(HookEvent e, const char* data) {
    build_start = time(NULL);
    return true;
}

static bool on_post_build(HookEvent e, const char* data) {
    time_t elapsed = time(NULL) - build_start;
    printf("Build took %ld seconds\n", elapsed);
    return true;
}
```

---

## Best Practices

### 1. Version Compatibility

Always check API version:

```c
if (CYXMAKE_PLUGIN_API_VERSION != expected_version) {
    return false;
}
```

### 2. Error Handling

Return meaningful error messages:

```c
if (!initialize_resources()) {
    plugin_log_error(ctx, "Failed to initialize: %s", get_error());
    return false;
}
```

### 3. Memory Management

- Free allocated memory in shutdown
- Don't free strings returned by CyxMake API
- Use static storage for plugin info

### 4. Thread Safety

- Plugins may be called from multiple threads
- Protect shared state with mutexes
- Use thread-local storage when possible

### 5. Graceful Degradation

```c
bool my_tool_available(void) {
    /* Check if external tool exists */
    return system("which mytool > /dev/null 2>&1") == 0;
}
```

### 6. Configuration

Use namespaced config keys:

```c
char* val = plugin_get_config(ctx, "my_plugin.setting");
```

### 7. Logging

Use appropriate log levels:

```c
plugin_log_debug(ctx, "Detailed info: %s", details);
plugin_log_info(ctx, "Normal operation");
plugin_log_warning(ctx, "Recoverable issue");
plugin_log_error(ctx, "Fatal error");
```

---

*CyxMake Plugin Guide v0.2.0*
