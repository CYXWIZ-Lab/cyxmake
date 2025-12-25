# CyxMake API Reference

> Complete C API reference for CyxMake v0.2.0

## Table of Contents

1. [Core API](#core-api)
2. [Security System](#security-system)
3. [Error Recovery](#error-recovery)
4. [Fix Validation](#fix-validation)
5. [Multi-Agent System](#multi-agent-system)
6. [Tool Executor](#tool-executor)
7. [Type Reference](#type-reference)

---

## Core API

**Header:** `#include <cyxmake/cyxmake.h>`

The core API provides the main entry points for CyxMake functionality.

### Initialization and Shutdown

#### cyxmake_init

```c
Orchestrator* cyxmake_init(const char* config_path);
```

Initialize the CyxMake orchestrator.

| Parameter | Type | Description |
|-----------|------|-------------|
| config_path | `const char*` | Path to configuration file, or NULL for defaults |

**Returns:** `Orchestrator*` - Orchestrator instance or NULL on failure

**Example:**
```c
Orchestrator* orch = cyxmake_init("cyxmake.toml");
if (!orch) {
    fprintf(stderr, "Failed to initialize CyxMake\n");
    return 1;
}
```

---

#### cyxmake_shutdown

```c
void cyxmake_shutdown(Orchestrator* orch);
```

Shutdown the CyxMake orchestrator and free all resources.

| Parameter | Type | Description |
|-----------|------|-------------|
| orch | `Orchestrator*` | Orchestrator instance to shutdown |

---

### Build Operations

#### cyxmake_analyze_project

```c
CyxMakeError cyxmake_analyze_project(Orchestrator* orch, const char* project_path);
```

Analyze a project and create/update the cache.

| Parameter | Type | Description |
|-----------|------|-------------|
| orch | `Orchestrator*` | Orchestrator instance |
| project_path | `const char*` | Path to project root directory |

**Returns:** `CyxMakeError` - Error code (CYXMAKE_SUCCESS on success)

---

#### cyxmake_build

```c
CyxMakeError cyxmake_build(Orchestrator* orch, const char* project_path);
```

Build a project using detected build system.

| Parameter | Type | Description |
|-----------|------|-------------|
| orch | `Orchestrator*` | Orchestrator instance |
| project_path | `const char*` | Path to project root directory |

**Returns:** `CyxMakeError` - Error code

---

#### cyxmake_build_autonomous

```c
CyxMakeError cyxmake_build_autonomous(Orchestrator* orch, const char* project_path);
```

AI-powered autonomous build with automatic error recovery.

Uses AI to:
1. Analyze project structure
2. Create step-by-step build plan
3. Execute the plan
4. Diagnose and fix errors automatically
5. Retry until success or max attempts

| Parameter | Type | Description |
|-----------|------|-------------|
| orch | `Orchestrator*` | Orchestrator instance |
| project_path | `const char*` | Path to project root directory |

**Returns:** `CyxMakeError` - Error code

---

### Project Creation

#### cyxmake_create_project

```c
CyxMakeError cyxmake_create_project(Orchestrator* orch,
                                     const char* description,
                                     const char* output_path);
```

Create a new project from natural language description.

| Parameter | Type | Description |
|-----------|------|-------------|
| orch | `Orchestrator*` | Orchestrator instance |
| description | `const char*` | Natural language project description |
| output_path | `const char*` | Where to create the project |

**Returns:** `CyxMakeError` - Error code

**Example:**
```c
CyxMakeError err = cyxmake_create_project(orch,
    "C++ game with SDL2 and OpenGL",
    "./my_game");
```

---

### Utility Functions

#### cyxmake_version

```c
const char* cyxmake_version(void);
```

Get the CyxMake version string.

**Returns:** `const char*` - Version string (e.g., "0.2.0")

---

#### cyxmake_set_log_level

```c
void cyxmake_set_log_level(LogLevel level);
```

Set the minimum log level for output.

| Parameter | Type | Description |
|-----------|------|-------------|
| level | `LogLevel` | Minimum log level (LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR) |

---

#### cyxmake_ai_enabled

```c
bool cyxmake_ai_enabled(Orchestrator* orch);
```

Check if AI functionality is available.

**Returns:** `bool` - true if AI engine is available

---

### Component Access

```c
LLMContext* cyxmake_get_llm(Orchestrator* orch);
ToolRegistry* cyxmake_get_tools(Orchestrator* orch);
AgentRegistry* cyxmake_get_agent_registry(Orchestrator* orch);
AgentCoordinator* cyxmake_get_coordinator(Orchestrator* orch);
MessageBus* cyxmake_get_message_bus(Orchestrator* orch);
SharedState* cyxmake_get_shared_state(Orchestrator* orch);
TaskQueue* cyxmake_get_task_queue(Orchestrator* orch);
ThreadPool* cyxmake_get_thread_pool(Orchestrator* orch);
```

Get internal components from the orchestrator.

---

## Security System

**Header:** `#include <cyxmake/security.h>`

The security system provides audit logging, dry-run mode, rollback support, and sandboxed execution.

### Audit Logger

#### audit_logger_create

```c
AuditLogger* audit_logger_create(const AuditConfig* config);
AuditLogger* audit_logger_create_default(void);
void audit_logger_free(AuditLogger* logger);
```

Create and manage audit loggers.

**AuditConfig Fields:**

| Field | Type | Description |
|-------|------|-------------|
| enabled | `bool` | Enable audit logging |
| log_file | `const char*` | Path to audit log file |
| log_to_console | `bool` | Also log to console |
| min_severity | `AuditSeverity` | Minimum severity to log |
| include_timestamps | `bool` | Include timestamps |
| include_user | `bool` | Include user information |
| max_entries | `int` | Max entries in memory (0 = unlimited) |
| rotation_size_mb | `int` | Log rotation size (0 = no rotation) |

---

#### audit_log_action

```c
void audit_log_action(AuditLogger* logger, AuditSeverity severity,
                       ActionType action, const char* target,
                       const char* description, bool success);
```

Log a simple action.

| Parameter | Type | Description |
|-----------|------|-------------|
| logger | `AuditLogger*` | Audit logger |
| severity | `AuditSeverity` | Severity level |
| action | `ActionType` | Action type |
| target | `const char*` | Target resource |
| description | `const char*` | Description |
| success | `bool` | Whether action succeeded |

---

#### audit_log_command

```c
void audit_log_command(AuditLogger* logger, const char* command,
                        const char* args, int exit_code, bool success);
```

Log a command execution.

---

#### audit_export

```c
bool audit_export(AuditLogger* logger, const char* filepath, const char* format);
```

Export audit log to file.

| Parameter | Type | Description |
|-----------|------|-------------|
| logger | `AuditLogger*` | Audit logger |
| filepath | `const char*` | Output file path |
| format | `const char*` | Format: "json", "csv", or "text" |

---

### Dry-Run Mode

#### dry_run_create

```c
DryRunContext* dry_run_create(void);
void dry_run_free(DryRunContext* ctx);
```

Create and manage dry-run contexts.

---

#### dry_run_set_enabled

```c
void dry_run_set_enabled(DryRunContext* ctx, bool enabled);
bool dry_run_is_enabled(DryRunContext* ctx);
```

Enable or disable dry-run mode.

---

#### dry_run_record_file

```c
void dry_run_record_file(DryRunContext* ctx, ActionType action,
                          const char* filepath, const char* description);
```

Record a file operation that would be performed.

---

#### dry_run_record_command

```c
void dry_run_record_command(DryRunContext* ctx, const char* command,
                             const char* working_dir);
```

Record a command that would be executed.

---

#### dry_run_get_actions

```c
const DryRunAction** dry_run_get_actions(DryRunContext* ctx, int* count);
```

Get all recorded dry-run actions.

**Returns:** Array of DryRunAction pointers (do not free)

---

### Rollback Support

#### rollback_create

```c
RollbackManager* rollback_create(const RollbackConfig* config);
RollbackManager* rollback_create_default(void);
void rollback_free(RollbackManager* mgr);
```

Create and manage rollback managers.

**RollbackConfig Fields:**

| Field | Type | Description |
|-------|------|-------------|
| enabled | `bool` | Enable rollback support |
| backup_dir | `const char*` | Directory for backups |
| max_entries | `int` | Maximum rollback entries |
| max_file_size | `size_t` | Max file size to backup in memory |
| backup_large_files | `bool` | Backup large files to disk |
| retention_hours | `int` | How long to keep backups |

---

#### rollback_backup_file

```c
bool rollback_backup_file(RollbackManager* mgr, const char* filepath,
                          RollbackType type);
```

Backup a file before modification.

| Parameter | Type | Description |
|-----------|------|-------------|
| mgr | `RollbackManager*` | Rollback manager |
| filepath | `const char*` | File to backup |
| type | `RollbackType` | Type of modification |

---

#### rollback_last

```c
int rollback_last(RollbackManager* mgr, int count);
```

Rollback the last N operations.

**Returns:** Number of entries rolled back

---

### Security Context

#### security_context_create

```c
SecurityContext* security_context_create(const SecurityConfig* config);
SecurityContext* security_context_create_default(void);
void security_context_free(SecurityContext* ctx);
```

Create a unified security context combining permissions, audit, dry-run, and rollback.

---

#### security_check_permission

```c
bool security_check_permission(SecurityContext* ctx, ActionType action,
                                const char* target, const char* reason);
```

Check permission with audit logging.

**Returns:** `bool` - true if action is allowed

---

### Sandboxed Execution

#### sandbox_execute

```c
SandboxResult* sandbox_execute(const char* command,
                                char* const* args,
                                const char* working_dir,
                                const SandboxConfig* config);
void sandbox_result_free(SandboxResult* result);
```

Execute a command in a sandbox with resource limits.

**SandboxConfig Fields:**

| Field | Type | Description |
|-------|------|-------------|
| level | `SandboxLevel` | Restriction level (NONE, LIGHT, MEDIUM, STRICT) |
| allow_network | `bool` | Allow network access |
| allow_subprocesses | `bool` | Allow spawning child processes |
| allowed_read_paths | `const char**` | Paths allowed for reading |
| allowed_write_paths | `const char**` | Paths allowed for writing |
| max_memory_mb | `int` | Max memory in MB |
| max_cpu_sec | `int` | Max CPU time in seconds |

---

## Error Recovery

**Header:** `#include <cyxmake/error_recovery.h>`

The error recovery system provides pattern matching, diagnosis, and automatic fix generation.

### Pattern Database

#### error_patterns_init

```c
bool error_patterns_init(void);
void error_patterns_shutdown(void);
```

Initialize and shutdown the error pattern database.

---

#### error_patterns_match

```c
ErrorPatternType error_patterns_match(const char* error_output);
```

Match error output against known patterns.

**Returns:** `ErrorPatternType` - Matched pattern type or ERROR_PATTERN_UNKNOWN

---

### Error Diagnosis

#### error_diagnose

```c
ErrorDiagnosis* error_diagnose(const BuildResult* build_result,
                               const ProjectContext* ctx);
void error_diagnosis_free(ErrorDiagnosis* diagnosis);
```

Diagnose a build error and generate suggested fixes.

**ErrorDiagnosis Fields:**

| Field | Type | Description |
|-------|------|-------------|
| pattern_type | `ErrorPatternType` | Type of error pattern matched |
| error_message | `char*` | Original error message |
| diagnosis | `char*` | Human-readable diagnosis |
| suggested_fixes | `FixAction**` | Array of suggested fixes |
| fix_count | `size_t` | Number of fixes |
| confidence | `double` | Confidence in diagnosis (0.0-1.0) |

---

#### error_diagnose_with_llm

```c
ErrorDiagnosis* error_diagnose_with_llm(const BuildResult* build_result,
                                         const ProjectContext* ctx,
                                         LLMContext* llm_ctx);
```

Diagnose with optional LLM analysis for complex errors.

---

### Fix Execution

#### fix_execute

```c
bool fix_execute(const FixAction* action, const ProjectContext* ctx);
```

Execute a fix action.

**Returns:** `bool` - true if fix was applied successfully

---

#### fix_execute_with_tools

```c
bool fix_execute_with_tools(const FixAction* action,
                            const ProjectContext* ctx,
                            const ToolRegistry* registry);
```

Execute fix using tool registry for smart package installation.

---

### Recovery Context

#### recovery_context_create

```c
RecoveryContext* recovery_context_create(const RecoveryStrategy* strategy);
void recovery_context_free(RecoveryContext* ctx);
```

Create a recovery context with retry strategy.

**RecoveryStrategy Fields:**

| Field | Type | Description |
|-------|------|-------------|
| max_retries | `int` | Maximum retry attempts |
| retry_delay_ms | `int` | Initial retry delay |
| backoff_multiplier | `float` | Exponential backoff multiplier |
| max_delay_ms | `int` | Maximum delay |
| use_ai_analysis | `bool` | Use LLM for complex errors |
| auto_apply_fixes | `bool` | Auto-apply fixes |

---

#### recovery_attempt

```c
BuildResult* recovery_attempt(RecoveryContext* ctx,
                              const BuildResult* build_result,
                              ProjectContext* project_ctx);
```

Attempt to recover from a build failure.

**Returns:** `BuildResult*` - Result after recovery (caller must free)

---

## Fix Validation

**Header:** `#include <cyxmake/fix_validation.h>`

Enhanced error recovery with validation, verification, and learning.

### Fix Validator

#### fix_validator_create

```c
FixValidator* fix_validator_create(const ToolRegistry* registry);
void fix_validator_free(FixValidator* validator);
```

Create a fix validator.

---

#### fix_validate

```c
ValidationResult* fix_validate(FixValidator* validator,
                               const FixAction* action,
                               const ProjectContext* ctx);
void validation_result_free(ValidationResult* result);
```

Validate a fix action before applying.

**ValidationResult Fields:**

| Field | Type | Description |
|-------|------|-------------|
| status | `ValidationStatus` | PASSED, WARNING, FAILED, or SKIPPED |
| message | `char*` | Human-readable message |
| details | `char*` | Technical details |
| can_proceed | `bool` | Whether to allow proceeding |
| confidence | `double` | Confidence in result (0.0-1.0) |

---

### Risk Assessment

#### fix_assess_risk

```c
RiskAssessment* fix_assess_risk(const FixAction* action,
                                const ProjectContext* ctx);
void risk_assessment_free(RiskAssessment* assessment);
```

Assess risk of a fix action.

**RiskAssessment Fields:**

| Field | Type | Description |
|-------|------|-------------|
| level | `RiskLevel` | NONE, LOW, MEDIUM, HIGH, or CRITICAL |
| description | `char*` | Risk description |
| requires_confirmation | `bool` | Must confirm with user |
| requires_backup | `bool` | Should backup before applying |
| is_reversible | `bool` | Can be rolled back |
| affected_files | `char**` | Files that will be modified |
| affected_count | `size_t` | Number of affected files |

---

### Incremental Fix Application

#### incremental_fix_session_create

```c
IncrementalFixSession* incremental_fix_session_create(
    ProjectContext* ctx,
    const ToolRegistry* registry,
    RollbackManager* rollback,
    SecurityContext* security);
void incremental_fix_session_free(IncrementalFixSession* session);
```

Create an incremental fix session with rollback support.

---

#### incremental_fix_apply

```c
int incremental_fix_apply(IncrementalFixSession* session,
                          FixAction** fixes,
                          size_t fix_count,
                          bool verify_each,
                          bool stop_on_failure);
```

Apply fixes incrementally with validation and verification.

For each fix:
1. Validate the fix
2. Assess risk and get user confirmation if needed
3. Create backup if required
4. Apply the fix
5. Verify with rebuild (optional)
6. Record result

**Returns:** Number of successfully applied fixes

---

### Fix History (Learning)

#### fix_history_create

```c
FixHistory* fix_history_create(const char* history_path);
void fix_history_free(FixHistory* history);
```

Create/open a fix history database.

---

#### fix_history_record

```c
void fix_history_record(FixHistory* history,
                        const ErrorDiagnosis* diagnosis,
                        const FixAction* action,
                        bool success,
                        double fix_time_ms);
```

Record a fix attempt for learning.

---

#### fix_history_suggest

```c
FixAction* fix_history_suggest(const FixHistory* history,
                               const ErrorDiagnosis* diagnosis);
```

Get suggested fix based on history.

**Returns:** Best fix suggestion (caller must free), or NULL

---

#### fix_history_stats

```c
void fix_history_stats(const FixHistory* history,
                       int* total_fixes,
                       int* successful_fixes,
                       int* unique_errors);
```

Get statistics from fix history.

---

## Multi-Agent System

**Header:** `#include <cyxmake/agent_registry.h>`

The multi-agent system provides named agents, async execution, and agent coordination.

### Agent Registry

#### agent_registry_create

```c
AgentRegistry* agent_registry_create(AIProvider* ai, ToolRegistry* tools,
                                     ThreadPool* thread_pool);
void agent_registry_free(AgentRegistry* registry);
```

Create an agent registry.

---

#### agent_registry_create_agent

```c
AgentInstance* agent_registry_create_agent(AgentRegistry* registry,
                                           const char* name,
                                           AgentType type,
                                           const AgentInstanceConfig* config);
```

Create and register a new agent.

| Parameter | Type | Description |
|-----------|------|-------------|
| registry | `AgentRegistry*` | The registry |
| name | `const char*` | Unique name for the agent |
| type | `AgentType` | SMART, AUTONOMOUS, BUILD, COORDINATOR, or CUSTOM |
| config | `AgentInstanceConfig*` | Configuration (NULL for defaults) |

**Returns:** `AgentInstance*` - New agent or NULL on failure

---

#### agent_registry_get

```c
AgentInstance* agent_registry_get(AgentRegistry* registry, const char* name_or_id);
```

Get an agent by name or ID.

---

#### agent_registry_list

```c
AgentInstance** agent_registry_list(AgentRegistry* registry, int* count);
```

Get all agents in the registry.

---

### Agent Lifecycle

#### agent_start

```c
bool agent_start(AgentInstance* agent);
bool agent_pause(AgentInstance* agent);
bool agent_resume(AgentInstance* agent);
bool agent_terminate(AgentInstance* agent);
```

Control agent lifecycle.

---

#### agent_wait

```c
bool agent_wait(AgentInstance* agent, int timeout_ms);
```

Wait for an agent to complete.

| Parameter | Type | Description |
|-----------|------|-------------|
| agent | `AgentInstance*` | Agent to wait on |
| timeout_ms | `int` | Timeout in milliseconds (0 = infinite) |

**Returns:** `bool` - true if completed, false if timeout

---

### Task Assignment

#### agent_run_sync

```c
char* agent_run_sync(AgentInstance* agent, const char* task_description);
```

Run a task synchronously (blocking).

**Returns:** Result string (caller must free) or NULL on error

---

#### agent_run_async

```c
bool agent_run_async(AgentInstance* agent, const char* task_description);
```

Run a task asynchronously (non-blocking).

---

### Agent Spawning

#### agent_spawn_child

```c
AgentInstance* agent_spawn_child(AgentInstance* parent, const char* name,
                                 AgentType type, const AgentInstanceConfig* config);
```

Spawn a child agent from a parent.

---

#### agent_wait_children

```c
bool agent_wait_children(AgentInstance* parent, int timeout_ms);
```

Wait for all children to complete.

---

## Tool Executor

**Header:** `#include <cyxmake/tool_executor.h>`

The tool executor provides tool discovery, registry, and execution.

### Tool Discovery

#### tool_discovery_scan

```c
ToolRegistry* tool_discovery_scan(void);
```

Scan the system for available tools.

---

### Tool Registry

#### tool_registry_create

```c
ToolRegistry* tool_registry_create(void);
void tool_registry_free(ToolRegistry* registry);
```

Create and manage tool registries.

---

#### tool_registry_add

```c
bool tool_registry_add(ToolRegistry* registry, ToolInfo* tool);
```

Add a tool to the registry.

---

#### tool_registry_find

```c
ToolInfo* tool_registry_find(const ToolRegistry* registry, const char* name);
```

Find a tool by name.

---

### Tool Execution

#### tool_execute

```c
ToolExecResult* tool_execute(const ToolInfo* tool, const ToolExecOptions* options);
void tool_exec_result_free(ToolExecResult* result);
```

Execute a tool with options.

**ToolExecResult Fields:**

| Field | Type | Description |
|-------|------|-------------|
| success | `bool` | Whether execution succeeded |
| exit_code | `int` | Exit code |
| stdout_output | `char*` | Captured stdout |
| stderr_output | `char*` | Captured stderr |
| duration_ms | `double` | Execution time |

---

## Type Reference

### Error Codes

```c
typedef enum {
    CYXMAKE_SUCCESS = 0,
    CYXMAKE_ERROR_INVALID_ARG = 1,
    CYXMAKE_ERROR_NOT_FOUND = 2,
    CYXMAKE_ERROR_IO = 3,
    CYXMAKE_ERROR_PARSE = 4,
    CYXMAKE_ERROR_BUILD = 5,
    CYXMAKE_ERROR_INTERNAL = 99
} CyxMakeError;
```

### Log Levels

```c
typedef enum {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;
```

### Audit Severity

```c
typedef enum {
    AUDIT_DEBUG,
    AUDIT_INFO,
    AUDIT_WARNING,
    AUDIT_ACTION,
    AUDIT_DENIED,
    AUDIT_ERROR,
    AUDIT_SECURITY
} AuditSeverity;
```

### Error Pattern Types

```c
typedef enum {
    ERROR_PATTERN_MISSING_FILE,
    ERROR_PATTERN_MISSING_LIBRARY,
    ERROR_PATTERN_MISSING_HEADER,
    ERROR_PATTERN_PERMISSION_DENIED,
    ERROR_PATTERN_DISK_FULL,
    ERROR_PATTERN_SYNTAX_ERROR,
    ERROR_PATTERN_UNDEFINED_REFERENCE,
    ERROR_PATTERN_VERSION_MISMATCH,
    ERROR_PATTERN_CMAKE_VERSION,
    ERROR_PATTERN_CMAKE_PACKAGE,
    ERROR_PATTERN_NETWORK_ERROR,
    ERROR_PATTERN_TIMEOUT,
    ERROR_PATTERN_UNKNOWN
} ErrorPatternType;
```

### Fix Action Types

```c
typedef enum {
    FIX_ACTION_INSTALL_PACKAGE,
    FIX_ACTION_CREATE_FILE,
    FIX_ACTION_MODIFY_FILE,
    FIX_ACTION_SET_ENV_VAR,
    FIX_ACTION_RUN_COMMAND,
    FIX_ACTION_CLEAN_BUILD,
    FIX_ACTION_FIX_CMAKE_VERSION,
    FIX_ACTION_RETRY,
    FIX_ACTION_NONE
} FixActionType;
```

### Risk Levels

```c
typedef enum {
    RISK_NONE,
    RISK_LOW,
    RISK_MEDIUM,
    RISK_HIGH,
    RISK_CRITICAL
} RiskLevel;
```

### Validation Status

```c
typedef enum {
    VALIDATION_PASSED,
    VALIDATION_WARNING,
    VALIDATION_FAILED,
    VALIDATION_SKIPPED
} ValidationStatus;
```

### Agent Types

```c
typedef enum {
    AGENT_TYPE_SMART,
    AGENT_TYPE_AUTONOMOUS,
    AGENT_TYPE_BUILD,
    AGENT_TYPE_COORDINATOR,
    AGENT_TYPE_CUSTOM
} AgentType;
```

### Agent States

```c
typedef enum {
    AGENT_STATE_CREATED,
    AGENT_STATE_INITIALIZING,
    AGENT_STATE_IDLE,
    AGENT_STATE_RUNNING,
    AGENT_STATE_PAUSED,
    AGENT_STATE_COMPLETING,
    AGENT_STATE_COMPLETED,
    AGENT_STATE_TERMINATED,
    AGENT_STATE_ERROR
} AgentState;
```

### Agent Capabilities

```c
typedef enum {
    AGENT_CAP_NONE          = 0,
    AGENT_CAP_BUILD         = (1 << 0),
    AGENT_CAP_FIX_ERRORS    = (1 << 1),
    AGENT_CAP_READ_FILES    = (1 << 2),
    AGENT_CAP_WRITE_FILES   = (1 << 3),
    AGENT_CAP_EXECUTE       = (1 << 4),
    AGENT_CAP_INSTALL_DEPS  = (1 << 5),
    AGENT_CAP_ANALYZE       = (1 << 6),
    AGENT_CAP_REASON        = (1 << 7),
    AGENT_CAP_SPAWN         = (1 << 8),
    AGENT_CAP_ALL           = 0xFFFF
} AgentCapability;
```

### Permission Levels

```c
typedef enum {
    PERM_SAFE,
    PERM_ASK,
    PERM_DANGEROUS,
    PERM_BLOCKED
} PermissionLevel;
```

### Sandbox Levels

```c
typedef enum {
    SANDBOX_NONE,
    SANDBOX_LIGHT,
    SANDBOX_MEDIUM,
    SANDBOX_STRICT
} SandboxLevel;
```

---

*CyxMake API Reference v0.2.0*
