/**
 * @file project_generator.c
 * @brief Project scaffolding and generation from natural language
 */

#include "cyxmake/project_generator.h"
#include "cyxmake/logger.h"
#include "cyxmake/file_ops.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#define strdup _strdup
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <sys/stat.h>
#endif

/* ========================================================================
 * Helper Functions
 * ======================================================================== */

static char* str_tolower(const char* str) {
    if (!str) return NULL;
    char* result = strdup(str);
    for (char* p = result; *p; p++) {
        *p = (char)tolower(*p);
    }
    return result;
}

static bool str_contains(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;
    char* h = str_tolower(haystack);
    char* n = str_tolower(needle);
    bool found = strstr(h, n) != NULL;
    free(h);
    free(n);
    return found;
}

static char* path_join(const char* dir, const char* file) {
    size_t len = strlen(dir) + strlen(file) + 2;
    char* result = malloc(len);
    if (!result) return NULL;
#ifdef _WIN32
    snprintf(result, len, "%s\\%s", dir, file);
#else
    snprintf(result, len, "%s/%s", dir, file);
#endif
    return result;
}

static bool ensure_directory(const char* path) {
    if (!path) return false;
#ifdef _WIN32
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

static bool write_file(const char* path, const char* content) {
    if (!path || !content) return false;
    FILE* fp = fopen(path, "w");
    if (!fp) return false;
    fputs(content, fp);
    fclose(fp);
    return true;
}

/* ========================================================================
 * Language/Build System Utilities
 * Note: language_from_string, language_to_string, and build_system_from_string
 * are defined in project_context.c and declared in project_context.h
 * ======================================================================== */

const char* language_extension(Language language, bool is_header) {
    switch (language) {
        case LANG_C: return is_header ? ".h" : ".c";
        case LANG_CPP: return is_header ? ".hpp" : ".cpp";
        case LANG_RUST: return ".rs";
        case LANG_PYTHON: return ".py";
        case LANG_JAVASCRIPT: return ".js";
        case LANG_TYPESCRIPT: return ".ts";
        case LANG_GO: return ".go";
        case LANG_JAVA: return ".java";
        case LANG_CSHARP: return ".cs";
        case LANG_RUBY: return ".rb";
        case LANG_PHP: return ".php";
        case LANG_SHELL: return ".sh";
        default: return "";
    }
}

BuildSystem default_build_system(Language language) {
    switch (language) {
        case LANG_C:
        case LANG_CPP:
            return BUILD_CMAKE;
        case LANG_RUST:
            return BUILD_CARGO;
        case LANG_PYTHON:
            return BUILD_SETUPTOOLS;
        case LANG_JAVASCRIPT:
        case LANG_TYPESCRIPT:
            return BUILD_NPM;
        case LANG_GO:
            return BUILD_CUSTOM;  /* Go modules */
        case LANG_JAVA:
            return BUILD_GRADLE;
        default:
            return BUILD_MAKE;
    }
}

/* ========================================================================
 * Project Specification
 * ======================================================================== */

ProjectSpec* project_spec_create(void) {
    ProjectSpec* spec = calloc(1, sizeof(ProjectSpec));
    if (!spec) return NULL;

    spec->language = LANG_CPP;
    spec->build_system = BUILD_CMAKE;
    spec->type = PROJECT_EXECUTABLE;
    spec->with_git = true;
    spec->cpp_standard = strdup("17");
    spec->c_standard = strdup("11");

    return spec;
}

void project_spec_free(ProjectSpec* spec) {
    if (!spec) return;

    free(spec->name);
    free(spec->cpp_standard);
    free(spec->c_standard);
    free(spec->license);
    free(spec->description);

    if (spec->dependencies) {
        for (int i = 0; i < spec->dependency_count; i++) {
            free(spec->dependencies[i]);
        }
        free(spec->dependencies);
    }

    free(spec);
}

/* Keyword patterns for parsing */
static const char* cpp_keywords[] = {"c++", "cpp", "cxx", NULL};
static const char* c_keywords[] = {"pure c", " c ", "in c", NULL};
static const char* rust_keywords[] = {"rust", NULL};
static const char* python_keywords[] = {"python", "py", NULL};
static const char* js_keywords[] = {"javascript", "node", "js ", NULL};
static const char* ts_keywords[] = {"typescript", "ts ", NULL};
static const char* go_keywords[] = {"golang", " go ", NULL};

static const char* game_keywords[] = {"game", "sdl", "opengl", "vulkan", "directx", "graphics", NULL};
static const char* lib_keywords[] = {"library", "lib ", "shared", "static lib", NULL};
static const char* cli_keywords[] = {"cli", "command line", "terminal", "console app", NULL};
static const char* web_keywords[] = {"web", "api", "server", "rest", "http", NULL};
static const char* gui_keywords[] = {"gui", "desktop", "qt", "gtk", "ui ", "window", NULL};

/* Common dependencies to detect */
static const char* common_deps[] = {
    "sdl2", "sdl", "opengl", "vulkan", "glfw", "glew",
    "boost", "qt", "gtk", "curl", "json", "sqlite",
    "spdlog", "fmt", "gtest", "catch2", "doctest",
    "imgui", "raylib", "sfml", "allegro",
    NULL
};

ProjectSpec* project_spec_parse(const char* description) {
    if (!description) return NULL;

    ProjectSpec* spec = project_spec_create();
    if (!spec) return NULL;

    /* Store original description */
    spec->description = strdup(description);

    /* Detect language */
    for (int i = 0; cpp_keywords[i]; i++) {
        if (str_contains(description, cpp_keywords[i])) {
            spec->language = LANG_CPP;
            goto lang_done;
        }
    }
    for (int i = 0; rust_keywords[i]; i++) {
        if (str_contains(description, rust_keywords[i])) {
            spec->language = LANG_RUST;
            spec->build_system = BUILD_CARGO;
            goto lang_done;
        }
    }
    for (int i = 0; python_keywords[i]; i++) {
        if (str_contains(description, python_keywords[i])) {
            spec->language = LANG_PYTHON;
            spec->build_system = BUILD_SETUPTOOLS;
            goto lang_done;
        }
    }
    for (int i = 0; ts_keywords[i]; i++) {
        if (str_contains(description, ts_keywords[i])) {
            spec->language = LANG_TYPESCRIPT;
            spec->build_system = BUILD_NPM;
            goto lang_done;
        }
    }
    for (int i = 0; js_keywords[i]; i++) {
        if (str_contains(description, js_keywords[i])) {
            spec->language = LANG_JAVASCRIPT;
            spec->build_system = BUILD_NPM;
            goto lang_done;
        }
    }
    for (int i = 0; go_keywords[i]; i++) {
        if (str_contains(description, go_keywords[i])) {
            spec->language = LANG_GO;
            spec->build_system = BUILD_CUSTOM;
            goto lang_done;
        }
    }
    for (int i = 0; c_keywords[i]; i++) {
        if (str_contains(description, c_keywords[i])) {
            spec->language = LANG_C;
            goto lang_done;
        }
    }

lang_done:

    /* Detect project type */
    for (int i = 0; game_keywords[i]; i++) {
        if (str_contains(description, game_keywords[i])) {
            spec->type = PROJECT_GAME;
            goto type_done;
        }
    }
    for (int i = 0; lib_keywords[i]; i++) {
        if (str_contains(description, lib_keywords[i])) {
            spec->type = PROJECT_LIBRARY;
            goto type_done;
        }
    }
    for (int i = 0; cli_keywords[i]; i++) {
        if (str_contains(description, cli_keywords[i])) {
            spec->type = PROJECT_CLI;
            goto type_done;
        }
    }
    for (int i = 0; web_keywords[i]; i++) {
        if (str_contains(description, web_keywords[i])) {
            spec->type = PROJECT_WEB;
            goto type_done;
        }
    }
    for (int i = 0; gui_keywords[i]; i++) {
        if (str_contains(description, gui_keywords[i])) {
            spec->type = PROJECT_GUI;
            goto type_done;
        }
    }

type_done:

    /* Detect dependencies */
    int dep_capacity = 10;
    spec->dependencies = malloc(dep_capacity * sizeof(char*));
    spec->dependency_count = 0;

    for (int i = 0; common_deps[i]; i++) {
        if (str_contains(description, common_deps[i])) {
            if (spec->dependency_count >= dep_capacity) {
                dep_capacity *= 2;
                spec->dependencies = realloc(spec->dependencies, dep_capacity * sizeof(char*));
            }
            spec->dependencies[spec->dependency_count++] = strdup(common_deps[i]);
        }
    }

    /* Detect options */
    spec->with_tests = str_contains(description, "test");
    spec->with_docs = str_contains(description, "doc");

    /* Extract project name (simple: use first word or "my_project") */
    spec->name = strdup("my_project");

    /* Detect C++ standard */
    if (str_contains(description, "c++20") || str_contains(description, "cpp20")) {
        free(spec->cpp_standard);
        spec->cpp_standard = strdup("20");
    } else if (str_contains(description, "c++14") || str_contains(description, "cpp14")) {
        free(spec->cpp_standard);
        spec->cpp_standard = strdup("14");
    }

    /* Detect license */
    if (str_contains(description, "mit")) {
        spec->license = strdup("MIT");
    } else if (str_contains(description, "apache")) {
        spec->license = strdup("Apache-2.0");
    } else if (str_contains(description, "gpl")) {
        spec->license = strdup("GPL-3.0");
    }

    return spec;
}

/* ========================================================================
 * Template Generation
 * ======================================================================== */

char* generate_cmake_content(const ProjectSpec* spec) {
    if (!spec) return NULL;

    char* content = malloc(4096);
    if (!content) return NULL;

    const char* lang_std = spec->language == LANG_CPP ? spec->cpp_standard : spec->c_standard;
    const char* cmake_lang = spec->language == LANG_CPP ? "CXX" : "C";
    const char* std_var = spec->language == LANG_CPP ? "CMAKE_CXX_STANDARD" : "CMAKE_C_STANDARD";
    const char* ext = spec->language == LANG_CPP ? "cpp" : "c";

    int written = snprintf(content, 4096,
        "cmake_minimum_required(VERSION 3.16)\n"
        "project(%s LANGUAGES %s)\n\n"
        "set(%s %s)\n"
        "set(%s_REQUIRED ON)\n\n",
        spec->name, cmake_lang, std_var, lang_std, std_var);

    /* Add dependencies */
    for (int i = 0; i < spec->dependency_count; i++) {
        written += snprintf(content + written, 4096 - written,
            "find_package(%s REQUIRED)\n", spec->dependencies[i]);
    }
    if (spec->dependency_count > 0) {
        written += snprintf(content + written, 4096 - written, "\n");
    }

    /* Add target */
    if (spec->type == PROJECT_LIBRARY) {
        written += snprintf(content + written, 4096 - written,
            "add_library(%s\n"
            "    src/%s.%s\n"
            ")\n\n"
            "target_include_directories(%s PUBLIC\n"
            "    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>\n"
            "    $<INSTALL_INTERFACE:include>\n"
            ")\n",
            spec->name, spec->name, ext, spec->name);
    } else {
        written += snprintf(content + written, 4096 - written,
            "add_executable(%s\n"
            "    src/main.%s\n"
            ")\n",
            spec->name, ext);
    }

    /* Link dependencies */
    if (spec->dependency_count > 0) {
        written += snprintf(content + written, 4096 - written,
            "\ntarget_link_libraries(%s PRIVATE\n", spec->name);
        for (int i = 0; i < spec->dependency_count; i++) {
            written += snprintf(content + written, 4096 - written,
                "    %s\n", spec->dependencies[i]);
        }
        written += snprintf(content + written, 4096 - written, ")\n");
    }

    /* Add tests if requested */
    if (spec->with_tests) {
        snprintf(content + written, 4096 - written,
            "\n# Testing\n"
            "enable_testing()\n"
            "add_subdirectory(tests)\n");
    }

    return content;
}

char* generate_cargo_content(const ProjectSpec* spec) {
    if (!spec) return NULL;

    char* content = malloc(1024);
    if (!content) return NULL;

    int written = snprintf(content, 1024,
        "[package]\n"
        "name = \"%s\"\n"
        "version = \"0.1.0\"\n"
        "edition = \"2021\"\n\n"
        "[dependencies]\n",
        spec->name);

    for (int i = 0; i < spec->dependency_count; i++) {
        written += snprintf(content + written, 1024 - written,
            "%s = \"*\"\n", spec->dependencies[i]);
    }

    return content;
}

char* generate_package_json_content(const ProjectSpec* spec) {
    if (!spec) return NULL;

    char* content = malloc(2048);
    if (!content) return NULL;

    const char* main_file = spec->language == LANG_TYPESCRIPT ? "dist/index.js" : "src/index.js";

    snprintf(content, 2048,
        "{\n"
        "  \"name\": \"%s\",\n"
        "  \"version\": \"0.1.0\",\n"
        "  \"description\": \"%s\",\n"
        "  \"main\": \"%s\",\n"
        "  \"scripts\": {\n"
        "    \"start\": \"node %s\",\n"
        "    \"test\": \"echo \\\"Error: no test specified\\\" && exit 1\"\n"
        "  },\n"
        "  \"keywords\": [],\n"
        "  \"author\": \"\",\n"
        "  \"license\": \"%s\"\n"
        "}\n",
        spec->name,
        spec->description ? spec->description : "",
        main_file,
        main_file,
        spec->license ? spec->license : "MIT");

    return content;
}

char* generate_main_source(const ProjectSpec* spec) {
    if (!spec) return NULL;

    char* content = malloc(2048);
    if (!content) return NULL;

    switch (spec->language) {
        case LANG_C:
            snprintf(content, 2048,
                "#include <stdio.h>\n\n"
                "int main(int argc, char* argv[]) {\n"
                "    (void)argc;\n"
                "    (void)argv;\n"
                "    printf(\"Hello from %s!\\n\");\n"
                "    return 0;\n"
                "}\n",
                spec->name);
            break;

        case LANG_CPP:
            if (spec->type == PROJECT_GAME) {
                snprintf(content, 2048,
                    "#include <iostream>\n\n"
                    "// TODO: Include game libraries (SDL2, OpenGL, etc.)\n\n"
                    "int main(int argc, char* argv[]) {\n"
                    "    (void)argc;\n"
                    "    (void)argv;\n\n"
                    "    std::cout << \"Starting %s...\" << std::endl;\n\n"
                    "    // TODO: Initialize game\n"
                    "    // TODO: Game loop\n"
                    "    // TODO: Cleanup\n\n"
                    "    return 0;\n"
                    "}\n",
                    spec->name);
            } else {
                snprintf(content, 2048,
                    "#include <iostream>\n\n"
                    "int main(int argc, char* argv[]) {\n"
                    "    (void)argc;\n"
                    "    (void)argv;\n"
                    "    std::cout << \"Hello from %s!\" << std::endl;\n"
                    "    return 0;\n"
                    "}\n",
                    spec->name);
            }
            break;

        case LANG_RUST:
            snprintf(content, 2048,
                "fn main() {\n"
                "    println!(\"Hello from %s!\");\n"
                "}\n",
                spec->name);
            break;

        case LANG_PYTHON:
            snprintf(content, 2048,
                "#!/usr/bin/env python3\n"
                "\"\"\"%s - %s\"\"\"\n\n"
                "def main():\n"
                "    print(\"Hello from %s!\")\n\n"
                "if __name__ == \"__main__\":\n"
                "    main()\n",
                spec->name,
                spec->description ? spec->description : "A Python project",
                spec->name);
            break;

        case LANG_JAVASCRIPT:
        case LANG_TYPESCRIPT:
            snprintf(content, 2048,
                "// %s\n\n"
                "console.log('Hello from %s!');\n",
                spec->description ? spec->description : spec->name,
                spec->name);
            break;

        case LANG_GO:
            snprintf(content, 2048,
                "package main\n\n"
                "import \"fmt\"\n\n"
                "func main() {\n"
                "    fmt.Println(\"Hello from %s!\")\n"
                "}\n",
                spec->name);
            break;

        default:
            snprintf(content, 2048,
                "// %s\n"
                "// TODO: Implement main\n",
                spec->name);
            break;
    }

    return content;
}

char* generate_readme(const ProjectSpec* spec) {
    if (!spec) return NULL;

    char* content = malloc(2048);
    if (!content) return NULL;

    const char* lang_name = language_to_string(spec->language);

    int written = snprintf(content, 2048,
        "# %s\n\n"
        "%s\n\n"
        "## Language\n\n"
        "%s\n\n",
        spec->name,
        spec->description ? spec->description : "A new project generated by CyxMake.",
        lang_name);

    /* Build instructions */
    written += snprintf(content + written, 2048 - written, "## Building\n\n");

    switch (spec->build_system) {
        case BUILD_CMAKE:
            written += snprintf(content + written, 2048 - written,
                "```bash\n"
                "mkdir build && cd build\n"
                "cmake ..\n"
                "cmake --build .\n"
                "```\n\n");
            break;
        case BUILD_CARGO:
            written += snprintf(content + written, 2048 - written,
                "```bash\n"
                "cargo build\n"
                "cargo run\n"
                "```\n\n");
            break;
        case BUILD_NPM:
            written += snprintf(content + written, 2048 - written,
                "```bash\n"
                "npm install\n"
                "npm start\n"
                "```\n\n");
            break;
        default:
            written += snprintf(content + written, 2048 - written,
                "See project-specific build instructions.\n\n");
            break;
    }

    /* Dependencies */
    if (spec->dependency_count > 0) {
        written += snprintf(content + written, 2048 - written, "## Dependencies\n\n");
        for (int i = 0; i < spec->dependency_count; i++) {
            written += snprintf(content + written, 2048 - written,
                "- %s\n", spec->dependencies[i]);
        }
        written += snprintf(content + written, 2048 - written, "\n");
    }

    /* License */
    if (spec->license) {
        snprintf(content + written, 2048 - written,
            "## License\n\n"
            "This project is licensed under the %s license.\n",
            spec->license);
    }

    return content;
}

char* generate_gitignore(Language language, BuildSystem build_system) {
    char* content = malloc(1024);
    if (!content) return NULL;

    int written = snprintf(content, 1024,
        "# Build directories\n"
        "build/\n"
        "out/\n"
        "bin/\n"
        "lib/\n\n"
        "# IDE\n"
        ".vscode/\n"
        ".idea/\n"
        "*.swp\n"
        "*.swo\n"
        "*~\n\n");

    switch (language) {
        case LANG_C:
        case LANG_CPP:
            written += snprintf(content + written, 1024 - written,
                "# C/C++\n"
                "*.o\n"
                "*.obj\n"
                "*.a\n"
                "*.lib\n"
                "*.so\n"
                "*.dll\n"
                "*.exe\n"
                "*.pdb\n\n");
            break;
        case LANG_RUST:
            written += snprintf(content + written, 1024 - written,
                "# Rust\n"
                "target/\n"
                "Cargo.lock\n\n");
            break;
        case LANG_PYTHON:
            written += snprintf(content + written, 1024 - written,
                "# Python\n"
                "__pycache__/\n"
                "*.py[cod]\n"
                ".venv/\n"
                "venv/\n"
                "*.egg-info/\n"
                "dist/\n\n");
            break;
        case LANG_JAVASCRIPT:
        case LANG_TYPESCRIPT:
            written += snprintf(content + written, 1024 - written,
                "# Node.js\n"
                "node_modules/\n"
                "dist/\n"
                "*.log\n\n");
            break;
        default:
            break;
    }

    if (build_system == BUILD_CMAKE) {
        snprintf(content + written, 1024 - written,
            "# CMake\n"
            "CMakeCache.txt\n"
            "CMakeFiles/\n"
            "cmake_install.cmake\n"
            "compile_commands.json\n");
    }

    return content;
}

/* ========================================================================
 * Project Generation
 * ======================================================================== */

GenerationResult* project_generate(const ProjectSpec* spec, const char* output_path) {
    if (!spec || !output_path) return NULL;

    GenerationResult* result = calloc(1, sizeof(GenerationResult));
    if (!result) return NULL;

    result->output_path = strdup(output_path);
    result->files_created = malloc(20 * sizeof(char*));
    result->file_count = 0;

    log_info("Generating %s project: %s", language_to_string(spec->language), spec->name);

    /* Create project directory */
    if (!ensure_directory(output_path)) {
        result->success = false;
        result->error_message = strdup("Failed to create project directory");
        return result;
    }

    /* Create subdirectories */
    char* src_dir = path_join(output_path, "src");
    char* include_dir = path_join(output_path, "include");

    ensure_directory(src_dir);
    if (spec->language == LANG_C || spec->language == LANG_CPP) {
        ensure_directory(include_dir);
    }

    if (spec->with_tests) {
        char* tests_dir = path_join(output_path, "tests");
        ensure_directory(tests_dir);
        free(tests_dir);
    }

    if (spec->with_docs) {
        char* docs_dir = path_join(output_path, "docs");
        ensure_directory(docs_dir);
        free(docs_dir);
    }

    /* Generate build system file */
    char* build_file = NULL;
    char* build_content = NULL;

    switch (spec->build_system) {
        case BUILD_CMAKE:
            build_file = path_join(output_path, "CMakeLists.txt");
            build_content = generate_cmake_content(spec);
            break;
        case BUILD_CARGO:
            build_file = path_join(output_path, "Cargo.toml");
            build_content = generate_cargo_content(spec);
            break;
        case BUILD_NPM:
            build_file = path_join(output_path, "package.json");
            build_content = generate_package_json_content(spec);
            break;
        default:
            break;
    }

    if (build_file && build_content) {
        if (write_file(build_file, build_content)) {
            result->files_created[result->file_count++] = strdup(build_file);
            log_success("Created %s", build_file);
        }
        free(build_content);
        free(build_file);
    }

    /* Generate main source file */
    char* main_content = generate_main_source(spec);
    if (main_content) {
        char main_filename[64];
        const char* ext = language_extension(spec->language, false);

        if (spec->build_system == BUILD_CARGO) {
            snprintf(main_filename, sizeof(main_filename), "main%s", ext);
        } else if (spec->language == LANG_JAVASCRIPT || spec->language == LANG_TYPESCRIPT) {
            snprintf(main_filename, sizeof(main_filename), "index%s", ext);
        } else {
            snprintf(main_filename, sizeof(main_filename), "main%s", ext);
        }

        char* main_path = path_join(src_dir, main_filename);
        if (write_file(main_path, main_content)) {
            result->files_created[result->file_count++] = strdup(main_path);
            log_success("Created %s", main_path);
        }
        free(main_path);
        free(main_content);
    }

    /* Generate README */
    char* readme_content = generate_readme(spec);
    if (readme_content) {
        char* readme_path = path_join(output_path, "README.md");
        if (write_file(readme_path, readme_content)) {
            result->files_created[result->file_count++] = strdup(readme_path);
            log_success("Created README.md");
        }
        free(readme_path);
        free(readme_content);
    }

    /* Generate .gitignore */
    if (spec->with_git) {
        char* gitignore_content = generate_gitignore(spec->language, spec->build_system);
        if (gitignore_content) {
            char* gitignore_path = path_join(output_path, ".gitignore");
            if (write_file(gitignore_path, gitignore_content)) {
                result->files_created[result->file_count++] = strdup(gitignore_path);
                log_success("Created .gitignore");
            }
            free(gitignore_path);
            free(gitignore_content);
        }
    }

    free(src_dir);
    free(include_dir);

    result->success = true;
    log_success("Project generated successfully at: %s", output_path);
    log_info("Created %d files", result->file_count);

    return result;
}

void generation_result_free(GenerationResult* result) {
    if (!result) return;

    free(result->output_path);
    free(result->error_message);

    if (result->files_created) {
        for (int i = 0; i < result->file_count; i++) {
            free(result->files_created[i]);
        }
        free(result->files_created);
    }

    free(result);
}
