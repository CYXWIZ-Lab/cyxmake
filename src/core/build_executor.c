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
    #define popen _popen
    #define pclose _pclose
    #define getcwd _getcwd
    #define chdir _chdir
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

    /* Get build command */
    char* command = build_get_command(ctx->build_system.type, opts);
    if (!command) {
        log_error("Unsupported build system: %s",
                  build_system_to_string(ctx->build_system.type));
        return NULL;
    }

    log_info("Build system: %s", build_system_to_string(ctx->build_system.type));
    log_info("Build command: %s", command);

    /* Execute command */
    BuildResult* result = build_execute_command(command, ctx->root_path);
    free(command);

    return result;
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
