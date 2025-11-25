# CyxMake Logger System

## Overview

The CyxMake logger provides a comprehensive logging system with support for:
- Multiple log levels (DEBUG, INFO, SUCCESS, WARNING, ERROR)
- Colored console output (automatic terminal detection)
- File logging with timestamps
- Step-based progress logging
- Custom prefixes and plain output
- Cross-platform support (Windows, Linux, macOS)

## Quick Start

```c
#include "cyxmake/logger.h"

int main() {
    // Initialize with defaults
    log_init(NULL);

    // Basic logging
    log_info("Starting application...");
    log_success("Configuration loaded!");
    log_warning("Low disk space");
    log_error("Failed to connect");
    log_debug("Variable x = %d", 42);

    // Cleanup
    log_shutdown();
    return 0;
}
```

## Log Levels

| Level | Function | Use Case | Output |
|-------|----------|----------|--------|
| DEBUG | `log_debug()` | Detailed debug information | Gray text with [DEBUG] prefix |
| INFO | `log_info()` | Informational messages | Normal text, no prefix |
| SUCCESS | `log_success()` | Success confirmations | Green text  |
| WARNING | `log_warning()` | Warning messages | Yellow "Warning:" prefix |
| ERROR | `log_error()` | Error messages | Red "Error:" prefix (stderr) |

## Configuration

### Basic Configuration

```c
log_init(NULL); // Use defaults
```

### Custom Configuration

```c
LogConfig config = {
    .min_level = LOG_LEVEL_DEBUG,    // Show all levels
    .use_colors = true,              // Enable colors
    .show_timestamp = true,          // Show timestamps
    .show_level = true,              // Show level prefix
    .output = stdout,                // Output stream
    .log_file = "app.log"            // Optional file logging
};

log_init(&config);
```

## File Logging

Enable logging to a file:

```c
// Enable file logging
log_set_file("application.log");

log_info("This goes to both console and file");
log_error("Errors are logged to file with timestamps");

// Disable file logging
log_set_file(NULL);
```

File output format:
```
[2025-11-25 08:01:07] [INFO]  Application started
[2025-11-25 08:01:08] [DEBUG] Loading configuration
[2025-11-25 08:01:09] [ERROR] Failed to connect to database
```

## Step Logging

Track progress through multi-step operations:

```c
log_info("Analyzing project...");
log_step(1, 5, "Detecting primary language...");
log_step(2, 5, "Detecting build system...");
log_step(3, 5, "Scanning source files...");
log_step(4, 5, "Calculating statistics...");
log_step(5, 5, "Generating cache...");
log_success("Analysis complete!");
```

Output:
```
Analyzing project...
  [1/5] Detecting primary language...
  [2/5] Detecting build system...
  [3/5] Scanning source files...
  [4/5] Calculating statistics...
  [5/5] Generating cache...
Analysis complete!
```

## Custom Formatting

### Custom Prefix

```c
log_with_prefix("[BUILD]", "Compiling main.c...");
log_with_prefix("[TEST] ", "Running unit tests...");
```

### Plain Output

```c
log_plain("Raw output without formatting\n");
log_plain("Useful for machine-readable output\n");
```

## Dynamic Configuration

### Set Log Level at Runtime

```c
// Initially show everything
log_set_level(LOG_LEVEL_DEBUG);

log_debug("This will appear");
log_info("This will also appear");

// Switch to warnings and errors only
log_set_level(LOG_LEVEL_WARNING);

log_debug("This will NOT appear");
log_warning("This WILL appear");
```

### Enable/Disable Colors

```c
log_set_colors(true);   // Enable colors
log_set_colors(false);  // Disable colors (useful for file redirection)
```

## Common Patterns

### Application Startup

```c
int main(int argc, char** argv) {
    // Initialize logger
    LogConfig config = {
        .min_level = LOG_LEVEL_INFO,
        .use_colors = true,
        .show_timestamp = false,
        .show_level = false,
        .output = stdout,
        .log_file = NULL
    };
    log_init(&config);

    log_info("Application v1.0.0 starting...");

    // ... application code ...

    log_info("Shutdown complete");
    log_shutdown();
    return 0;
}
```

### Build Process

```c
void build_project(const char* project_path) {
    log_info("Building project at: %s", project_path);

    log_step(1, 4, "Analyzing dependencies...");
    if (!analyze_deps()) {
        log_error("Dependency analysis failed");
        return;
    }

    log_step(2, 4, "Compiling source files...");
    if (!compile()) {
        log_error("Compilation failed");
        return;
    }

    log_step(3, 4, "Linking...");
    if (!link()) {
        log_error("Linking failed");
        return;
    }

    log_step(4, 4, "Running tests...");
    if (!run_tests()) {
        log_warning("Some tests failed");
    }

    log_success("Build completed successfully!");
}
```

### Debugging Mode

```c
#ifdef DEBUG
    log_set_level(LOG_LEVEL_DEBUG);
#else
    log_set_level(LOG_LEVEL_INFO);
#endif

log_debug("This only appears in debug builds");
log_info("This always appears");
```

### Production with File Logging

```c
// Enable file logging for production
LogConfig config = {
    .min_level = LOG_LEVEL_INFO,     // Don't log debug in production
    .use_colors = false,             // No colors needed for production
    .show_timestamp = true,          // Timestamps important for production
    .show_level = true,              // Show levels for clarity
    .output = stdout,
    .log_file = "/var/log/cyxmake/app.log"  // Log to file
};
log_init(&config);
```

## API Reference

### Initialization

- `void log_init(const LogConfig* config)` - Initialize logger
- `void log_shutdown(void)` - Shutdown logger and close files

### Logging Functions

- `void log_debug(const char* format, ...)` - Log debug message
- `void log_info(const char* format, ...)` - Log info message
- `void log_success(const char* format, ...)` - Log success message
- `void log_warning(const char* format, ...)` - Log warning message
- `void log_error(const char* format, ...)` - Log error message
- `void log_plain(const char* format, ...)` - Log without formatting
- `void log_with_prefix(const char* prefix, const char* format, ...)` - Log with custom prefix
- `void log_step(int current, int total, const char* format, ...)` - Log progress step
- `void log_message(LogLevel level, const char* format, ...)` - Log with specific level

### Configuration Functions

- `void log_set_level(LogLevel level)` - Set minimum log level
- `LogLevel log_get_level(void)` - Get current log level
- `void log_set_colors(bool enable)` - Enable/disable colors
- `bool log_colors_enabled(void)` - Check if colors are enabled
- `bool log_set_file(const char* file_path)` - Set log file (NULL to disable)
- `const char* log_get_file(void)` - Get current log file path
- `const char* log_level_to_string(LogLevel level)` - Convert level to string

## Thread Safety

The logger uses `fflush()` after each write to ensure messages are immediately visible. However, it is not thread-safe by default. For multi-threaded applications, you should:

1. Use separate logger instances per thread, or
2. Add mutex locking around log calls, or
3. Use a thread-safe logging library

## Performance

- Minimal overhead for disabled log levels (early return)
- Buffered output with explicit flushing
- Colors only enabled when supported by terminal
- File logging is append-mode with immediate flush

## Platform Support

| Platform | Console Colors | File Logging | Terminal Detection |
|----------|---------------|--------------|-------------------|
| Windows 10+ | ✅ (ANSI) | ✅ | ✅ |
| Linux | ✅ (ANSI) | ✅ | ✅ |
| macOS | ✅ (ANSI) | ✅ | ✅ |
| Windows <10 | ❌ | ✅ | ✅ |

## Examples

See `tests/test_logger.c` for comprehensive examples of all logger features.

## Best Practices

1. **Always initialize** - Call `log_init()` before any logging
2. **Always shutdown** - Call `log_shutdown()` to close files
3. **Use appropriate levels** - DEBUG for development, INFO for normal, WARNING/ERROR for issues
4. **Enable file logging in production** - Makes debugging easier
5. **Use timestamps in production** - Critical for troubleshooting
6. **Disable colors for redirected output** - Prevents ANSI codes in log files
7. **Check return values** - `log_set_file()` returns false on error

## Troubleshooting

### Colors not working

- Ensure terminal supports ANSI colors
- Check `log_colors_enabled()` returns true
- Try `log_set_colors(true)` explicitly

### File not created

- Check file path is valid
- Verify write permissions
- Check `log_set_file()` return value
- Ensure `log_shutdown()` is called (flushes and closes file)

### Messages not appearing

- Check current log level with `log_get_level()`
- Verify message level is >= minimum level
- Ensure logger is initialized

### Timestamps missing

- Set `show_timestamp = true` in config
- Or use file logging (always has timestamps)
