/**
 * @file solution_generator.c
 * @brief Generate fix actions for detected errors
 */

#include "cyxmake/error_recovery.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
    #define strdup _strdup
    #define strcasecmp _stricmp
#endif

/* Forward declaration */
char* extract_error_detail(const char* error_output, ErrorPatternType type);

/* Helper to create a fix action */
static FixAction* create_fix_action(FixActionType type,
                                    const char* description,
                                    const char* command,
                                    const char* target,
                                    const char* value,
                                    bool requires_confirmation) {
    FixAction* action = calloc(1, sizeof(FixAction));
    if (!action) return NULL;

    action->type = type;
    action->description = description ? strdup(description) : NULL;
    action->command = command ? strdup(command) : NULL;
    action->target = target ? strdup(target) : NULL;
    action->value = value ? strdup(value) : NULL;
    action->requires_confirmation = requires_confirmation;

    return action;
}

/* Common package mappings - error name to platform-specific package names */
typedef struct {
    const char* error_name;      /* Name from error message (e.g., "SDL2", "curl") */
    const char* generic_pkg;     /* Generic/canonical package name */
    const char* ubuntu_pkg;      /* apt package name */
    const char* fedora_pkg;      /* dnf/yum package name */
    const char* arch_pkg;        /* pacman package name */
    const char* macos_pkg;       /* brew package name */
    const char* vcpkg_pkg;       /* vcpkg package name */
    const char* winget_pkg;      /* winget package name */
} PackageMapping;

static const PackageMapping package_map[] = {
    /* Threading */
    {"pthread",  "pthread",        "libpthread-stubs0-dev", "glibc-devel",     "glibc",          NULL,        NULL,        NULL},

    /* Graphics/SDL */
    {"SDL2",     "sdl2",           "libsdl2-dev",           "SDL2-devel",      "sdl2",           "sdl2",      "sdl2",      NULL},
    {"SDL",      "sdl",            "libsdl1.2-dev",         "SDL-devel",       "sdl",            "sdl",       "sdl1",      NULL},
    {"OpenGL",   "opengl",         "libgl1-mesa-dev",       "mesa-libGL-devel","mesa",           NULL,        "opengl",    NULL},
    {"GLEW",     "glew",           "libglew-dev",           "glew-devel",      "glew",           "glew",      "glew",      NULL},
    {"GLFW",     "glfw",           "libglfw3-dev",          "glfw-devel",      "glfw",           "glfw",      "glfw3",     NULL},
    {"vulkan",   "vulkan",         "libvulkan-dev",         "vulkan-devel",    "vulkan-icd-loader","vulkan-loader","vulkan", NULL},

    /* Networking */
    {"curl",     "curl",           "libcurl4-openssl-dev",  "libcurl-devel",   "curl",           "curl",      "curl",      NULL},
    {"ssl",      "openssl",        "libssl-dev",            "openssl-devel",   "openssl",        "openssl",   "openssl",   NULL},
    {"openssl",  "openssl",        "libssl-dev",            "openssl-devel",   "openssl",        "openssl",   "openssl",   NULL},

    /* Compression */
    {"z",        "zlib",           "zlib1g-dev",            "zlib-devel",      "zlib",           "zlib",      "zlib",      NULL},
    {"zlib",     "zlib",           "zlib1g-dev",            "zlib-devel",      "zlib",           "zlib",      "zlib",      NULL},
    {"lz4",      "lz4",            "liblz4-dev",            "lz4-devel",       "lz4",            "lz4",       "lz4",       NULL},
    {"zstd",     "zstd",           "libzstd-dev",           "libzstd-devel",   "zstd",           "zstd",      "zstd",      NULL},

    /* XML/JSON */
    {"xml2",     "libxml2",        "libxml2-dev",           "libxml2-devel",   "libxml2",        "libxml2",   "libxml2",   NULL},
    {"json-c",   "json-c",         "libjson-c-dev",         "json-c-devel",    "json-c",         "json-c",    "json-c",    NULL},

    /* Image */
    {"png",      "libpng",         "libpng-dev",            "libpng-devel",    "libpng",         "libpng",    "libpng",    NULL},
    {"jpeg",     "libjpeg",        "libjpeg-dev",           "libjpeg-devel",   "libjpeg-turbo",  "jpeg",      "libjpeg-turbo", NULL},
    {"tiff",     "libtiff",        "libtiff-dev",           "libtiff-devel",   "libtiff",        "libtiff",   "tiff",      NULL},

    /* Math/Science */
    {"gmp",      "gmp",            "libgmp-dev",            "gmp-devel",       "gmp",            "gmp",       "gmp",       NULL},
    {"fftw",     "fftw",           "libfftw3-dev",          "fftw-devel",      "fftw",           "fftw",      "fftw3",     NULL},

    /* Boost */
    {"boost",    "boost",          "libboost-all-dev",      "boost-devel",     "boost",          "boost",     "boost",     NULL},

    /* Database */
    {"sqlite3",  "sqlite3",        "libsqlite3-dev",        "sqlite-devel",    "sqlite",         "sqlite3",   "sqlite3",   NULL},
    {"pq",       "postgresql",     "libpq-dev",             "postgresql-devel","postgresql-libs","libpq",     "libpq",     NULL},
    {"mysql",    "mysql",          "libmysqlclient-dev",    "mysql-devel",     "mariadb-libs",   "mysql",     "libmysql",  NULL},

    /* Audio */
    {"openal",   "openal",         "libopenal-dev",         "openal-soft-devel","openal",        "openal-soft","openal-soft",NULL},
    {"portaudio","portaudio",      "portaudio19-dev",       "portaudio-devel", "portaudio",      "portaudio", "portaudio", NULL},

    /* Misc */
    {"ncurses",  "ncurses",        "libncurses5-dev",       "ncurses-devel",   "ncurses",        "ncurses",   "ncurses",   NULL},
    {"readline", "readline",       "libreadline-dev",       "readline-devel",  "readline",       "readline",  "readline",  NULL},
    {"fmt",      "fmt",            "libfmt-dev",            "fmt-devel",       "fmt",            "fmt",       "fmt",       NULL},
    {"spdlog",   "spdlog",         "libspdlog-dev",         "spdlog-devel",    "spdlog",         "spdlog",    "spdlog",    NULL},

    /* End marker */
    {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};

/* Get the canonical package name for a library */
static const char* get_canonical_package_name(const char* library_name) {
    if (!library_name) return NULL;

    for (int i = 0; package_map[i].error_name != NULL; i++) {
        if (strcasecmp(library_name, package_map[i].error_name) == 0) {
            return package_map[i].generic_pkg;
        }
    }

    /* Return original name if not found in mapping */
    return library_name;
}

/* Generate install commands for different platforms (legacy fallback) */
static char* get_install_command(const char* package_name, const ProjectContext* ctx) {
    char buffer[512];
    const char* os = NULL;
    (void)ctx;  /* Platform not in ProjectContext yet */

    if (!os) {
#ifdef _WIN32
        os = "windows";
#elif defined(__APPLE__)
        os = "macos";
#else
        os = "linux";
#endif
    }

    /* Find package in map */
    const char* mapped_package = package_name;
    for (int i = 0; package_map[i].error_name != NULL; i++) {
        if (strcasecmp(package_name, package_map[i].error_name) == 0) {
            if (strcmp(os, "linux") == 0 && package_map[i].ubuntu_pkg) {
                mapped_package = package_map[i].ubuntu_pkg;
            } else if (strcmp(os, "macos") == 0 && package_map[i].macos_pkg) {
                mapped_package = package_map[i].macos_pkg;
            } else if (strcmp(os, "windows") == 0 && package_map[i].vcpkg_pkg) {
                mapped_package = package_map[i].vcpkg_pkg;
            }
            break;
        }
    }

    /* Generate platform-specific command */
    if (strcmp(os, "linux") == 0) {
        snprintf(buffer, sizeof(buffer), "sudo apt-get install -y %s", mapped_package);
    } else if (strcmp(os, "macos") == 0) {
        snprintf(buffer, sizeof(buffer), "brew install %s", mapped_package);
    } else if (strcmp(os, "windows") == 0) {
        snprintf(buffer, sizeof(buffer), "vcpkg install %s", mapped_package);
    } else {
        snprintf(buffer, sizeof(buffer), "# Install %s for your platform", package_name);
    }

    return strdup(buffer);
}

/* Generate CMake find commands */
static char* generate_cmake_find(const char* library_name) {
    char buffer[512];

    /* Common CMake package names */
    struct {
        const char* lib_name;
        const char* cmake_name;
    } cmake_map[] = {
        {"pthread", "Threads"},
        {"SDL2", "SDL2"},
        {"OpenGL", "OpenGL"},
        {"boost", "Boost"},
        {"curl", "CURL"},
        {"ssl", "OpenSSL"},
        {"z", "ZLIB"},
        {"xml2", "LibXml2"},
        {"png", "PNG"},
        {"jpeg", "JPEG"},
        {NULL, NULL}
    };

    const char* cmake_name = library_name;
    for (int i = 0; cmake_map[i].lib_name; i++) {
        if (strcasecmp(library_name, cmake_map[i].lib_name) == 0) {
            cmake_name = cmake_map[i].cmake_name;
            break;
        }
    }

    snprintf(buffer, sizeof(buffer),
             "find_package(%s REQUIRED)\n"
             "target_link_libraries(${PROJECT_NAME} ${%s_LIBRARIES})",
             cmake_name, cmake_name);

    return strdup(buffer);
}

/* Generate fix actions for missing library */
static FixAction** generate_missing_library_fixes(const char* library_name,
                                                  const ProjectContext* ctx,
                                                  size_t* fix_count) {
    FixAction** fixes = calloc(4, sizeof(FixAction*));
    if (!fixes) {
        *fix_count = 0;
        return NULL;
    }

    int count = 0;

    /* Get canonical package name for tool registry */
    const char* canonical_pkg = get_canonical_package_name(library_name);

    /* Fix 1: Install the package */
    /* Note: When tool registry is available, it uses 'target' field (canonical name)
     * When falling back to legacy system(), it uses 'command' field */
    char* install_cmd = get_install_command(library_name, ctx);
    if (install_cmd) {
        char* desc = malloc(256);
        if (desc) {
            snprintf(desc, 256, "Install %s library", library_name);
            fixes[count++] = create_fix_action(
                FIX_ACTION_INSTALL_PACKAGE,
                desc,
                install_cmd,        /* Legacy command for fallback */
                canonical_pkg,      /* Canonical name for tool registry */
                NULL,
                true
            );
            free(desc);  /* create_fix_action makes a copy */
        }
        free(install_cmd);
    }

    /* Fix 2: Add to CMakeLists.txt if using CMake */
    if (ctx && ctx->build_system.type == BUILD_CMAKE) {
        char* cmake_code = generate_cmake_find(library_name);
        fixes[count++] = create_fix_action(
            FIX_ACTION_MODIFY_FILE,
            "Add library to CMakeLists.txt",
            NULL,
            "CMakeLists.txt",
            cmake_code,
            true
        );
        free(cmake_code);
    }

    /* Fix 3: Set library path */
    char* path_desc = malloc(256);
    if (path_desc) {
        snprintf(path_desc, 256, "Set %s library path", library_name);
        fixes[count++] = create_fix_action(
            FIX_ACTION_SET_ENV_VAR,
            path_desc,
            NULL,
            "LD_LIBRARY_PATH",
            "/usr/local/lib:/usr/lib",
            false
        );
        free(path_desc);
    }

    /* Fix 4: Clean and rebuild */
    fixes[count++] = create_fix_action(
        FIX_ACTION_CLEAN_BUILD,
        "Clean build directory and rebuild",
        "rm -rf build && mkdir build",
        NULL,
        NULL,
        false
    );

    *fix_count = count;
    return fixes;
}

/* Generate fix actions for missing header */
static FixAction** generate_missing_header_fixes(const char* header_name,
                                                 const ProjectContext* ctx,
                                                 size_t* fix_count) {
    FixAction** fixes = calloc(3, sizeof(FixAction*));
    if (!fixes) {
        *fix_count = 0;
        return NULL;
    }

    int count = 0;

    /* Determine package name from header */
    char package_name[128];
    if (strstr(header_name, "SDL")) {
        snprintf(package_name, sizeof(package_name), "SDL2");
    } else if (strstr(header_name, "GL/gl")) {
        snprintf(package_name, sizeof(package_name), "OpenGL");
    } else if (strstr(header_name, "GLEW") || strstr(header_name, "glew")) {
        snprintf(package_name, sizeof(package_name), "GLEW");
    } else if (strstr(header_name, "GLFW") || strstr(header_name, "glfw")) {
        snprintf(package_name, sizeof(package_name), "GLFW");
    } else if (strstr(header_name, "vulkan")) {
        snprintf(package_name, sizeof(package_name), "vulkan");
    } else if (strstr(header_name, "boost")) {
        snprintf(package_name, sizeof(package_name), "boost");
    } else if (strstr(header_name, "curl")) {
        snprintf(package_name, sizeof(package_name), "curl");
    } else if (strstr(header_name, "openssl") || strstr(header_name, "ssl")) {
        snprintf(package_name, sizeof(package_name), "openssl");
    } else if (strstr(header_name, "zlib") || strstr(header_name, "zconf")) {
        snprintf(package_name, sizeof(package_name), "zlib");
    } else if (strstr(header_name, "png")) {
        snprintf(package_name, sizeof(package_name), "png");
    } else if (strstr(header_name, "jpeg") || strstr(header_name, "jpeglib")) {
        snprintf(package_name, sizeof(package_name), "jpeg");
    } else if (strstr(header_name, "sqlite3")) {
        snprintf(package_name, sizeof(package_name), "sqlite3");
    } else if (strstr(header_name, "fmt")) {
        snprintf(package_name, sizeof(package_name), "fmt");
    } else if (strstr(header_name, "spdlog")) {
        snprintf(package_name, sizeof(package_name), "spdlog");
    } else {
        /* Extract base name */
        snprintf(package_name, sizeof(package_name), "%s", header_name);
        char* slash = strrchr(package_name, '/');
        if (slash) {
            memmove(package_name, slash + 1, strlen(slash));
        }
        char* dot = strchr(package_name, '.');
        if (dot) *dot = '\0';
    }

    /* Get canonical package name for tool registry */
    const char* canonical_pkg = get_canonical_package_name(package_name);

    /* Fix 1: Install development package */
    char* install_cmd = get_install_command(package_name, ctx);
    if (install_cmd) {
        char desc[256];
        snprintf(desc, sizeof(desc), "Install %s development files", package_name);
        fixes[count++] = create_fix_action(
            FIX_ACTION_INSTALL_PACKAGE,
            desc,
            install_cmd,        /* Legacy command */
            canonical_pkg,      /* Canonical name for tool registry */
            NULL,
            true
        );
        free(install_cmd);
    }

    /* Fix 2: Add include path */
    fixes[count++] = create_fix_action(
        FIX_ACTION_MODIFY_FILE,
        "Add include directory to build configuration",
        NULL,
        "CMakeLists.txt",
        "include_directories(/usr/local/include)",
        true
    );

    /* Fix 3: Create missing header (if local) */
    if (!strchr(header_name, '/') && strchr(header_name, '.')) {
        char desc[256];
        snprintf(desc, sizeof(desc), "Create empty %s file", header_name);
        fixes[count++] = create_fix_action(
            FIX_ACTION_CREATE_FILE,
            desc,
            NULL,
            header_name,
            "/* Auto-generated header file */\n#pragma once\n",
            true
        );
    }

    *fix_count = count;
    return fixes;
}

/* Generate fix actions for missing file */
static FixAction** generate_missing_file_fixes(const char* file_path,
                                               const ProjectContext* ctx,
                                               size_t* fix_count) {
    (void)ctx;  /* Unused for now */

    FixAction** fixes = calloc(2, sizeof(FixAction*));
    if (!fixes) {
        *fix_count = 0;
        return NULL;
    }

    int count = 0;

    /* Fix 1: Create the missing file */
    char desc[256];
    snprintf(desc, sizeof(desc), "Create missing file: %s", file_path);
    fixes[count++] = create_fix_action(
        FIX_ACTION_CREATE_FILE,
        desc,
        NULL,
        file_path,
        "",
        true
    );

    /* Fix 2: Check working directory */
    fixes[count++] = create_fix_action(
        FIX_ACTION_RUN_COMMAND,
        "Verify current working directory",
        "pwd && ls -la",
        NULL,
        NULL,
        false
    );

    *fix_count = count;
    return fixes;
}

/* Generate fix actions for permission denied */
static FixAction** generate_permission_fixes(const char* resource,
                                             const ProjectContext* ctx,
                                             size_t* fix_count) {
    (void)ctx;  /* Unused for now */

    FixAction** fixes = calloc(3, sizeof(FixAction*));
    if (!fixes) {
        *fix_count = 0;
        return NULL;
    }

    int count = 0;

    /* Fix 1: Change permissions */
    char chmod_cmd[512];
    if (resource && strlen(resource) > 0) {
        snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod 755 %s", resource);
    } else {
        snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod -R 755 .");
    }

    fixes[count++] = create_fix_action(
        FIX_ACTION_RUN_COMMAND,
        "Fix file permissions",
        chmod_cmd,
        NULL,
        NULL,
        true
    );

    /* Fix 2: Change ownership */
    fixes[count++] = create_fix_action(
        FIX_ACTION_RUN_COMMAND,
        "Change file ownership to current user",
        "sudo chown -R $(whoami) .",
        NULL,
        NULL,
        true
    );

    /* Fix 3: Run with elevated privileges */
    fixes[count++] = create_fix_action(
        FIX_ACTION_RETRY,
        "Retry build with elevated privileges (sudo)",
        NULL,
        NULL,
        NULL,
        true
    );

    *fix_count = count;
    return fixes;
}

/* Generate fix actions for disk full */
static FixAction** generate_disk_full_fixes(const ProjectContext* ctx,
                                            size_t* fix_count) {
    (void)ctx;  /* Unused for now */

    FixAction** fixes = calloc(3, sizeof(FixAction*));
    if (!fixes) {
        *fix_count = 0;
        return NULL;
    }

    int count = 0;

    /* Fix 1: Clean build artifacts */
    fixes[count++] = create_fix_action(
        FIX_ACTION_CLEAN_BUILD,
        "Clean all build artifacts",
        "rm -rf build/* && rm -rf *.o *.obj *.exe",
        NULL,
        NULL,
        true
    );

    /* Fix 2: Clear package cache */
    fixes[count++] = create_fix_action(
        FIX_ACTION_RUN_COMMAND,
        "Clear package manager cache",
        "sudo apt-get clean || brew cleanup || true",
        NULL,
        NULL,
        true
    );

    /* Fix 3: Show disk usage */
    fixes[count++] = create_fix_action(
        FIX_ACTION_RUN_COMMAND,
        "Show disk usage",
        "df -h . && du -sh * | sort -h | tail -20",
        NULL,
        NULL,
        false
    );

    *fix_count = count;
    return fixes;
}

/* Generate fix actions for CMake version compatibility */
static FixAction** generate_cmake_version_fixes(const char* min_version,
                                                 const ProjectContext* ctx,
                                                 size_t* fix_count) {
    (void)ctx;  /* May use for project path in future */

    FixAction** fixes = calloc(2, sizeof(FixAction*));
    if (!fixes) {
        *fix_count = 0;
        return NULL;
    }

    int count = 0;

    /* Determine the target version - use at least 3.10 for compatibility */
    const char* target_version = "3.10";
    if (min_version && strcmp(min_version, "3.5") == 0) {
        target_version = "3.10";  /* If CMake says < 3.5, use 3.10 */
    }

    /* Fix 1: Update cmake_minimum_required in CMakeLists.txt */
    char desc[256];
    snprintf(desc, sizeof(desc),
             "Update cmake_minimum_required to VERSION %s in CMakeLists.txt",
             target_version);

    fixes[count++] = create_fix_action(
        FIX_ACTION_FIX_CMAKE_VERSION,
        desc,
        NULL,                    /* No shell command needed */
        "CMakeLists.txt",        /* Target file */
        (char*)target_version,   /* New version to set */
        false                    /* Auto-apply - safe operation */
    );

    /* Fix 2: Retry build after fix */
    fixes[count++] = create_fix_action(
        FIX_ACTION_RETRY,
        "Retry build after CMake version fix",
        NULL,
        NULL,
        NULL,
        false
    );

    *fix_count = count;
    return fixes;
}

/* Main solution generation function */
FixAction** solution_generate(ErrorPatternType pattern_type,
                              const char* error_details,
                              const ProjectContext* ctx,
                              size_t* fix_count) {
    if (!fix_count) return NULL;
    *fix_count = 0;

    log_debug("Generating fixes for pattern type %d, detail: %s",
              pattern_type, error_details ? error_details : "none");

    switch (pattern_type) {
        case ERROR_PATTERN_CMAKE_VERSION:
            return generate_cmake_version_fixes(error_details, ctx, fix_count);

        case ERROR_PATTERN_MISSING_LIBRARY:
            return generate_missing_library_fixes(error_details, ctx, fix_count);

        case ERROR_PATTERN_MISSING_HEADER:
            return generate_missing_header_fixes(error_details, ctx, fix_count);

        case ERROR_PATTERN_MISSING_FILE:
            return generate_missing_file_fixes(error_details, ctx, fix_count);

        case ERROR_PATTERN_PERMISSION_DENIED:
            return generate_permission_fixes(error_details, ctx, fix_count);

        case ERROR_PATTERN_DISK_FULL:
            return generate_disk_full_fixes(ctx, fix_count);

        case ERROR_PATTERN_SYNTAX_ERROR:
        case ERROR_PATTERN_UNDEFINED_REFERENCE:
            /* These typically need manual code fixes */
            {
                FixAction** fixes = calloc(1, sizeof(FixAction*));
                if (fixes) {
                    fixes[0] = create_fix_action(
                        FIX_ACTION_NONE,
                        "Manual code fix required - check error output",
                        NULL,
                        NULL,
                        NULL,
                        false
                    );
                    *fix_count = 1;
                }
                return fixes;
            }

        case ERROR_PATTERN_VERSION_MISMATCH:
            /* Version issues need specific handling */
            {
                FixAction** fixes = calloc(2, sizeof(FixAction*));
                if (fixes) {
                    int count = 0;
                    fixes[count++] = create_fix_action(
                        FIX_ACTION_RUN_COMMAND,
                        "Update all packages",
                        "sudo apt-get update && sudo apt-get upgrade",
                        NULL,
                        NULL,
                        true
                    );
                    fixes[count++] = create_fix_action(
                        FIX_ACTION_CLEAN_BUILD,
                        "Clean and rebuild",
                        NULL,
                        NULL,
                        NULL,
                        false
                    );
                    *fix_count = count;
                }
                return fixes;
            }

        case ERROR_PATTERN_NETWORK_ERROR:
            /* Network issues */
            {
                FixAction** fixes = calloc(2, sizeof(FixAction*));
                if (fixes) {
                    int count = 0;
                    fixes[count++] = create_fix_action(
                        FIX_ACTION_RUN_COMMAND,
                        "Check network connectivity",
                        "ping -c 4 8.8.8.8",
                        NULL,
                        NULL,
                        false
                    );
                    fixes[count++] = create_fix_action(
                        FIX_ACTION_RETRY,
                        "Retry after checking network",
                        NULL,
                        NULL,
                        NULL,
                        false
                    );
                    *fix_count = count;
                }
                return fixes;
            }

        case ERROR_PATTERN_TIMEOUT:
            /* Timeout issues */
            {
                FixAction** fixes = calloc(1, sizeof(FixAction*));
                if (fixes) {
                    fixes[0] = create_fix_action(
                        FIX_ACTION_RETRY,
                        "Retry with increased timeout",
                        NULL,
                        NULL,
                        NULL,
                        false
                    );
                    *fix_count = 1;
                }
                return fixes;
            }

        case ERROR_PATTERN_UNKNOWN:
        default:
            /* Unknown error - suggest generic fixes */
            {
                FixAction** fixes = calloc(2, sizeof(FixAction*));
                if (fixes) {
                    int count = 0;
                    fixes[count++] = create_fix_action(
                        FIX_ACTION_CLEAN_BUILD,
                        "Clean and rebuild",
                        NULL,
                        NULL,
                        NULL,
                        false
                    );
                    fixes[count++] = create_fix_action(
                        FIX_ACTION_RETRY,
                        "Retry build",
                        NULL,
                        NULL,
                        NULL,
                        false
                    );
                    *fix_count = count;
                }
                return fixes;
            }
    }
}

/* Free fix actions array */
void fix_actions_free(FixAction** actions, size_t count) {
    if (!actions) return;

    for (size_t i = 0; i < count; i++) {
        if (actions[i]) {
            free((void*)actions[i]->description);  /* Cast away const */
            free(actions[i]->command);
            free(actions[i]->target);
            free(actions[i]->value);
            free(actions[i]);
        }
    }

    free(actions);
}