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
8. [Multi-Provider AI System](#multi-provider-ai-system)
9. [Tool Discovery & Execution](#tool-discovery--execution)
10. [Error Recovery System](#error-recovery-system)
11. [Configuration](#configuration)
12. [For Developers](#for-developers)

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
| Multi-Provider AI | ✅ Complete | OpenAI, Gemini, Anthropic, Ollama, etc. |
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

## Multi-Provider AI System

CyxMake supports multiple AI providers, allowing you to use cloud-hosted models (OpenAI, Gemini, Anthropic, etc.) or local models interchangeably.

### Supported Providers

| Provider | Type | API Key Required | Description |
|----------|------|------------------|-------------|
| **OpenAI** | Cloud | Yes | GPT-4o, GPT-4o-mini, GPT-3.5-turbo |
| **Anthropic** | Cloud | Yes | Claude 3.5 Sonnet, Claude 3 Opus/Haiku |
| **Google Gemini** | Cloud | Yes | Gemini 1.5 Pro, Gemini 1.5 Flash |
| **Ollama** | Local | No | Local models via Ollama server |
| **xAI Grok** | Cloud | Yes | Grok models (OpenAI-compatible) |
| **OpenRouter** | Cloud | Yes | Access to 100+ models via one API |
| **Groq** | Cloud | Yes | Fast inference for Llama models |
| **Together AI** | Cloud | Yes | Open-source models hosted |
| **llama.cpp** | Local | No | Direct GGUF model loading |
| **Custom** | Any | Optional | Any OpenAI-compatible server |

### Quick Setup

**1. Copy the example configuration:**
```bash
cp cyxmake.example.toml cyxmake.toml
```

**2. Configure your providers in `cyxmake.toml`:**
```toml
[ai]
default_provider = "ollama"    # Provider to use by default
fallback_provider = "openai"   # Backup if default fails
timeout = 60                   # Request timeout (seconds)
max_tokens = 1024              # Max response tokens
temperature = 0.7              # Generation temperature

# Ollama (Local) - No API key needed!
[ai.providers.ollama]
enabled = true
type = "ollama"
base_url = "http://localhost:11434"
model = "llama2"

# OpenAI
[ai.providers.openai]
enabled = true
type = "openai"
api_key = "${OPENAI_API_KEY}"  # Uses environment variable
base_url = "https://api.openai.com/v1"
model = "gpt-4o-mini"

# Google Gemini
[ai.providers.gemini]
enabled = false
type = "gemini"
api_key = "${GEMINI_API_KEY}"
base_url = "https://generativelanguage.googleapis.com/v1beta"
model = "gemini-1.5-flash"

# Anthropic Claude
[ai.providers.anthropic]
enabled = false
type = "anthropic"
api_key = "${ANTHROPIC_API_KEY}"
base_url = "https://api.anthropic.com/v1"
model = "claude-3-haiku-20240307"
```

**3. Set environment variables for API keys:**
```bash
# Linux/macOS
export OPENAI_API_KEY="sk-..."
export ANTHROPIC_API_KEY="sk-ant-..."
export GEMINI_API_KEY="..."

# Windows (PowerShell)
$env:OPENAI_API_KEY = "sk-..."

# Windows (CMD)
set OPENAI_API_KEY=sk-...
```

### Using AI Providers

**Check AI Status:**
```
cyxmake> /ai

AI Status

Cloud Provider: ollama (ollama)
  Model: llama2
  Status: ready

Configured providers: 2

Commands:
  /ai providers     - List available providers
  /ai use <name>    - Switch to a provider
  /ai test          - Test current AI
  /ai load [path]   - Load local GGUF model
  /ai unload        - Unload AI
```

**List Available Providers:**
```
cyxmake> /ai providers

AI Providers

  * ollama (ollama) - ready
      Model: llama2
    openai (openai) - ready
      Model: gpt-4o-mini
```

**Switch Providers at Runtime:**
```
cyxmake> /ai use openai
[OK] Switched to provider: openai
  Model: gpt-4o-mini

cyxmake> /ai use ollama
[OK] Switched to provider: ollama
  Model: llama2
```

**Test Current Provider:**
```
cyxmake> /ai test
Testing AI...
[OK] AI response: Hello! AI is working.
```

### Provider-Specific Configuration

**OpenRouter (Access 100+ Models):**
```toml
[ai.providers.openrouter]
enabled = true
type = "openai"  # Uses OpenAI-compatible API
api_key = "${OPENROUTER_API_KEY}"
base_url = "https://openrouter.ai/api/v1"
model = "anthropic/claude-3.5-sonnet"

# Optional headers for rankings
[ai.providers.openrouter.headers]
"HTTP-Referer" = "https://cyxmake.dev"
"X-Title" = "CyxMake"
```

**Groq (Fast Llama Inference):**
```toml
[ai.providers.groq]
enabled = true
type = "openai"
api_key = "${GROQ_API_KEY}"
base_url = "https://api.groq.com/openai/v1"
model = "llama-3.1-70b-versatile"
```

**Local llama.cpp (Direct GGUF):**
```toml
[ai.providers.local]
enabled = true
type = "llamacpp"
model_path = "/path/to/model.gguf"
context_size = 4096
gpu_layers = 0    # 0 = CPU only, set higher for GPU
threads = 4
```

**Custom OpenAI-Compatible Server (LM Studio, vLLM, text-generation-webui):**
```toml
[ai.providers.custom]
enabled = true
type = "custom"
api_key = "not-needed"  # Most local servers don't need this
base_url = "http://localhost:1234/v1"  # IMPORTANT: Must include /v1
model = "local-model"
```

**LM Studio Example:**
```toml
[ai.providers.lmstudio]
name = "lmstudio"
enabled = true
type = "custom"
api_key = "not-needed"
base_url = "http://192.168.1.116:1234/v1"  # Your LM Studio server IP
model = "openai/gpt-oss-20b"               # Model loaded in LM Studio
```

> **Important: The `/v1` suffix is required!**
>
> CyxMake appends `/chat/completions` to the base URL when making requests.
> - Without `/v1`: `http://localhost:1234/chat/completions` ❌ (404 error)
> - With `/v1`: `http://localhost:1234/v1/chat/completions` ✅ (correct)
>
> Most OpenAI-compatible servers (LM Studio, vLLM, Ollama's OpenAI mode) expect
> the full path `/v1/chat/completions`, so always include `/v1` in your base_url.

### Hybrid Usage (Cloud + Local)

CyxMake can use both cloud providers and local models:

```
# Use cloud provider
cyxmake> /ai use openai
cyxmake> explain this complex error in detail

# Switch to local for privacy-sensitive work
cyxmake> /ai use ollama
cyxmake> analyze my source code

# Load a local GGUF model directly
cyxmake> /ai load ~/.cyxmake/models/codellama-7b-q4.gguf
```

### Prerequisites for HTTP Providers

Cloud providers require **libcurl** for HTTP requests. If curl is not available:

```bash
# Ubuntu/Debian
sudo apt install libcurl4-openssl-dev

# macOS (usually pre-installed)
brew install curl

# Windows (vcpkg)
vcpkg install curl:x64-windows
```

Then rebuild CyxMake:
```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

Without curl, only the local llama.cpp provider will work.

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
