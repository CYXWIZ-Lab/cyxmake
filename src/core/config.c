/**
 * @file config.c
 * @brief Configuration management implementation
 */

#include "cyxmake/config.h"
#include "cyxmake/logger.h"
#include "tomlc99/toml.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define access _access
#define F_OK 0
#define strdup _strdup
#else
#include <unistd.h>
#endif

/* ========================================================================
 * Helpers
 * ======================================================================== */

static char* get_home_dir(void) {
#ifdef _WIN32
    char* home = getenv("USERPROFILE");
    if (!home) home = getenv("HOMEPATH");
#else
    char* home = getenv("HOME");
#endif
    return home ? strdup(home) : NULL;
}

static char* toml_string_or_null(toml_table_t* table, const char* key) {
    toml_datum_t d = toml_string_in(table, key);
    return d.ok ? d.u.s : NULL;  /* d.u.s is already allocated by toml */
}

static int toml_int_or_default(toml_table_t* table, const char* key, int def) {
    toml_datum_t d = toml_int_in(table, key);
    return d.ok ? (int)d.u.i : def;
}

static bool toml_bool_or_default(toml_table_t* table, const char* key, bool def) {
    toml_datum_t d = toml_bool_in(table, key);
    return d.ok ? (bool)d.u.b : def;
}

static double toml_double_or_default(toml_table_t* table, const char* key, double def) {
    toml_datum_t d = toml_double_in(table, key);
    return d.ok ? d.u.d : def;
}

/* ========================================================================
 * Default Configuration
 * ======================================================================== */

Config* config_create_default(void) {
    Config* config = calloc(1, sizeof(Config));
    if (!config) return NULL;

    /* Project defaults */
    config->project.name = NULL;       /* Auto-detect */
    config->project.language = NULL;   /* Auto-detect */
    config->project.build_system = NULL; /* Auto-detect */

    /* Build defaults */
    config->build.type = strdup("Debug");
    config->build.build_dir = strdup("build");
    config->build.parallel_jobs = 0;   /* Auto-detect */
    config->build.clean_first = false;

    /* Permission defaults - safe by default */
    config->permissions.auto_approve_read = true;
    config->permissions.auto_approve_build = true;
    config->permissions.auto_approve_list = true;
    config->permissions.always_confirm_delete = true;
    config->permissions.always_confirm_install = true;
    config->permissions.always_confirm_command = true;
    config->permissions.remember_choices = true;

    /* Logging defaults */
    config->logging.level = strdup("info");
    config->logging.colors = true;
    config->logging.timestamps = false;
    config->logging.file = NULL;

    /* AI defaults */
    config->ai.default_provider = NULL;
    config->ai.fallback_provider = NULL;
    config->ai.timeout = 300;
    config->ai.max_tokens = 1024;
    config->ai.temperature = 0.7f;

    config->loaded = false;
    return config;
}

/* ========================================================================
 * Config File Discovery
 * ======================================================================== */

char* config_find_file(void) {
    /* Check locations in order */
    const char* local_paths[] = {
        "cyxmake.toml",
        ".cyxmake/config.toml",
        NULL
    };

    /* Check local paths */
    for (int i = 0; local_paths[i]; i++) {
        if (access(local_paths[i], F_OK) == 0) {
            return strdup(local_paths[i]);
        }
    }

    /* Check home directory paths */
    char* home = get_home_dir();
    if (home) {
        char path[512];

        snprintf(path, sizeof(path), "%s/.cyxmake/config.toml", home);
        if (access(path, F_OK) == 0) {
            free(home);
            return strdup(path);
        }

        snprintf(path, sizeof(path), "%s/.config/cyxmake/config.toml", home);
        if (access(path, F_OK) == 0) {
            free(home);
            return strdup(path);
        }

        free(home);
    }

    return NULL;
}

/* ========================================================================
 * Config Loading
 * ======================================================================== */

Config* config_load(const char* config_path) {
    Config* config = config_create_default();
    if (!config) return NULL;

    /* Find config file if not specified */
    char* path = config_path ? strdup(config_path) : config_find_file();
    if (!path) {
        log_debug("No config file found, using defaults");
        return config;
    }

    /* Open and parse TOML file */
    FILE* fp = fopen(path, "r");
    if (!fp) {
        log_debug("Cannot open config file: %s", path);
        free(path);
        return config;
    }

    char errbuf[256];
    toml_table_t* root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!root) {
        log_warning("Failed to parse config: %s", errbuf);
        free(path);
        return config;
    }

    config->config_path = path;
    config->loaded = true;
    log_debug("Loading config from: %s", path);

    /* Parse [project] section */
    toml_table_t* project = toml_table_in(root, "project");
    if (project) {
        free(config->project.name);
        free(config->project.language);
        free(config->project.build_system);
        config->project.name = toml_string_or_null(project, "name");
        config->project.language = toml_string_or_null(project, "language");
        config->project.build_system = toml_string_or_null(project, "build_system");
    }

    /* Parse [build] section */
    toml_table_t* build = toml_table_in(root, "build");
    if (build) {
        char* type = toml_string_or_null(build, "type");
        if (type) { free(config->build.type); config->build.type = type; }

        char* build_dir = toml_string_or_null(build, "build_dir");
        if (build_dir) { free(config->build.build_dir); config->build.build_dir = build_dir; }

        config->build.parallel_jobs = toml_int_or_default(build, "parallel_jobs", 0);
        config->build.clean_first = toml_bool_or_default(build, "clean_first", false);
    }

    /* Parse [permissions] section */
    toml_table_t* perms = toml_table_in(root, "permissions");
    if (perms) {
        config->permissions.auto_approve_read = toml_bool_or_default(perms, "auto_approve_read", true);
        config->permissions.auto_approve_build = toml_bool_or_default(perms, "auto_approve_build", true);
        config->permissions.auto_approve_list = toml_bool_or_default(perms, "auto_approve_list", true);
        config->permissions.always_confirm_delete = toml_bool_or_default(perms, "always_confirm_delete", true);
        config->permissions.always_confirm_install = toml_bool_or_default(perms, "always_confirm_install", true);
        config->permissions.always_confirm_command = toml_bool_or_default(perms, "always_confirm_command", true);
        config->permissions.remember_choices = toml_bool_or_default(perms, "remember_choices", true);
    }

    /* Parse [logging] section */
    toml_table_t* logging = toml_table_in(root, "logging");
    if (logging) {
        char* level = toml_string_or_null(logging, "level");
        if (level) { free(config->logging.level); config->logging.level = level; }

        config->logging.colors = toml_bool_or_default(logging, "colors", true);
        config->logging.timestamps = toml_bool_or_default(logging, "timestamps", false);

        config->logging.file = toml_string_or_null(logging, "file");
    }

    /* Parse [ai] section (basic settings) */
    toml_table_t* ai = toml_table_in(root, "ai");
    if (ai) {
        config->ai.default_provider = toml_string_or_null(ai, "default_provider");
        config->ai.fallback_provider = toml_string_or_null(ai, "fallback_provider");
        config->ai.timeout = toml_int_or_default(ai, "timeout", 300);
        config->ai.max_tokens = toml_int_or_default(ai, "max_tokens", 1024);
        config->ai.temperature = (float)toml_double_or_default(ai, "temperature", 0.7);
    }

    toml_free(root);
    return config;
}

/* ========================================================================
 * Config Free
 * ======================================================================== */

void config_free(Config* config) {
    if (!config) return;

    free(config->config_path);

    /* Project */
    free(config->project.name);
    free(config->project.language);
    free(config->project.build_system);

    /* Build */
    free(config->build.type);
    free(config->build.build_dir);

    /* Logging */
    free(config->logging.level);
    free(config->logging.file);

    /* AI */
    free(config->ai.default_provider);
    free(config->ai.fallback_provider);

    free(config);
}

/* ========================================================================
 * Config Application
 * ======================================================================== */

LogLevel config_parse_log_level(const char* level_str) {
    if (!level_str) return LOG_LEVEL_INFO;

    if (strcmp(level_str, "debug") == 0) return LOG_LEVEL_DEBUG;
    if (strcmp(level_str, "info") == 0) return LOG_LEVEL_INFO;
    if (strcmp(level_str, "warning") == 0) return LOG_LEVEL_WARNING;
    if (strcmp(level_str, "error") == 0) return LOG_LEVEL_ERROR;
    if (strcmp(level_str, "none") == 0) return LOG_LEVEL_NONE;

    return LOG_LEVEL_INFO;
}

void config_apply_logging(const Config* config) {
    if (!config) return;

    /* Apply log level */
    log_set_level(config_parse_log_level(config->logging.level));

    /* Apply color setting */
    log_set_colors(config->logging.colors);

    /* Apply file logging if specified */
    if (config->logging.file) {
        log_set_file(config->logging.file);
    }
}

const char* config_get_build_type(const Config* config) {
    return config && config->build.type ? config->build.type : "Debug";
}

const char* config_get_build_dir(const Config* config) {
    return config && config->build.build_dir ? config->build.build_dir : "build";
}
