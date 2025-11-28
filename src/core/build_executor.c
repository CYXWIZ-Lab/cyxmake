/**
 * @file build_executor.c
 * @brief Build command execution implementation
 */

#include "cyxmake/build_executor.h"
#include "cyxmake/project_context.h"
#include "cyxmake/logger.h"
#include "cyxmake/compat.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #define popen _popen
    #define pclose _pclose
    #define getcwd _getcwd
    #define chdir _chdir
    #define access _access
    #define F_OK 0
#else
    #include <sys/wait.h>
    #include <unistd.h>
#endif

/* Maximum output buffer size */
#define MAX_OUTPUT_SIZE (1024 * 1024)  /* 1 MB */

/* Create default build options */
BuildOptions* build_options_default(void) {
    BuildOptions* opts = calloc(1, sizeof(BuildOptions));
    if (!opts) return NULL;

    opts->verbose = false;
    opts->clean_first = false;
    opts->parallel_jobs = 0;  /* Auto-detect */
    opts->target = NULL;
    opts->build_dir = NULL;

    return opts;
}

/* Free build options */
void build_options_free(BuildOptions* opts) {
    if (!opts) return;
    free(opts->target);
    free(opts->build_dir);
    free(opts);
}

/* Get build command for build system */
char* build_get_command(BuildSystem build_system, const BuildOptions* opts) {
    BuildOptions* default_opts = NULL;
    if (!opts) {
        default_opts = build_options_default();
        opts = default_opts;
    }

    char* command = malloc(1024);
    if (!command) {
        if (default_opts) build_options_free(default_opts);
        return NULL;
    }

    switch (build_system) {
        case BUILD_CMAKE:
            if (opts->clean_first) {
                snprintf(command, 1024, "cmake --build . --clean-first");
            } else {
                snprintf(command, 1024, "cmake --build .");
            }
            if (opts->parallel_jobs > 0) {
                char parallel[64];
                snprintf(parallel, sizeof(parallel), " -j %d", opts->parallel_jobs);
                strcat(command, parallel);
            }
            if (opts->target) {
                strcat(command, " --target ");
                strcat(command, opts->target);
            }
            break;

        case BUILD_MAKE:
            if (opts->clean_first) {
                snprintf(command, 1024, "make clean && make");
            } else {
                snprintf(command, 1024, "make");
            }
            if (opts->parallel_jobs > 0) {
                char parallel[64];
                snprintf(parallel, sizeof(parallel), " -j%d", opts->parallel_jobs);
                strcat(command, parallel);
            }
            if (opts->target) {
                strcat(command, " ");
                strcat(command, opts->target);
            }
            break;

        case BUILD_CARGO:
            if (opts->clean_first) {
                snprintf(command, 1024, "cargo clean && cargo build");
            } else {
                snprintf(command, 1024, "cargo build");
            }
            if (opts->parallel_jobs > 0) {
                char parallel[64];
                snprintf(parallel, sizeof(parallel), " -j %d", opts->parallel_jobs);
                strcat(command, parallel);
            }
            break;

        case BUILD_NPM:
            if (opts->clean_first) {
                snprintf(command, 1024, "npm run clean && npm run build");
            } else {
                snprintf(command, 1024, "npm run build");
            }
            break;

        case BUILD_MAVEN:
            if (opts->clean_first) {
                snprintf(command, 1024, "mvn clean package");
            } else {
                snprintf(command, 1024, "mvn package");
            }
            break;

        case BUILD_GRADLE:
            if (opts->clean_first) {
                snprintf(command, 1024, "./gradlew clean build");
            } else {
                snprintf(command, 1024, "./gradlew build");
            }
            break;

        case BUILD_MESON:
            snprintf(command, 1024, "ninja -C build");
            if (opts->parallel_jobs > 0) {
                char parallel[64];
                snprintf(parallel, sizeof(parallel), " -j %d", opts->parallel_jobs);
                strcat(command, parallel);
            }
            break;

        case BUILD_BAZEL:
            snprintf(command, 1024, "bazel build //...");
            break;

        case BUILD_SETUPTOOLS:
            snprintf(command, 1024, "python setup.py build");
            break;

        case BUILD_POETRY:
            snprintf(command, 1024, "poetry build");
            break;

        default:
            free(command);
            if (default_opts) build_options_free(default_opts);
            return NULL;
    }

    if (default_opts) build_options_free(default_opts);
    return command;
}

/* Execute command and capture output */
BuildResult* build_execute_command(const char* command, const char* working_dir) {
    if (!command) return NULL;

    BuildResult* result = calloc(1, sizeof(BuildResult));
    if (!result) return NULL;

    /* Allocate output buffers */
    result->stdout_output = malloc(MAX_OUTPUT_SIZE);
    result->stderr_output = malloc(MAX_OUTPUT_SIZE);

    if (!result->stdout_output || !result->stderr_output) {
        build_result_free(result);
        return NULL;
    }

    result->stdout_output[0] = '\0';
    result->stderr_output[0] = '\0';

    log_debug("Executing command: %s", command);

    /* Start timing */
    clock_t start = clock();

    /* Change directory if specified and not current directory */
    char* old_dir = NULL;
    if (working_dir && strcmp(working_dir, ".") != 0) {
        old_dir = getcwd(NULL, 0);
        if (!old_dir) {
            log_error("Failed to get current directory");
            build_result_free(result);
            return NULL;
        }
        if (chdir(working_dir) != 0) {
            log_error("Failed to change to directory: %s", working_dir);
            free(old_dir);
            build_result_free(result);
            return NULL;
        }
    }

    /* Execute command and capture output */
#ifdef _WIN32
    /* Windows: Redirect stderr to stdout for combined output */
    char cmd_with_redirect[2048];
    snprintf(cmd_with_redirect, sizeof(cmd_with_redirect), "%s 2>&1", command);

    FILE* pipe = popen(cmd_with_redirect, "r");
    if (!pipe) {
        log_error("Failed to execute command");
        if (old_dir) {
            chdir(old_dir);
            free(old_dir);
        }
        build_result_free(result);
        return NULL;
    }

    /* Read output */
    size_t offset = 0;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL && offset < MAX_OUTPUT_SIZE - 1) {
        size_t len = strlen(buffer);
        if (offset + len < MAX_OUTPUT_SIZE) {
            memcpy(result->stdout_output + offset, buffer, len);
            offset += len;
        }
    }
    result->stdout_output[offset] = '\0';

    int exit_code = pclose(pipe);
    result->exit_code = exit_code;

#else
    /* POSIX: Use popen */
    FILE* pipe = popen(command, "r");
    if (!pipe) {
        log_error("Failed to execute command");
        if (old_dir) {
            chdir(old_dir);
            free(old_dir);
        }
        build_result_free(result);
        return NULL;
    }

    /* Read output */
    size_t offset = 0;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL && offset < MAX_OUTPUT_SIZE - 1) {
        size_t len = strlen(buffer);
        if (offset + len < MAX_OUTPUT_SIZE) {
            memcpy(result->stdout_output + offset, buffer, len);
            offset += len;
        }
    }
    result->stdout_output[offset] = '\0';

    int status = pclose(pipe);
    result->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif

    /* Restore directory */
    if (old_dir) {
        chdir(old_dir);
        free(old_dir);
    }

    /* Calculate duration */
    clock_t end = clock();
    result->duration_sec = ((double)(end - start)) / CLOCKS_PER_SEC;

    /* Determine success */
    result->success = (result->exit_code == 0);

    log_debug("Command completed with exit code: %d", result->exit_code);

    return result;
}

/* Execute build */
BuildResult* build_execute(const ProjectContext* ctx, const BuildOptions* opts) {
    if (!ctx) return NULL;

    /* Validate build system */
    if (!build_validate_system(ctx->build_system.type)) {
        BuildResult* result = calloc(1, sizeof(BuildResult));
        if (result) {
            result->success = false;
            result->exit_code = -1;
            result->stdout_output = strdup("");

            const char* tool = build_system_to_string(ctx->build_system.type);
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg),
                     "Build system '%s' is not installed or not in PATH.\n"
                     "Please install the required tools and try again.", tool);
            result->stderr_output = strdup(error_msg);

            log_error("Build system not available: %s", tool);

            /* Provide installation hints */
            switch (ctx->build_system.type) {
                case BUILD_CMAKE:
                    log_info("Install CMake from: https://cmake.org/download/");
                    break;
                case BUILD_NPM:
                    log_info("Install Node.js/npm from: https://nodejs.org/");
                    break;
                case BUILD_CARGO:
                    log_info("Install Rust from: https://rustup.rs/");
                    break;
                case BUILD_POETRY:
                    log_info("Install Poetry: pip install poetry");
                    break;
                default:
                    break;
            }
        }
        return result;
    }

    /* Create default options if not provided */
    BuildOptions* default_opts = NULL;
    if (!opts) {
        default_opts = build_options_default();
        opts = default_opts;

        /* Auto-detect parallel jobs if not specified */
        if (opts && opts->parallel_jobs == 0) {
            int cores = build_get_cpu_cores();
            /* Use n-1 cores, minimum 1 */
            ((BuildOptions*)opts)->parallel_jobs = cores > 1 ? cores - 1 : 1;
            log_debug("Auto-detected %d CPU cores, using %d parallel jobs",
                      cores, opts->parallel_jobs);
        }
    }

    /* Find build directory */
    char* build_dir = NULL;
    if (!opts->build_dir) {
        build_dir = build_find_directory(ctx->root_path, ctx->build_system.type);
        if (build_dir) {
            log_debug("Using build directory: %s", build_dir);
        } else if (ctx->build_system.type == BUILD_CMAKE) {
            /* CMake project not configured - run cmake configure first */
            log_info("CMake project not configured, running cmake configure...");
            char configure_cmd[1024];
            /* Add CMAKE_POLICY_VERSION_MINIMUM for compatibility with older CMakeLists.txt */
            snprintf(configure_cmd, sizeof(configure_cmd),
                     "cmake -B build -S \"%s\" -DCMAKE_POLICY_VERSION_MINIMUM=3.5",
                     ctx->root_path);

            BuildResult* config_result = build_execute_command(configure_cmd, ctx->root_path);
            if (!config_result || !config_result->success) {
                log_error("Failed to configure CMake project");
                if (config_result) {
                    if (config_result->stdout_output && config_result->stdout_output[0]) {
                        log_plain("%s", config_result->stdout_output);
                    }
                    build_result_free(config_result);
                }
                if (default_opts) build_options_free(default_opts);
                return NULL;
            }
            log_success("CMake project configured successfully");
            if (config_result->stdout_output && config_result->stdout_output[0]) {
                log_debug("Configure output:\n%s", config_result->stdout_output);
            }
            build_result_free(config_result);

            /* Now use the build directory */
            build_dir = malloc(512);
            if (build_dir) {
                snprintf(build_dir, 512, "%s/build", ctx->root_path);
            }
        }
    }

    /* Get build command */
    char* command = build_get_command(ctx->build_system.type, opts);
    if (!command) {
        log_error("Unsupported build system: %s",
                  build_system_to_string(ctx->build_system.type));
        free(build_dir);
        if (default_opts) build_options_free(default_opts);
        return NULL;
    }

    log_info("Build system: %s", build_system_to_string(ctx->build_system.type));
    log_info("Build command: %s", command);

    /* Use build directory if found, otherwise use project root */
    const char* working_dir = opts->build_dir ? opts->build_dir :
                             (build_dir ? build_dir : ctx->root_path);

    /* Execute command */
    log_plain("\n");
    BuildResult* result = build_execute_command(command, working_dir);

    free(command);
    free(build_dir);
    if (default_opts) build_options_free(default_opts);

    return result;
}

/* Check if a command exists in PATH */
static bool command_exists(const char* cmd) {
    char test_cmd[512];
#ifdef _WIN32
    snprintf(test_cmd, sizeof(test_cmd), "where %s >nul 2>&1", cmd);
#else
    snprintf(test_cmd, sizeof(test_cmd), "which %s >/dev/null 2>&1", cmd);
#endif
    return system(test_cmd) == 0;
}

/* Validate build system availability */
bool build_validate_system(BuildSystem build_system) {
    switch (build_system) {
        case BUILD_CMAKE:
            return command_exists("cmake");
        case BUILD_MAKE:
            return command_exists("make");
        case BUILD_CARGO:
            return command_exists("cargo");
        case BUILD_NPM:
            return command_exists("npm");
        case BUILD_MAVEN:
            return command_exists("mvn");
        case BUILD_GRADLE:
#ifdef _WIN32
            return command_exists("gradlew.bat") || command_exists("gradle");
#else
            return command_exists("./gradlew") || command_exists("gradle");
#endif
        case BUILD_MESON:
            return command_exists("meson") && command_exists("ninja");
        case BUILD_BAZEL:
            return command_exists("bazel");
        case BUILD_SETUPTOOLS:
            return command_exists("python") || command_exists("python3");
        case BUILD_POETRY:
            return command_exists("poetry");
        default:
            return false;
    }
}

/* Find build directory */
char* build_find_directory(const char* project_path, BuildSystem build_system) {
    if (!project_path) project_path = ".";

    char* build_dir = malloc(512);
    if (!build_dir) return NULL;

    /* Check common build directory patterns */
    const char* common_dirs[] = {
        "build",
        "_build",
        "Build",
        "out",
        "output",
        "cmake-build",
        "cmake-build-debug",
        "cmake-build-release",
        "target",  /* Rust/Java */
        "dist",    /* Python/JS */
        NULL
    };

    /* Check build-system-specific directories */
    switch (build_system) {
        case BUILD_CMAKE: {
            /* Look for CMakeCache.txt in common build directories */
            for (const char** dir = common_dirs; *dir; dir++) {
                snprintf(build_dir, 512, "%s/%s/CMakeCache.txt", project_path, *dir);
                if (access(build_dir, F_OK) == 0) {
                    snprintf(build_dir, 512, "%s/%s", project_path, *dir);
                    return build_dir;
                }
            }
            /* Check if CMakeLists.txt exists in project root (in-source build possible) */
            snprintf(build_dir, 512, "%s/CMakeLists.txt", project_path);
            if (access(build_dir, F_OK) == 0) {
                /* Check if CMakeCache.txt exists in project root (in-source build) */
                snprintf(build_dir, 512, "%s/CMakeCache.txt", project_path);
                if (access(build_dir, F_OK) == 0) {
                    snprintf(build_dir, 512, "%s", project_path);
                    return build_dir;
                }
                /* No configured build found - return NULL to signal need for configure */
                free(build_dir);
                return NULL;
            }
            /* Default to project path */
            snprintf(build_dir, 512, "%s", project_path);
            return build_dir;
        }

        case BUILD_CARGO:
            snprintf(build_dir, 512, "%s/target", project_path);
            /* Check if directory exists */
            if (access(build_dir, F_OK) != 0) {
                snprintf(build_dir, 512, "%s", project_path);
            }
            return build_dir;

        case BUILD_MAVEN:
        case BUILD_GRADLE:
            snprintf(build_dir, 512, "%s/target", project_path);
            /* Check if directory exists */
            if (access(build_dir, F_OK) != 0) {
                snprintf(build_dir, 512, "%s", project_path);
            }
            return build_dir;

        case BUILD_NPM:
            /* npm builds from project root, not dist */
            snprintf(build_dir, 512, "%s", project_path);
            return build_dir;

        case BUILD_MESON: {
            /* Look for build.ninja */
            for (const char** dir = common_dirs; *dir; dir++) {
                snprintf(build_dir, 512, "%s/%s/build.ninja", project_path, *dir);
                if (access(build_dir, F_OK) == 0) {
                    snprintf(build_dir, 512, "%s/%s", project_path, *dir);
                    return build_dir;
                }
            }
            snprintf(build_dir, 512, "%s/builddir", project_path);
            return build_dir;
        }

        default:
            /* Use project root as build directory */
            snprintf(build_dir, 512, "%s", project_path);
            return build_dir;
    }
}

/* Get number of CPU cores */
int build_get_cpu_cores(void) {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int)sysinfo.dwNumberOfProcessors;
#else
    int cores = sysconf(_SC_NPROCESSORS_ONLN);
    return cores > 0 ? cores : 1;
#endif
}

/* Free build result */
void build_result_free(BuildResult* result) {
    if (!result) return;
    free(result->stdout_output);
    free(result->stderr_output);
    free(result);
}

/* Print build result */
void build_result_print(const BuildResult* result) {
    if (!result) return;

    if (result->success) {
        log_success("Build completed successfully");
    } else {
        log_error("Build failed with exit code: %d", result->exit_code);
    }

    log_info("Duration: %.2f seconds", result->duration_sec);

    /* Print stdout if not empty */
    if (result->stdout_output && result->stdout_output[0] != '\0') {
        log_plain("\n--- Build Output ---\n");
        log_plain("%s", result->stdout_output);
    }

    /* Print stderr if not empty */
    if (result->stderr_output && result->stderr_output[0] != '\0') {
        log_plain("\n--- Build Errors ---\n");
        log_plain("%s", result->stderr_output);
    }
}
