/**
 * @file build_intelligence.c
 * @brief Rule-based build intelligence implementation
 *
 * Provides intelligent defaults and error recovery without requiring AI.
 * This serves as a reliable fallback when AI is unavailable or produces
 * poor suggestions.
 */

#include "cyxmake/build_intelligence.h"
#include "cyxmake/logger.h"
#include "cyxmake/compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define popen _popen
    #define pclose _pclose
#else
    #include <unistd.h>
#endif

/* ========================================================================
 * Error Pattern Database
 * ======================================================================== */

typedef struct {
    const char* pattern;
    BuildErrorType type;
    const char* description;
    const char* fix_hint;
} ErrorPattern;

/* Common error patterns we can recognize */
static const ErrorPattern ERROR_PATTERNS[] = {
    /* CMake version issues */
    {
        "CMAKE_POLICY_VERSION_MINIMUM",
        BUILD_ERROR_CMAKE_VERSION,
        "CMake version policy compatibility issue",
        "Add -DCMAKE_POLICY_VERSION_MINIMUM=3.5 to cmake command"
    },
    {
        "Compatibility with CMake < 3.5 has been removed",
        BUILD_ERROR_CMAKE_VERSION,
        "CMake version too old",
        "Use cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5"
    },
    {
        "cmake_minimum_required",
        BUILD_ERROR_CMAKE_VERSION,
        "CMake minimum version requirement not met",
        "Update CMakeLists.txt or use policy flag"
    },

    /* Missing dependencies */
    {
        "fatal error: '",
        BUILD_ERROR_MISSING_INCLUDE,
        "Missing header file",
        "Install the development package for this library"
    },
    {
        "fatal error:",
        BUILD_ERROR_MISSING_INCLUDE,
        "Fatal compilation error",
        "Check for missing headers or dependencies"
    },
    {
        "cannot find -l",
        BUILD_ERROR_MISSING_DEPENDENCY,
        "Missing library for linking",
        "Install the library package"
    },
    {
        "No package '",
        BUILD_ERROR_MISSING_DEPENDENCY,
        "pkg-config cannot find package",
        "Install the package and its development files"
    },
    {
        "Could not find a package configuration file",
        BUILD_ERROR_MISSING_DEPENDENCY,
        "CMake cannot find required package",
        "Install the package or set CMAKE_PREFIX_PATH"
    },
    {
        "find_package",
        BUILD_ERROR_MISSING_DEPENDENCY,
        "CMake find_package failed",
        "Install the missing dependency"
    },

    /* Directory/path issues */
    {
        "does not appear to contain CMakeLists.txt",
        BUILD_ERROR_NO_CMAKE_LISTS,
        "No CMakeLists.txt in specified directory",
        "Run cmake from the correct directory or use -S flag"
    },
    {
        "CMakeLists.txt not found",
        BUILD_ERROR_NO_CMAKE_LISTS,
        "CMakeLists.txt not found",
        "Ensure you're in the project root directory"
    },
    {
        "Ignoring extra path from command line",
        BUILD_ERROR_WRONG_DIRECTORY,
        "CMake called with incorrect path arguments",
        "Use 'cmake -B build -S .' instead of 'cmake ..'"
    },

    /* Compiler issues */
    {
        "'cl' is not recognized",
        BUILD_ERROR_COMPILER_NOT_FOUND,
        "MSVC compiler not found",
        "Run from Developer Command Prompt or install Visual Studio"
    },
    {
        "'gcc' is not recognized",
        BUILD_ERROR_COMPILER_NOT_FOUND,
        "GCC compiler not found",
        "Install GCC or add it to PATH"
    },
    {
        "No CMAKE_C_COMPILER could be found",
        BUILD_ERROR_COMPILER_NOT_FOUND,
        "CMake cannot find C compiler",
        "Install a C compiler (gcc, clang, or MSVC)"
    },
    {
        "No CMAKE_CXX_COMPILER could be found",
        BUILD_ERROR_COMPILER_NOT_FOUND,
        "CMake cannot find C++ compiler",
        "Install a C++ compiler (g++, clang++, or MSVC)"
    },

    /* Syntax errors */
    {
        "error: expected",
        BUILD_ERROR_SYNTAX_ERROR,
        "C/C++ syntax error",
        "Fix the code syntax error"
    },
    {
        "error C",
        BUILD_ERROR_SYNTAX_ERROR,
        "MSVC compilation error",
        "Fix the code error"
    },

    /* Link errors */
    {
        "undefined reference to",
        BUILD_ERROR_LINK_ERROR,
        "Linker cannot find symbol definition",
        "Check library linking order or missing library"
    },
    {
        "unresolved external symbol",
        BUILD_ERROR_LINK_ERROR,
        "MSVC linker error",
        "Add the library containing this symbol"
    },
    {
        "LNK2019",
        BUILD_ERROR_LINK_ERROR,
        "MSVC unresolved external",
        "Link against the required library"
    },
    {
        "LNK1120",
        BUILD_ERROR_LINK_ERROR,
        "MSVC unresolved externals",
        "Resolve all missing symbol references"
    },

    /* Permission issues */
    {
        "Permission denied",
        BUILD_ERROR_PERMISSION_DENIED,
        "Permission denied",
        "Run with elevated privileges or check file permissions"
    },
    {
        "Access is denied",
        BUILD_ERROR_PERMISSION_DENIED,
        "Access denied (Windows)",
        "Run as Administrator or close other programs using the files"
    },

    /* Memory issues */
    {
        "out of memory",
        BUILD_ERROR_OUT_OF_MEMORY,
        "Compiler ran out of memory",
        "Close other programs or reduce parallel build jobs"
    },
    {
        "virtual memory exhausted",
        BUILD_ERROR_OUT_OF_MEMORY,
        "Virtual memory exhausted",
        "Increase swap space or reduce build parallelism"
    },

    /* End marker */
    {NULL, BUILD_ERROR_NONE, NULL, NULL}
};

/* ========================================================================
 * Build Command Templates
 * ======================================================================== */

BuildCommandSet build_intelligence_get_commands(BuildSystem type) {
    BuildCommandSet cmds = {0};

    switch (type) {
        case BUILD_CMAKE:
            cmds.configure_cmd = "cmake -B build -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5";
            cmds.build_cmd = "cmake --build build";
            cmds.clean_cmd = "cmake --build build --target clean";
            cmds.test_cmd = "ctest --test-dir build --output-on-failure";
            cmds.needs_build_dir = true;
            cmds.build_dir_name = "build";
            break;

        case BUILD_MAKE:
            cmds.configure_cmd = NULL;  /* No configure step */
            cmds.build_cmd = "make";
            cmds.clean_cmd = "make clean";
            cmds.test_cmd = "make test";
            cmds.needs_build_dir = false;
            cmds.build_dir_name = NULL;
            break;

        case BUILD_MESON:
            cmds.configure_cmd = "meson setup build";
            cmds.build_cmd = "meson compile -C build";
            cmds.clean_cmd = "meson compile -C build --clean";
            cmds.test_cmd = "meson test -C build";
            cmds.needs_build_dir = true;
            cmds.build_dir_name = "build";
            break;

        case BUILD_BAZEL:
            cmds.configure_cmd = NULL;
            cmds.build_cmd = "bazel build //...";
            cmds.clean_cmd = "bazel clean";
            cmds.test_cmd = "bazel test //...";
            cmds.needs_build_dir = false;
            cmds.build_dir_name = "bazel-bin";
            break;

        case BUILD_CARGO:
            cmds.configure_cmd = NULL;
            cmds.build_cmd = "cargo build";
            cmds.clean_cmd = "cargo clean";
            cmds.test_cmd = "cargo test";
            cmds.needs_build_dir = false;  /* Cargo handles it */
            cmds.build_dir_name = "target";
            break;

        case BUILD_NPM:
            cmds.configure_cmd = "npm install";
            cmds.build_cmd = "npm run build";
            cmds.clean_cmd = "npm run clean";
            cmds.test_cmd = "npm test";
            cmds.needs_build_dir = false;
            cmds.build_dir_name = NULL;
            break;

        case BUILD_GRADLE:
            cmds.configure_cmd = NULL;
            cmds.build_cmd = "gradle build";
            cmds.clean_cmd = "gradle clean";
            cmds.test_cmd = "gradle test";
            cmds.needs_build_dir = false;
            cmds.build_dir_name = "build";
            break;

        case BUILD_MAVEN:
            cmds.configure_cmd = NULL;
            cmds.build_cmd = "mvn package";
            cmds.clean_cmd = "mvn clean";
            cmds.test_cmd = "mvn test";
            cmds.needs_build_dir = false;
            cmds.build_dir_name = "target";
            break;

        case BUILD_SETUPTOOLS:
        case BUILD_POETRY:
            cmds.configure_cmd = NULL;
            cmds.build_cmd = "pip install -e .";
            cmds.clean_cmd = "rm -rf build dist *.egg-info";
            cmds.test_cmd = "pytest";
            cmds.needs_build_dir = false;
            cmds.build_dir_name = NULL;
            break;

        default:
            /* Try to be helpful even for unknown systems */
            cmds.configure_cmd = NULL;
            cmds.build_cmd = "make";
            cmds.clean_cmd = "make clean";
            cmds.test_cmd = "make test";
            cmds.needs_build_dir = false;
            cmds.build_dir_name = NULL;
            break;
    }

    return cmds;
}

char* build_intelligence_cmake_configure(const char* source_dir,
                                          const char* build_dir,
                                          const char* extra_args) {
    char* cmd = malloc(1024);
    if (!cmd) return NULL;

    const char* src = source_dir ? source_dir : ".";
    const char* bld = build_dir ? build_dir : "build";
    const char* extra = extra_args ? extra_args : "";

    /* Always include the policy version flag to avoid cmake version issues */
    snprintf(cmd, 1024,
             "cmake -B \"%s\" -S \"%s\" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 %s",
             bld, src, extra);

    return cmd;
}

char* build_intelligence_cmake_build(const char* build_dir, const char* config) {
    char* cmd = malloc(256);
    if (!cmd) return NULL;

    const char* bld = build_dir ? build_dir : "build";

    if (config && strlen(config) > 0) {
        snprintf(cmd, 256, "cmake --build \"%s\" --config %s", bld, config);
    } else {
        snprintf(cmd, 256, "cmake --build \"%s\"", bld);
    }

    return cmd;
}

/* ========================================================================
 * Error Pattern Recognition
 * ======================================================================== */

static char* extract_context(const char* error_output, const char* pattern_pos, int context_chars) {
    /* Extract some context around the pattern match */
    const char* start = pattern_pos;
    const char* end = pattern_pos;

    /* Go back up to context_chars or start of line */
    while (start > error_output && (pattern_pos - start) < context_chars && *(start - 1) != '\n') {
        start--;
    }

    /* Go forward up to context_chars or end of line */
    while (*end && (end - pattern_pos) < context_chars && *end != '\n') {
        end++;
    }

    size_t len = end - start;
    char* context = malloc(len + 1);
    if (!context) return NULL;

    memcpy(context, start, len);
    context[len] = '\0';

    return context;
}

DetectedBuildError** build_intelligence_analyze_error(const char* error_output,
                                                       const ProjectContext* ctx) {
    if (!error_output) return NULL;

    /* Allocate array for results (max 10 errors) */
    DetectedBuildError** results = calloc(11, sizeof(DetectedBuildError*));
    if (!results) return NULL;

    int result_count = 0;

    /* Search for each known pattern */
    for (int i = 0; ERROR_PATTERNS[i].pattern != NULL && result_count < 10; i++) {
        const char* match = strstr(error_output, ERROR_PATTERNS[i].pattern);
        if (match) {
            /* Check if we already have this error type */
            bool duplicate = false;
            for (int j = 0; j < result_count; j++) {
                if (results[j]->type == ERROR_PATTERNS[i].type) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) continue;

            /* Create error entry */
            DetectedBuildError* error = calloc(1, sizeof(DetectedBuildError));
            if (!error) continue;

            error->type = ERROR_PATTERNS[i].type;
            error->pattern_matched = strdup(ERROR_PATTERNS[i].pattern);
            error->description = strdup(ERROR_PATTERNS[i].description);
            error->fix_description = strdup(ERROR_PATTERNS[i].fix_hint);
            error->confidence = 0.9f;  /* High confidence for pattern matches */

            /* Generate specific fix command if possible */
            error->fix_command = build_intelligence_generate_fix(
                error->type,
                extract_context(error_output, match, 100),
                ctx
            );

            results[result_count++] = error;
        }
    }

    results[result_count] = NULL;  /* NULL terminate */

    if (result_count == 0) {
        free(results);
        return NULL;
    }

    return results;
}

void detected_build_error_free(DetectedBuildError* error) {
    if (!error) return;
    free(error->pattern_matched);
    free(error->description);
    free(error->fix_command);
    free(error->fix_description);
    free(error);
}

void detected_build_errors_free(DetectedBuildError** errors) {
    if (!errors) return;
    for (int i = 0; errors[i]; i++) {
        detected_build_error_free(errors[i]);
    }
    free(errors);
}

/* ========================================================================
 * Smart Fix Generation
 * ======================================================================== */

char* build_intelligence_generate_fix(BuildErrorType error_type,
                                       const char* error_details,
                                       const ProjectContext* ctx) {
    char* fix = malloc(512);
    if (!fix) return NULL;

    switch (error_type) {
        case BUILD_ERROR_CMAKE_VERSION:
            /* Use our known-good cmake command */
            snprintf(fix, 512,
                     "cmake -B build -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5");
            break;

        case BUILD_ERROR_NO_CMAKE_LISTS:
        case BUILD_ERROR_WRONG_DIRECTORY:
            /* Correct cmake invocation */
            if (ctx && ctx->root_path) {
                snprintf(fix, 512,
                         "cmake -B build -S \"%s\" -DCMAKE_POLICY_VERSION_MINIMUM=3.5",
                         ctx->root_path);
            } else {
                snprintf(fix, 512,
                         "cmake -B build -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5");
            }
            break;

        case BUILD_ERROR_MISSING_DEPENDENCY:
        case BUILD_ERROR_MISSING_INCLUDE:
            /* Try to extract package name and suggest install */
            if (error_details) {
                /* Look for library name in error */
                const char* lib_start = strstr(error_details, "-l");
                if (lib_start) {
                    lib_start += 2;
                    char lib_name[64] = {0};
                    int j = 0;
                    while (lib_start[j] && !isspace(lib_start[j]) && j < 63) {
                        lib_name[j] = lib_start[j];
                        j++;
                    }
                    if (j > 0) {
                        char* install_cmd = build_intelligence_package_install_cmd(lib_name);
                        if (install_cmd) {
                            strncpy(fix, install_cmd, 511);
                            free(install_cmd);
                            return fix;
                        }
                    }
                }
            }
            /* Generic suggestion */
#ifdef _WIN32
            snprintf(fix, 512, "vcpkg install <package-name>");
#elif __APPLE__
            snprintf(fix, 512, "brew install <package-name>");
#else
            snprintf(fix, 512, "sudo apt-get install lib<package>-dev");
#endif
            break;

        case BUILD_ERROR_COMPILER_NOT_FOUND:
#ifdef _WIN32
            snprintf(fix, 512,
                     "Run from 'Developer Command Prompt for VS' or install Visual Studio Build Tools");
#elif __APPLE__
            snprintf(fix, 512, "xcode-select --install");
#else
            snprintf(fix, 512, "sudo apt-get install build-essential");
#endif
            break;

        default:
            free(fix);
            return NULL;
    }

    return fix;
}

/* ========================================================================
 * Fallback Build Plan
 * ======================================================================== */

BuildIntelligencePlan* build_intelligence_fallback_plan(const ProjectContext* ctx) {
    if (!ctx) return NULL;

    BuildIntelligencePlan* plan = calloc(1, sizeof(BuildIntelligencePlan));
    if (!plan) return NULL;

    /* Get standard commands for this build system */
    BuildCommandSet cmds = build_intelligence_get_commands(ctx->build_system.type);

    /* Count how many commands we need */
    int count = 0;
    if (cmds.configure_cmd) count++;
    count++;  /* Always have build */

    plan->commands = calloc(count + 1, sizeof(char*));
    plan->descriptions = calloc(count + 1, sizeof(char*));
    if (!plan->commands || !plan->descriptions) {
        build_intelligence_plan_free(plan);
        return NULL;
    }

    int idx = 0;

    /* Add configure step if needed */
    if (cmds.configure_cmd) {
        plan->commands[idx] = strdup(cmds.configure_cmd);
        plan->descriptions[idx] = strdup("Configure build system");
        idx++;
    }

    /* Add build step */
    plan->commands[idx] = strdup(cmds.build_cmd);
    plan->descriptions[idx] = strdup("Build project");
    idx++;

    plan->command_count = idx;

    return plan;
}

void build_intelligence_plan_free(BuildIntelligencePlan* plan) {
    if (!plan) return;

    if (plan->commands) {
        for (int i = 0; i < plan->command_count; i++) {
            free(plan->commands[i]);
        }
        free(plan->commands);
    }

    if (plan->descriptions) {
        for (int i = 0; i < plan->command_count; i++) {
            free(plan->descriptions[i]);
        }
        free(plan->descriptions);
    }

    free(plan);
}

/* ========================================================================
 * Platform-Specific Knowledge
 * ======================================================================== */

char* build_intelligence_package_install_cmd(const char* package_name) {
    if (!package_name) return NULL;

    char* cmd = malloc(256);
    if (!cmd) return NULL;

#ifdef _WIN32
    /* Try vcpkg first on Windows */
    if (build_intelligence_command_exists("vcpkg")) {
        snprintf(cmd, 256, "vcpkg install %s", package_name);
    } else if (build_intelligence_command_exists("choco")) {
        snprintf(cmd, 256, "choco install %s -y", package_name);
    } else if (build_intelligence_command_exists("winget")) {
        snprintf(cmd, 256, "winget install %s", package_name);
    } else {
        snprintf(cmd, 256, "vcpkg install %s", package_name);
    }
#elif __APPLE__
    snprintf(cmd, 256, "brew install %s", package_name);
#else
    /* Linux - try to detect package manager */
    if (build_intelligence_command_exists("apt-get")) {
        snprintf(cmd, 256, "sudo apt-get install -y %s", package_name);
    } else if (build_intelligence_command_exists("dnf")) {
        snprintf(cmd, 256, "sudo dnf install -y %s", package_name);
    } else if (build_intelligence_command_exists("pacman")) {
        snprintf(cmd, 256, "sudo pacman -S --noconfirm %s", package_name);
    } else if (build_intelligence_command_exists("yum")) {
        snprintf(cmd, 256, "sudo yum install -y %s", package_name);
    } else {
        snprintf(cmd, 256, "sudo apt-get install -y %s", package_name);
    }
#endif

    return cmd;
}

bool build_intelligence_command_exists(const char* command) {
    if (!command) return false;

    char check_cmd[256];
#ifdef _WIN32
    snprintf(check_cmd, sizeof(check_cmd), "where %s >nul 2>&1", command);
#else
    snprintf(check_cmd, sizeof(check_cmd), "which %s >/dev/null 2>&1", command);
#endif

    return system(check_cmd) == 0;
}

const char* build_intelligence_get_cmake_path(void) {
    /* Check if cmake is available */
    if (build_intelligence_command_exists("cmake")) {
        return "cmake";
    }

#ifdef _WIN32
    /* Common Windows paths */
    const char* common_paths[] = {
        "C:\\Program Files\\CMake\\bin\\cmake.exe",
        "C:\\Program Files (x86)\\CMake\\bin\\cmake.exe",
        NULL
    };

    for (int i = 0; common_paths[i]; i++) {
        FILE* f = fopen(common_paths[i], "r");
        if (f) {
            fclose(f);
            return common_paths[i];
        }
    }
#endif

    return "cmake";  /* Return default and let it fail if not found */
}
