# CyxMake - Complete System Guide

> **AI-Powered Build Automation System**
>
> Build any project, on any platform, without learning domain-specific languages.

---

## Table of Contents

1. [Overview](#overview)
2. [System Architecture](#system-architecture)
3. [Getting Started](#getting-started)
4. [Building CyxMake](#building-cyxmake)
5. [Using the REPL](#using-the-repl)
6. [Natural Language Commands](#natural-language-commands)
7. [AI Agent System](#ai-agent-system)
8. [Tool Discovery & Execution](#tool-discovery--execution)
9. [Error Recovery System](#error-recovery-system)
10. [Configuration](#configuration)
11. [For Developers](#for-developers)

---

## Overview

### What is CyxMake?

CyxMake is an intelligent build system that understands plain English. Instead of writing complex CMake, Makefile, or package.json configurations, you simply tell CyxMake what you want:

```
cyxmake> build the project
cyxmake> install SDL2
cyxmake> read the error log and fix it
cyxmake> clean up and rebuild
```

### Core Features (Current Implementation)

| Feature | Status | Description |
|---------|--------|-------------|
| Interactive REPL | ✅ Complete | Command-line interface with natural language |
| Natural Language Parsing | ✅ Complete | Understands build, read, clean, install, etc. |
| Slash Commands | ✅ Complete | /build, /clean, /help, /ai, etc. |
| Permission System | ✅ Complete | Asks before destructive operations |
| Conversation Context | ✅ Complete | Remembers files, errors, and history |
| Tool Discovery | ✅ Complete | Finds compilers, build tools, package managers |
| Tool Execution | ✅ Complete | Runs discovered tools safely |
| Error Recovery | ✅ Complete | Diagnoses and suggests fixes for build errors |
| AI Agent System | ✅ Complete | LLM-powered action execution |
| LLM Integration | 🔄 Partial | llama.cpp interface ready, needs model |

### What CyxMake Can Do Now

1. **Analyze any project** - Detects language, build system, dependencies
2. **Execute builds** - CMake, Make, Cargo, npm, etc.
3. **Read/Create/Delete files** - With permission checks
4. **Install packages** - Using the best available package manager
5. **Diagnose errors** - Pattern matching + AI analysis
6. **Remember context** - Current file, last error, conversation history
7. **AI-powered assistance** - When LLM is loaded, handles complex requests

---

## System Architecture

### High-Level Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     User Interface                           │
│  ┌─────────────────────────────────────────────────────┐    │
│  │                    REPL (repl.c)                     │    │
│  │  • Natural language input                            │    │
│  │  • Slash commands (/build, /help, etc.)             │    │
│  │  • Colored output                                    │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   Command Processing                         │
│  ┌──────────────────┐  ┌──────────────────────────────┐    │
│  │ Slash Commands   │  │  Natural Language Parser     │    │
│  │ (slash_commands.c)│  │  (prompt_templates.c)        │    │
│  │                  │  │                              │    │
│  │ /build /clean    │  │  "build" → INTENT_BUILD     │    │
│  │ /help /ai /exit  │  │  "read X" → INTENT_READ_FILE│    │
│  └──────────────────┘  └──────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Core Systems                              │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐   │
│  │ Permission  │ │ Conversation│ │    AI Agent         │   │
│  │   System    │ │   Context   │ │    System           │   │
│  │             │ │             │ │                     │   │
│  │ • Ask user  │ │ • History   │ │ • Parse JSON        │   │
│  │ • Remember  │ │ • Files     │ │ • Execute actions   │   │
│  │   choices   │ │ • Errors    │ │ • Chain operations  │   │
│  └─────────────┘ └─────────────┘ └─────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   Execution Layer                            │
│  ┌─────────────────┐  ┌─────────────────┐  ┌────────────┐  │
│  │ Build Executor  │  │  Tool Executor  │  │ File Ops   │  │
│  │                 │  │                 │  │            │  │
│  │ • CMake builds  │  │ • Run any tool  │  │ • Read     │  │
│  │ • Make builds   │  │ • Capture output│  │ • Write    │  │
│  │ • npm/cargo     │  │ • Handle errors │  │ • Delete   │  │
│  └─────────────────┘  └─────────────────┘  └────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   Tool Discovery                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              Tool Registry (tool_registry.c)         │    │
│  │                                                      │    │
│  │  Compilers: gcc, g++, clang, cl (MSVC), rustc       │    │
│  │  Build Tools: cmake, make, ninja, msbuild, cargo    │    │
│  │  Package Managers: apt, brew, vcpkg, npm, pip, winget│   │
│  │  VCS: git, svn, hg                                  │    │
│  │  Linters: clang-tidy, cppcheck, eslint              │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

### Directory Structure

```
cyxmake/
├── include/cyxmake/          # Public headers
│   ├── cyxmake.h             # Main API, version info
│   ├── orchestrator.h        # Core orchestration
│   ├── repl.h                # REPL interface
│   ├── slash_commands.h      # Command definitions
│   ├── permission.h          # Permission system
│   ├── conversation_context.h # Context tracking
│   ├── prompt_templates.h    # AI prompts & parsing
│   ├── llm_interface.h       # LLM abstraction
│   ├── tool_executor.h       # Tool system
│   ├── build_executor.h      # Build execution
│   ├── file_ops.h            # File operations
│   ├── project_context.h     # Project analysis
│   ├── error_recovery.h      # Error diagnosis
│   └── logger.h              # Logging system
│
├── src/
│   ├── cli/                  # Command-line interface
│   │   ├── main.c            # Entry point
│   │   ├── repl.c            # REPL implementation
│   │   ├── slash_commands.c  # Command handlers
│   │   ├── permission.c      # Permission checks
│   │   └── conversation_context.c
│   │
│   ├── core/                 # Core functionality
│   │   ├── orchestrator.c    # Main coordinator
│   │   ├── build_executor.c  # Build execution
│   │   ├── file_ops.c        # File I/O
│   │   └── logger.c          # Logging
│   │
│   ├── llm/                  # AI/LLM integration
│   │   ├── llm_interface.c   # LLM abstraction
│   │   ├── prompt_templates.c # Prompts & AI agent
│   │   └── error_analyzer.c  # AI error analysis
│   │
│   ├── tools/                # Tool management
│   │   ├── tool_registry.c   # Tool storage
│   │   ├── tool_discovery.c  # Find tools on system
│   │   └── tool_executor.c   # Run tools
│   │
│   ├── recovery/             # Error recovery
│   │   ├── error_diagnosis.c # Categorize errors
│   │   ├── error_patterns.c  # Known error patterns
│   │   ├── solution_generator.c # Generate fixes
│   │   └── fix_executor.c    # Apply fixes
│   │
│   └── context/              # Project analysis
│       ├── project_context.c # Project state
│       ├── project_analyzer.c # Analyze projects
│       └── cache_manager.c   # Cache results
│
├── tests/                    # Test suite
│   ├── test_logger.c
│   ├── test_error_recovery.c
│   ├── test_tool_executor.c
│   └── test_ai_agent.c
│
├── external/                 # Dependencies
│   ├── cJSON/                # JSON parsing
│   ├── tomlc99/              # TOML config files
│   └── llama.cpp/            # Local LLM inference
│
└── build/                    # Build output (generated)
```

---

## Getting Started

### Prerequisites

**Windows:**
- Visual Studio 2019+ with C/C++ workload
- CMake 3.20+
- Git

**Linux/macOS:**
- GCC 9+ or Clang 10+
- CMake 3.20+
- Git

### Quick Start

```bash
# Clone the repository
git clone https://github.com/your-org/cyxmake.git
cd cyxmake

# Initialize submodules (llama.cpp)
git submodule update --init --recursive

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .

# Run
./bin/Debug/cyxmake.exe    # Windows
./bin/cyxmake              # Linux/macOS
```

---

## Building CyxMake

### Build Options

```bash
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \      # Debug or Release
  -DCYXMAKE_BUILD_TESTS=ON \      # Build test suite
  -DCYXMAKE_BUILD_TOOLS=ON \      # Build bundled tools
  -DCYXMAKE_USE_SANITIZERS=OFF    # Address sanitizer
```

### Build Commands

```bash
# Full build
cmake --build .

# Specific targets
cmake --build . --target cyxmake        # Main executable
cmake --build . --target cyxmake_core   # Core library
cmake --build . --target test_ai_agent  # Specific test

# Release build
cmake --build . --config Release
```

### Running Tests

```bash
# All tests
ctest --output-on-failure

# Individual tests (from bin/Debug directory)
./test_logger.exe
./test_error_recovery.exe
./test_tool_executor.exe
./test_ai_agent.exe
```

---

## Using the REPL

### Starting CyxMake

```bash
# From any project directory
cd /path/to/your/project
cyxmake
```

### Welcome Screen

```
+--------------------------------------------------------------+
|  CyxMake v0.1.0 - AI Build Assistant                         |
|  Type naturally or /help for commands                        |
+--------------------------------------------------------------+

cyxmake>
```

### Slash Commands

| Command | Alias | Description |
|---------|-------|-------------|
| `/help` | `/h` | Show available commands |
| `/exit` | `/q` | Exit CyxMake |
| `/clear` | `/cls` | Clear the screen |
| `/init` | `/i` | Analyze current project |
| `/build` | `/b` | Build the project |
| `/clean` | `/c` | Clean build artifacts |
| `/status` | `/s` | Show project status |
| `/config` | `/cfg` | Show configuration |
| `/history` | `/hist` | Show command history |
| `/version` | `/v` | Show version info |
| `/context` | `/ctx` | Show conversation context |
| `/ai` | - | AI status and commands |

### AI Commands

```bash
/ai              # Show AI status
/ai load <path>  # Load an LLM model
/ai unload       # Unload the model
/ai test         # Test AI with a simple prompt
```

---

## Natural Language Commands

CyxMake understands plain English commands. Here's what you can say:

### Build Commands

```
build                      # Build the project
build the project          # Same as above
compile everything         # Triggers build
make                       # Triggers build
```

### File Operations

```
read main.c                # Display file contents
show me README.md          # Display file contents
open config.json           # Display file contents
view the log file          # If context knows which file

create hello.c             # Create new file (with template)
create test.py             # Creates Python template
```

### Clean Operations

```
clean                      # Remove build directory
clean the project          # Same as above
clear build files          # Same as above
remove build directory     # Same as above
```

### Package Installation

```
install SDL2               # Install using best package manager
install openssl            # Auto-selects apt/brew/vcpkg/etc.
add dependency boost       # Same as install
```

### Status & Help

```
status                     # Show project status
help                       # Show help
what can you do?           # Triggers help
```

### AI-Powered (when model loaded)

```
can you read README.md and follow the build instructions?
delete the build folder and rebuild
explain this error to me
fix the undefined reference error
```

### Intent Detection

CyxMake parses your input and detects intent:

```
cyxmake> read main.c
* Detected: READ FILE (80% confidence)
  Target: main.c

File: main.c
----------------------------------------
   1 | #include <stdio.h>
   2 |
   3 | int main() {
...
```

---

## AI Agent System

### How It Works

When you load an LLM model, CyxMake gains powerful AI capabilities:

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   User Input    │ ──▶ │   AI Agent      │ ──▶ │   Actions       │
│                 │     │                 │     │                 │
│ "read README    │     │ Generates JSON  │     │ read_file       │
│  and build"     │     │ with actions    │     │ build           │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

### Available AI Actions

| Action | Description | Needs Confirmation |
|--------|-------------|-------------------|
| `read_file` | Read and display a file | No |
| `create_file` | Create a file with content | Yes |
| `delete_file` | Delete a file | Yes |
| `delete_dir` | Delete a directory | Yes |
| `build` | Build the project | No |
| `clean` | Clean build artifacts | Yes |
| `install` | Install a package | Yes |
| `run_command` | Execute a shell command | Yes |
| `list_files` | List directory contents | No |

### Example AI Interaction

```
cyxmake> can you read the README and then build the project?

AI: I'll read the README.md file and then build the project for you.

* Executing: Read file - README.md
  User wants to understand the project

File: README.md
----------------------------------------
   1 | # My Project
...

* Executing: Build project - build
  Following README instructions

Building project in build...
[OK] Build completed successfully
```

### Configuring AI Models

1. **Download a GGUF model** (e.g., from HuggingFace):
   - Recommended: `llama-2-7b-chat.Q4_K_M.gguf` (4-bit quantized, ~4GB)
   - Smaller: `phi-2.Q4_K_M.gguf` (~1.5GB)

2. **Load the model:**
   ```
   cyxmake> /ai load /path/to/model.gguf
   ```

3. **Test it:**
   ```
   cyxmake> /ai test
   ```

### Model Requirements

| Model Size | RAM Required | Quality |
|------------|--------------|---------|
| 3B params (Q4) | ~2GB | Good for simple tasks |
| 7B params (Q4) | ~4GB | Recommended |
| 13B params (Q4) | ~8GB | Best quality |

---

## Tool Discovery & Execution

### Automatic Tool Discovery

CyxMake automatically finds tools on your system:

```
cyxmake> /status

Discovered Tools:
  Compilers:
    • gcc 11.4.0 at /usr/bin/gcc
    • g++ 11.4.0 at /usr/bin/g++
    • clang 14.0.0 at /usr/bin/clang

  Build Systems:
    • cmake 3.28.0 at /usr/bin/cmake
    • make 4.3 at /usr/bin/make
    • ninja 1.11.1 at /usr/bin/ninja

  Package Managers:
    • apt at /usr/bin/apt (default)
    • pip at /usr/bin/pip3
    • npm at /usr/bin/npm
```

### Supported Tools

**Compilers:**
- C/C++: gcc, g++, clang, clang++, cl (MSVC)
- Rust: rustc
- Go: go

**Build Systems:**
- CMake, Make, Ninja
- MSBuild (Windows)
- Cargo (Rust)
- npm, yarn (Node.js)

**Package Managers:**
- Linux: apt, yum, dnf, pacman, zypper
- macOS: brew, port
- Windows: winget, choco, scoop
- Cross-platform: vcpkg, conan
- Language-specific: pip, npm, yarn, cargo

**Version Control:**
- git, svn, hg

**Linters/Formatters:**
- clang-tidy, cppcheck
- eslint, prettier

### Package Manager Priority

CyxMake selects the best package manager automatically:

| Platform | Priority Order |
|----------|---------------|
| Windows | vcpkg > winget > choco > scoop |
| macOS | brew > vcpkg > port |
| Linux (Debian) | apt > vcpkg |
| Linux (Fedora) | dnf > vcpkg |
| Linux (Arch) | pacman > vcpkg |

---

## Error Recovery System

### How It Works

```
Build fails → Error captured → Pattern matching → Solution generated → Fix applied
```

### Error Categories

| Category | Examples |
|----------|----------|
| Missing Dependency | `cannot find -lSDL2`, `package not found` |
| Syntax Error | `expected ';'`, `unexpected token` |
| Linker Error | `undefined reference to 'foo'` |
| Missing Header | `fatal error: SDL.h: No such file` |
| Type Error | `cannot convert 'int' to 'string'` |
| Permission Error | `access denied`, `permission denied` |

### Automatic Fix Suggestions

```
cyxmake> build

[ERROR] Build failed!

Error: cannot find -lSDL2

Diagnosis: Missing library dependency
Category: MISSING_DEPENDENCY

Suggested fixes (in order of priority):
  1. Install SDL2 using apt:
     sudo apt install libsdl2-dev

  2. Install SDL2 using vcpkg:
     vcpkg install sdl2

  3. Download from: https://libsdl.org

Would you like to try fix #1? [Y/n]
```

---

## Configuration

### Configuration File

CyxMake looks for `cyxmake.toml` in your project root:

```toml
[project]
name = "my-project"
version = "1.0.0"
language = "c++"

[build]
system = "cmake"
build_dir = "build"
type = "Debug"

[llm]
model_path = "/path/to/model.gguf"
use_gpu = false
context_size = 4096

[permissions]
auto_approve_read = true
auto_approve_build = true
always_confirm_delete = true
```

### Environment Variables

| Variable | Description |
|----------|-------------|
| `CYXMAKE_LOG_LEVEL` | Set log level (debug, info, warning, error) |
| `CYXMAKE_MODEL_PATH` | Default LLM model path |
| `CYXMAKE_NO_COLOR` | Disable colored output |

---

## For Developers

### Adding New Commands

1. **Add to `slash_commands.h`:**
   ```c
   bool cmd_mycommand(ReplSession* session, const char* args);
   ```

2. **Implement in `slash_commands.c`:**
   ```c
   bool cmd_mycommand(ReplSession* session, const char* args) {
       printf("My command executed!\n");
       return true;  // Continue REPL
   }
   ```

3. **Register in `execute_slash_command()`:**
   ```c
   else if (strcmp(cmd, "mycommand") == 0 || strcmp(cmd, "mc") == 0) {
       return cmd_mycommand(session, args);
   }
   ```

### Adding New Intent Detection

In `prompt_templates.c`, add to `parse_command_local()`:

```c
/* My new intent */
else if (contains_word(input, "deploy") ||
         contains_word(input, "publish")) {
    cmd->intent = INTENT_DEPLOY;  // Add to enum first
    cmd->confidence = 0.85;
}
```

### Adding New AI Actions

1. **Add to enum in `prompt_templates.h`:**
   ```c
   typedef enum {
       // ...existing...
       AI_ACTION_DEPLOY,
   } AIActionType;
   ```

2. **Update `parse_action_type()` in `prompt_templates.c`:**
   ```c
   if (strcmp(action_str, "deploy") == 0) return AI_ACTION_DEPLOY;
   ```

3. **Handle in `execute_single_ai_action()` in `repl.c`:**
   ```c
   case AI_ACTION_DEPLOY:
       // Implementation
       break;
   ```

4. **Update the prompt in `prompt_ai_agent()`:**
   ```c
   "- deploy: Deploy the application\n"
   ```

### Memory Management Rules

- Every `malloc/calloc` must have a corresponding `free`
- Use `*_free()` functions for structs (e.g., `ai_agent_response_free()`)
- Set pointers to `NULL` after freeing
- Never free strings owned by other structs (use `strdup` if needed)

### Running Tests

```bash
# Build and run all tests
cd build
cmake --build . --config Debug
ctest --output-on-failure

# Run specific test with verbose output
./bin/Debug/test_ai_agent.exe
```

### Code Style

- C99 standard
- 4-space indentation
- `snake_case` for functions and variables
- `UPPER_CASE` for constants and macros
- Comments in `/* */` style for multi-line, `//` for single-line

---

## Roadmap

### Current Phase: Phase 4 Complete

- [x] Phase 0: Foundation (logging, file ops, project analysis)
- [x] Phase 1: Tool System (discovery, registry, execution)
- [x] Phase 2: Error Recovery (diagnosis, patterns, solutions)
- [x] Phase 3: REPL & Commands (interactive shell, slash commands)
- [x] Phase 4: AI Integration (prompts, parsing, agent system)

### Upcoming

- [ ] Phase 5: Full LLM Integration (model loading, inference)
- [ ] Phase 6: Advanced Features
  - Interactive error fixing
  - Project generation from description
  - Multi-language project support
  - CI/CD integration
  - Plugin system

---

## Troubleshooting

### Common Issues

**"I didn't understand that"**
- The AI model isn't loaded. Use `/ai load <path>` first.
- Or try a more specific command like `/build` instead of "build it".

**"Permission denied"**
- CyxMake asks before destructive operations.
- Use `/permissions reset` to reset saved choices.

**"Tool not found"**
- The required tool isn't installed or not in PATH.
- Check `/status` to see discovered tools.

**"Build failed"**
- Check the error output for specific issues.
- Use "fix the error" or "explain this error" with AI loaded.

### Getting Help

- `/help` - Show all commands
- `/ai test` - Test AI functionality
- `/status` - Show system status
- Check logs in `.cyxmake/logs/`

---

## License

Apache 2.0 - See LICENSE file.

---

*Built with ❤️ for developers who want to build, not configure.*
