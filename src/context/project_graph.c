/**
 * @file project_graph.c
 * @brief Project Graph implementation - Deep file analysis with imports/exports
 */

#include "cyxmake/project_graph.h"
#include "cyxmake/logger.h"
#include "cyxmake/compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef _WIN32
    #include <windows.h>
    #define PATH_SEP '\\'
    #define PATH_SEP_STR "\\"
#else
    #define PATH_SEP '/'
    #define PATH_SEP_STR "/"
#endif

#define MAX_LINE_LENGTH 4096
#define INITIAL_CAPACITY 16

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/* Skip whitespace in string */
static const char* skip_whitespace(const char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

/* Trim whitespace from end of string (in place) */
static void trim_end(char* s) {
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

/* Get file extension */
static const char* get_extension(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot || dot == path) return "";
    return dot + 1;
}

/* Make relative path from absolute path */
static char* make_relative_path(const char* path, const char* root) {
    size_t root_len = strlen(root);
    if (strncmp(path, root, root_len) == 0) {
        const char* rel = path + root_len;
        while (*rel == PATH_SEP || *rel == '/' || *rel == '\\') rel++;
        return strdup(rel);
    }
    return strdup(path);
}

/* Check if file exists */
static bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

/* Detect language from extension */
static Language detect_language(const char* path) {
    const char* ext = get_extension(path);
    if (!ext || !*ext) return LANG_UNKNOWN;

    if (strcmp(ext, "c") == 0) return LANG_C;
    if (strcmp(ext, "h") == 0) return LANG_C;
    if (strcmp(ext, "cpp") == 0 || strcmp(ext, "cc") == 0 ||
        strcmp(ext, "cxx") == 0 || strcmp(ext, "hpp") == 0) return LANG_CPP;
    if (strcmp(ext, "py") == 0) return LANG_PYTHON;
    if (strcmp(ext, "js") == 0 || strcmp(ext, "mjs") == 0 ||
        strcmp(ext, "jsx") == 0) return LANG_JAVASCRIPT;
    if (strcmp(ext, "ts") == 0 || strcmp(ext, "tsx") == 0) return LANG_TYPESCRIPT;
    if (strcmp(ext, "rs") == 0) return LANG_RUST;
    if (strcmp(ext, "go") == 0) return LANG_GO;

    return LANG_UNKNOWN;
}

/* ============================================================================
 * Import/Export Creation and Freeing
 * ============================================================================ */

FileImport* file_import_create(void) {
    FileImport* import = calloc(1, sizeof(FileImport));
    return import;
}

void file_import_free(FileImport* import) {
    if (!import) return;
    free(import->raw_statement);
    free(import->module_name);
    free(import->resolved_path);
    for (int i = 0; i < import->symbol_count; i++) {
        free(import->imported_symbols[i]);
    }
    free(import->imported_symbols);
    free(import);
}

FileExport* file_export_create(void) {
    FileExport* export_sym = calloc(1, sizeof(FileExport));
    return export_sym;
}

void file_export_free(FileExport* export_sym) {
    if (!export_sym) return;
    free(export_sym->name);
    free(export_sym->type);
    free(export_sym);
}

/* ============================================================================
 * Graph Node Functions
 * ============================================================================ */

GraphNode* graph_node_create(const char* path, const char* project_root) {
    if (!path) return NULL;

    GraphNode* node = calloc(1, sizeof(GraphNode));
    if (!node) return NULL;

    node->path = strdup(path);
    node->relative_path = make_relative_path(path, project_root);
    node->language = detect_language(path);

    /* Initialize import array */
    node->import_capacity = INITIAL_CAPACITY;
    node->imports = calloc(node->import_capacity, sizeof(FileImport*));

    /* Initialize export array */
    node->export_capacity = INITIAL_CAPACITY;
    node->exports = calloc(node->export_capacity, sizeof(FileExport*));

    /* Check for entry points and test files */
    const char* filename = strrchr(path, PATH_SEP);
    if (!filename) filename = strrchr(path, '/');
    if (!filename) filename = path;
    else filename++;

    /* Common entry points */
    if (strcmp(filename, "main.c") == 0 ||
        strcmp(filename, "main.cpp") == 0 ||
        strcmp(filename, "main.rs") == 0 ||
        strcmp(filename, "index.js") == 0 ||
        strcmp(filename, "index.ts") == 0 ||
        strcmp(filename, "__main__.py") == 0) {
        node->is_entry_point = true;
    }

    /* Test files */
    if (strstr(filename, "test_") == filename ||
        strstr(filename, "_test.") != NULL ||
        strstr(filename, ".test.") != NULL ||
        strstr(filename, ".spec.") != NULL) {
        node->is_test_file = true;
    }

    return node;
}

void graph_node_free(GraphNode* node) {
    if (!node) return;

    free(node->path);
    free(node->relative_path);

    /* Free imports */
    for (int i = 0; i < node->import_count; i++) {
        file_import_free(node->imports[i]);
    }
    free(node->imports);

    /* Free exports */
    for (int i = 0; i < node->export_count; i++) {
        file_export_free(node->exports[i]);
    }
    free(node->exports);

    /* Don't free depends_on/depended_by arrays - they contain pointers to other nodes */
    free(node->depends_on);
    free(node->depended_by);

    free(node);
}

bool graph_node_add_import(GraphNode* node, FileImport* import) {
    if (!node || !import) return false;

    if (node->import_count >= node->import_capacity) {
        int new_cap = node->import_capacity * 2;
        FileImport** new_imports = realloc(node->imports, new_cap * sizeof(FileImport*));
        if (!new_imports) return false;
        node->imports = new_imports;
        node->import_capacity = new_cap;
    }

    node->imports[node->import_count++] = import;
    return true;
}

bool graph_node_add_export(GraphNode* node, FileExport* export_sym) {
    if (!node || !export_sym) return false;

    if (node->export_count >= node->export_capacity) {
        int new_cap = node->export_capacity * 2;
        FileExport** new_exports = realloc(node->exports, new_cap * sizeof(FileExport*));
        if (!new_exports) return false;
        node->exports = new_exports;
        node->export_capacity = new_cap;
    }

    node->exports[node->export_count++] = export_sym;
    return true;
}

/* ============================================================================
 * Import Parsing - Language Specific
 * ============================================================================ */

/* Parse C/C++ #include directive */
static FileImport* parse_c_include(const char* line, int line_num) {
    const char* p = skip_whitespace(line);
    if (*p != '#') return NULL;
    p = skip_whitespace(p + 1);
    if (strncmp(p, "include", 7) != 0) return NULL;
    p = skip_whitespace(p + 7);

    FileImport* import = file_import_create();
    if (!import) return NULL;

    import->type = IMPORT_TYPE_INCLUDE;
    import->line_number = line_num;
    import->raw_statement = strdup(line);

    if (*p == '<') {
        /* System include: #include <header.h> */
        import->scope = IMPORT_SCOPE_SYSTEM;
        p++;
        const char* end = strchr(p, '>');
        if (end) {
            size_t len = end - p;
            import->module_name = malloc(len + 1);
            if (import->module_name) {
                strncpy(import->module_name, p, len);
                import->module_name[len] = '\0';
            }
        }
    } else if (*p == '"') {
        /* Local include: #include "header.h" */
        import->scope = IMPORT_SCOPE_LOCAL;
        p++;
        const char* end = strchr(p, '"');
        if (end) {
            size_t len = end - p;
            import->module_name = malloc(len + 1);
            if (import->module_name) {
                strncpy(import->module_name, p, len);
                import->module_name[len] = '\0';
            }
        }
    } else {
        file_import_free(import);
        return NULL;
    }

    return import;
}

/* Parse Python import statement */
static FileImport* parse_python_import(const char* line, int line_num) {
    const char* p = skip_whitespace(line);

    /* "from X import Y" */
    if (strncmp(p, "from", 4) == 0 && isspace((unsigned char)p[4])) {
        FileImport* import = file_import_create();
        if (!import) return NULL;

        import->type = IMPORT_TYPE_FROM;
        import->line_number = line_num;
        import->raw_statement = strdup(line);

        p = skip_whitespace(p + 4);
        const char* import_kw = strstr(p, " import ");
        if (import_kw) {
            /* Extract module name */
            size_t mod_len = import_kw - p;
            import->module_name = malloc(mod_len + 1);
            if (import->module_name) {
                strncpy(import->module_name, p, mod_len);
                import->module_name[mod_len] = '\0';
                trim_end(import->module_name);
            }

            /* Determine scope */
            if (import->module_name && import->module_name[0] == '.') {
                import->scope = IMPORT_SCOPE_LOCAL;
            } else {
                import->scope = IMPORT_SCOPE_EXTERNAL;
            }

            /* TODO: Parse imported symbols */
        }

        return import;
    }

    /* "import X" */
    if (strncmp(p, "import", 6) == 0 && isspace((unsigned char)p[6])) {
        FileImport* import = file_import_create();
        if (!import) return NULL;

        import->type = IMPORT_TYPE_IMPORT;
        import->line_number = line_num;
        import->raw_statement = strdup(line);

        p = skip_whitespace(p + 6);

        /* Extract module name (stop at comma, 'as', newline) */
        size_t len = 0;
        while (p[len] && p[len] != ',' && p[len] != '\n' &&
               !(p[len] == ' ' && p[len+1] == 'a' && p[len+2] == 's' && p[len+3] == ' ')) {
            len++;
        }

        import->module_name = malloc(len + 1);
        if (import->module_name) {
            strncpy(import->module_name, p, len);
            import->module_name[len] = '\0';
            trim_end(import->module_name);
        }

        /* Relative imports start with . */
        if (import->module_name && import->module_name[0] == '.') {
            import->scope = IMPORT_SCOPE_LOCAL;
        } else {
            import->scope = IMPORT_SCOPE_EXTERNAL;
        }

        return import;
    }

    return NULL;
}

/* Parse JavaScript/TypeScript import */
static FileImport* parse_js_import(const char* line, int line_num) {
    const char* p = skip_whitespace(line);

    /* import statement */
    if (strncmp(p, "import", 6) == 0) {
        FileImport* import = file_import_create();
        if (!import) return NULL;

        import->type = IMPORT_TYPE_IMPORT;
        import->line_number = line_num;
        import->raw_statement = strdup(line);

        /* Find 'from' keyword to get module path */
        const char* from = strstr(p, "from");
        if (from) {
            from = skip_whitespace(from + 4);
            char quote = *from;
            if (quote == '"' || quote == '\'') {
                from++;
                const char* end = strchr(from, quote);
                if (end) {
                    size_t len = end - from;
                    import->module_name = malloc(len + 1);
                    if (import->module_name) {
                        strncpy(import->module_name, from, len);
                        import->module_name[len] = '\0';
                    }
                }
            }
        }

        /* Determine scope */
        if (import->module_name) {
            if (import->module_name[0] == '.' || import->module_name[0] == '/') {
                import->scope = IMPORT_SCOPE_LOCAL;
            } else {
                import->scope = IMPORT_SCOPE_EXTERNAL;
            }

            /* Check for namespace import: import * as X */
            if (strstr(p, "* as ")) {
                import->is_namespace_import = true;
            }
        }

        return import;
    }

    /* require() call */
    const char* req = strstr(p, "require(");
    if (req) {
        FileImport* import = file_import_create();
        if (!import) return NULL;

        import->type = IMPORT_TYPE_REQUIRE;
        import->line_number = line_num;
        import->raw_statement = strdup(line);

        req += 8;  /* Skip "require(" */
        char quote = *req;
        if (quote == '"' || quote == '\'') {
            req++;
            const char* end = strchr(req, quote);
            if (end) {
                size_t len = end - req;
                import->module_name = malloc(len + 1);
                if (import->module_name) {
                    strncpy(import->module_name, req, len);
                    import->module_name[len] = '\0';
                }
            }
        }

        if (import->module_name) {
            if (import->module_name[0] == '.' || import->module_name[0] == '/') {
                import->scope = IMPORT_SCOPE_LOCAL;
            } else {
                import->scope = IMPORT_SCOPE_EXTERNAL;
            }
        }

        return import;
    }

    return NULL;
}

/* Parse Rust use statement */
static FileImport* parse_rust_use(const char* line, int line_num) {
    const char* p = skip_whitespace(line);

    /* mod statement */
    if (strncmp(p, "mod", 3) == 0 && isspace((unsigned char)p[3])) {
        FileImport* import = file_import_create();
        if (!import) return NULL;

        import->type = IMPORT_TYPE_MOD;
        import->line_number = line_num;
        import->raw_statement = strdup(line);
        import->scope = IMPORT_SCOPE_LOCAL;

        p = skip_whitespace(p + 3);
        size_t len = 0;
        while (p[len] && p[len] != ';' && !isspace((unsigned char)p[len])) {
            len++;
        }

        import->module_name = malloc(len + 1);
        if (import->module_name) {
            strncpy(import->module_name, p, len);
            import->module_name[len] = '\0';
        }

        return import;
    }

    /* use statement */
    if (strncmp(p, "use", 3) == 0 && isspace((unsigned char)p[3])) {
        FileImport* import = file_import_create();
        if (!import) return NULL;

        import->type = IMPORT_TYPE_USE;
        import->line_number = line_num;
        import->raw_statement = strdup(line);

        p = skip_whitespace(p + 3);

        /* Determine scope */
        if (strncmp(p, "crate::", 7) == 0 || strncmp(p, "self::", 6) == 0 ||
            strncmp(p, "super::", 7) == 0) {
            import->scope = IMPORT_SCOPE_LOCAL;
        } else if (strncmp(p, "std::", 5) == 0) {
            import->scope = IMPORT_SCOPE_SYSTEM;
        } else {
            import->scope = IMPORT_SCOPE_EXTERNAL;
        }

        /* Extract module path */
        size_t len = 0;
        while (p[len] && p[len] != ';' && p[len] != '{') {
            len++;
        }

        import->module_name = malloc(len + 1);
        if (import->module_name) {
            strncpy(import->module_name, p, len);
            import->module_name[len] = '\0';
            trim_end(import->module_name);
        }

        return import;
    }

    return NULL;
}

/* ============================================================================
 * Import Analysis
 * ============================================================================ */

int project_graph_analyze_imports(GraphNode* node) {
    if (!node || !node->path) return 0;

    FILE* f = fopen(node->path, "r");
    if (!f) {
        log_debug("Cannot open file for import analysis: %s", node->path);
        return 0;
    }

    char line[MAX_LINE_LENGTH];
    int line_num = 0;
    int found_imports = 0;

    while (fgets(line, sizeof(line), f)) {
        line_num++;
        FileImport* import = NULL;

        switch (node->language) {
            case LANG_C:
            case LANG_CPP:
                import = parse_c_include(line, line_num);
                break;
            case LANG_PYTHON:
                import = parse_python_import(line, line_num);
                break;
            case LANG_JAVASCRIPT:
            case LANG_TYPESCRIPT:
                import = parse_js_import(line, line_num);
                break;
            case LANG_RUST:
                import = parse_rust_use(line, line_num);
                break;
            default:
                break;
        }

        if (import) {
            if (graph_node_add_import(node, import)) {
                found_imports++;
            } else {
                file_import_free(import);
            }
        }

        /* Count lines */
        node->total_lines++;
        const char* p = skip_whitespace(line);
        if (*p && *p != '\n') {
            node->code_lines++;
        }
    }

    fclose(f);
    node->is_analyzed = true;
    return found_imports;
}

/* ============================================================================
 * Export Analysis (simplified)
 * ============================================================================ */

int project_graph_analyze_exports(GraphNode* node) {
    if (!node || !node->path) return 0;

    FILE* f = fopen(node->path, "r");
    if (!f) return 0;

    char line[MAX_LINE_LENGTH];
    int line_num = 0;
    int found_exports = 0;

    while (fgets(line, sizeof(line), f)) {
        line_num++;
        const char* p = skip_whitespace(line);

        /* JavaScript/TypeScript export */
        if ((node->language == LANG_JAVASCRIPT || node->language == LANG_TYPESCRIPT) &&
            strncmp(p, "export", 6) == 0) {
            FileExport* exp = file_export_create();
            if (exp) {
                exp->line_number = line_num;
                if (strstr(p, "default")) {
                    exp->is_default_export = true;
                    exp->name = strdup("default");
                } else if (strstr(p, "function")) {
                    exp->type = strdup("function");
                    /* TODO: Extract function name */
                } else if (strstr(p, "class")) {
                    exp->type = strdup("class");
                } else if (strstr(p, "const") || strstr(p, "let") || strstr(p, "var")) {
                    exp->type = strdup("variable");
                }

                if (graph_node_add_export(node, exp)) {
                    found_exports++;
                } else {
                    file_export_free(exp);
                }
            }
        }

        /* Python __all__ (simplified) */
        if (node->language == LANG_PYTHON && strncmp(p, "__all__", 7) == 0) {
            /* TODO: Parse __all__ list */
        }

        /* Rust pub */
        if (node->language == LANG_RUST && strncmp(p, "pub ", 4) == 0) {
            FileExport* exp = file_export_create();
            if (exp) {
                exp->line_number = line_num;
                exp->is_public = true;
                if (strstr(p, "fn ")) {
                    exp->type = strdup("function");
                } else if (strstr(p, "struct ")) {
                    exp->type = strdup("struct");
                } else if (strstr(p, "enum ")) {
                    exp->type = strdup("enum");
                } else if (strstr(p, "mod ")) {
                    exp->type = strdup("module");
                }

                if (graph_node_add_export(node, exp)) {
                    found_exports++;
                } else {
                    file_export_free(exp);
                }
            }
        }
    }

    fclose(f);
    return found_exports;
}

/* ============================================================================
 * Import Resolution
 * ============================================================================ */

int project_graph_resolve_imports(ProjectGraph* graph, GraphNode* node) {
    if (!graph || !node) return 0;

    int resolved = 0;

    for (int i = 0; i < node->import_count; i++) {
        FileImport* import = node->imports[i];
        if (!import || !import->module_name) continue;
        if (import->resolved_path) continue;  /* Already resolved */
        if (import->scope == IMPORT_SCOPE_SYSTEM ||
            import->scope == IMPORT_SCOPE_EXTERNAL) {
            continue;  /* Don't try to resolve external imports */
        }

        /* Try to resolve local import */
        char resolved_path[4096];
        const char* node_dir = node->path;
        char* last_sep = strrchr(node->path, PATH_SEP);
        if (!last_sep) last_sep = strrchr(node->path, '/');

        char dir[4096];
        if (last_sep) {
            size_t len = last_sep - node->path;
            strncpy(dir, node->path, len);
            dir[len] = '\0';
        } else {
            strcpy(dir, graph->project_root);
        }

        /* Build possible resolved paths */
        const char* mod = import->module_name;

        /* Try direct path */
        snprintf(resolved_path, sizeof(resolved_path), "%s%c%s", dir, PATH_SEP, mod);
        if (file_exists(resolved_path)) {
            import->resolved_path = strdup(resolved_path);
            resolved++;
            continue;
        }

        /* Try with common extensions */
        const char* extensions[] = {".c", ".h", ".cpp", ".hpp", ".py", ".js", ".ts", ".rs", NULL};
        for (int e = 0; extensions[e]; e++) {
            snprintf(resolved_path, sizeof(resolved_path), "%s%c%s%s",
                     dir, PATH_SEP, mod, extensions[e]);
            if (file_exists(resolved_path)) {
                import->resolved_path = strdup(resolved_path);
                resolved++;
                break;
            }
        }

        /* For JS/TS, try index files */
        if (!import->resolved_path &&
            (node->language == LANG_JAVASCRIPT || node->language == LANG_TYPESCRIPT)) {
            snprintf(resolved_path, sizeof(resolved_path), "%s%c%s%cindex.js",
                     dir, PATH_SEP, mod, PATH_SEP);
            if (file_exists(resolved_path)) {
                import->resolved_path = strdup(resolved_path);
                resolved++;
            } else {
                snprintf(resolved_path, sizeof(resolved_path), "%s%c%s%cindex.ts",
                         dir, PATH_SEP, mod, PATH_SEP);
                if (file_exists(resolved_path)) {
                    import->resolved_path = strdup(resolved_path);
                    resolved++;
                }
            }
        }
    }

    return resolved;
}

/* ============================================================================
 * Project Graph Functions
 * ============================================================================ */

ProjectGraph* project_graph_create(const char* project_root) {
    if (!project_root) return NULL;

    ProjectGraph* graph = calloc(1, sizeof(ProjectGraph));
    if (!graph) return NULL;

    graph->project_root = strdup(project_root);
    graph->node_capacity = INITIAL_CAPACITY;
    graph->nodes = calloc(graph->node_capacity, sizeof(GraphNode*));

    if (!graph->nodes) {
        free(graph->project_root);
        free(graph);
        return NULL;
    }

    return graph;
}

void project_graph_free(ProjectGraph* graph) {
    if (!graph) return;

    free(graph->project_root);

    /* Free all nodes */
    for (int i = 0; i < graph->node_count; i++) {
        graph_node_free(graph->nodes[i]);
    }
    free(graph->nodes);

    /* Free entry points array (nodes already freed above) */
    free(graph->entry_points);

    /* Free external deps */
    for (int i = 0; i < graph->external_dep_count; i++) {
        free(graph->external_deps[i]);
    }
    free(graph->external_deps);

    /* Free circular deps */
    for (int i = 0; i < graph->circular_dep_count; i++) {
        free(graph->circular_deps[i]);
    }
    free(graph->circular_deps);

    /* Free build order array (nodes already freed above) */
    free(graph->build_order);

    free(graph);
}

GraphNode* project_graph_add_file(ProjectGraph* graph, const char* file_path) {
    if (!graph || !file_path) return NULL;

    /* Check if already exists */
    GraphNode* existing = project_graph_find(graph, file_path);
    if (existing) return existing;

    /* Expand capacity if needed */
    if (graph->node_count >= graph->node_capacity) {
        int new_cap = graph->node_capacity * 2;
        GraphNode** new_nodes = realloc(graph->nodes, new_cap * sizeof(GraphNode*));
        if (!new_nodes) return NULL;
        graph->nodes = new_nodes;
        graph->node_capacity = new_cap;
    }

    /* Create and add node */
    GraphNode* node = graph_node_create(file_path, graph->project_root);
    if (!node) return NULL;

    graph->nodes[graph->node_count++] = node;

    /* Track entry points */
    if (node->is_entry_point) {
        int new_count = graph->entry_point_count + 1;
        GraphNode** new_entries = realloc(graph->entry_points, new_count * sizeof(GraphNode*));
        if (new_entries) {
            graph->entry_points = new_entries;
            graph->entry_points[graph->entry_point_count++] = node;
        }
    }

    return node;
}

bool project_graph_build(ProjectGraph* graph, SourceFile** files, size_t file_count) {
    if (!graph || !files || file_count == 0) return false;

    log_info("Building project graph from %zu files...", file_count);

    /* Phase 1: Add all files as nodes */
    for (size_t i = 0; i < file_count; i++) {
        if (files[i] && files[i]->path) {
            project_graph_add_file(graph, files[i]->path);
        }
    }

    /* Phase 2: Analyze imports for each node */
    log_debug("Analyzing imports...");
    for (int i = 0; i < graph->node_count; i++) {
        int imports = project_graph_analyze_imports(graph->nodes[i]);
        graph->total_imports += imports;
    }

    /* Phase 3: Resolve local imports */
    log_debug("Resolving import paths...");
    for (int i = 0; i < graph->node_count; i++) {
        int resolved = project_graph_resolve_imports(graph, graph->nodes[i]);
        graph->resolved_imports += resolved;
    }

    /* Phase 4: Build dependency edges */
    log_debug("Building dependency edges...");
    for (int i = 0; i < graph->node_count; i++) {
        GraphNode* node = graph->nodes[i];

        for (int j = 0; j < node->import_count; j++) {
            FileImport* import = node->imports[j];
            if (!import || !import->resolved_path) continue;

            GraphNode* dep = project_graph_find(graph, import->resolved_path);
            if (dep) {
                /* Add edge: node depends on dep */
                int new_count = node->depends_on_count + 1;
                GraphNode** new_deps = realloc(node->depends_on, new_count * sizeof(GraphNode*));
                if (new_deps) {
                    node->depends_on = new_deps;
                    node->depends_on[node->depends_on_count++] = dep;
                }

                /* Add reverse edge: dep is depended on by node */
                new_count = dep->depended_by_count + 1;
                new_deps = realloc(dep->depended_by, new_count * sizeof(GraphNode*));
                if (new_deps) {
                    dep->depended_by = new_deps;
                    dep->depended_by[dep->depended_by_count++] = node;
                }
            }
        }
    }

    /* Phase 5: Collect external dependencies */
    log_debug("Collecting external dependencies...");
    for (int i = 0; i < graph->node_count; i++) {
        GraphNode* node = graph->nodes[i];
        for (int j = 0; j < node->import_count; j++) {
            FileImport* import = node->imports[j];
            if (!import || !import->module_name) continue;
            if (import->scope == IMPORT_SCOPE_EXTERNAL ||
                import->scope == IMPORT_SCOPE_SYSTEM) {
                /* Add to external deps if not already there */
                bool found = false;
                for (int k = 0; k < graph->external_dep_count; k++) {
                    if (strcmp(graph->external_deps[k], import->module_name) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    int new_count = graph->external_dep_count + 1;
                    char** new_deps = realloc(graph->external_deps, new_count * sizeof(char*));
                    if (new_deps) {
                        graph->external_deps = new_deps;
                        graph->external_deps[graph->external_dep_count++] = strdup(import->module_name);
                    }
                }
            }
        }
    }

    graph->unresolved_imports = graph->total_imports - graph->resolved_imports;
    if (graph->node_count > 0) {
        graph->average_imports_per_file = (float)graph->total_imports / graph->node_count;
    }

    graph->is_complete = true;

    log_success("Project graph built: %d files, %d imports (%d resolved, %d external)",
                graph->node_count, graph->total_imports,
                graph->resolved_imports, graph->external_dep_count);

    return true;
}

GraphNode* project_graph_find(ProjectGraph* graph, const char* path) {
    if (!graph || !path) return NULL;

    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]) {
            if (strcmp(graph->nodes[i]->path, path) == 0 ||
                (graph->nodes[i]->relative_path &&
                 strcmp(graph->nodes[i]->relative_path, path) == 0)) {
                return graph->nodes[i];
            }
        }
    }

    return NULL;
}

GraphNode** project_graph_get_dependents(ProjectGraph* graph, const char* path, int* count) {
    *count = 0;
    GraphNode* node = project_graph_find(graph, path);
    if (!node) return NULL;

    *count = node->depended_by_count;
    if (*count == 0) return NULL;

    GraphNode** result = malloc(*count * sizeof(GraphNode*));
    if (!result) {
        *count = 0;
        return NULL;
    }

    memcpy(result, node->depended_by, *count * sizeof(GraphNode*));
    return result;
}

GraphNode** project_graph_get_dependencies(ProjectGraph* graph, const char* path, int* count) {
    *count = 0;
    GraphNode* node = project_graph_find(graph, path);
    if (!node) return NULL;

    *count = node->depends_on_count;
    if (*count == 0) return NULL;

    GraphNode** result = malloc(*count * sizeof(GraphNode*));
    if (!result) {
        *count = 0;
        return NULL;
    }

    memcpy(result, node->depends_on, *count * sizeof(GraphNode*));
    return result;
}

/* DFS helper for impact analysis */
static void impact_dfs(GraphNode* node, GraphNode** visited, int* visited_count, int max_visited) {
    if (*visited_count >= max_visited) return;

    /* Check if already visited */
    for (int i = 0; i < *visited_count; i++) {
        if (visited[i] == node) return;
    }

    visited[(*visited_count)++] = node;

    /* Visit all nodes that depend on this one */
    for (int i = 0; i < node->depended_by_count; i++) {
        impact_dfs(node->depended_by[i], visited, visited_count, max_visited);
    }
}

GraphNode** project_graph_impact_analysis(ProjectGraph* graph, const char* path, int* count) {
    *count = 0;
    GraphNode* node = project_graph_find(graph, path);
    if (!node) return NULL;

    /* Allocate space for all possible nodes */
    int max_nodes = graph->node_count;
    GraphNode** visited = calloc(max_nodes, sizeof(GraphNode*));
    if (!visited) return NULL;

    /* DFS to find all affected nodes */
    impact_dfs(node, visited, count, max_nodes);

    /* Reallocate to exact size */
    if (*count > 0) {
        GraphNode** result = realloc(visited, *count * sizeof(GraphNode*));
        if (result) return result;
    }

    return visited;
}

/* Cycle detection helper */
static bool detect_cycle_dfs(GraphNode* node, GraphNode** path, int path_len,
                             bool* visited, bool* in_stack, ProjectGraph* graph) {
    int node_idx = -1;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i] == node) {
            node_idx = i;
            break;
        }
    }
    if (node_idx < 0) return false;

    if (in_stack[node_idx]) {
        /* Found a cycle! */
        return true;
    }

    if (visited[node_idx]) return false;

    visited[node_idx] = true;
    in_stack[node_idx] = true;
    path[path_len] = node;

    for (int i = 0; i < node->depends_on_count; i++) {
        if (detect_cycle_dfs(node->depends_on[i], path, path_len + 1, visited, in_stack, graph)) {
            return true;
        }
    }

    in_stack[node_idx] = false;
    return false;
}

int project_graph_detect_cycles(ProjectGraph* graph) {
    if (!graph || graph->node_count == 0) return 0;

    bool* visited = calloc(graph->node_count, sizeof(bool));
    bool* in_stack = calloc(graph->node_count, sizeof(bool));
    GraphNode** path = calloc(graph->node_count, sizeof(GraphNode*));

    if (!visited || !in_stack || !path) {
        free(visited);
        free(in_stack);
        free(path);
        return 0;
    }

    int cycles = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (!visited[i]) {
            if (detect_cycle_dfs(graph->nodes[i], path, 0, visited, in_stack, graph)) {
                cycles++;
            }
        }
    }

    graph->has_cycles = (cycles > 0);

    free(visited);
    free(in_stack);
    free(path);

    return cycles;
}

bool project_graph_calculate_build_order(ProjectGraph* graph) {
    if (!graph || graph->node_count == 0) return false;

    /* Check for cycles first */
    if (project_graph_detect_cycles(graph) > 0) {
        log_warning("Circular dependencies detected - build order may be incomplete");
    }

    /* Kahn's algorithm for topological sort */
    int* in_degree = calloc(graph->node_count, sizeof(int));
    GraphNode** queue = calloc(graph->node_count, sizeof(GraphNode*));
    int queue_start = 0, queue_end = 0;

    if (!in_degree || !queue) {
        free(in_degree);
        free(queue);
        return false;
    }

    /* Calculate in-degrees */
    for (int i = 0; i < graph->node_count; i++) {
        in_degree[i] = graph->nodes[i]->depends_on_count;
        if (in_degree[i] == 0) {
            queue[queue_end++] = graph->nodes[i];
        }
    }

    /* Allocate build order array */
    free(graph->build_order);
    graph->build_order = calloc(graph->node_count, sizeof(GraphNode*));
    graph->build_order_count = 0;

    /* Process queue */
    while (queue_start < queue_end) {
        GraphNode* node = queue[queue_start++];
        graph->build_order[graph->build_order_count++] = node;

        /* Reduce in-degree of dependents */
        for (int i = 0; i < node->depended_by_count; i++) {
            GraphNode* dep = node->depended_by[i];
            /* Find index of dep */
            for (int j = 0; j < graph->node_count; j++) {
                if (graph->nodes[j] == dep) {
                    in_degree[j]--;
                    if (in_degree[j] == 0) {
                        queue[queue_end++] = dep;
                    }
                    break;
                }
            }
        }
    }

    free(in_degree);
    free(queue);

    return graph->build_order_count == graph->node_count;
}

void project_graph_calculate_stats(ProjectGraph* graph) {
    if (!graph) return;

    graph->total_imports = 0;
    graph->resolved_imports = 0;

    for (int i = 0; i < graph->node_count; i++) {
        GraphNode* node = graph->nodes[i];
        graph->total_imports += node->import_count;

        for (int j = 0; j < node->import_count; j++) {
            if (node->imports[j]->resolved_path) {
                graph->resolved_imports++;
            }
        }
    }

    graph->unresolved_imports = graph->total_imports - graph->resolved_imports;
    if (graph->node_count > 0) {
        graph->average_imports_per_file = (float)graph->total_imports / graph->node_count;
    }
}

GraphNode** project_graph_get_hotspots(ProjectGraph* graph, int limit, int* count) {
    *count = 0;
    if (!graph || graph->node_count == 0) return NULL;

    /* Copy nodes array for sorting */
    GraphNode** sorted = malloc(graph->node_count * sizeof(GraphNode*));
    if (!sorted) return NULL;

    memcpy(sorted, graph->nodes, graph->node_count * sizeof(GraphNode*));

    /* Sort by depended_by_count (descending) - simple bubble sort for small arrays */
    for (int i = 0; i < graph->node_count - 1; i++) {
        for (int j = 0; j < graph->node_count - i - 1; j++) {
            if (sorted[j]->depended_by_count < sorted[j + 1]->depended_by_count) {
                GraphNode* temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }

    *count = (limit < graph->node_count) ? limit : graph->node_count;

    /* Reallocate to limit */
    if (*count < graph->node_count) {
        GraphNode** result = realloc(sorted, *count * sizeof(GraphNode*));
        if (result) return result;
    }

    return sorted;
}

char* project_graph_summarize(ProjectGraph* graph) {
    if (!graph) return strdup("No graph data available.");

    char* summary = malloc(4096);
    if (!summary) return NULL;

    int offset = 0;
    offset += snprintf(summary + offset, 4096 - offset,
                       "Project Graph Summary:\n"
                       "  Files: %d\n"
                       "  Entry points: %d\n"
                       "  Total imports: %d\n"
                       "  Resolved imports: %d (%.0f%%)\n"
                       "  External dependencies: %d\n"
                       "  Avg imports/file: %.1f\n",
                       graph->node_count,
                       graph->entry_point_count,
                       graph->total_imports,
                       graph->resolved_imports,
                       graph->total_imports > 0 ?
                           (float)graph->resolved_imports / graph->total_imports * 100 : 0,
                       graph->external_dep_count,
                       graph->average_imports_per_file);

    if (graph->has_cycles) {
        offset += snprintf(summary + offset, 4096 - offset,
                           "  WARNING: Circular dependencies detected!\n");
    }

    /* Top hotspots */
    int hotspot_count;
    GraphNode** hotspots = project_graph_get_hotspots(graph, 5, &hotspot_count);
    if (hotspots && hotspot_count > 0) {
        offset += snprintf(summary + offset, 4096 - offset, "\nMost imported files:\n");
        for (int i = 0; i < hotspot_count && i < 5; i++) {
            if (hotspots[i]->depended_by_count > 0) {
                offset += snprintf(summary + offset, 4096 - offset,
                                   "  %s (%d dependents)\n",
                                   hotspots[i]->relative_path,
                                   hotspots[i]->depended_by_count);
            }
        }
        free(hotspots);
    }

    return summary;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* import_type_to_string(ImportType type) {
    switch (type) {
        case IMPORT_TYPE_INCLUDE: return "include";
        case IMPORT_TYPE_IMPORT:  return "import";
        case IMPORT_TYPE_REQUIRE: return "require";
        case IMPORT_TYPE_USE:     return "use";
        case IMPORT_TYPE_MOD:     return "mod";
        case IMPORT_TYPE_FROM:    return "from";
        default:                  return "unknown";
    }
}

const char* import_scope_to_string(ImportScope scope) {
    switch (scope) {
        case IMPORT_SCOPE_SYSTEM:   return "system";
        case IMPORT_SCOPE_LOCAL:    return "local";
        case IMPORT_SCOPE_EXTERNAL: return "external";
        default:                    return "unknown";
    }
}
