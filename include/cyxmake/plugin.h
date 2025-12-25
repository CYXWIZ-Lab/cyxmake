/**
 * @file plugin.h
 * @brief CyxMake Plugin System API
 *
 * This header defines the plugin interface for extending CyxMake with
 * custom tools, error patterns, and AI providers.
 *
 * Plugins are shared libraries (.so/.dll/.dylib) that export specific
 * functions following the CyxMake plugin protocol.
 */

#ifndef CYXMAKE_PLUGIN_H
#define CYXMAKE_PLUGIN_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Plugin Version and Compatibility
 * ======================================================================== */

#define CYXMAKE_PLUGIN_API_VERSION 1
#define CYXMAKE_PLUGIN_API_VERSION_STR "1.0"

/**
 * Plugin type flags
 */
typedef enum {
    PLUGIN_TYPE_TOOL       = (1 << 0),  /* Provides custom tools */
    PLUGIN_TYPE_PATTERN    = (1 << 1),  /* Provides error patterns */
    PLUGIN_TYPE_PROVIDER   = (1 << 2),  /* Provides AI provider */
    PLUGIN_TYPE_HOOK       = (1 << 3),  /* Provides lifecycle hooks */
    PLUGIN_TYPE_COMMAND    = (1 << 4),  /* Provides REPL commands */
} PluginType;

/**
 * Plugin load priority
 */
typedef enum {
    PLUGIN_PRIORITY_LOW    = 0,
    PLUGIN_PRIORITY_NORMAL = 50,
    PLUGIN_PRIORITY_HIGH   = 100,
} PluginPriority;

/* ========================================================================
 * Plugin Information
 * ======================================================================== */

/**
 * Plugin metadata
 */
typedef struct {
    const char* name;           /* Plugin name (unique identifier) */
    const char* display_name;   /* Human-readable name */
    const char* version;        /* Plugin version (semver) */
    const char* author;         /* Plugin author */
    const char* description;    /* Short description */
    const char* url;            /* Plugin homepage/repository */
    const char* license;        /* License identifier (e.g., "MIT") */
    unsigned int types;         /* Bitmask of PluginType */
    int api_version;            /* CYXMAKE_PLUGIN_API_VERSION */
    PluginPriority priority;    /* Load priority */
} PluginInfo;

/**
 * Plugin context (passed to all plugin functions)
 */
typedef struct PluginContext PluginContext;

/* ========================================================================
 * Plugin Lifecycle Functions
 * ======================================================================== */

/**
 * Plugin initialization function
 * Called when plugin is loaded
 *
 * @param ctx Plugin context
 * @return true if initialization succeeded
 */
typedef bool (*PluginInitFunc)(PluginContext* ctx);

/**
 * Plugin shutdown function
 * Called when plugin is unloaded
 *
 * @param ctx Plugin context
 */
typedef void (*PluginShutdownFunc)(PluginContext* ctx);

/**
 * Get plugin information
 * Must be exported as 'cyxmake_plugin_info'
 *
 * @return Static plugin info (do not free)
 */
typedef const PluginInfo* (*PluginInfoFunc)(void);

/* ========================================================================
 * Custom Tool Interface
 * ======================================================================== */

/**
 * Tool capability flags
 */
typedef enum {
    TOOL_CAP_BUILD      = (1 << 0),
    TOOL_CAP_ANALYZE    = (1 << 1),
    TOOL_CAP_FORMAT     = (1 << 2),
    TOOL_CAP_LINT       = (1 << 3),
    TOOL_CAP_TEST       = (1 << 4),
    TOOL_CAP_DEPLOY     = (1 << 5),
} ToolCapability;

/**
 * Custom tool definition
 */
typedef struct {
    const char* name;           /* Tool name */
    const char* description;    /* Tool description */
    const char* version;        /* Tool version */
    unsigned int capabilities;  /* Bitmask of ToolCapability */

    /**
     * Check if tool is available
     * @return true if tool can be used
     */
    bool (*is_available)(void);

    /**
     * Execute tool
     * @param command Command to execute
     * @param args Arguments array (NULL-terminated)
     * @param working_dir Working directory
     * @param stdout_buf Output buffer for stdout
     * @param stdout_size Size of stdout buffer
     * @param stderr_buf Output buffer for stderr
     * @param stderr_size Size of stderr buffer
     * @return Exit code
     */
    int (*execute)(const char* command,
                   char* const* args,
                   const char* working_dir,
                   char* stdout_buf, size_t stdout_size,
                   char* stderr_buf, size_t stderr_size);

    /**
     * Get tool help
     * @return Help text (do not free)
     */
    const char* (*get_help)(void);
} PluginTool;

/**
 * Register tools
 * Called during plugin initialization
 *
 * @param ctx Plugin context
 * @param tools Array of tools to register (NULL-terminated)
 * @return Number of tools registered
 */
typedef int (*PluginRegisterToolsFunc)(PluginContext* ctx, const PluginTool** tools);

/* ========================================================================
 * Custom Error Pattern Interface
 * ======================================================================== */

/**
 * Error pattern definition
 */
typedef struct {
    const char* name;           /* Pattern name */
    const char* description;    /* Pattern description */
    const char** patterns;      /* Regex patterns (NULL-terminated) */
    int priority;               /* Match priority (higher = checked first) */

    /**
     * Generate fix suggestions
     * @param error_text Matched error text
     * @param suggestions Output array of suggestions
     * @param max_suggestions Maximum suggestions to return
     * @return Number of suggestions generated
     */
    int (*suggest_fixes)(const char* error_text,
                         char** suggestions,
                         int max_suggestions);
} PluginErrorPattern;

/**
 * Register error patterns
 *
 * @param ctx Plugin context
 * @param patterns Array of patterns (NULL-terminated)
 * @return Number of patterns registered
 */
typedef int (*PluginRegisterPatternsFunc)(PluginContext* ctx,
                                           const PluginErrorPattern** patterns);

/* ========================================================================
 * Custom AI Provider Interface
 * ======================================================================== */

/**
 * AI provider definition
 */
typedef struct {
    const char* name;           /* Provider name */
    const char* description;    /* Provider description */

    /**
     * Initialize provider
     * @param config_json JSON configuration
     * @return true if initialized
     */
    bool (*init)(const char* config_json);

    /**
     * Shutdown provider
     */
    void (*shutdown)(void);

    /**
     * Check if provider is available
     * @return true if ready to use
     */
    bool (*is_available)(void);

    /**
     * Send prompt and get response
     * @param prompt User prompt
     * @param system_prompt System prompt (can be NULL)
     * @param response_buf Output buffer for response
     * @param response_size Size of response buffer
     * @return true if successful
     */
    bool (*complete)(const char* prompt,
                     const char* system_prompt,
                     char* response_buf,
                     size_t response_size);

    /**
     * Get provider health status
     * @return Status message (do not free)
     */
    const char* (*get_status)(void);
} PluginAIProvider;

/**
 * Register AI provider
 *
 * @param ctx Plugin context
 * @param provider Provider to register
 * @return true if registered
 */
typedef bool (*PluginRegisterProviderFunc)(PluginContext* ctx,
                                            const PluginAIProvider* provider);

/* ========================================================================
 * Custom Command Interface
 * ======================================================================== */

/**
 * REPL command definition
 */
typedef struct {
    const char* name;           /* Command name (e.g., "mycommand") */
    const char* alias;          /* Short alias (e.g., "mc") */
    const char* description;    /* Command description */
    const char* usage;          /* Usage string */

    /**
     * Execute command
     * @param args Arguments after command name
     * @param output_buf Output buffer
     * @param output_size Size of output buffer
     * @return Exit code (0 = success)
     */
    int (*execute)(const char* args,
                   char* output_buf,
                   size_t output_size);

    /**
     * Tab completion
     * @param partial Partial input
     * @param completions Output array
     * @param max_completions Maximum completions
     * @return Number of completions
     */
    int (*complete)(const char* partial,
                    char** completions,
                    int max_completions);
} PluginCommand;

/**
 * Register commands
 *
 * @param ctx Plugin context
 * @param commands Array of commands (NULL-terminated)
 * @return Number of commands registered
 */
typedef int (*PluginRegisterCommandsFunc)(PluginContext* ctx,
                                           const PluginCommand** commands);

/* ========================================================================
 * Hook Interface
 * ======================================================================== */

/**
 * Hook events
 */
typedef enum {
    HOOK_PRE_BUILD,         /* Before build starts */
    HOOK_POST_BUILD,        /* After build completes */
    HOOK_PRE_FIX,           /* Before fix is applied */
    HOOK_POST_FIX,          /* After fix is applied */
    HOOK_ERROR_DETECTED,    /* When error is detected */
    HOOK_PROJECT_LOADED,    /* When project is analyzed */
} HookEvent;

/**
 * Hook callback
 * @param event Event type
 * @param data Event-specific data (JSON)
 * @return true to continue, false to cancel
 */
typedef bool (*PluginHookCallback)(HookEvent event, const char* data);

/**
 * Register hook
 *
 * @param ctx Plugin context
 * @param event Event to hook
 * @param callback Callback function
 * @return true if registered
 */
typedef bool (*PluginRegisterHookFunc)(PluginContext* ctx,
                                        HookEvent event,
                                        PluginHookCallback callback);

/* ========================================================================
 * Plugin Context API
 * ======================================================================== */

/**
 * Log a message from plugin
 */
void plugin_log_debug(PluginContext* ctx, const char* format, ...);
void plugin_log_info(PluginContext* ctx, const char* format, ...);
void plugin_log_warning(PluginContext* ctx, const char* format, ...);
void plugin_log_error(PluginContext* ctx, const char* format, ...);

/**
 * Get configuration value
 * @param ctx Plugin context
 * @param key Configuration key (e.g., "my_plugin.setting")
 * @return Value string (caller must free) or NULL
 */
char* plugin_get_config(PluginContext* ctx, const char* key);

/**
 * Set configuration value
 * @param ctx Plugin context
 * @param key Configuration key
 * @param value Value to set
 * @return true if set
 */
bool plugin_set_config(PluginContext* ctx, const char* key, const char* value);

/**
 * Get project root path
 * @param ctx Plugin context
 * @return Project path (do not free) or NULL
 */
const char* plugin_get_project_path(PluginContext* ctx);

/**
 * Get CyxMake version
 * @return Version string (do not free)
 */
const char* plugin_get_cyxmake_version(void);

/* ========================================================================
 * Plugin Manager API
 * ======================================================================== */

/**
 * Plugin manager (opaque)
 */
typedef struct PluginManager PluginManager;

/**
 * Create plugin manager
 * @param plugin_dir Directory to search for plugins
 * @return New plugin manager (caller must free)
 */
PluginManager* plugin_manager_create(const char* plugin_dir);

/**
 * Free plugin manager
 * @param mgr Manager to free
 */
void plugin_manager_free(PluginManager* mgr);

/**
 * Load all plugins from directory
 * @param mgr Plugin manager
 * @return Number of plugins loaded
 */
int plugin_manager_load_all(PluginManager* mgr);

/**
 * Load a specific plugin
 * @param mgr Plugin manager
 * @param path Path to plugin library
 * @return true if loaded
 */
bool plugin_manager_load(PluginManager* mgr, const char* path);

/**
 * Unload a plugin
 * @param mgr Plugin manager
 * @param name Plugin name
 * @return true if unloaded
 */
bool plugin_manager_unload(PluginManager* mgr, const char* name);

/**
 * Get loaded plugin info
 * @param mgr Plugin manager
 * @param name Plugin name
 * @return Plugin info or NULL
 */
const PluginInfo* plugin_manager_get_info(PluginManager* mgr, const char* name);

/**
 * List loaded plugins
 * @param mgr Plugin manager
 * @param count Output: number of plugins
 * @return Array of plugin names (caller must free array, not strings)
 */
const char** plugin_manager_list(PluginManager* mgr, int* count);

/* ========================================================================
 * Plugin Export Macros
 * ======================================================================== */

#ifdef _WIN32
    #define PLUGIN_EXPORT __declspec(dllexport)
#else
    #define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

/**
 * Required exports for a plugin:
 *
 * PLUGIN_EXPORT const PluginInfo* cyxmake_plugin_info(void);
 * PLUGIN_EXPORT bool cyxmake_plugin_init(PluginContext* ctx);
 * PLUGIN_EXPORT void cyxmake_plugin_shutdown(PluginContext* ctx);
 *
 * Optional exports (depending on plugin type):
 *
 * PLUGIN_EXPORT int cyxmake_plugin_register_tools(PluginContext* ctx, const PluginTool** tools);
 * PLUGIN_EXPORT int cyxmake_plugin_register_patterns(PluginContext* ctx, const PluginErrorPattern** patterns);
 * PLUGIN_EXPORT bool cyxmake_plugin_register_provider(PluginContext* ctx, const PluginAIProvider* provider);
 * PLUGIN_EXPORT int cyxmake_plugin_register_commands(PluginContext* ctx, const PluginCommand** commands);
 */

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_PLUGIN_H */
