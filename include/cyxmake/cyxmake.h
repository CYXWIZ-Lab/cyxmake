/**
 * @file cyxmake.h
 * @brief CyxMake - AI-Powered Build Automation System
 * @version 0.1.0
 * @license Apache-2.0
 */

#ifndef CYXMAKE_H
#define CYXMAKE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* Version information */
#define CYXMAKE_VERSION_MAJOR 0
#define CYXMAKE_VERSION_MINOR 1
#define CYXMAKE_VERSION_PATCH 0
#define CYXMAKE_VERSION "0.1.0"

/* Forward declarations */
typedef struct Orchestrator Orchestrator;
typedef struct ProjectContext ProjectContext;
typedef struct LLMContext LLMContext;
typedef struct ToolRegistry ToolRegistry;
typedef struct Config Config;

/* Common types */
typedef enum {
    CYXMAKE_SUCCESS = 0,
    CYXMAKE_ERROR_INVALID_ARG = 1,
    CYXMAKE_ERROR_NOT_FOUND = 2,
    CYXMAKE_ERROR_IO = 3,
    CYXMAKE_ERROR_PARSE = 4,
    CYXMAKE_ERROR_BUILD = 5,
    CYXMAKE_ERROR_INTERNAL = 99
} CyxMakeError;

typedef enum {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

/* Main API */

/**
 * Initialize CyxMake orchestrator
 * @param config_path Path to configuration file (or NULL for defaults)
 * @return Orchestrator instance or NULL on failure
 */
Orchestrator* cyxmake_init(const char* config_path);

/**
 * Shutdown CyxMake orchestrator
 * @param orch Orchestrator instance
 */
void cyxmake_shutdown(Orchestrator* orch);

/**
 * Analyze project and create cache
 * @param orch Orchestrator instance
 * @param project_path Path to project root directory
 * @return Error code
 */
CyxMakeError cyxmake_analyze_project(Orchestrator* orch, const char* project_path);

/**
 * Build project
 * @param orch Orchestrator instance
 * @param project_path Path to project root directory
 * @return Error code
 */
CyxMakeError cyxmake_build(Orchestrator* orch, const char* project_path);

/**
 * Create new project from natural language description
 * @param orch Orchestrator instance
 * @param description Natural language project description
 * @param output_path Where to create the project
 * @return Error code
 */
CyxMakeError cyxmake_create_project(Orchestrator* orch,
                                     const char* description,
                                     const char* output_path);

/**
 * Get version string
 * @return Version string (e.g., "0.1.0")
 */
const char* cyxmake_version(void);

/**
 * Set log level
 * @param level Minimum log level to display
 */
void cyxmake_set_log_level(LogLevel level);

/**
 * AI-powered autonomous build
 *
 * Uses AI to:
 * 1. Analyze the project structure
 * 2. Create a step-by-step build plan
 * 3. Execute the plan
 * 4. When errors occur, analyze and fix them automatically
 * 5. Retry until success or max attempts reached
 *
 * @param orch Orchestrator instance
 * @param project_path Path to project root directory
 * @return Error code
 */
CyxMakeError cyxmake_build_autonomous(Orchestrator* orch, const char* project_path);

/**
 * Get LLM context from orchestrator
 * @param orch Orchestrator instance
 * @return LLM context or NULL if AI not enabled
 */
LLMContext* cyxmake_get_llm(Orchestrator* orch);

/**
 * Get tool registry from orchestrator
 * @param orch Orchestrator instance
 * @return Tool registry or NULL
 */
ToolRegistry* cyxmake_get_tools(Orchestrator* orch);

/**
 * Check if AI is enabled
 * @param orch Orchestrator instance
 * @return true if AI engine is available
 */
bool cyxmake_ai_enabled(Orchestrator* orch);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_H */
