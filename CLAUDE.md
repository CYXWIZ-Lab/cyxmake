# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**CyxMake** is an AI-powered build automation system that eliminates traditional build system complexity through natural language interfaces and autonomous error recovery. Written in C for performance and portability, it orchestrates existing build tools (CMake, Make, npm, cargo, etc.) with AI intelligence from a local LLM (llama.cpp) and optional cloud fallback.

**Core Innovation**: Instead of learning CMake/Make/Ninja syntax, developers use plain English. CyxMake autonomously understands project structure, executes builds, diagnoses errors, and self-corrects until success.

## Build Commands

### Standard Build Process
```bash
# Configure and build (from scratch)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .

# Or in Release mode
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Build specific components
cmake --build . --target cyxmake_core    # Core library only
cmake --build . --target test_logger     # Specific test
```

### Running Tests
```bash
# Build and run all tests
cd build
ctest --output-on-failure

# Run specific tests
./bin/test_logger
./bin/test_error_recovery
./bin/test_tool_executor

# Tests available:
# - test_logger: Logger functionality
# - test_error_recovery: Error diagnosis and recovery system
# - test_tool_executor: Tool discovery, registry, and execution
```

### CMake Build Options
```bash
# Configure with options
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCYXMAKE_BUILD_TESTS=ON \
  -DCYXMAKE_BUILD_TOOLS=ON \
  -DCYXMAKE_USE_SANITIZERS=OFF

# Available options:
# CYXMAKE_BUILD_TESTS (default: ON) - Build test suite
# CYXMAKE_BUILD_TOOLS (default: ON) - Build bundled tools
# CYXMAKE_USE_SANITIZERS (default: OFF) - Enable address sanitizer
# CYXMAKE_STATIC_LINK (default: OFF) - Static linking
```

### Using CyxMake Itself
```bash
# Navigate to any project
cd /path/to/some/project

# Analyze project
cyxmake init

# Build automatically (autonomous error recovery)
cyxmake build

# Create new project from natural language
cyxmake create "C++ game with SDL2 and OpenGL"
```

## Architecture Overview

### High-Level Design

CyxMake follows a **modular, tool-centric architecture** with clear separation of concerns:

```
┌─────────────────────────────────────┐
│    Core Orchestrator (C)            │  ← Entry point, coordinates everything
│  ┌──────────────────────────────┐   │
│  │   LLM Integration Layer      │   │  ← Local (llama.cpp) + Cloud API
│  └──────────────────────────────┘   │
└─────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────┐
│  Tool Orchestration Layer           │  ← Tool registry, executor, selector
└─────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────┐
│  Specialized Tools                  │  ← Analysis, Execution, Generation tools
│  • Project analyzer                 │
│  • Dependency scanner               │
│  • Build executors                  │
│  • Package managers                 │
└─────────────────────────────────────┘
```

### Key Subsystems

**1. Core Orchestrator** (`src/core/orchestrator.c`)
- Entry point for all operations
- Manages lifecycle, routes requests, coordinates subsystems
- Maintains conversation context and error recovery state

**2. LLM Integration** (`src/llm/`)
- `llm_interface.c`: Abstraction layer for local/cloud LLMs
- `llm_local.c`: llama.cpp integration (bundled 3B-7B quantized models)
- `llm_cloud.c`: Claude/GPT API fallback for complex errors
- `prompt_templates.c`: Prompt engineering for build tasks
- `error_analyzer.c`: AI-powered error diagnosis

**3. Project Context System** (`src/context/`)
- `project_analyzer.c`: Deep project structure analysis
- `project_context.c`: Semantic project representation
- `cache_manager.c`: Persistent .cyxmake/cache.json storage
- `change_detector.c`: Incremental update detection

**4. Error Recovery System** (`src/recovery/`)
- `error_diagnosis.c`: Error categorization and pattern matching
- `error_patterns.c`: Knowledge base of common build errors
- `solution_generator.c`: Fix strategy generation
- `fix_executor.c`: Autonomous fix application and retry logic

**5. Tool System** (`src/tools/`)
- `tool_registry.c`: Dynamic tool storage and lookup
- `tool_discovery.c`: System scanning for compilers, build tools, package managers
- `tool_executor.c`: Safe command execution with output capture

**Critical Design Pattern**: The tool system uses a **registry pattern** where tools are discovered at runtime, registered by capability, and selected by the LLM based on context. This enables extensibility without recompilation.

### Cross-Platform Abstraction

All OS-specific code is isolated behind `include/cyxmake/compat.h`:
- Path handling (Windows backslashes vs Unix forward slashes)
- Process execution (Windows CreateProcess vs Unix fork/exec)
- Directory operations (_getcwd/_chdir vs getcwd/chdir)
- File I/O with proper line ending handling

**Important**: When adding Windows-specific code, always use the macros defined in compat.h and add the corresponding POSIX version.

## Component Interaction Patterns

### Build Flow Example
```
User runs: cyxmake build
    ↓
[Orchestrator] Loads cached context from .cyxmake/cache.json
    ↓
[Project Analyzer] Detects: C++, CMake, missing fmt dependency
    ↓
[LLM Local] Plans: "Configure CMake → Install deps → Build"
    ↓
[Tool Executor] Runs cmake, detects error: "fmt not found"
    ↓
[Error Recovery] Diagnoses: MISSING_DEPENDENCY → category
    ↓
[Solution Generator] Suggests: "vcpkg install fmt" (priority 1)
    ↓
[Tool Executor] Executes fix → Retry build → Success
```

### Tool Discovery Flow
```
[Tool Discovery] Scans PATH using 'where' (Windows) or 'which' (Unix)
    ↓
[Tool Registry] Stores: {name: "cmake", path: "C:\Program Files\CMake\bin\cmake.exe", version: "3.28.0"}
    ↓
[Tool Executor] When needed, looks up tool by name → executes with proper path quoting
```

## Implementation Status (Phase 1 Complete)

### ✅ Implemented
- Core orchestrator with CLI interface
- Project context analysis and caching
- Logger with colored output and log levels
- Build executor with CMake/Make support
- LLM interface abstraction (llama.cpp integration)
- Error recovery system (diagnosis, patterns, solution generation)
- **Tool system (registry, discovery, execution)**
- **Package manager integration (apt, brew, vcpkg, npm, pip, cargo, winget, etc.)**

### 🚧 Partial/Stub
- LLM local inference (llama.cpp wrapper exists, model loading needs work)
- Cloud API fallback (API client exists, needs authentication)
- Config management (reads TOML, needs validation)

### ❌ Not Implemented
- Natural language project generation
- Multi-language project support (currently best with C/C++)
- Interactive shell mode
- CI/CD integration plugins
- Sandboxed execution (security feature)

## Development Guidelines

### Adding a New Tool

1. **Define capabilities** in `include/cyxmake/tool_executor.h` if needed
2. **Add discovery logic** to `src/tools/tool_discovery.c`:
   ```c
   const char* new_tools[] = {"your-tool", NULL};
   for (int i = 0; new_tools[i]; i++) {
       ToolInfo* tool = discover_tool(new_tools[i], NULL, TOOL_TYPE_FOO, 0);
       tool_registry_add(registry, tool);
   }
   ```
3. **Add execution wrapper** if complex, or use `tool_execute_command()` directly
4. **Write tests** in `tests/test_tool_executor.c`

### Adding Error Recovery Patterns

1. **Identify error pattern** (regex or substring match)
2. **Add to** `src/recovery/error_patterns.c`:
   ```c
   {.pattern = "cannot find -lfoo", .category = ERR_MISSING_DEPENDENCY,
    .suggested_action = "Install libfoo-dev or libfoo-devel"}
   ```
3. **Update solution generator** in `src/recovery/solution_generator.c` if complex logic needed

### Memory Management

**Critical**: All C code must follow strict memory discipline:
- Every `malloc/calloc` must have a corresponding `free`
- Use `_free()` helper functions for complex structs (e.g., `tool_exec_result_free()`)
- Run tests with Valgrind or AddressSanitizer (`-DCYXMAKE_USE_SANITIZERS=ON`)
- Check for double-frees (common in cleanup code)

**Common Pitfalls**:
- Don't free strings owned by other structs (use `strdup` if you need a copy)
- Free arrays of strings by iterating through each element first
- Set pointers to NULL after freeing to catch use-after-free bugs

### Error Handling Patterns

```c
// Standard error handling
ToolExecResult* result = tool_execute(tool, options);
if (!result) {
    log_error("Execution failed");
    return CYXMAKE_ERROR_TOOL_FAILED;
}

if (!result->success) {
    log_warning("Tool returned error: %d", result->exit_code);
    // Trigger error recovery here
}

tool_exec_result_free(result);
```

### Logging Best Practices

```c
// Use appropriate log levels
log_debug("Tool path: %s", tool->path);           // Debugging info
log_info("Building project...");                   // User-visible progress
log_warning("Dependency missing: %s", dep_name);   // Recoverable issues
log_error("Fatal error: %s", error_msg);          // Unrecoverable errors
log_success("Build completed in %.2fs", duration); // Success messages
```

## Testing Philosophy

**Test Coverage Priorities**:
1. **Critical Path**: Tool discovery/execution, error recovery, project analysis
2. **Cross-Platform**: Test on Windows, Linux, macOS (use CI)
3. **Real World**: Test with actual open-source projects, not just synthetic examples
4. **Error Injection**: Simulate missing tools, bad paths, corrupted files

**Current Tests**:
- `test_logger`: Validates all log levels, file output, colored output
- `test_error_recovery`: Tests error diagnosis, pattern matching, solution generation
- `test_tool_executor`: Tests tool discovery (found 11 tools on dev machine), registry, execution with git

**Adding Tests**: Each new component should have a corresponding `test_<component>.c` file that asserts core functionality. Use the existing tests as templates.

## Dependencies

### Required (Bundled)
- **cJSON** (`external/cJSON/`): JSON parsing for cache and IPC
- **tomlc99** (`external/tomlc99/`): TOML config file parsing
- **llama.cpp** (`external/llama.cpp/`): Local LLM inference (submodule)

### Optional (System)
- **CURL**: For cloud API fallback (detected via CMake find_package)
- **Sanitizers**: AddressSanitizer/UndefinedBehaviorSanitizer for debugging

### Tool Discovery (Runtime)
CyxMake discovers these at runtime via PATH scanning:
- **Build Systems**: cmake, make, ninja, msbuild, cargo, npm
- **Compilers**: gcc, g++, clang, clang++, cl (MSVC), rustc
- **Package Managers**: apt, yum, dnf, pacman, brew, vcpkg, conan, npm, yarn, pip, cargo, choco, winget

## Key Files and Their Purpose

### Core Headers (`include/cyxmake/`)
- `cyxmake.h`: Main API header, version defines
- `orchestrator.h`: Core orchestration interface
- `project_context.h`: Project semantic representation
- `llm_interface.h`: LLM abstraction (local + cloud)
- `error_recovery.h`: Error diagnosis and recovery
- `tool_executor.h`: **Tool discovery and execution system**
- `logger.h`: Logging API
- `compat.h`: Cross-platform compatibility macros

### Implementation (`src/`)
Organized by subsystem:
- `core/`: Orchestrator, logger, build executor, config
- `context/`: Project analyzer, cache manager, change detector
- `llm/`: LLM interface, local/cloud implementations, error analyzer
- `recovery/`: Error diagnosis, patterns, solution generator, fix executor
- `tools/`: **Tool registry, discovery, executor** ← Phase 1 just completed
- `cli/`: Command-line interface and argument parsing

### Configuration Files
- `.cyxmake/cache.json`: Per-project context cache (auto-generated)
- `cyxmake.toml`: User configuration (LLM settings, execution options)

### Documentation
- `architecture_claude.md`: Comprehensive technical architecture
- `core_components_design.md`: Detailed design docs for subsystems
- `tool_interface_design.md`: Tool system specification
- `ai.md`, `llm.md`, `quantization_strategy.md`: LLM integration details

## Common Workflows

### Debugging a Build Failure

1. **Run with debug logging**:
   ```bash
   export CYXMAKE_LOG_LEVEL=debug
   cyxmake build
   ```

2. **Check cache**:
   ```bash
   cat .cyxmake/cache.json | jq .
   ```

3. **Run error recovery standalone**:
   ```bash
   ./build/bin/test_error_recovery
   ```

4. **Validate tool discovery**:
   ```bash
   ./build/bin/test_tool_executor
   # Should list all discovered tools
   ```

### Adding Cross-Platform Support

When adding OS-specific code:

1. **Check existing patterns** in `include/cyxmake/compat.h`
2. **Use conditional compilation**:
   ```c
   #ifdef _WIN32
       // Windows implementation
   #else
       // POSIX implementation
   #endif
   ```
3. **Test on multiple platforms** using CI or Docker
4. **Document platform-specific behavior** in code comments

### Debugging Tool Execution Issues

Common issues:
- **Tool not found**: Check PATH, verify `tool_find_in_path()` works
- **Path with spaces**: Ensure paths are quoted in `build_command_string()`
- **getcwd/chdir errors**: Windows needs `_getcwd`/`_chdir` (use compat.h macros)
- **Double-free**: Check `tool_exec_options_free()` usage (don't manually free args after)

## Phase 1 Milestone Complete

The **Tool Executor System** is now fully implemented and tested:

✅ Tool registry with dynamic storage and capability-based lookup
✅ Cross-platform tool discovery (Windows/Linux/macOS)
✅ Tool execution with output capture and error handling
✅ Package manager integration with priority-based selection
✅ Comprehensive test suite (5 test categories, all passing)
✅ Support for 13+ package managers, compilers, build systems, VCS, linters, formatters

**Next Phase**: Integrate tool system with error recovery to enable autonomous dependency installation and build configuration.

## REPL Interactive Mode

The **Interactive REPL** is now fully implemented (All 6 phases complete):

✅ Basic REPL loop with colored prompt
✅ Slash commands with aliases (`/build`, `/b`, `/help`, `/h`, etc.)
✅ Natural language command parsing
✅ Permission system with Y/N prompts
✅ Conversation context tracking
✅ Arrow key history navigation (up/down)
✅ Tab completion for slash commands and file paths
✅ Line editing (cursor movement, backspace, delete, home, end)
✅ Cross-platform input handling (Windows Console API / POSIX termios)
✅ Multi-step action planning with approval (Y/Step-by-step/No)
✅ Rollback support for reversible actions

**REPL Complete!** All 6 phases implemented. See `repl.md` for details.

## Notes for Future Claude Instances

- **Phase 0 (Foundation)**: Complete - basic structure, logging, project analysis
- **Phase 1 (Tool Integration)**: Complete - tool discovery, registry, execution, package managers
- **Phase 2 (Error Recovery Integration)**: In Progress - connect tools to error recovery system
- **REPL System**: Complete - all 6 phases implemented (input, commands, permissions, context, enhanced terminal, action planning)
- **LLM Integration**: Partial - interface exists, needs model loading and inference work
- **Code Quality**: Memory-safe C with comprehensive error handling, cross-platform tested
- **Testing**: Each component has dedicated test suite, run before committing

**Architecture Philosophy**: Tool-centric, modular design. LLM is an orchestrator that delegates to specialized tools written in C. Tools are self-contained executables with standardized JSON I/O. This enables incremental development and easy extensibility.
