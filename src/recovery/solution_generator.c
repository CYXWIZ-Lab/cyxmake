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

/* Generate install commands for different platforms */
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

    /* Common package mappings */
    struct {
        const char* error_name;
        const char* ubuntu_pkg;
        const char* macos_pkg;
        const char* windows_pkg;
    } package_map[] = {
        {"pthread", "libpthread-stubs0-dev", NULL, NULL},
        {"SDL2", "libsdl2-dev", "sdl2", "SDL2"},
        {"OpenGL", "libgl1-mesa-dev", NULL, "OpenGL"},
        {"boost", "libboost-all-dev", "boost", "boost"},
        {"curl", "libcurl4-openssl-dev", "curl", "curl"},
        {"ssl", "libssl-dev", "openssl", "openssl"},
        {"z", "zlib1g-dev", "zlib", "zlib"},
        {"xml2", "libxml2-dev", "libxml2", "libxml2"},
        {"png", "libpng-dev", "libpng", "libpng"},
        {"jpeg", "libjpeg-dev", "jpeg", "libjpeg"},
        {NULL, NULL, NULL, NULL}
    };

    /* Find package in map */
    const char* mapped_package = package_name;
    if (strcmp(os, "linux") == 0) {
        for (int i = 0; package_map[i].error_name; i++) {
            if (strcasecmp(package_name, package_map[i].error_name) == 0) {
                if (package_map[i].ubuntu_pkg) {
                    mapped_package = package_map[i].ubuntu_pkg;
                }
                break;
            }
        }
    } else if (strcmp(os, "macos") == 0) {
        for (int i = 0; package_map[i].error_name; i++) {
            if (strcasecmp(package_name, package_map[i].error_name) == 0) {
                if (package_map[i].macos_pkg) {
                    mapped_package = package_map[i].macos_pkg;
                }
                break;
            }
        }
    } else if (strcmp(os, "windows") == 0) {
        for (int i = 0; package_map[i].error_name; i++) {
            if (strcasecmp(package_name, package_map[i].error_name) == 0) {
                if (package_map[i].windows_pkg) {
                    mapped_package = package_map[i].windows_pkg;
                }
                break;
            }
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

    /* Fix 1: Install the package */
    char* install_cmd = get_install_command(library_name, ctx);
    if (install_cmd) {
        char* desc = malloc(256);
        if (desc) {
            snprintf(desc, 256, "Install %s library", library_name);
            fixes[count++] = create_fix_action(
                FIX_ACTION_INSTALL_PACKAGE,
                desc,
                install_cmd,
                library_name,
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
        strcpy(package_name, "SDL2");
    } else if (strstr(header_name, "GL/gl")) {
        strcpy(package_name, "OpenGL");
    } else if (strstr(header_name, "boost")) {
        strcpy(package_name, "boost");
    } else if (strstr(header_name, "curl")) {
        strcpy(package_name, "curl");
    } else {
        /* Extract base name */
        strncpy(package_name, header_name, sizeof(package_name) - 1);
        char* dot = strchr(package_name, '.');
        if (dot) *dot = '\0';
    }

    /* Fix 1: Install development package */
    char* install_cmd = get_install_command(package_name, ctx);
    if (install_cmd) {
        char desc[256];
        snprintf(desc, sizeof(desc), "Install %s development files", package_name);
        fixes[count++] = create_fix_action(
            FIX_ACTION_INSTALL_PACKAGE,
            desc,
            install_cmd,
            package_name,
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
        strcpy(chmod_cmd, "chmod -R 755 .");
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
            free(actions[i]->command);
            free(actions[i]->target);
            free(actions[i]->value);
            free(actions[i]);
        }
    }

    free(actions);
}