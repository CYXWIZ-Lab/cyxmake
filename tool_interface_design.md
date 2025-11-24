# CyxMake Tool Interface Design Specification

## Overview

Tools are the fundamental execution units in CyxMake. They are self-contained, single-purpose programs that perform specific tasks like parsing build files, executing commands, installing dependencies, or generating configurations.

**Design Principles**:
1. **Single Responsibility**: Each tool does one thing well
2. **Language Agnostic**: Tools can be written in any language
3. **Standardized I/O**: All tools use JSON for input/output
4. **Discoverable**: Tools self-describe via manifest files
5. **Testable**: Tools can be tested independently
6. **Composable**: Tools can be chained to accomplish complex tasks
7. **Fail-Safe**: Tools report errors in structured formats

---

## Tool Execution Models

CyxMake supports three execution models:

### Model 1: Subprocess Execution (Recommended)

**Characteristics**:
- Tool is a standalone executable
- Communicates via stdin/stdout
- Process isolation (security)
- Language independent
- Easy to develop and test

**Use Cases**: Most tools (parsers, generators, command executors)

### Model 2: Dynamic Library (Future)

**Characteristics**:
- Shared library (.so/.dll) loaded at runtime
- C ABI interface
- Faster (no process spawn overhead)
- Shared memory space

**Use Cases**: Performance-critical tools, tight integration needs

### Model 3: Built-in (Core Tools)

**Characteristics**:
- Compiled directly into CyxMake binary
- Maximum performance
- No external dependencies

**Use Cases**: Essential tools (project analyzer, basic executor)

**Initial Focus**: Model 1 (Subprocess) for Phase 0-3, evaluate others later.

---

## Tool Manifest Format

Each tool must have a **manifest file** that describes its capabilities, interface, and requirements.

### Manifest File: `tool.toml`

**Location**: Same directory as tool executable, or in `~/.cyxmake/tools/<tool-name>/`

**Format**: TOML (human-friendly, no quotes, supports comments)

```toml
[tool]
name = "cmake_parser"
version = "1.0.0"
description = "Parse CMakeLists.txt files and extract project structure"
author = "CyxMake Team"
license = "MIT"

# Executable information
executable = "cmake_parser"  # Relative to manifest location
# On Windows, .exe is appended automatically

[capabilities]
# What this tool can do (tags for discovery)
categories = ["analysis", "cmake", "parser"]
languages = ["c", "cpp"]  # Relevant programming languages
build_systems = ["cmake"]

# Specific capabilities (used for tool selection)
# Format: capability_name = "description"
parse = "Parse CMakeLists.txt into structured JSON"
validate = "Validate CMakeLists.txt syntax"
extract_targets = "Extract all targets (executables, libraries)"
extract_dependencies = "Extract external dependencies"

[input]
# Input schema (JSON Schema draft-07)
schema = '''
{
  "type": "object",
  "properties": {
    "action": {
      "type": "string",
      "enum": ["parse", "validate", "extract_targets", "extract_dependencies"]
    },
    "file_path": {
      "type": "string",
      "description": "Absolute path to CMakeLists.txt"
    },
    "options": {
      "type": "object",
      "properties": {
        "include_comments": {"type": "boolean", "default": false},
        "resolve_variables": {"type": "boolean", "default": true}
      }
    }
  },
  "required": ["action", "file_path"]
}
'''

[output]
# Output schema
schema = '''
{
  "type": "object",
  "properties": {
    "status": {
      "type": "string",
      "enum": ["success", "partial", "failed", "retry"]
    },
    "exit_code": {"type": "integer"},
    "data": {"type": "object"},
    "errors": {
      "type": "array",
      "items": {"type": "string"}
    },
    "warnings": {
      "type": "array",
      "items": {"type": "string"}
    }
  },
  "required": ["status", "exit_code"]
}
'''

[requirements]
# System requirements
os = ["linux", "windows", "macos"]  # Empty = all platforms
min_memory_mb = 50
min_disk_mb = 10

# External dependencies (optional)
commands = ["cmake"]  # Must be in PATH
libraries = []  # Shared libraries needed

[runtime]
timeout_seconds = 30  # Max execution time
retry_on_failure = true
max_retries = 2
network_required = false

[metadata]
# Additional metadata
homepage = "https://github.com/cyxmake/tools/cmake_parser"
documentation = "https://docs.cyxmake.com/tools/cmake_parser"
tags = ["build", "cmake", "c++"]
```

---

## Tool Communication Protocol

### Input Protocol (JSON via stdin)

**Structure**:
```json
{
  "request_id": "uuid-1234-5678",
  "action": "parse",
  "parameters": {
    "file_path": "/home/user/project/CMakeLists.txt",
    "options": {
      "include_comments": false,
      "resolve_variables": true
    }
  },
  "context": {
    "project_root": "/home/user/project",
    "working_directory": "/home/user/project/build",
    "environment": {
      "CMAKE_PREFIX_PATH": "/usr/local",
      "CC": "gcc"
    }
  },
  "metadata": {
    "caller": "cyxmake-orchestrator",
    "version": "0.1.0",
    "timestamp": "2025-11-24T10:30:00Z"
  }
}
```

**Fields**:
- `request_id`: Unique identifier for this request (for logging/tracing)
- `action`: Which capability to execute (must match manifest)
- `parameters`: Action-specific parameters (validated against input schema)
- `context`: Additional context about the project/environment
- `metadata`: Optional metadata for logging/telemetry

### Output Protocol (JSON via stdout)

**Structure**:
```json
{
  "request_id": "uuid-1234-5678",
  "status": "success",
  "exit_code": 0,
  "data": {
    "project_name": "MyProject",
    "cmake_minimum_required": "3.15",
    "targets": [
      {
        "name": "myapp",
        "type": "executable",
        "sources": ["main.cpp", "app.cpp"],
        "link_libraries": ["pthread", "boost_filesystem"]
      }
    ],
    "dependencies": [
      {"name": "Boost", "version": ">=1.70", "components": ["filesystem"]},
      {"name": "Threads", "required": true}
    ]
  },
  "errors": [],
  "warnings": [
    "CMake minimum version 3.15 is quite old, consider updating to 3.20+"
  ],
  "metrics": {
    "parse_time_ms": 145,
    "file_size_bytes": 2048,
    "lines_parsed": 87
  },
  "logs": [
    {"level": "info", "message": "Started parsing CMakeLists.txt"},
    {"level": "info", "message": "Found 1 executable target"},
    {"level": "debug", "message": "Resolved variable CMAKE_CXX_STANDARD to 17"}
  ]
}
```

**Status Values**:
- `success`: Operation completed successfully
- `partial`: Partially completed (some data available, but not all)
- `failed`: Operation failed completely
- `retry`: Transient failure, caller should retry

**Standard Fields**:
- `request_id`: Echo back from request
- `status`: Execution status
- `exit_code`: 0 = success, non-zero = failure
- `data`: Action-specific structured data (arbitrary JSON)
- `errors`: Array of error messages (for `failed` status)
- `warnings`: Non-fatal issues
- `metrics`: Performance/usage metrics (optional)
- `logs`: Detailed log entries (optional, for debugging)

### Error Output (stderr)

**Use stderr for**:
- Debugging information (when `--verbose` flag passed)
- Unstructured crash dumps
- Tool-internal errors (not part of normal operation)

**Important**: stderr is NOT parsed by CyxMake, only logged. All structured communication must use stdout JSON.

---

## Tool Discovery Mechanism

### Discovery Process

```c
// Pseudocode for tool discovery

function discover_tools(tool_directories):
    registry = new ToolRegistry()

    for each directory in tool_directories:
        for each subdirectory in directory:
            manifest_path = join(subdirectory, "tool.toml")

            if file_exists(manifest_path):
                manifest = parse_toml(manifest_path)

                // Validate manifest
                if validate_manifest(manifest):
                    tool = create_tool_from_manifest(manifest, subdirectory)

                    // Check if executable exists
                    exe_path = join(subdirectory, manifest.tool.executable)
                    if file_exists(exe_path) and is_executable(exe_path):
                        registry.register(tool)
                    else:
                        log_warning("Executable not found for tool: " + manifest.tool.name)

    return registry
```

### Tool Directory Structure

```
~/.cyxmake/tools/
├── cmake_parser/
│   ├── tool.toml              # Manifest
│   ├── cmake_parser           # Executable (Linux/macOS)
│   ├── cmake_parser.exe       # Executable (Windows)
│   ├── README.md              # Optional documentation
│   └── tests/                 # Optional test suite
│       ├── test_input_1.json
│       └── test_output_1.json
│
├── npm_installer/
│   ├── tool.toml
│   ├── npm_installer          # Could be a Python script
│   └── requirements.txt       # Tool-specific deps
│
└── bash_executor/
    ├── tool.toml
    └── bash_executor.sh       # Shell script
```

### Tool Search Paths (Priority Order)

1. `~/.cyxmake/tools/` - User-installed tools
2. `/usr/local/lib/cyxmake/tools/` - System-wide tools (Linux/macOS)
3. `C:\Program Files\CyxMake\tools\` - System-wide tools (Windows)
4. `./tools/` - Project-local tools (optional, must enable in config)
5. Environment variable `CYXMAKE_TOOL_PATH` (colon/semicolon separated)

### Tool Registry Structure

```c
// tool_registry.h

typedef struct {
    char* name;
    char* version;
    char* description;
    char* executable_path;

    // Capabilities
    char** categories;
    size_t category_count;

    char** capabilities;  // e.g., ["parse", "validate"]
    size_t capability_count;

    // Requirements
    char** required_commands;
    char** supported_os;

    // Runtime settings
    int timeout_seconds;
    bool retry_on_failure;
    bool network_required;

    // Parsed manifest (full data)
    void* manifest_data;  // Opaque pointer to TOML structure
} ToolMetadata;

typedef struct {
    ToolMetadata** tools;
    size_t count;

    // Indexes for fast lookup
    HashMap* name_index;           // name -> ToolMetadata
    HashMap* category_index;       // category -> ToolMetadata[]
    HashMap* capability_index;     // capability -> ToolMetadata[]
} ToolRegistry;

// API
ToolRegistry* tool_registry_create();
void tool_registry_discover(ToolRegistry* reg, const char** search_paths);
ToolMetadata* tool_registry_find(ToolRegistry* reg, const char* name);
ToolMetadata** tool_registry_find_by_capability(ToolRegistry* reg,
                                                 const char* capability,
                                                 size_t* count);
ToolMetadata** tool_registry_find_by_category(ToolRegistry* reg,
                                               const char* category,
                                               size_t* count);
void tool_registry_destroy(ToolRegistry* reg);
```

---

## Tool Execution Engine

```c
// tool_executor.h

typedef struct {
    char* json_input;
    char** env_vars;       // Additional environment variables
    char* working_dir;     // Working directory for execution
    int timeout_seconds;   // Override manifest timeout
    bool capture_stderr;   // Whether to capture stderr
} ToolExecutionRequest;

typedef struct {
    char* request_id;
    ToolStatus status;     // SUCCESS, PARTIAL, FAILED, RETRY, TIMEOUT
    int exit_code;

    char* stdout_output;   // Raw stdout (includes JSON)
    char* stderr_output;   // Raw stderr (debugging)

    // Parsed JSON output
    cJSON* json_output;
    cJSON* data;           // Shortcut to json_output->data

    char** errors;         // Extracted from json_output
    size_t error_count;

    char** warnings;
    size_t warning_count;

    // Metrics
    double execution_time_ms;
    size_t memory_used_kb;
} ToolExecutionResult;

// API
ToolExecutionResult* tool_execute(ToolMetadata* tool, ToolExecutionRequest* request);
void tool_result_free(ToolExecutionResult* result);

// Helper: Create JSON input
char* tool_create_input_json(const char* action, cJSON* parameters, cJSON* context);
```

### Execution Flow

```c
// Implementation sketch

ToolExecutionResult* tool_execute(ToolMetadata* tool, ToolExecutionRequest* request) {
    ToolExecutionResult* result = calloc(1, sizeof(ToolExecutionResult));

    // 1. Validate input against tool's input schema
    if (!validate_json_against_schema(request->json_input, tool->input_schema)) {
        result->status = FAILED;
        result->exit_code = -1;
        result->errors = create_error_array("Invalid input JSON");
        return result;
    }

    // 2. Prepare execution environment
    ProcessConfig proc_config = {
        .executable = tool->executable_path,
        .working_dir = request->working_dir,
        .env_vars = request->env_vars,
        .timeout_ms = request->timeout_seconds * 1000,
        .capture_stdout = true,
        .capture_stderr = request->capture_stderr
    };

    // 3. Spawn process
    Process* proc = process_spawn(&proc_config);
    if (!proc) {
        result->status = FAILED;
        result->exit_code = -1;
        result->errors = create_error_array("Failed to spawn tool process");
        return result;
    }

    // 4. Write input JSON to stdin
    process_write_stdin(proc, request->json_input);
    process_close_stdin(proc);

    // 5. Wait for completion (with timeout)
    clock_t start = clock();
    int exit_code = process_wait(proc, request->timeout_seconds);
    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC * 1000;

    // 6. Collect output
    result->stdout_output = process_read_stdout(proc);
    result->stderr_output = process_read_stderr(proc);
    result->exit_code = exit_code;
    result->execution_time_ms = elapsed;

    // 7. Parse JSON output
    result->json_output = cJSON_Parse(result->stdout_output);
    if (!result->json_output) {
        result->status = FAILED;
        result->errors = create_error_array("Tool produced invalid JSON");
        process_destroy(proc);
        return result;
    }

    // 8. Extract status
    cJSON* status_obj = cJSON_GetObjectItem(result->json_output, "status");
    if (status_obj && status_obj->valuestring) {
        result->status = parse_status(status_obj->valuestring);
    }

    // 9. Extract data, errors, warnings
    result->data = cJSON_GetObjectItem(result->json_output, "data");
    extract_string_array(result->json_output, "errors", &result->errors, &result->error_count);
    extract_string_array(result->json_output, "warnings", &result->warnings, &result->warning_count);

    // 10. Cleanup
    process_destroy(proc);

    return result;
}
```

---

## Example Tool Implementations

### Example 1: Python Project Analyzer (Python)

**File**: `tools/python_analyzer/python_analyzer.py`

```python
#!/usr/bin/env python3
import sys
import json
import os
from pathlib import Path

def analyze_python_project(file_path, options):
    """Analyze a Python project by examining requirements.txt, setup.py, pyproject.toml"""
    project_root = Path(file_path).parent

    result = {
        "project_type": "python",
        "has_requirements": False,
        "has_setup_py": False,
        "has_pyproject_toml": False,
        "dependencies": [],
        "python_version": None,
        "build_system": None
    }

    # Check for requirements.txt
    req_file = project_root / "requirements.txt"
    if req_file.exists():
        result["has_requirements"] = True
        with open(req_file, 'r') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#'):
                    # Parse dependency (simple version)
                    dep_name = line.split('==')[0].split('>=')[0].split('<=')[0].strip()
                    result["dependencies"].append({
                        "name": dep_name,
                        "spec": line
                    })

    # Check for setup.py
    setup_file = project_root / "setup.py"
    if setup_file.exists():
        result["has_setup_py"] = True
        result["build_system"] = "setuptools"

    # Check for pyproject.toml
    pyproject_file = project_root / "pyproject.toml"
    if pyproject_file.exists():
        result["has_pyproject_toml"] = True
        try:
            import tomllib  # Python 3.11+
            with open(pyproject_file, 'rb') as f:
                data = tomllib.load(f)
                if "build-system" in data:
                    result["build_system"] = data["build-system"].get("build-backend", "unknown")
                if "project" in data and "requires-python" in data["project"]:
                    result["python_version"] = data["project"]["requires-python"]
        except Exception as e:
            # Fallback for older Python or parse errors
            pass

    return result

def main():
    # Read input from stdin
    input_data = json.load(sys.stdin)

    request_id = input_data.get("request_id")
    action = input_data["action"]
    params = input_data["parameters"]

    # Prepare output structure
    output = {
        "request_id": request_id,
        "status": "success",
        "exit_code": 0,
        "data": {},
        "errors": [],
        "warnings": [],
        "logs": []
    }

    try:
        if action == "analyze":
            file_path = params["file_path"]
            options = params.get("options", {})

            if not os.path.exists(file_path):
                output["status"] = "failed"
                output["exit_code"] = 1
                output["errors"].append(f"File not found: {file_path}")
            else:
                output["logs"].append({
                    "level": "info",
                    "message": f"Analyzing Python project at {file_path}"
                })

                output["data"] = analyze_python_project(file_path, options)

                if output["data"]["has_requirements"]:
                    output["logs"].append({
                        "level": "info",
                        "message": f"Found {len(output['data']['dependencies'])} dependencies"
                    })

        else:
            output["status"] = "failed"
            output["exit_code"] = 1
            output["errors"].append(f"Unknown action: {action}")

    except Exception as e:
        output["status"] = "failed"
        output["exit_code"] = 1
        output["errors"].append(f"Exception: {str(e)}")

        # Debug info to stderr
        import traceback
        traceback.print_exc(file=sys.stderr)

    # Write output to stdout
    json.dump(output, sys.stdout, indent=2)
    sys.exit(output["exit_code"])

if __name__ == "__main__":
    main()
```

**Manifest**: `tools/python_analyzer/tool.toml`

```toml
[tool]
name = "python_analyzer"
version = "1.0.0"
description = "Analyze Python project structure and dependencies"
executable = "python_analyzer.py"

[capabilities]
categories = ["analysis", "python"]
languages = ["python"]
analyze = "Analyze Python project structure"

[requirements]
os = ["linux", "windows", "macos"]
commands = ["python3"]

[runtime]
timeout_seconds = 10
retry_on_failure = false
network_required = false
```

---

### Example 2: Bash Executor (C)

**File**: `tools/bash_executor/bash_executor.c`

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "cJSON.h"

#define MAX_OUTPUT 1048576  // 1MB

typedef struct {
    char* command;
    char* working_dir;
    char** env_vars;
    int timeout_seconds;
} ExecuteRequest;

typedef struct {
    int exit_code;
    char* stdout_data;
    char* stderr_data;
} ExecuteResult;

ExecuteResult execute_command(ExecuteRequest* req) {
    ExecuteResult result = {0};

    int stdout_pipe[2], stderr_pipe[2];
    pipe(stdout_pipe);
    pipe(stderr_pipe);

    pid_t pid = fork();

    if (pid == 0) {
        // Child process

        // Redirect stdout/stderr
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        // Change directory
        if (req->working_dir) {
            chdir(req->working_dir);
        }

        // Execute command
        execl("/bin/sh", "sh", "-c", req->command, NULL);
        exit(127);  // execl failed
    } else {
        // Parent process
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Read output
        result.stdout_data = malloc(MAX_OUTPUT);
        result.stderr_data = malloc(MAX_OUTPUT);

        ssize_t n_stdout = read(stdout_pipe[0], result.stdout_data, MAX_OUTPUT - 1);
        ssize_t n_stderr = read(stderr_pipe[0], result.stderr_data, MAX_OUTPUT - 1);

        if (n_stdout > 0) result.stdout_data[n_stdout] = '\0';
        if (n_stderr > 0) result.stderr_data[n_stderr] = '\0';

        // Wait for child
        int status;
        waitpid(pid, &status, 0);
        result.exit_code = WEXITSTATUS(status);

        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
    }

    return result;
}

int main() {
    // Read input JSON from stdin
    char input_buffer[65536];
    size_t input_len = fread(input_buffer, 1, sizeof(input_buffer) - 1, stdin);
    input_buffer[input_len] = '\0';

    cJSON* input = cJSON_Parse(input_buffer);
    if (!input) {
        fprintf(stderr, "Failed to parse input JSON\n");
        return 1;
    }

    cJSON* request_id_obj = cJSON_GetObjectItem(input, "request_id");
    cJSON* action_obj = cJSON_GetObjectItem(input, "action");
    cJSON* params_obj = cJSON_GetObjectItem(input, "parameters");

    const char* request_id = request_id_obj ? request_id_obj->valuestring : "unknown";
    const char* action = action_obj ? action_obj->valuestring : "";

    // Prepare output
    cJSON* output = cJSON_CreateObject();
    cJSON_AddStringToObject(output, "request_id", request_id);
    cJSON_AddStringToObject(output, "status", "success");
    cJSON_AddNumberToObject(output, "exit_code", 0);

    cJSON* data = cJSON_CreateObject();
    cJSON* errors = cJSON_CreateArray();
    cJSON* warnings = cJSON_CreateArray();

    if (strcmp(action, "execute") == 0) {
        cJSON* command_obj = cJSON_GetObjectItem(params_obj, "command");
        cJSON* working_dir_obj = cJSON_GetObjectItem(params_obj, "working_directory");

        if (!command_obj) {
            cJSON_SetValuestring(cJSON_GetObjectItem(output, "status"), "failed");
            cJSON_SetNumberValue(cJSON_GetObjectItem(output, "exit_code"), 1);
            cJSON_AddItemToArray(errors, cJSON_CreateString("Missing 'command' parameter"));
        } else {
            ExecuteRequest req = {
                .command = command_obj->valuestring,
                .working_dir = working_dir_obj ? working_dir_obj->valuestring : NULL,
                .env_vars = NULL,
                .timeout_seconds = 300
            };

            ExecuteResult result = execute_command(&req);

            cJSON_AddNumberToObject(data, "command_exit_code", result.exit_code);
            cJSON_AddStringToObject(data, "stdout", result.stdout_data);
            cJSON_AddStringToObject(data, "stderr", result.stderr_data);

            if (result.exit_code != 0) {
                cJSON_AddItemToArray(warnings,
                    cJSON_CreateString("Command exited with non-zero status"));
            }

            free(result.stdout_data);
            free(result.stderr_data);
        }
    } else {
        cJSON_SetValuestring(cJSON_GetObjectItem(output, "status"), "failed");
        cJSON_SetNumberValue(cJSON_GetObjectItem(output, "exit_code"), 1);
        cJSON_AddItemToArray(errors, cJSON_CreateString("Unknown action"));
    }

    cJSON_AddItemToObject(output, "data", data);
    cJSON_AddItemToObject(output, "errors", errors);
    cJSON_AddItemToObject(output, "warnings", warnings);

    // Print output
    char* output_str = cJSON_Print(output);
    printf("%s\n", output_str);

    // Cleanup
    free(output_str);
    cJSON_Delete(output);
    cJSON_Delete(input);

    return 0;
}
```

---

### Example 3: CMakeLists.txt Generator (Go)

**File**: `tools/cmake_generator/main.go`

```go
package main

import (
    "encoding/json"
    "fmt"
    "os"
    "strings"
)

type Input struct {
    RequestID  string          `json:"request_id"`
    Action     string          `json:"action"`
    Parameters json.RawMessage `json:"parameters"`
}

type GenerateParams struct {
    ProjectName   string   `json:"project_name"`
    Language      string   `json:"language"`
    TargetType    string   `json:"target_type"`  // "executable", "shared_library", "static_library"
    Sources       []string `json:"sources"`
    Dependencies  []string `json:"dependencies"`
    CXXStandard   string   `json:"cxx_standard"`
    OutputPath    string   `json:"output_path"`
}

type Output struct {
    RequestID  string   `json:"request_id"`
    Status     string   `json:"status"`
    ExitCode   int      `json:"exit_code"`
    Data       any      `json:"data"`
    Errors     []string `json:"errors"`
    Warnings   []string `json:"warnings"`
}

func generateCMakeLists(params GenerateParams) (string, error) {
    var builder strings.Builder

    // Header
    builder.WriteString(fmt.Sprintf("cmake_minimum_required(VERSION 3.15)\n"))
    builder.WriteString(fmt.Sprintf("project(%s)\n\n", params.ProjectName))

    // C++ standard
    if params.Language == "cpp" || params.Language == "c++" {
        standard := params.CXXStandard
        if standard == "" {
            standard = "17"
        }
        builder.WriteString(fmt.Sprintf("set(CMAKE_CXX_STANDARD %s)\n", standard))
        builder.WriteString("set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n")
    }

    // Find dependencies
    for _, dep := range params.Dependencies {
        builder.WriteString(fmt.Sprintf("find_package(%s REQUIRED)\n", dep))
    }
    if len(params.Dependencies) > 0 {
        builder.WriteString("\n")
    }

    // Target
    sourcesList := strings.Join(params.Sources, "\n    ")
    switch params.TargetType {
    case "executable":
        builder.WriteString(fmt.Sprintf("add_executable(%s\n    %s\n)\n\n",
            params.ProjectName, sourcesList))
    case "shared_library":
        builder.WriteString(fmt.Sprintf("add_library(%s SHARED\n    %s\n)\n\n",
            params.ProjectName, sourcesList))
    case "static_library":
        builder.WriteString(fmt.Sprintf("add_library(%s STATIC\n    %s\n)\n\n",
            params.ProjectName, sourcesList))
    }

    // Link libraries
    if len(params.Dependencies) > 0 {
        builder.WriteString(fmt.Sprintf("target_link_libraries(%s PRIVATE\n", params.ProjectName))
        for _, dep := range params.Dependencies {
            builder.WriteString(fmt.Sprintf("    %s\n", dep))
        }
        builder.WriteString(")\n")
    }

    return builder.String(), nil
}

func main() {
    // Read input
    var input Input
    decoder := json.NewDecoder(os.Stdin)
    if err := decoder.Decode(&input); err != nil {
        fmt.Fprintf(os.Stderr, "Failed to parse input: %v\n", err)
        os.Exit(1)
    }

    output := Output{
        RequestID: input.RequestID,
        Status:    "success",
        ExitCode:  0,
        Errors:    []string{},
        Warnings:  []string{},
    }

    if input.Action == "generate" {
        var params GenerateParams
        if err := json.Unmarshal(input.Parameters, &params); err != nil {
            output.Status = "failed"
            output.ExitCode = 1
            output.Errors = append(output.Errors, fmt.Sprintf("Invalid parameters: %v", err))
        } else {
            content, err := generateCMakeLists(params)
            if err != nil {
                output.Status = "failed"
                output.ExitCode = 1
                output.Errors = append(output.Errors, fmt.Sprintf("Generation failed: %v", err))
            } else {
                // Write file if output path specified
                if params.OutputPath != "" {
                    if err := os.WriteFile(params.OutputPath, []byte(content), 0644); err != nil {
                        output.Status = "failed"
                        output.ExitCode = 1
                        output.Errors = append(output.Errors, fmt.Sprintf("Failed to write file: %v", err))
                    }
                }

                output.Data = map[string]interface{}{
                    "content": content,
                    "file_path": params.OutputPath,
                    "lines": len(strings.Split(content, "\n")),
                }
            }
        }
    } else {
        output.Status = "failed"
        output.ExitCode = 1
        output.Errors = append(output.Errors, fmt.Sprintf("Unknown action: %s", input.Action))
    }

    // Output JSON
    encoder := json.NewEncoder(os.Stdout)
    encoder.SetIndent("", "  ")
    encoder.Encode(output)
}
```

---

## Tool Development Guidelines

### 1. Structure Your Tool

```
my_tool/
├── tool.toml              # REQUIRED: Manifest
├── my_tool                # REQUIRED: Executable
├── README.md              # RECOMMENDED: Documentation
├── LICENSE                # RECOMMENDED: License file
├── tests/                 # RECOMMENDED: Test suite
│   ├── test_case_1/
│   │   ├── input.json
│   │   └── expected_output.json
│   └── test_case_2/
│       ├── input.json
│       └── expected_output.json
└── src/                   # OPTIONAL: Source code (if not compiled in-place)
```

### 2. Input Validation

**Always validate**:
- Required fields exist
- Types are correct
- File paths exist (if applicable)
- Values are within expected ranges

**Example (Python)**:
```python
def validate_input(input_data):
    if "action" not in input_data:
        return False, "Missing 'action' field"

    if "parameters" not in input_data:
        return False, "Missing 'parameters' field"

    action = input_data["action"]
    params = input_data["parameters"]

    if action == "parse":
        if "file_path" not in params:
            return False, "Missing 'file_path' in parameters"

        if not os.path.exists(params["file_path"]):
            return False, f"File not found: {params['file_path']}"

    return True, None
```

### 3. Error Handling

**Distinguish between**:
- **User errors**: Invalid input, file not found → `status: "failed"`, exit_code 1
- **Transient errors**: Network timeout, resource busy → `status: "retry"`, exit_code 2
- **Partial success**: Some data extracted → `status: "partial"`, exit_code 0
- **Internal errors**: Bugs, crashes → stderr output, exit_code > 2

**Example**:
```python
try:
    result = perform_operation(params)
    output["status"] = "success"
    output["data"] = result
except FileNotFoundError as e:
    output["status"] = "failed"
    output["exit_code"] = 1
    output["errors"].append(f"File not found: {e.filename}")
except requests.Timeout:
    output["status"] = "retry"
    output["exit_code"] = 2
    output["errors"].append("Network timeout, please retry")
except Exception as e:
    output["status"] = "failed"
    output["exit_code"] = 3
    output["errors"].append(f"Unexpected error: {str(e)}")
    traceback.print_exc(file=sys.stderr)
```

### 4. Logging Best Practices

**Use the `logs` array for structured logging**:
```json
"logs": [
  {"level": "debug", "message": "Started parsing file", "timestamp": "2025-11-24T10:30:00Z"},
  {"level": "info", "message": "Found 5 targets"},
  {"level": "warn", "message": "Deprecated syntax detected"},
  {"level": "error", "message": "Failed to resolve variable"}
]
```

**Levels**:
- `debug`: Detailed information for debugging
- `info`: General informational messages
- `warn`: Non-fatal issues
- `error`: Errors that prevented completion

### 5. Performance Considerations

**Optimize for**:
- Fast startup (< 100ms if possible)
- Low memory usage (< 100MB for most tools)
- Quick execution (< 10 seconds for typical operations)

**Avoid**:
- Loading large dependencies unnecessarily
- Reading entire files into memory (stream when possible)
- Synchronous network calls without timeouts

### 6. Testing

**Create test cases**:
```
tests/
├── test_parse_simple/
│   ├── input.json
│   ├── expected_output.json
│   └── fixture_files/
│       └── CMakeLists.txt
├── test_parse_complex/
│   ├── input.json
│   └── expected_output.json
└── test_error_handling/
    ├── input.json
    └── expected_output.json
```

**Test runner** (provided by CyxMake):
```bash
cyxmake test-tool ./tools/my_tool
```

---

## Tool Versioning & Compatibility

### Semantic Versioning

Tools use semantic versioning: `MAJOR.MINOR.PATCH`

- **MAJOR**: Breaking changes to input/output format
- **MINOR**: New capabilities added (backward compatible)
- **PATCH**: Bug fixes, performance improvements

### Compatibility Declaration

```toml
[tool]
name = "cmake_parser"
version = "2.1.0"
compatible_with = ["2.0.0", "2.1.0"]  # Versions this can replace
api_version = "1"  # CyxMake tool API version
```

### Deprecation

```toml
[deprecation]
deprecated = true
deprecated_since = "2024-01-01"
replacement = "cmake_parser_v3"
removal_date = "2025-01-01"
```

---

## Advanced: Dynamic Library Tools (Future)

### Shared Library Interface

```c
// tool_plugin.h - Standard interface for .so/.dll tools

#define TOOL_API_VERSION 1

typedef struct {
    int api_version;
    const char* name;
    const char* version;
    const char** capabilities;
} ToolInfo;

// Entry point: Get tool metadata
EXPORT ToolInfo* tool_get_info();

// Entry point: Execute action
EXPORT int tool_execute(
    const char* action,
    const char* input_json,
    char** output_json,
    char** error_message
);

// Entry point: Cleanup
EXPORT void tool_cleanup();
```

**Advantages**:
- No process spawn overhead
- Shared memory (faster for large data)
- Direct C integration

**Disadvantages**:
- Must be compiled for each platform
- Crash can bring down orchestrator
- More complex to develop

---

## Tool Security Checklist

- [ ] Validate all input (never trust user data)
- [ ] Sanitize file paths (prevent directory traversal)
- [ ] Use absolute paths internally
- [ ] Don't execute user-provided code without sandboxing
- [ ] Limit resource usage (memory, CPU, disk)
- [ ] Set timeouts for all operations
- [ ] Don't log sensitive data (passwords, API keys)
- [ ] Use secure temp directories
- [ ] Clean up temp files after execution
- [ ] Check file permissions before reading/writing

---

## Tool Development SDK (Future)

Provide libraries to simplify tool development:

```python
# cyxmake_sdk.py
from cyxmake import Tool, action, validate

class MyTool(Tool):
    name = "my_tool"
    version = "1.0.0"

    @action
    @validate(schema={"file_path": str, "options": dict})
    def parse(self, file_path, options):
        # Implementation
        return {"result": "..."}

if __name__ == "__main__":
    MyTool().run()
```

---

## Summary

**Key Decisions**:
1. **Subprocess execution model** (stdin/stdout JSON)
2. **TOML manifests** for tool metadata
3. **Structured JSON I/O** for all communication
4. **Directory-based discovery** with priority search paths
5. **Status codes**: success, partial, failed, retry
6. **Multi-language support**: Tools can be in any language

**Next Steps**:
1. Implement tool registry in C
2. Implement tool executor with process spawning
3. Create TOML parser integration
4. Build 3-5 essential tools (project analyzer, bash executor, file parser)
5. Create tool testing framework
6. Write tool development documentation

This design provides a solid foundation for extensible, language-agnostic tool system that's the heart of CyxMake's modularity.

---

**Document Version**: 1.0
**Last Updated**: 2025-11-24
**Status**: Ready for Implementation
