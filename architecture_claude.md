# CyxMake: Technical Architecture Document

## Executive Summary

CyxMake is an AI-powered build automation system that eliminates the complexity of traditional build tools through natural language interfaces and autonomous error recovery. This document provides the technical architecture for building a production-ready implementation.

**Core Innovation**: Instead of learning CMake, Make, or Ninja syntax, developers interact with CyxMake using plain English. The system autonomously understands project structure, executes builds, diagnoses errors, and self-corrects until success.

**Design Philosophy**: Tool-centric, modular architecture where a lightweight LLM orchestrator delegates to specialized, high-performance tools written in C.

---

## System Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     User Interface Layer                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │     CLI      │  │  Interactive │  │   IDE Plugin     │  │
│  │   cyxmake    │  │    Shell     │  │   (Future)       │  │
│  └──────────────┘  └──────────────┘  └──────────────────┘  │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│                  Core Orchestrator (C)                       │
│  ┌───────────────────────────────────────────────────────┐  │
│  │          LLM Integration Layer                        │  │
│  │  ┌────────────────┐      ┌───────────────────────┐   │  │
│  │  │  Local SLM     │◄────►│  Cloud LLM (Fallback) │   │  │
│  │  │  (llama.cpp)   │      │  (API: Claude/GPT)    │   │  │
│  │  └────────────────┘      └───────────────────────┘   │  │
│  └───────────────────────────────────────────────────────┘  │
│                            │                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │          Decision Engine & Planning                    │  │
│  │  • Task decomposition  • Error diagnosis              │  │
│  │  • Tool selection      • Recovery strategies          │  │
│  └───────────────────────────────────────────────────────┘  │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│                   Tool Orchestration Layer                   │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────────────┐    │
│  │   Tool      │  │    Tool     │  │   Tool Registry  │    │
│  │  Executor   │  │   Selector  │  │   & Discovery    │    │
│  └─────────────┘  └─────────────┘  └──────────────────┘    │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│                      Tool Repository                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │  Analysis    │  │   Execution  │  │   Generation     │  │
│  │    Tools     │  │    Tools     │  │     Tools        │  │
│  ├──────────────┤  ├──────────────┤  ├──────────────────┤  │
│  │• File Parser │  │• Bash Exec   │  │• CMake Gen       │  │
│  │• Dep Scanner │  │• Safe Runner │  │• Makefile Gen    │  │
│  │• Env Detect  │  │• Log Monitor │  │• Config Writer   │  │
│  │• Project Map │  │• Pkg Mgr     │  │• README Gen      │  │
│  └──────────────┘  └──────────────┘  └──────────────────┘  │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│              Project Context & State Manager                 │
│  ┌────────────────┐  ┌────────────────┐  ┌──────────────┐  │
│  │  Project Graph │  │  Build State   │  │ Error Context│  │
│  │   (Semantic)   │  │   Tracker      │  │   History    │  │
│  └────────────────┘  └────────────────┘  └──────────────┘  │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│              Environment Abstraction Layer                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │   Windows    │  │    Linux     │  │      macOS       │  │
│  │   Adapter    │  │   Adapter    │  │     Adapter      │  │
│  └──────────────┘  └──────────────┘  └──────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## Core Components Detailed

### 1. Core Orchestrator

**Language**: C (for performance, portability, minimal footprint)

**Responsibilities**:
- Initialize and manage the agent lifecycle
- Route user requests to appropriate subsystems
- Maintain conversation context
- Coordinate between LLM and tool layers
- Handle errors and recovery strategies
- Manage configuration and state persistence

**Key Modules**:

```c
// core_orchestrator.h
typedef struct {
    LLMContext* llm;
    ToolRegistry* tools;
    ProjectContext* project;
    ConfigManager* config;
    ErrorRecovery* recovery;
} Orchestrator;

// Main entry points
Orchestrator* orchestrator_init(Config* cfg);
void orchestrator_execute_command(Orchestrator* orch, Command* cmd);
void orchestrator_shutdown(Orchestrator* orch);
```

---

### 2. LLM Integration Layer

**Hybrid Strategy**:

#### Local SLM (Primary)
- **Runtime**: llama.cpp (C++ library with C bindings)
- **Model Size**: 3B-7B parameters (quantized to 4-bit)
- **Use Cases**:
  - Tool selection and sequencing
  - Common error pattern recognition
  - Routine build orchestration
- **Memory Footprint**: 2-4GB RAM
- **Inference Speed**: 20-50 tokens/second on CPU

#### Cloud LLM (Fallback)
- **Triggers**:
  - Local model confidence < threshold
  - Novel error patterns
  - Complex multi-step reasoning
  - User explicitly requests deep analysis
- **Providers**: Claude API, OpenAI API (configurable)
- **Cost Control**: Token budgets, rate limiting

**Implementation Structure**:

```c
// llm_interface.h
typedef enum {
    LLM_BACKEND_LOCAL,
    LLM_BACKEND_CLOUD,
    LLM_BACKEND_AUTO
} LLMBackend;

typedef struct {
    char* prompt;
    float temperature;
    int max_tokens;
    LLMBackend backend_preference;
} LLMRequest;

typedef struct {
    char* response;
    float confidence;
    LLMBackend used_backend;
    int tokens_used;
} LLMResponse;

// Core interface
LLMContext* llm_init(const char* model_path, const char* api_key);
LLMResponse* llm_query(LLMContext* ctx, LLMRequest* req);
void llm_free_response(LLMResponse* resp);
```

---

### 3. Tool System Architecture

**Design Principle**: Tools are self-contained, executable units with standardized I/O.

#### Tool Interface Standard

```c
// tool_interface.h
typedef struct {
    char* name;              // "cmake_parser", "npm_install"
    char* version;           // "1.0.0"
    char* description;       // Human-readable purpose
    char** capabilities;     // ["parse", "validate", "suggest"]
    ToolCategory category;   // ANALYSIS, EXECUTION, GENERATION
} ToolMetadata;

typedef struct {
    char* json_input;        // Standardized JSON input
    char** env_vars;         // Environment variables
    char* working_dir;       // Execution context
} ToolInput;

typedef struct {
    int exit_code;           // 0 = success
    char* stdout_output;     // Standard output
    char* stderr_output;     // Error output
    char* structured_data;   // JSON for machine parsing
    ToolStatus status;       // SUCCESS, PARTIAL, FAILED, NEEDS_RETRY
} ToolOutput;

// Tool function signature
typedef ToolOutput* (*ToolFunction)(ToolInput* input);
```

#### Tool Registry

```c
// tool_registry.h
typedef struct {
    ToolMetadata** tools;
    size_t count;
    HashMap* name_index;     // Fast lookup by name
    HashMap* capability_index; // Lookup by capability
} ToolRegistry;

ToolRegistry* registry_init();
void registry_register_tool(ToolRegistry* reg, ToolMetadata* tool);
ToolMetadata* registry_find_by_capability(ToolRegistry* reg, const char* cap);
ToolMetadata** registry_find_all_for_language(ToolRegistry* reg, const char* lang);
```

#### Tool Categories & Examples

**Analysis Tools**:
- `cmake_parser`: Parse CMakeLists.txt → semantic graph
- `package_json_parser`: Extract deps, scripts from package.json
- `env_detector`: Detect OS, compilers, installed tools
- `project_structure_analyzer`: Identify language, build system
- `dependency_scanner`: Find missing dependencies

**Execution Tools**:
- `bash_executor`: Safe command execution with logging
- `npm_runner`: Execute npm commands with error parsing
- `cmake_runner`: Run cmake with structured output
- `compiler_invoker`: Invoke GCC/Clang/MSVC with error extraction
- `package_installer`: Install deps via apt/brew/vcpkg/pip

**Generation Tools**:
- `cmake_generator`: Create CMakeLists.txt from spec
- `makefile_generator`: Create Makefile from spec
- `readme_generator`: Generate build documentation
- `dockerfile_generator`: Create containerized build env

---

### 4. Project Context Manager

**Purpose**: Maintain a live, semantic representation of the project being built.

```c
// project_context.h
typedef enum {
    LANG_C, LANG_CPP, LANG_PYTHON, LANG_RUST,
    LANG_JAVASCRIPT, LANG_JAVA, LANG_GO, LANG_UNKNOWN
} ProjectLanguage;

typedef enum {
    BUILD_CMAKE, BUILD_MAKE, BUILD_CARGO,
    BUILD_NPM, BUILD_MAVEN, BUILD_GRADLE, BUILD_CUSTOM
} BuildSystem;

typedef struct {
    char* name;
    char* version;
    char* source;            // "vcpkg", "pip", "npm"
    bool is_installed;
} Dependency;

typedef struct {
    char* root_path;
    ProjectLanguage primary_language;
    ProjectLanguage* other_languages;
    BuildSystem build_system;
    Dependency** dependencies;
    char** source_files;
    char** config_files;
    char* readme_path;
    HashMap* metadata;        // Flexible key-value storage
} ProjectContext;

ProjectContext* project_analyze(const char* root_path, ToolRegistry* tools);
void project_update_state(ProjectContext* ctx, const char* key, const char* value);
char* project_to_json(ProjectContext* ctx);
```

---

### 5. Error Recovery System

**Multi-Level Recovery Strategy**:

```c
// error_recovery.h
typedef enum {
    ERR_MISSING_DEPENDENCY,
    ERR_COMPILER_ERROR,
    ERR_LINKER_ERROR,
    ERR_PERMISSION_DENIED,
    ERR_ENV_MISCONFIGURATION,
    ERR_NETWORK_FAILURE,
    ERR_UNKNOWN
} ErrorCategory;

typedef struct {
    ErrorCategory category;
    char* raw_message;
    char* parsed_details;
    float confidence;         // How confident we are about diagnosis
    int occurrence_count;     // How many times this error occurred
} ErrorDiagnosis;

typedef struct {
    char* description;        // Human-readable action
    char** commands;          // Commands to execute
    char** file_modifications; // Files to modify (JSON patches)
    int priority;             // Try high priority first
    float success_probability; // ML-based prediction
} RecoveryAction;

// Core recovery interface
ErrorDiagnosis* error_diagnose(const char* error_output, ProjectContext* ctx);
RecoveryAction** error_generate_solutions(ErrorDiagnosis* diag, int* count);
bool error_execute_recovery(RecoveryAction* action, ProjectContext* ctx);
```

**Recovery Flow**:

1. **Capture Error**: Monitor tool execution outputs
2. **Diagnose**: Use pattern matching + LLM to categorize error
3. **Generate Solutions**: Retrieve from knowledge base or ask LLM
4. **Execute**: Apply fixes in order of priority
5. **Verify**: Re-run failed step
6. **Learn**: Log successful recovery for future use

---

### 6. Environment Abstraction Layer

**Purpose**: Provide uniform interface across Windows, Linux, macOS.

```c
// env_abstraction.h
typedef struct {
    OSType os;                // WINDOWS, LINUX, MACOS
    Architecture arch;        // X86_64, ARM64
    char* shell;              // cmd.exe, bash, zsh
    char* path_separator;     // ; or :
    char* line_ending;        // \r\n or \n
    HashMap* env_vars;
} EnvironmentInfo;

// Cross-platform operations
char* env_get_temp_dir();
char* env_find_executable(const char* name);
bool env_has_command(const char* command);
int env_execute_command(const char* cmd, char** output, char** error);
char* env_normalize_path(const char* path);
```

---

## Data Flow & Interaction Patterns

### Scenario 1: Build Existing Project

```
User: cyxmake build
    │
    ▼
[CLI] Parse command → create BuildRequest
    │
    ▼
[Orchestrator] Initialize project context
    │
    ▼
[Tool: project_analyzer] Scan directory structure
    │
    ├─► Detect: Python project with requirements.txt
    ├─► Detect: No virtual environment
    └─► Detect: Build instructions in README.md
    │
    ▼
[Tool: readme_parser] Extract build steps
    │
    └─► Steps: ["pip install -r requirements.txt", "python setup.py build"]
    │
    ▼
[Orchestrator] Plan execution sequence
    │
    ▼
[LLM Local] "Should I create venv first?" → YES
    │
    ▼
[Tool: bash_executor] python -m venv .venv
    │
    ├─► Success
    │
    ▼
[Tool: bash_executor] pip install -r requirements.txt
    │
    ├─► ERROR: "Could not find a version that satisfies numpy==1.25.0"
    │
    ▼
[Error Recovery] Diagnose error
    │
    ├─► Category: MISSING_DEPENDENCY
    ├─► Hypothesis: numpy version unavailable for Python 3.12
    │
    ▼
[LLM Local] Generate recovery actions
    │
    ├─► Action 1: Try numpy==1.24.0 (high priority)
    ├─► Action 2: Use latest numpy (medium priority)
    │
    ▼
[Tool: file_editor] Modify requirements.txt (numpy==1.24.0)
    │
    ▼
[Tool: bash_executor] RETRY pip install -r requirements.txt
    │
    ├─► Success
    │
    ▼
[Tool: bash_executor] python setup.py build
    │
    ├─► Success
    │
    ▼
[Orchestrator] Report success to user
```

### Scenario 2: Generate Build Configuration

```
User: cyxmake create
User: "C++ project with SDL2 and OpenGL, targeting Linux and Windows"
    │
    ▼
[Orchestrator] Parse natural language intent
    │
    ▼
[LLM Local] Extract requirements
    │
    ├─► Language: C++
    ├─► Libraries: SDL2, OpenGL
    ├─► Platforms: Linux, Windows
    ├─► Output: Infer CMake (most common for multi-platform C++)
    │
    ▼
[LLM Local] Ask clarifying questions (if needed)
    │
    └─► "Shared library or executable?" → User: "Executable"
    │
    ▼
[Tool: cmake_generator] Generate CMakeLists.txt
    │
    ├─► Input: {lang: "cpp", libs: ["SDL2", "OpenGL"],
    │           platforms: ["linux", "windows"], type: "exe"}
    ├─► Output: CMakeLists.txt with find_package(), target_link_libraries()
    │
    ▼
[Tool: readme_generator] Generate build instructions
    │
    └─► Output: README.md with platform-specific build steps
    │
    ▼
[Orchestrator] Write files to disk
    │
    ▼
[Orchestrator] (Optional) Test generated config
    │
    └─► Run: cmake . && make
    │
    ▼
[Orchestrator] Report success, show generated files
```

---

## Technology Stack Justification

### Core Language: C

**Reasons**:
1. **Performance**: Direct memory control, minimal overhead
2. **Portability**: Compile to native binaries on all platforms
3. **Small footprint**: Executable size 1-5MB (vs 50-100MB for Rust/Go)
4. **LLM Integration**: llama.cpp is C++, C bindings readily available
5. **System Access**: Direct OS API access without FFI layers
6. **Stability**: C ABI is universal and stable

**Challenges**:
- Manual memory management (mitigated with disciplined patterns)
- No built-in JSON/HTTP libraries (use cJSON, libcurl)
- More verbose than modern languages

**Mitigation**:
- Use strict coding standards (MISRA-C style)
- Valgrind/AddressSanitizer for memory safety
- Comprehensive test suite
- Consider C++ for complex modules (STL, RAII)

### LLM Runtime: llama.cpp

**Reasons**:
1. **Proven**: Used in production by thousands of projects
2. **Performance**: Optimized CPU/GPU inference
3. **Quantization**: Supports GGUF 2-bit to 8-bit quantization
4. **Cross-platform**: Windows, Linux, macOS, even mobile
5. **No Python**: Pure C++, fits our stack
6. **Active development**: Regular updates, model support

**Integration**:
```c
#include "llama.h"

// Load quantized model
llama_model* model = llama_load_model_from_file("model-q4.gguf", params);
llama_context* ctx = llama_new_context_with_model(model, ctx_params);

// Generate response
int n_tokens = llama_tokenize(ctx, prompt, tokens, max_tokens);
for (int i = 0; i < n_predict; i++) {
    int next_token = llama_sample_token(ctx, candidates);
    llama_decode(ctx, ...);
}
```

### Data Formats

- **Configuration**: TOML (human-friendly, no quotes)
- **IPC (Inter-tool communication)**: JSON (universal, parseable)
- **Logging**: Structured JSON logs for ML processing
- **Models**: GGUF (llama.cpp native format)

### Build System (for CyxMake itself)

- **CMake**: Ironic but practical, widely supported
- **vcpkg/Conan**: Dependency management for C libraries
- **Alternative**: Write custom build script using CyxMake itself (dogfooding)

---

## Security & Sandboxing

**Threat Model**:
1. Malicious build scripts executing arbitrary code
2. Dependency confusion attacks
3. Information leakage (source code, credentials)
4. Resource exhaustion (infinite loops, fork bombs)

**Mitigation Strategies**:

### 1. Command Execution Sandbox

```c
// sandbox.h
typedef struct {
    char** allowed_commands;     // Whitelist
    char** blocked_paths;        // Prevent access to sensitive dirs
    size_t max_memory_mb;        // Resource limits
    size_t max_time_seconds;
    size_t max_processes;
    bool network_allowed;
} SandboxConfig;

int sandbox_execute(const char* cmd, SandboxConfig* config, char** output);
```

**Implementation**:
- **Linux**: Use `seccomp`, `namespaces`, `cgroups`
- **Windows**: Job Objects, restricted tokens
- **macOS**: Sandbox API, App Sandbox

### 2. Privilege Separation

- Core orchestrator runs as regular user
- Tool executor has minimal permissions
- Sudo/admin access requires explicit user confirmation

### 3. Audit Logging

```c
typedef struct {
    time_t timestamp;
    char* command;
    char* working_dir;
    int exit_code;
    char* user_id;
} AuditLogEntry;
```

All executed commands logged for forensics.

### 4. Dependency Verification

- Verify checksums of downloaded packages
- Use official package registries
- Warn on suspicious dependency modifications

---

## Implementation Phases

### Phase 0: Foundation (4-6 weeks)

**Goals**: Core infrastructure, single language support

**Deliverables**:
- [ ] C project structure with CMake
- [ ] CLI argument parsing (opt, argp, or custom)
- [ ] Logging system (structured JSON logs)
- [ ] Configuration management (TOML parser)
- [ ] Tool interface definition + registry
- [ ] Environment abstraction layer
- [ ] Basic project context manager
- [ ] 3-5 analysis tools (Python/C++ project detection)
- [ ] 3-5 execution tools (bash, pip, cmake)
- [ ] LLM interface abstraction (mock implementation)

**Success Criteria**: Can detect a Python project, read requirements.txt, execute pip install

### Phase 1: Local LLM Integration (3-4 weeks)

**Goals**: Integrate llama.cpp, basic reasoning

**Deliverables**:
- [ ] llama.cpp C bindings
- [ ] Model loading and inference
- [ ] Quantized model selection (test 3B/7B models)
- [ ] Prompt engineering for tool selection
- [ ] Basic error diagnosis with LLM
- [ ] Confidence scoring

**Success Criteria**: LLM can select appropriate tools for simple build tasks

### Phase 2: Error Recovery (4-6 weeks)

**Goals**: Autonomous error detection and fixing

**Deliverables**:
- [ ] Error capture from tool outputs
- [ ] Pattern-based error categorization
- [ ] Recovery action generator
- [ ] Fix execution engine
- [ ] Retry logic with backoff
- [ ] Recovery success tracking

**Success Criteria**: Can autonomously fix 5+ common error types (missing deps, compiler flags, etc.)

### Phase 3: Build System Support (6-8 weeks)

**Goals**: Support 5+ build systems

**Deliverables**:
- [ ] CMake tools (parser, generator, runner)
- [ ] Make tools
- [ ] npm/package.json tools
- [ ] Cargo tools
- [ ] pip/requirements.txt tools
- [ ] README.md parser with build instruction extraction

**Success Criteria**: Successfully build 20+ real-world open source projects

### Phase 4: Generation Mode (4-6 weeks)

**Goals**: CyxMake create functionality

**Deliverables**:
- [ ] Natural language intent parser
- [ ] CMakeLists.txt generator
- [ ] Makefile generator
- [ ] package.json generator
- [ ] README.md generator
- [ ] Interactive refinement loop

**Success Criteria**: Generate working build configs from English descriptions

### Phase 5: Cloud LLM Fallback (2-3 weeks)

**Goals**: Hybrid local/cloud architecture

**Deliverables**:
- [ ] API client for Claude/GPT
- [ ] Confidence threshold system
- [ ] Automatic escalation logic
- [ ] Cost tracking
- [ ] Rate limiting

**Success Criteria**: Complex errors automatically escalate to cloud LLM

### Phase 6: Advanced Features (8-10 weeks)

**Goals**: Production readiness

**Deliverables**:
- [ ] Sandboxed execution
- [ ] Comprehensive logging and telemetry
- [ ] User preferences and profiles
- [ ] Interactive shell mode
- [ ] JSON output mode for IDE integration
- [ ] Resume from checkpoint
- [ ] Parallel build support
- [ ] Cross-compilation support

**Success Criteria**: Beta testing with 50+ users

---

## Critical Technical Challenges

### Challenge 1: LLM Context Window Management

**Problem**: Build logs can be 10,000+ lines, exceeding context limits

**Solutions**:
1. **Intelligent truncation**: Only send relevant excerpts (last 50 lines, error vicinity)
2. **Summarization**: Use local model to summarize logs before sending to main LLM
3. **Chunking**: Process logs in chunks with state carried forward
4. **RAG**: Index logs, retrieve relevant sections based on error type

### Challenge 2: Tool Versioning & Compatibility

**Problem**: Tools may break across versions of build systems

**Solutions**:
1. **Semantic versioning**: Tools declare min/max versions they support
2. **Runtime detection**: Query installed tool versions before execution
3. **Adapter pattern**: Abstract version differences behind unified interface
4. **Fallback chains**: Try multiple strategies (cmake 3.20 syntax → 3.10 fallback)

### Challenge 3: Error Diagnosis Accuracy

**Problem**: Infinite variety of error messages, many ambiguous

**Solutions**:
1. **Pattern library**: Curated database of error regex → category mappings
2. **LLM ensemble**: Combine local pattern matching + LLM reasoning
3. **Similarity search**: Vector DB of known errors (cosine similarity)
4. **Iterative refinement**: If first fix fails, reanalyze with more context
5. **Telemetry**: Learn from successful/failed recoveries

### Challenge 4: Cross-Platform File Path Handling

**Problem**: Windows (C:\), Linux (/home), different separators, symlinks

**Solutions**:
1. **Path normalization**: Convert all paths to internal canonical form
2. **OS detection**: Runtime platform detection, use appropriate conventions
3. **Abstraction layer**: Never manipulate paths as strings directly
4. **Test matrix**: CI on Windows/Linux/macOS

### Challenge 5: Secure Command Execution

**Problem**: Build scripts can contain malicious commands

**Solutions**:
1. **Whitelist mode**: Only allow known-safe commands (opt-in risky ones)
2. **Dry-run mode**: Show what would be executed, require confirmation
3. **Sandbox**: OS-level isolation (namespaces, containers)
4. **Static analysis**: Scan build scripts for dangerous patterns before execution
5. **User trust levels**: Configure per-project security policies

---

## Metrics & Observability

### Key Metrics to Track

**Performance**:
- Time to first analysis (< 5 seconds)
- Time to build completion
- LLM inference latency (local vs cloud)
- Memory usage (target: < 4GB)
- CPU usage during idle (< 5%)

**Reliability**:
- Build success rate (target: > 90% on first attempt)
- Error recovery success rate (target: > 70%)
- Median attempts to successful build (target: < 3)

**User Experience**:
- Commands until first error (resilience)
- User interventions required per build (target: < 2)
- False positive error diagnoses (target: < 10%)

**Cost**:
- Cloud LLM API calls per build
- Average tokens per build
- Cost per successful build

### Logging Architecture

```c
// logger.h
typedef enum {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

void log_structured(LogLevel level, const char* component,
                    const char* event, const char* json_metadata);

// Example
log_structured(LOG_INFO, "tool_executor", "command_completed",
               "{\"tool\": \"npm_install\", \"duration_ms\": 2345, \"exit_code\": 0}");
```

---

## Configuration Management

### Configuration File: `cyxmake.toml`

```toml
[general]
log_level = "info"
log_file = "~/.cyxmake/logs/cyxmake.log"
cache_dir = "~/.cyxmake/cache"

[llm]
# Local model settings
local_model = "~/.cyxmake/models/codellama-7b-q4.gguf"
local_enabled = true
local_context_size = 4096
local_threads = 4

# Cloud fallback settings
cloud_enabled = true
cloud_provider = "anthropic"  # or "openai"
cloud_api_key_env = "ANTHROPIC_API_KEY"
cloud_model = "claude-3-5-sonnet-20241022"
confidence_threshold = 0.7  # Escalate to cloud if local confidence < 0.7

[execution]
sandbox_enabled = true
max_retries = 3
timeout_seconds = 600
allow_network = true
allow_sudo = false  # Prompt user if needed

[tools]
tool_path = ["~/.cyxmake/tools", "/usr/local/lib/cyxmake/tools"]
auto_update = false

[build]
parallel_jobs = 4  # For -j flag
verbose = false
```

---

## API Design (Future IDE Integration)

### JSON-RPC Interface

```json
// Request: Build project
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "build",
  "params": {
    "project_path": "/home/user/myproject",
    "target": "all",
    "verbose": true
  }
}

// Response: Success
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "status": "success",
    "duration_seconds": 42.3,
    "artifacts": [
      {"type": "executable", "path": "./build/myapp"}
    ],
    "warnings": 3,
    "errors_fixed": 2
  }
}

// Response: Error
{
  "jsonrpc": "2.0",
  "id": 1,
  "error": {
    "code": -32001,
    "message": "Build failed after 3 recovery attempts",
    "data": {
      "last_error": "linker error: undefined reference to 'boost::filesystem'",
      "diagnosis": "Missing Boost library",
      "suggested_actions": [
        "Install boost-devel package",
        "Add -lboost_filesystem to linker flags"
      ]
    }
  }
}
```

---

## Testing Strategy

### Unit Tests (C with Check or Unity framework)

- Test each tool in isolation
- Mock LLM responses
- Test error parsing logic
- Test path normalization
- Coverage target: > 80%

### Integration Tests

- Real project builds (small open source repos)
- Test error injection and recovery
- Test cross-platform behavior (Docker containers)

### End-to-End Tests

- Clone real GitHub repos, run `cyxmake build`
- Measure success rate
- Benchmark against manual builds

### Chaos Testing

- Inject random errors (kill processes, corrupt files)
- Verify graceful degradation
- Test recovery mechanisms

---

## Open Questions & Future Research

1. **Model Selection**: Which 3B-7B models work best for build orchestration? (Phi-3, Llama-3.2, Qwen)

2. **Fine-tuning Strategy**: Should we fine-tune on build logs, or is in-context learning sufficient?

3. **Tool Discovery**: Should tools be compiled in, dynamically loaded (.so/.dll), or subprocesses?

4. **Concurrency Model**: Single-threaded event loop, thread pool, or async I/O?

5. **Error Pattern Database**: Maintain centrally, or crowdsource from telemetry?

6. **Multi-language Projects**: How to handle monorepos with Python + C++ + JS?

7. **IDE Integration**: VSCode extension, LSP, or custom protocol?

8. **Containerization**: Should we auto-generate Dockerfiles for reproducible builds?

9. **CI/CD Integration**: GitHub Actions plugin, Jenkins integration?

10. **Telemetry & Privacy**: What data to collect without compromising user privacy?

---

## Conclusion

This architecture provides a solid foundation for building CyxMake as a production-ready system. The modular, tool-centric design allows for incremental development while maintaining flexibility for future enhancements.

**Next Steps**:
1. Validate this architecture with stakeholders
2. Set up development environment (C toolchain, CMake)
3. Begin Phase 0 implementation
4. Establish CI/CD pipeline
5. Create initial tool implementations

**Success Criteria for v1.0**:
- Successfully build 50+ diverse open-source projects
- 90%+ build success rate on first attempt
- < 3 recovery attempts on average for failed builds
- < 4GB memory footprint
- Support for C/C++, Python, JavaScript, Rust
- Cross-platform (Windows, Linux, macOS)

---

**Document Version**: 1.0
**Last Updated**: 2025-11-23
**Authors**: Development Team + Claude
**Status**: Approved for Implementation
