/**
 * @file config.h
 * @brief Configuration management for CyxMake
 */

#ifndef CYXMAKE_CONFIG_H
#define CYXMAKE_CONFIG_H

#include <stdbool.h>
#include "cyxmake/logger.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Configuration Structures
 * ======================================================================== */

/**
 * Project configuration from [project] section
 */
typedef struct {
    char* name;              /* Project name (auto-detected if NULL) */
    char* language;          /* Primary language (auto-detected if NULL) */
    char* build_system;      /* Build system (auto-detected if NULL) */
} ProjectConfig;

/**
 * Build configuration from [build] section
 */
typedef struct {
    char* type;              /* Build type: "Debug", "Release", etc. */
    char* build_dir;         /* Build directory (default: "build") */
    int parallel_jobs;       /* Parallel jobs (0 = auto) */
    bool clean_first;        /* Clean before building */
} BuildConfig;

/**
 * Permission configuration from [permissions] section
 */
typedef struct {
    bool auto_approve_read;      /* Auto-approve file reads */
    bool auto_approve_build;     /* Auto-approve build operations */
    bool auto_approve_list;      /* Auto-approve directory listing */
    bool always_confirm_delete;  /* Always confirm deletions */
    bool always_confirm_install; /* Always confirm package installs */
    bool always_confirm_command; /* Always confirm shell commands */
    bool remember_choices;       /* Remember user choices for session */
} PermissionsConfig;

/**
 * Logging configuration from [logging] section
 */
typedef struct {
    char* level;             /* Log level: "debug", "info", "warning", "error" */
    bool colors;             /* Enable colored output */
    bool timestamps;         /* Show timestamps */
    char* file;              /* Log file path (NULL = no file logging) */
} LoggingConfig;

/**
 * AI configuration from [ai] section (basic settings only)
 * Provider-specific config is in ai_provider.h
 */
typedef struct {
    char* default_provider;   /* Default provider name */
    char* fallback_provider;  /* Fallback provider name */
    int timeout;              /* Request timeout in seconds */
    int max_tokens;           /* Max response tokens */
    float temperature;        /* Generation temperature */
} AIConfig;

/**
 * Main configuration structure
 */
typedef struct Config {
    char* config_path;        /* Path to loaded config file */

    ProjectConfig project;
    BuildConfig build;
    PermissionsConfig permissions;
    LoggingConfig logging;
    AIConfig ai;

    bool loaded;              /* True if config was loaded from file */
} Config;

/* ========================================================================
 * Configuration API
 * ======================================================================== */

/**
 * Create default configuration
 * @return New Config with defaults (caller must free with config_free)
 */
Config* config_create_default(void);

/**
 * Load configuration from file
 * @param config_path Path to cyxmake.toml (NULL to search default locations)
 * @return Loaded Config or default config if file not found
 */
Config* config_load(const char* config_path);

/**
 * Free configuration
 * @param config Config to free
 */
void config_free(Config* config);

/**
 * Find config file in default locations
 * Search order:
 *   1. ./cyxmake.toml
 *   2. ./.cyxmake/config.toml
 *   3. ~/.cyxmake/config.toml
 *   4. ~/.config/cyxmake/config.toml
 * @return Allocated path to found config, or NULL if not found
 */
char* config_find_file(void);

/**
 * Get log level from config string
 * @param level_str Level string ("debug", "info", etc.)
 * @return LogLevel enum value
 */
LogLevel config_parse_log_level(const char* level_str);

/**
 * Apply logging configuration
 * @param config Configuration
 */
void config_apply_logging(const Config* config);

/**
 * Get build type from config
 * @param config Configuration
 * @return Build type string (e.g., "Debug")
 */
const char* config_get_build_type(const Config* config);

/**
 * Get build directory from config
 * @param config Configuration
 * @return Build directory path
 */
const char* config_get_build_dir(const Config* config);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_CONFIG_H */
