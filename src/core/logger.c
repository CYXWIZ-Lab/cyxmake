/**
 * @file logger.c
 * @brief Logging system implementation
 */

#include "cyxmake/logger.h"
#include "cyxmake/compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* strdup is not ANSI C, provide implementation if needed */
#if defined(_WIN32) && !defined(__MINGW32__)
    #define strdup _strdup
#endif

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #define isatty _isatty
    #define fileno _fileno
#else
    #include <unistd.h>
#endif

/* ANSI color codes */
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_GRAY    "\033[90m"
#define COLOR_BOLD    "\033[1m"

/* Forward declarations */
static void write_to_file(LogLevel level, const char* format, va_list args);

/* Global logger state */
static struct {
    LogLevel min_level;
    bool use_colors;
    bool show_timestamp;
    bool show_level;
    FILE* output;
    FILE* log_file;
    char* log_file_path;
    bool initialized;
} g_logger = {
    .min_level = LOG_LEVEL_INFO,
    .use_colors = true,
    .show_timestamp = false,
    .show_level = true,
    .output = NULL,
    .log_file = NULL,
    .log_file_path = NULL,
    .initialized = false
};

/* Check if output supports colors */
static bool supports_colors(FILE* stream) {
    if (!stream) return false;

#ifdef _WIN32
    /* Windows 10+ supports ANSI colors */
    HANDLE hConsole = (HANDLE)_get_osfhandle(fileno(stream));
    if (hConsole == INVALID_HANDLE_VALUE) return false;

    DWORD mode = 0;
    if (!GetConsoleMode(hConsole, &mode)) return false;

    /* Try to enable virtual terminal processing */
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hConsole, mode)) return false;

    return true;
#else
    /* Unix: check if connected to a terminal */
    return isatty(fileno(stream)) != 0;
#endif
}

/* Initialize logger */
void log_init(const LogConfig* config) {
    if (config) {
        g_logger.min_level = config->min_level;
        g_logger.use_colors = config->use_colors;
        g_logger.show_timestamp = config->show_timestamp;
        g_logger.show_level = config->show_level;
        g_logger.output = config->output ? config->output : stdout;

        /* Open log file if specified */
        if (config->log_file) {
            log_set_file(config->log_file);
        }
    } else {
        /* Default configuration */
        g_logger.min_level = LOG_LEVEL_INFO;
        g_logger.use_colors = true;
        g_logger.show_timestamp = false;
        g_logger.show_level = true;
        g_logger.output = stdout;
    }

    /* Check if colors are actually supported */
    if (g_logger.use_colors) {
        g_logger.use_colors = supports_colors(g_logger.output);
    }

    g_logger.initialized = true;
}

/* Shutdown logger */
void log_shutdown(void) {
    /* Close log file if open */
    if (g_logger.log_file) {
        fclose(g_logger.log_file);
        g_logger.log_file = NULL;
    }

    /* Free log file path */
    if (g_logger.log_file_path) {
        free(g_logger.log_file_path);
        g_logger.log_file_path = NULL;
    }

    g_logger.initialized = false;
}

/* Set log level */
void log_set_level(LogLevel level) {
    g_logger.min_level = level;
}

/* Get log level */
LogLevel log_get_level(void) {
    return g_logger.min_level;
}

/* Set colors */
void log_set_colors(bool enable) {
    g_logger.use_colors = enable && supports_colors(g_logger.output);
}

/* Check if colors enabled */
bool log_colors_enabled(void) {
    return g_logger.use_colors;
}

/* Get color for log level */
static const char* get_level_color(LogLevel level) {
    if (!g_logger.use_colors) return "";

    switch (level) {
        case LOG_LEVEL_DEBUG:   return COLOR_GRAY;
        case LOG_LEVEL_INFO:    return COLOR_BLUE;
        case LOG_LEVEL_SUCCESS: return COLOR_GREEN;
        case LOG_LEVEL_WARNING: return COLOR_YELLOW;
        case LOG_LEVEL_ERROR:   return COLOR_RED;
        default:                return COLOR_RESET;
    }
}

/* Get prefix for log level */
static const char* get_level_prefix(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG:   return "[DEBUG]";
        case LOG_LEVEL_INFO:    return "[INFO] ";
        case LOG_LEVEL_SUCCESS: return "[OK]   ";
        case LOG_LEVEL_WARNING: return "[WARN] ";
        case LOG_LEVEL_ERROR:   return "[ERROR]";
        default:                return "[?]    ";
    }
}

/* Log level to string */
const char* log_level_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG:   return "DEBUG";
        case LOG_LEVEL_INFO:    return "INFO";
        case LOG_LEVEL_SUCCESS: return "SUCCESS";
        case LOG_LEVEL_WARNING: return "WARNING";
        case LOG_LEVEL_ERROR:   return "ERROR";
        case LOG_LEVEL_NONE:    return "NONE";
        default:                return "UNKNOWN";
    }
}

/* Log message with level */
void log_message(LogLevel level, const char* format, ...) {
    /* Ensure logger is initialized */
    if (!g_logger.initialized) {
        log_init(NULL);
    }

    /* Check if we should log this level */
    if (level < g_logger.min_level) {
        return;
    }

    FILE* out = g_logger.output;
    const char* color = get_level_color(level);
    const char* reset = g_logger.use_colors ? COLOR_RESET : "";

    /* Print timestamp if enabled */
    if (g_logger.show_timestamp) {
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        char time_buf[32];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(out, "%s[%s]%s ", COLOR_GRAY, time_buf, reset);
    }

    /* Print level prefix if enabled */
    if (g_logger.show_level) {
        fprintf(out, "%s%s%s ", color, get_level_prefix(level), reset);
    }

    /* Print message */
    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);

    fprintf(out, "\n");
    fflush(out);

    /* Also write to file if enabled */
    if (g_logger.log_file) {
        va_list args_file;
        va_start(args_file, format);
        write_to_file(level, format, args_file);
        va_end(args_file);
    }
}

/* Debug message */
void log_debug(const char* format, ...) {
    if (!g_logger.initialized) log_init(NULL);
    if (LOG_LEVEL_DEBUG < g_logger.min_level) return;

    FILE* out = g_logger.output;
    const char* color = g_logger.use_colors ? COLOR_GRAY : "";
    const char* reset = g_logger.use_colors ? COLOR_RESET : "";

    fprintf(out, "%s[DEBUG]%s ", color, reset);

    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);

    fprintf(out, "\n");
    fflush(out);

    /* Also write to file */
    if (g_logger.log_file) {
        va_list args_file;
        va_start(args_file, format);
        write_to_file(LOG_LEVEL_DEBUG, format, args_file);
        va_end(args_file);
    }
}

/* Info message */
void log_info(const char* format, ...) {
    if (!g_logger.initialized) log_init(NULL);
    if (LOG_LEVEL_INFO < g_logger.min_level) return;

    FILE* out = g_logger.output;

    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);

    fprintf(out, "\n");
    fflush(out);

    /* Also write to file */
    if (g_logger.log_file) {
        va_list args_file;
        va_start(args_file, format);
        write_to_file(LOG_LEVEL_INFO, format, args_file);
        va_end(args_file);
    }
}

/* Success message */
void log_success(const char* format, ...) {
    if (!g_logger.initialized) log_init(NULL);
    if (LOG_LEVEL_SUCCESS < g_logger.min_level) return;

    FILE* out = g_logger.output;
    const char* color = g_logger.use_colors ? COLOR_GREEN : "";
    const char* reset = g_logger.use_colors ? COLOR_RESET : "";

    fprintf(out, "%s ", color);

    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);

    fprintf(out, "%s\n", reset);
    fflush(out);

    /* Also write to file */
    if (g_logger.log_file) {
        va_list args_file;
        va_start(args_file, format);
        write_to_file(LOG_LEVEL_SUCCESS, format, args_file);
        va_end(args_file);
    }
}

/* Warning message */
void log_warning(const char* format, ...) {
    if (!g_logger.initialized) log_init(NULL);
    if (LOG_LEVEL_WARNING < g_logger.min_level) return;

    FILE* out = g_logger.output;
    const char* color = g_logger.use_colors ? COLOR_YELLOW : "";
    const char* reset = g_logger.use_colors ? COLOR_RESET : "";

    fprintf(out, "%sWarning: ", color);

    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);

    fprintf(out, "%s\n", reset);
    fflush(out);

    /* Also write to file */
    if (g_logger.log_file) {
        va_list args_file;
        va_start(args_file, format);
        write_to_file(LOG_LEVEL_WARNING, format, args_file);
        va_end(args_file);
    }
}

/* Error message */
void log_error(const char* format, ...) {
    if (!g_logger.initialized) log_init(NULL);

    FILE* out = stderr;  /* Errors always go to stderr */
    const char* color = g_logger.use_colors ? COLOR_RED : "";
    const char* reset = g_logger.use_colors ? COLOR_RESET : "";

    fprintf(out, "%sError: ", color);

    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);

    fprintf(out, "%s\n", reset);
    fflush(out);

    /* Also write to file */
    if (g_logger.log_file) {
        va_list args_file;
        va_start(args_file, format);
        write_to_file(LOG_LEVEL_ERROR, format, args_file);
        va_end(args_file);
    }
}

/* Plain message (no formatting) */
void log_plain(const char* format, ...) {
    if (!g_logger.initialized) log_init(NULL);

    FILE* out = g_logger.output;

    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);

    fflush(out);
}

/* Message with custom prefix */
void log_with_prefix(const char* prefix, const char* format, ...) {
    if (!g_logger.initialized) log_init(NULL);

    FILE* out = g_logger.output;

    fprintf(out, "%s ", prefix);

    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);

    fprintf(out, "\n");
    fflush(out);
}

/* Step message */
void log_step(int current, int total, const char* format, ...) {
    if (!g_logger.initialized) log_init(NULL);

    FILE* out = g_logger.output;
    const char* color = g_logger.use_colors ? COLOR_CYAN : "";
    const char* reset = g_logger.use_colors ? COLOR_RESET : "";

    fprintf(out, "  %s[%d/%d]%s ", color, current, total, reset);

    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);

    fprintf(out, "\n");
    fflush(out);

    /* Also write to file if enabled (without colors) */
    if (g_logger.log_file) {
        fprintf(g_logger.log_file, "  [%d/%d] ", current, total);
        va_list args_file;
        va_start(args_file, format);
        vfprintf(g_logger.log_file, format, args_file);
        va_end(args_file);
        fprintf(g_logger.log_file, "\n");
        fflush(g_logger.log_file);
    }
}

/* Helper to write to log file */
static void write_to_file(LogLevel level, const char* format, va_list args) {
    if (!g_logger.log_file) return;

    /* Write timestamp */
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(g_logger.log_file, "[%s] ", time_buf);

    /* Write level */
    fprintf(g_logger.log_file, "%s ", get_level_prefix(level));

    /* Write message */
    vfprintf(g_logger.log_file, format, args);
    fprintf(g_logger.log_file, "\n");
    fflush(g_logger.log_file);
}

/* Set log file */
bool log_set_file(const char* file_path) {
    /* Close existing file if open */
    if (g_logger.log_file) {
        fclose(g_logger.log_file);
        g_logger.log_file = NULL;
    }

    /* Free existing path */
    if (g_logger.log_file_path) {
        free(g_logger.log_file_path);
        g_logger.log_file_path = NULL;
    }

    /* Disable file logging if NULL */
    if (!file_path) {
        return true;
    }

    /* Open new log file */
    g_logger.log_file = fopen(file_path, "a");
    if (!g_logger.log_file) {
        return false;
    }

    /* Save path */
    g_logger.log_file_path = strdup(file_path);

    return true;
}

/* Get log file path */
const char* log_get_file(void) {
    return g_logger.log_file_path;
}
