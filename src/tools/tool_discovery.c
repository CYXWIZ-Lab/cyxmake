/**
 * @file tool_discovery.c
 * @brief Tool discovery implementation
 */

#include "cyxmake/tool_executor.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
    #define PATH_SEP ";"
    #define strdup _strdup
    #define popen _popen
    #define pclose _pclose
#else
    #include <unistd.h>
    #define PATH_SEP ":"
#endif

/* Helper to execute command and get output */
static char* execute_and_capture(const char* command) {
    FILE* pipe = popen(command, "r");
    if (!pipe) return NULL;

    char buffer[1024];
    size_t total_size = 0;
    char* result = NULL;

    while (fgets(buffer, sizeof(buffer), pipe)) {
        size_t len = strlen(buffer);
        char* new_result = realloc(result, total_size + len + 1);
        if (!new_result) {
            free(result);
            pclose(pipe);
            return NULL;
        }
        result = new_result;
        memcpy(result + total_size, buffer, len);
        total_size += len;
        result[total_size] = '\0';
    }

    pclose(pipe);
    return result;
}

/* Find tool in PATH */
char* tool_find_in_path(const char* tool_name) {
    if (!tool_name) return NULL;

#ifdef _WIN32
    /* Try 'where' command on Windows */
    char command[512];
    snprintf(command, sizeof(command), "where %s 2>nul", tool_name);
    char* output = execute_and_capture(command);

    if (output) {
        /* Take first line */
        char* newline = strchr(output, '\n');
        if (newline) *newline = '\0';

        /* Remove \r if present */
        char* cr = strchr(output, '\r');
        if (cr) *cr = '\0';

        return output;
    }
#else
    /* Try 'which' command on Unix */
    char command[512];
    snprintf(command, sizeof(command), "which %s 2>/dev/null", tool_name);
    char* output = execute_and_capture(command);

    if (output) {
        /* Remove trailing newline */
        size_t len = strlen(output);
        if (len > 0 && output[len-1] == '\n') {
            output[len-1] = '\0';
        }
        return output;
    }
#endif

    return NULL;
}

/* Check if tool is available */
bool tool_is_available(const char* tool_name) {
    char* path = tool_find_in_path(tool_name);
    if (path) {
        free(path);
        return true;
    }
    return false;
}

/* Get tool version */
char* tool_get_version(const char* tool_name) {
    if (!tool_name) return NULL;

    /* Try common version flags */
    const char* version_flags[] = {
        "--version",
        "-version",
        "-v",
        "version",
        NULL
    };

    for (int i = 0; version_flags[i]; i++) {
        char command[512];
        snprintf(command, sizeof(command), "%s %s 2>&1", tool_name, version_flags[i]);

        char* output = execute_and_capture(command);
        if (output && strlen(output) > 0) {
            /* Take first line as version */
            char* newline = strchr(output, '\n');
            if (newline) *newline = '\0';

            return output;
        }
        free(output);
    }

    return NULL;
}

/* Create tool info */
static ToolInfo* create_tool_info(const char* name,
                                   const char* display_name,
                                   ToolType type,
                                   int subtype) {
    ToolInfo* tool = calloc(1, sizeof(ToolInfo));
    if (!tool) return NULL;

    tool->name = strdup(name);
    tool->display_name = display_name ? strdup(display_name) : strdup(name);
    tool->type = type;
    tool->subtype = subtype;
    tool->is_available = false;
    tool->path = NULL;
    tool->version = NULL;
    tool->capabilities = NULL;
    tool->capability_count = 0;

    return tool;
}

/* Discover a single tool */
static ToolInfo* discover_tool(const char* name,
                                const char* display_name,
                                ToolType type,
                                int subtype) {
    ToolInfo* tool = create_tool_info(name, display_name, type, subtype);
    if (!tool) return NULL;

    /* Find tool path */
    tool->path = tool_find_in_path(name);
    tool->is_available = (tool->path != NULL);

    if (tool->is_available) {
        /* Get version */
        tool->version = tool_get_version(name);
        log_debug("Discovered %s: %s at %s",
                 tool_type_to_string(type),
                 name,
                 tool->path);
    }

    return tool;
}

/* Discover package managers */
int tool_discover_package_managers(ToolRegistry* registry) {
    if (!registry) return 0;

    int discovered = 0;

    /* Define package managers to discover */
    struct {
        const char* name;
        const char* display_name;
        PackageManagerType type;
    } pkg_managers[] = {
        {"apt", "APT (Debian/Ubuntu)", PKG_MGR_APT},
        {"apt-get", "APT-GET (Debian/Ubuntu)", PKG_MGR_APT},
        {"yum", "YUM (RedHat/CentOS)", PKG_MGR_YUM},
        {"dnf", "DNF (Fedora)", PKG_MGR_DNF},
        {"pacman", "Pacman (Arch Linux)", PKG_MGR_PACMAN},
        {"brew", "Homebrew", PKG_MGR_BREW},
        {"vcpkg", "vcpkg", PKG_MGR_VCPKG},
        {"conan", "Conan", PKG_MGR_CONAN},
        {"npm", "npm", PKG_MGR_NPM},
        {"yarn", "Yarn", PKG_MGR_YARN},
        {"pip", "pip", PKG_MGR_PIP},
        {"pip3", "pip3", PKG_MGR_PIP},
        {"cargo", "Cargo (Rust)", PKG_MGR_CARGO},
        {"choco", "Chocolatey", PKG_MGR_CHOCO},
        {"winget", "Windows Package Manager", PKG_MGR_WINGET},
        {NULL, NULL, PKG_MGR_UNKNOWN}
    };

    for (int i = 0; pkg_managers[i].name; i++) {
        ToolInfo* tool = discover_tool(
            pkg_managers[i].name,
            pkg_managers[i].display_name,
            TOOL_TYPE_PACKAGE_MANAGER,
            pkg_managers[i].type
        );

        if (tool) {
            tool_registry_add(registry, tool);
            if (tool->is_available) {
                discovered++;
            }
        }
    }

    log_info("Discovered %d package manager(s)", discovered);
    return discovered;
}

/* Discover compilers */
int tool_discover_compilers(ToolRegistry* registry) {
    if (!registry) return 0;

    int discovered = 0;

    /* Define compilers to discover */
    const char* compilers[] = {
        "gcc", "g++", "clang", "clang++",
        "cl", "msvc", "icc", "icpc",
        "rustc", "gfortran", "javac",
        NULL
    };

    for (int i = 0; compilers[i]; i++) {
        ToolInfo* tool = discover_tool(
            compilers[i],
            NULL,
            TOOL_TYPE_COMPILER,
            0
        );

        if (tool) {
            tool_registry_add(registry, tool);
            if (tool->is_available) {
                discovered++;
            }
        }
    }

    log_info("Discovered %d compiler(s)", discovered);
    return discovered;
}

/* Discover build systems */
int tool_discover_build_systems(ToolRegistry* registry) {
    if (!registry) return 0;

    int discovered = 0;

    /* Define build systems to discover */
    const char* build_systems[] = {
        "cmake", "make", "ninja", "msbuild",
        "xcodebuild", "bazel", "buck", "gradle",
        "maven", "ant", "scons", "meson",
        NULL
    };

    for (int i = 0; build_systems[i]; i++) {
        ToolInfo* tool = discover_tool(
            build_systems[i],
            NULL,
            TOOL_TYPE_BUILD_SYSTEM,
            0
        );

        if (tool) {
            tool_registry_add(registry, tool);
            if (tool->is_available) {
                discovered++;
            }
        }
    }

    log_info("Discovered %d build system(s)", discovered);
    return discovered;
}

/* Discover all tools */
int tool_discover_all(ToolRegistry* registry) {
    if (!registry) return 0;

    log_info("Discovering available tools...");

    int total = 0;
    total += tool_discover_package_managers(registry);
    total += tool_discover_compilers(registry);
    total += tool_discover_build_systems(registry);

    /* Discover version control tools */
    const char* vcs_tools[] = {"git", "svn", "hg", "bzr", NULL};
    for (int i = 0; vcs_tools[i]; i++) {
        ToolInfo* tool = discover_tool(
            vcs_tools[i],
            NULL,
            TOOL_TYPE_VERSION_CONTROL,
            0
        );

        if (tool) {
            tool_registry_add(registry, tool);
            if (tool->is_available) total++;
        }
    }

    /* Discover linters */
    const char* linters[] = {
        "clang-tidy", "cppcheck", "eslint", "pylint",
        "shellcheck", "hlint", NULL
    };
    for (int i = 0; linters[i]; i++) {
        ToolInfo* tool = discover_tool(
            linters[i],
            NULL,
            TOOL_TYPE_LINTER,
            0
        );

        if (tool) {
            tool_registry_add(registry, tool);
            if (tool->is_available) total++;
        }
    }

    /* Discover formatters */
    const char* formatters[] = {
        "clang-format", "prettier", "black",
        "rustfmt", "gofmt", NULL
    };
    for (int i = 0; formatters[i]; i++) {
        ToolInfo* tool = discover_tool(
            formatters[i],
            NULL,
            TOOL_TYPE_FORMATTER,
            0
        );

        if (tool) {
            tool_registry_add(registry, tool);
            if (tool->is_available) total++;
        }
    }

    log_success("Discovered %d total tool(s)", total);
    return total;
}
