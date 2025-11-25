/**
 * @file tool_executor.h
 * @brief Tool discovery, registry, and execution system
 */

#ifndef CYXMAKE_TOOL_EXECUTOR_H
#define CYXMAKE_TOOL_EXECUTOR_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Tool types
 */
typedef enum {
    TOOL_TYPE_PACKAGE_MANAGER,
    TOOL_TYPE_COMPILER,
    TOOL_TYPE_BUILD_SYSTEM,
    TOOL_TYPE_VERSION_CONTROL,
    TOOL_TYPE_LINTER,
    TOOL_TYPE_FORMATTER,
    TOOL_TYPE_TEST_RUNNER,
    TOOL_TYPE_DEBUGGER,
    TOOL_TYPE_PROFILER,
    TOOL_TYPE_UNKNOWN
} ToolType;

/**
 * Package manager types
 */
typedef enum {
    PKG_MGR_APT,        /* Debian/Ubuntu */
    PKG_MGR_YUM,        /* RedHat/CentOS */
    PKG_MGR_DNF,        /* Fedora */
    PKG_MGR_PACMAN,     /* Arch Linux */
    PKG_MGR_BREW,       /* macOS Homebrew */
    PKG_MGR_VCPKG,      /* Microsoft vcpkg */
    PKG_MGR_CONAN,      /* Conan C/C++ */
    PKG_MGR_NPM,        /* Node.js */
    PKG_MGR_YARN,       /* Node.js */
    PKG_MGR_PIP,        /* Python */
    PKG_MGR_CARGO,      /* Rust */
    PKG_MGR_CHOCO,      /* Windows Chocolatey */
    PKG_MGR_WINGET,     /* Windows Package Manager */
    PKG_MGR_UNKNOWN
} PackageManagerType;

/**
 * Tool information
 */
typedef struct {
    char* name;              /* Tool name (e.g., "gcc", "apt", "cmake") */
    char* display_name;      /* Human-readable name */
    char* path;              /* Full path to executable */
    char* version;           /* Version string */
    ToolType type;           /* Tool type */
    int subtype;             /* Subtype (e.g., PackageManagerType) */
    bool is_available;       /* Is tool available on system */
    char** capabilities;     /* Array of capability strings */
    size_t capability_count;
} ToolInfo;

/**
 * Tool execution result
 */
typedef struct {
    int exit_code;
    char* stdout_output;
    char* stderr_output;
    bool success;
    double duration_sec;
} ToolExecResult;

/**
 * Tool execution options
 */
typedef struct {
    char** args;              /* Command arguments */
    size_t arg_count;
    char** env_vars;          /* Environment variables (KEY=VALUE) */
    size_t env_var_count;
    const char* working_dir;  /* Working directory */
    int timeout_sec;          /* Timeout in seconds (0 = no timeout) */
    bool capture_output;      /* Capture stdout/stderr */
    bool show_output;         /* Show output in real-time */
} ToolExecOptions;

/* ========================================================================
 * Tool Registry
 * ======================================================================== */

typedef struct ToolRegistry ToolRegistry;

/**
 * Create tool registry
 * @return Registry instance (caller must free)
 */
ToolRegistry* tool_registry_create(void);

/**
 * Free tool registry
 * @param registry Registry to free
 */
void tool_registry_free(ToolRegistry* registry);

/**
 * Register a tool
 * @param registry Tool registry
 * @param tool Tool information (registry takes ownership)
 * @return True if registered successfully
 */
bool tool_registry_add(ToolRegistry* registry, ToolInfo* tool);

/**
 * Find tool by name
 * @param registry Tool registry
 * @param name Tool name
 * @return Tool info, or NULL if not found (do not free)
 */
const ToolInfo* tool_registry_find(const ToolRegistry* registry, const char* name);

/**
 * Find tools by type
 * @param registry Tool registry
 * @param type Tool type
 * @param count Output: number of tools found
 * @return Array of tool pointers (do not free individual tools)
 */
const ToolInfo** tool_registry_find_by_type(const ToolRegistry* registry,
                                             ToolType type,
                                             size_t* count);

/**
 * Get all registered tools
 * @param registry Tool registry
 * @param count Output: number of tools
 * @return Array of tool pointers (do not free)
 */
const ToolInfo** tool_registry_get_all(const ToolRegistry* registry, size_t* count);

/* ========================================================================
 * Tool Discovery
 * ======================================================================== */

/**
 * Discover all available tools on the system
 * @param registry Tool registry to populate
 * @return Number of tools discovered
 */
int tool_discover_all(ToolRegistry* registry);

/**
 * Discover package managers
 * @param registry Tool registry to populate
 * @return Number of package managers discovered
 */
int tool_discover_package_managers(ToolRegistry* registry);

/**
 * Discover compilers
 * @param registry Tool registry to populate
 * @return Number of compilers discovered
 */
int tool_discover_compilers(ToolRegistry* registry);

/**
 * Discover build systems
 * @param registry Tool registry to populate
 * @return Number of build systems discovered
 */
int tool_discover_build_systems(ToolRegistry* registry);

/**
 * Check if a specific tool is available
 * @param tool_name Tool name to check
 * @return True if tool is available
 */
bool tool_is_available(const char* tool_name);

/**
 * Get tool version
 * @param tool_name Tool name
 * @return Version string (caller must free), or NULL if not available
 */
char* tool_get_version(const char* tool_name);

/**
 * Find tool in system PATH
 * @param tool_name Tool name
 * @return Full path to tool (caller must free), or NULL if not found
 */
char* tool_find_in_path(const char* tool_name);

/* ========================================================================
 * Tool Execution
 * ======================================================================== */

/**
 * Execute a tool command
 * @param tool Tool information
 * @param options Execution options
 * @return Execution result (caller must free)
 */
ToolExecResult* tool_execute(const ToolInfo* tool, const ToolExecOptions* options);

/**
 * Execute a tool by name
 * @param registry Tool registry
 * @param tool_name Tool name
 * @param options Execution options
 * @return Execution result (caller must free)
 */
ToolExecResult* tool_execute_by_name(const ToolRegistry* registry,
                                      const char* tool_name,
                                      const ToolExecOptions* options);

/**
 * Execute a command directly (without registry)
 * @param command Command to execute
 * @param args Arguments array (NULL-terminated)
 * @param working_dir Working directory (NULL for current)
 * @return Execution result (caller must free)
 */
ToolExecResult* tool_execute_command(const char* command,
                                      char* const* args,
                                      const char* working_dir);

/**
 * Free tool execution result
 * @param result Result to free
 */
void tool_exec_result_free(ToolExecResult* result);

/**
 * Create default execution options
 * @return Default options (caller must free)
 */
ToolExecOptions* tool_exec_options_create(void);

/**
 * Free execution options
 * @param options Options to free
 */
void tool_exec_options_free(ToolExecOptions* options);

/* ========================================================================
 * Package Manager Operations
 * ======================================================================== */

/**
 * Install a package using the system package manager
 * @param registry Tool registry
 * @param package_name Package name to install
 * @param options Execution options (NULL for defaults)
 * @return Execution result (caller must free)
 */
ToolExecResult* package_install(const ToolRegistry* registry,
                                 const char* package_name,
                                 const ToolExecOptions* options);

/**
 * Update package manager cache
 * @param registry Tool registry
 * @param options Execution options (NULL for defaults)
 * @return Execution result (caller must free)
 */
ToolExecResult* package_update(const ToolRegistry* registry,
                                const ToolExecOptions* options);

/**
 * Search for a package
 * @param registry Tool registry
 * @param package_name Package name to search
 * @return True if package exists
 */
bool package_search(const ToolRegistry* registry, const char* package_name);

/**
 * Get the best available package manager for the platform
 * @param registry Tool registry
 * @return Package manager tool info, or NULL if none found
 */
const ToolInfo* package_get_default_manager(const ToolRegistry* registry);

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

/**
 * Convert tool type to string
 * @param type Tool type
 * @return String representation
 */
const char* tool_type_to_string(ToolType type);

/**
 * Convert package manager type to string
 * @param type Package manager type
 * @return String representation
 */
const char* package_manager_to_string(PackageManagerType type);

/**
 * Free tool info
 * @param tool Tool info to free
 */
void tool_info_free(ToolInfo* tool);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_TOOL_EXECUTOR_H */
