/**
 * @file logger.h
 * @brief Logging system for CyxMake
 */

#ifndef CYXMAKE_LOGGER_H
#define CYXMAKE_LOGGER_H

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Log levels
 */
typedef enum {
    LOG_LEVEL_DEBUG = 0,    /* Detailed debug information */
    LOG_LEVEL_INFO = 1,     /* Informational messages */
    LOG_LEVEL_SUCCESS = 2,  /* Success messages */
    LOG_LEVEL_WARNING = 3,  /* Warning messages */
    LOG_LEVEL_ERROR = 4,    /* Error messages */
    LOG_LEVEL_NONE = 5      /* Disable all logging */
} LogLevel;

/**
 * Logger configuration
 */
typedef struct {
    LogLevel min_level;     /* Minimum level to display */
    bool use_colors;        /* Enable colored output */
    bool show_timestamp;    /* Show timestamps */
    bool show_level;        /* Show log level prefix */
    FILE* output;           /* Output stream (stdout/stderr) */
    const char* log_file;   /* Optional log file path (NULL to disable file logging) */
} LogConfig;

/**
 * Initialize the logger with configuration
 * @param config Logger configuration (NULL for defaults)
 */
void log_init(const LogConfig* config);

/**
 * Shutdown the logger
 */
void log_shutdown(void);

/**
 * Set minimum log level
 * @param level Minimum level to display
 */
void log_set_level(LogLevel level);

/**
 * Get current log level
 * @return Current minimum log level
 */
LogLevel log_get_level(void);

/**
 * Enable/disable colored output
 * @param enable True to enable colors
 */
void log_set_colors(bool enable);

/**
 * Check if colors are enabled
 * @return True if colors are enabled
 */
bool log_colors_enabled(void);

/**
 * Log a message at a specific level
 * @param level Log level
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void log_message(LogLevel level, const char* format, ...);

/**
 * Log a debug message
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void log_debug(const char* format, ...);

/**
 * Log an info message
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void log_info(const char* format, ...);

/**
 * Log a success message
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void log_success(const char* format, ...);

/**
 * Log a warning message
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void log_warning(const char* format, ...);

/**
 * Log an error message
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void log_error(const char* format, ...);

/**
 * Log a plain message (no prefix, no colors)
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void log_plain(const char* format, ...);

/**
 * Log with custom prefix
 * @param prefix Custom prefix string
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void log_with_prefix(const char* prefix, const char* format, ...);

/**
 * Log a step in a process (e.g., "[1/5] Detecting language...")
 * @param current Current step number
 * @param total Total number of steps
 * @param format Printf-style format string
 * @param ... Format arguments
 */
void log_step(int current, int total, const char* format, ...);

/**
 * Get string representation of log level
 * @param level Log level
 * @return String representation
 */
const char* log_level_to_string(LogLevel level);

/**
 * Enable file logging
 * @param file_path Path to log file (NULL to disable)
 * @return True if successful, false on error
 */
bool log_set_file(const char* file_path);

/**
 * Get current log file path
 * @return Current log file path (NULL if not set)
 */
const char* log_get_file(void);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_LOGGER_H */
