/**
 * @file project_graph.h
 * @brief Project Graph - Deep file analysis with imports/exports tracking
 *
 * This module builds a dependency graph of the project's source files:
 * - Tracks #include directives for C/C++
 * - Tracks import/require statements for JavaScript/TypeScript
 * - Tracks import statements for Python
 * - Tracks use/mod statements for Rust
 * - Enables impact analysis ("what files are affected if I change X")
 * - Supports build order optimization
 */

#ifndef CYXMAKE_PROJECT_GRAPH_H
#define CYXMAKE_PROJECT_GRAPH_H

#include <stdbool.h>
#include <stddef.h>
#include "cyxmake/project_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Import/Export Types
 * ============================================================================ */

/**
 * Type of import/dependency relationship
 */
typedef enum {
    IMPORT_TYPE_UNKNOWN = 0,
    IMPORT_TYPE_INCLUDE,        /* C/C++ #include */
    IMPORT_TYPE_IMPORT,         /* JS/TS/Python import */
    IMPORT_TYPE_REQUIRE,        /* JS require() */
    IMPORT_TYPE_USE,            /* Rust use */
    IMPORT_TYPE_MOD,            /* Rust mod */
    IMPORT_TYPE_FROM,           /* Python from X import Y */
} ImportType;

/**
 * Whether the import is system/external or local
 */
typedef enum {
    IMPORT_SCOPE_UNKNOWN = 0,
    IMPORT_SCOPE_SYSTEM,        /* <stdio.h>, 'react' */
    IMPORT_SCOPE_LOCAL,         /* "myfile.h", './utils' */
    IMPORT_SCOPE_EXTERNAL,      /* External package */
} ImportScope;

/**
 * Represents a single import statement
 */
typedef struct {
    char* raw_statement;        /* Original import text */
    char* module_name;          /* Module/file being imported */
    char* resolved_path;        /* Absolute path if resolved, NULL if external */
    ImportType type;
    ImportScope scope;
    int line_number;            /* Line where import appears */
    char** imported_symbols;    /* Specific symbols imported (Python: from X import a, b) */
    int symbol_count;
    bool is_default_import;     /* JS default import */
    bool is_namespace_import;   /* JS import * as X */
} FileImport;

/**
 * Represents an exported symbol
 */
typedef struct {
    char* name;                 /* Symbol name */
    char* type;                 /* "function", "class", "variable", "type", etc. */
    int line_number;            /* Line where defined */
    bool is_default_export;     /* JS default export */
    bool is_public;             /* Rust pub, or implicitly public */
} FileExport;

/* ============================================================================
 * Graph Node - Represents a single file
 * ============================================================================ */

/**
 * Node in the project graph representing a source file
 */
typedef struct GraphNode {
    char* path;                 /* Absolute file path */
    char* relative_path;        /* Path relative to project root */
    Language language;

    /* Imports (dependencies) */
    FileImport** imports;
    int import_count;
    int import_capacity;

    /* Exports (public API) */
    FileExport** exports;
    int export_count;
    int export_capacity;

    /* Graph edges */
    struct GraphNode** depends_on;      /* Files this file imports */
    int depends_on_count;
    struct GraphNode** depended_by;     /* Files that import this file */
    int depended_by_count;

    /* Metrics */
    int total_lines;
    int code_lines;             /* Non-comment, non-blank lines */
    int import_depth;           /* Max depth in import chain */
    float complexity_score;     /* Rough complexity estimate */

    /* State flags */
    bool is_entry_point;        /* main.c, index.js, etc. */
    bool is_test_file;
    bool is_generated;
    bool is_analyzed;           /* Has deep analysis been done */
} GraphNode;

/* ============================================================================
 * Project Graph - Full dependency graph
 * ============================================================================ */

/**
 * The complete project dependency graph
 */
typedef struct ProjectGraph {
    char* project_root;

    /* All nodes indexed by path */
    GraphNode** nodes;
    int node_count;
    int node_capacity;

    /* Entry points */
    GraphNode** entry_points;
    int entry_point_count;

    /* External dependencies (unresolved imports) */
    char** external_deps;
    int external_dep_count;

    /* Circular dependency detection */
    char** circular_deps;       /* "A -> B -> A" strings */
    int circular_dep_count;

    /* Statistics */
    int total_imports;
    int resolved_imports;
    int unresolved_imports;
    float average_imports_per_file;

    /* Build order */
    GraphNode** build_order;    /* Topologically sorted */
    int build_order_count;

    /* State */
    bool is_complete;           /* All files analyzed */
    bool has_cycles;
} ProjectGraph;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* Graph lifecycle */

/**
 * Create a new project graph
 * @param project_root Absolute path to project root
 * @return New project graph or NULL on failure
 */
ProjectGraph* project_graph_create(const char* project_root);

/**
 * Free project graph and all nodes
 * @param graph Graph to free
 */
void project_graph_free(ProjectGraph* graph);

/* Building the graph */

/**
 * Build complete project graph from source files
 * @param graph Graph to populate
 * @param files Source files from project_analyzer
 * @param file_count Number of source files
 * @return true on success
 */
bool project_graph_build(ProjectGraph* graph, SourceFile** files, size_t file_count);

/**
 * Add a single file to the graph and analyze it
 * @param graph The project graph
 * @param file_path Absolute path to file
 * @return Created node or NULL on failure
 */
GraphNode* project_graph_add_file(ProjectGraph* graph, const char* file_path);

/**
 * Analyze imports in a file
 * @param node Node to analyze
 * @return Number of imports found
 */
int project_graph_analyze_imports(GraphNode* node);

/**
 * Analyze exports in a file
 * @param node Node to analyze
 * @return Number of exports found
 */
int project_graph_analyze_exports(GraphNode* node);

/**
 * Resolve import paths to actual files
 * @param graph The project graph
 * @param node Node whose imports to resolve
 * @return Number of resolved imports
 */
int project_graph_resolve_imports(ProjectGraph* graph, GraphNode* node);

/* Querying the graph */

/**
 * Find a node by file path
 * @param graph The project graph
 * @param path File path (absolute or relative)
 * @return Node or NULL if not found
 */
GraphNode* project_graph_find(ProjectGraph* graph, const char* path);

/**
 * Get all files that depend on a given file
 * @param graph The project graph
 * @param path File path
 * @param count Output: number of dependent files
 * @return Array of dependent nodes (caller must free array, not nodes)
 */
GraphNode** project_graph_get_dependents(ProjectGraph* graph, const char* path, int* count);

/**
 * Get all files that a given file depends on
 * @param graph The project graph
 * @param path File path
 * @param count Output: number of dependency files
 * @return Array of dependency nodes (caller must free array, not nodes)
 */
GraphNode** project_graph_get_dependencies(ProjectGraph* graph, const char* path, int* count);

/**
 * Get transitive closure of files affected by changing a file
 * @param graph The project graph
 * @param path File path
 * @param count Output: number of affected files
 * @return Array of affected nodes (caller must free array, not nodes)
 */
GraphNode** project_graph_impact_analysis(ProjectGraph* graph, const char* path, int* count);

/* Build order */

/**
 * Calculate topological build order
 * @param graph The project graph
 * @return true if order calculated (false if cycles prevent ordering)
 */
bool project_graph_calculate_build_order(ProjectGraph* graph);

/**
 * Detect circular dependencies
 * @param graph The project graph
 * @return Number of cycles detected
 */
int project_graph_detect_cycles(ProjectGraph* graph);

/* Statistics and reporting */

/**
 * Calculate graph statistics
 * @param graph The project graph
 */
void project_graph_calculate_stats(ProjectGraph* graph);

/**
 * Get most imported files (most depended upon)
 * @param graph The project graph
 * @param limit Max number of results
 * @param count Output: actual count
 * @return Array of nodes sorted by dependents count
 */
GraphNode** project_graph_get_hotspots(ProjectGraph* graph, int limit, int* count);

/**
 * Generate summary of the project graph
 * @param graph The project graph
 * @return Summary string (caller must free)
 */
char* project_graph_summarize(ProjectGraph* graph);

/* Node helpers */

/**
 * Create a new graph node
 * @param path File path
 * @param project_root Project root for relative path calculation
 * @return New node or NULL on failure
 */
GraphNode* graph_node_create(const char* path, const char* project_root);

/**
 * Free a graph node
 * @param node Node to free
 */
void graph_node_free(GraphNode* node);

/**
 * Add an import to a node
 * @param node The node
 * @param import Import to add (node takes ownership)
 * @return true on success
 */
bool graph_node_add_import(GraphNode* node, FileImport* import);

/**
 * Add an export to a node
 * @param node The node
 * @param export Export to add (node takes ownership)
 * @return true on success
 */
bool graph_node_add_export(GraphNode* node, FileExport* export_sym);

/* Import/Export helpers */

/**
 * Create a file import
 * @return New import or NULL
 */
FileImport* file_import_create(void);

/**
 * Free a file import
 */
void file_import_free(FileImport* import);

/**
 * Create a file export
 * @return New export or NULL
 */
FileExport* file_export_create(void);

/**
 * Free a file export
 */
void file_export_free(FileExport* export_sym);

/* Utility */

/**
 * Convert import type to string
 */
const char* import_type_to_string(ImportType type);

/**
 * Convert import scope to string
 */
const char* import_scope_to_string(ImportScope scope);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_PROJECT_GRAPH_H */
