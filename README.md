# CyxMake

> **AI-Powered Build Automation System**
>
> Build any project, on any platform, without learning domain-specific languages or debugging cryptic errors.

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()
[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://github.com/CYXWIZ-Lab/cyxmake/releases/tag/v1.0.0)
[![Sponsor](https://img.shields.io/badge/sponsor-GitHub%20Sponsors-ea4aaa.svg)](https://github.com/sponsors/cyxwiz-lab)
[![Support on Patreon](https://img.shields.io/badge/support-Patreon-orange.svg)](https://www.patreon.com/posts/cyxwiz-projects-146571889)

## Download

| Platform | Download | Size |
|----------|----------|------|
| **Windows x64** | [cyxmake-v1.0.0-windows-x64.zip](https://github.com/CYXWIZ-Lab/cyxmake/releases/download/v1.0.0/cyxmake-v1.0.0-windows-x64.zip) | ~1.4 MB |
| **Linux x64** | [cyxmake-v1.0.0-linux-x64.tar.gz](https://github.com/CYXWIZ-Lab/cyxmake/releases/download/v1.0.0/cyxmake-v1.0.0-linux-x64.tar.gz) | ~1.2 MB |
| **macOS ARM64** | [cyxmake-v1.0.0-macos-arm64.tar.gz](https://github.com/CYXWIZ-Lab/cyxmake/releases/download/v1.0.0/cyxmake-v1.0.0-macos-arm64.tar.gz) | ~1.1 MB |

[View all releases](https://github.com/CYXWIZ-Lab/cyxmake/releases)

---

## The Problem

Traditional build systems are **frustrating**:

```bash
$ git clone https://github.com/some-project/awesome-app.git
$ cd awesome-app
$ cmake ..
Error: Could not find SDL2

# [30 minutes of Googling...]

$ sudo apt install libsdl2-dev
$ cmake ..
Error: Boost 1.75 required but 1.71 found

# [Another hour of debugging...]
# Still not building...
```

**This is broken.**

---

## The Solution

**CyxMake uses AI to build projects autonomously:**

```bash
$ cyxmake init
✓ Detected: C++ project with CMake
✓ Dependencies: SDL2, Boost, OpenGL
✓ Build system: CMake 3.20+

$ cyxmake build
[1/5] Checking dependencies...
  ⚠ SDL2 not found
  → Installing libsdl2-dev via apt... ✓
[2/5] Configuring CMake... ✓
[3/5] Building project... ✓
[4/5] Running tests... ✓
[5/5] Verifying... ✓

Build successful! (2m 34s)
```

**No manual debugging. No Googling. Just works.**

---

## Key Features

### Natural Language Interface

Talk to your build system in plain English:

```bash
cyxmake> build the project
cyxmake> read the error log and fix it
cyxmake> install SDL2
cyxmake> @ai explain why this is failing
```

### Multi-Provider AI Support

Connect to any AI provider - local or cloud:

| Provider | Type | Description |
|----------|------|-------------|
| **Ollama** | Local | Run models locally, no API key needed |
| **LM Studio** | Local | Use any GGUF model via OpenAI-compatible API |
| **OpenAI** | Cloud | GPT-4o, GPT-4o-mini |
| **Anthropic** | Cloud | Claude 3.5 Sonnet, Claude 3 Opus |
| **Google Gemini** | Cloud | Gemini 1.5 Pro/Flash |
| **OpenRouter** | Cloud | Access 100+ models via one API |
| **Groq** | Cloud | Ultra-fast Llama inference |

### Intelligent Command Routing

CyxMake knows when to handle commands locally vs. when to ask the AI:

```bash
cyxmake> build              # Simple - handled locally (90% confidence)
cyxmake> read readme and follow the instructions to build
                            # Complex - routed to AI (45% confidence)
cyxmake> @ai what's wrong?  # Explicit AI routing
```

### AI-Powered Autonomous Build

Let AI handle everything - just run one command:

```bash
$ cyxmake auto
=== AI Build Agent Starting ===
Project: ./my-project
Using AI provider: lmstudio

AI analyzing project and creating build plan...
=== Build Plan ===
Summary: Build C++ project using CMake
Steps: 2
  [  ] 1. [Configure] Configure CMake project
  [  ] 2. [Build] Build the project

[Configure] Configure CMake project
   ✓ Step completed successfully
[Build] Build the project
   ✓ Step completed successfully

=== Build Successful! ===
```

When errors occur, the AI automatically:
1. **Analyzes** the error output
2. **Creates** a fix plan
3. **Executes** the fixes
4. **Retries** until success (up to 5 attempts)

### Natural Language Project Creation

Generate complete project scaffolds from plain English:

```bash
# Create a Python web API
$ cyxmake create "python web api called my_api"
✓ Detected: Python, Web project
✓ Project name: my_api
✓ Created pyproject.toml
✓ Created src/main.py
✓ Created README.md, .gitignore

# Create a C++ game with dependencies
$ cyxmake create "C++ game with SDL2 and OpenGL called space_shooter" ./games
✓ Detected: C++, Game project
✓ Dependencies: SDL2, OpenGL
✓ Created CMakeLists.txt with find_package()
✓ Created src/main.cpp with game boilerplate

# Create a Rust CLI tool
$ cyxmake create "rust cli tool named mycli"
✓ Created Cargo.toml
✓ Created src/main.rs

# Or use the REPL
cyxmake> /create go rest server
cyxmake> /create typescript node api
```

**Supported Languages**: C, C++, Rust, Python, JavaScript, TypeScript, Go, Java

**Supported Project Types**: Executable, Library, Game, CLI, Web, GUI

### Multi-Agent System

Spawn named agents to handle complex tasks in parallel:

```bash
cyxmake> /agent spawn builder build
✓ Created agent 'builder' (type: build, state: idle)

cyxmake> /agent spawn analyzer smart --mock
✓ Created agent 'analyzer' (type: smart, state: idle) [MOCK MODE]

cyxmake> /agent set analyzer temperature 0.8
✓ Agent 'analyzer' setting 'temperature' changed: 0.70 -> 0.8

cyxmake> /agent get analyzer
Agent Settings: analyzer
  timeout        300 seconds
  temperature    0.80
  max_tokens     2048
  verbose        false
  mock           true

cyxmake> /agent assign builder "Build with Release config"
✓ Task assigned to 'builder': Build with Release config
Agent executing task asynchronously...

cyxmake> /agent list
Active Agents
  * builder      build    running    Build with Release config
  * analyzer     smart    idle

cyxmake> /agent wait builder
Waiting for agent 'builder' to complete...
✓ Agent 'builder' finished (state: idle)
```

**Agent Types:**
- **smart** - Intelligent reasoning agent with chain-of-thought
- **build** - Specialized build agent with error recovery
- **auto** - Autonomous tool-using agent

**Agent Commands:**
| Command | Description |
|---------|-------------|
| `/agent spawn <name> <type>` | Create new agent |
| `/agent spawn <name> <type> --mock` | Create in mock mode (no AI needed) |
| `/agent list` | List all agents |
| `/agent assign <name> <task>` | Assign task to agent |
| `/agent status <name>` | Show agent status |
| `/agent get <name> [key]` | Show agent settings |
| `/agent set <name> <key> <val>` | Configure agent |
| `/agent wait <name>` | Wait for agent to complete |
| `/agent terminate <name>` | Stop an agent |
| `/agent send <from> <to> <msg>` | Send message between agents |
| `/agent inbox <name>` | View agent's received messages |
| `/agent broadcast <from> <msg>` | Broadcast message to all agents |

**Shared Memory (Multi-Agent State):**

Agents share a persistent key-value store for coordination. State is **automatically updated** during task execution:

```bash
cyxmake> /agent spawn worker smart --mock
✓ Created agent 'worker' (type: smart)

cyxmake> /agent assign worker "Build the project"
✓ Task assigned to 'worker': Build the project

cyxmake> /memory state
Shared State: (3 entries)
  worker.status = completed
  worker.task = Build the project
  worker.result = [MOCK RESULT] Agent 'worker' completed task...
```

**Auto-Updated Keys** (set automatically during task lifecycle):

| Key | Description |
|-----|-------------|
| `<agent>.status` | `running` → `completed` or `failed` |
| `<agent>.task` | Current task description |
| `<agent>.result` | Task result or error message |

**Manual Commands:**

| Command | Description |
|---------|-------------|
| `/memory state` | List all shared state entries |
| `/memory state get <key>` | Get value for key |
| `/memory state set <key> <value>` | Set key/value pair |
| `/memory state delete <key>` | Delete a key |
| `/memory state save` | Force save to disk |
| `/memory state clear` | Clear all entries |

State persists to `.cyxmake/agent_state.json` and loads automatically on startup.

**Message Passing:**

Agents can communicate via direct messages or broadcasts:

```bash
cyxmake> /agent spawn coordinator smart --mock
cyxmake> /agent spawn builder build --mock
cyxmake> /agent spawn tester smart --mock

# Send direct message
cyxmake> /agent send coordinator builder Start building the project
✓ Message sent: coordinator → builder

# Broadcast to all agents
cyxmake> /agent broadcast coordinator All agents please report status
✓ Broadcast sent from 'coordinator' to 2 agents

# Check inbox
cyxmake> /agent inbox builder
Messages for 'builder': (2 messages)
  [1] From: coordinator
      Start building the project
  [2] From: coordinator (broadcast)
      All agents please report status
```

**Conflict Resolution:**

When multiple agents try to access the same resource, conflicts are detected and resolved:

```bash
# Agent locks a resource
cyxmake> /agent lock builder CMakeLists.txt
✓ Agent 'builder' locked resource 'CMakeLists.txt'

# Another agent tries to lock the same resource
cyxmake> /agent lock fixer CMakeLists.txt
[!] Resource 'CMakeLists.txt' already locked by another agent

# View pending conflicts
cyxmake> /agent conflicts
=== Conflict History ===

Conflict 1: resource
  Agents: 'builder' vs 'fixer'
  Resource: CMakeLists.txt
  Status: UNRESOLVED

# Resolve the conflict interactively
cyxmake> /agent resolve

=== Conflict Detected ===

Type: resource
Resource: CMakeLists.txt

Agents:
  [1] builder: lock request
  [2] fixer: lock request

Choose resolution:
  1 - Let 'builder' proceed first
  2 - Let 'fixer' proceed first
  3 - Both proceed (sequential)
  4 - Cancel both

Choice [1-4]: 1
✓ Conflict resolved: agent1_wins

# Release resource when done
cyxmake> /agent unlock builder CMakeLists.txt
✓ Agent 'builder' released resource 'CMakeLists.txt'
```

**Conflict Resolution Commands:**

| Command | Description |
|---------|-------------|
| `/agent lock <name> <resource>` | Request exclusive access to resource |
| `/agent unlock <name> <resource>` | Release resource lock |
| `/agent conflicts` | List all conflicts |
| `/agent resolve` | Interactively resolve next conflict |

### Autonomous Error Recovery

When builds fail, CyxMake:
1. **Diagnoses** the error using pattern matching + AI
2. **Generates** fix strategies with priority ranking
3. **Suggests** solutions (install dependencies, fix configs)
4. **Learns** from your project context

### Universal Tool Discovery

Automatically finds and uses your installed tools:

- **Compilers**: gcc, g++, clang, MSVC (cl), rustc
- **Build Systems**: CMake, Make, Ninja, MSBuild, Cargo, npm
- **Package Managers**: apt, brew, vcpkg, winget, pip, npm, cargo
- **Version Control**: git, svn, hg
- **Linters**: clang-tidy, cppcheck, eslint

---

## Installation

### From Source (Recommended)

```bash
# Clone the repository
git clone https://github.com/CYXWIZ-Lab/cyxmake.git
cd cyxmake

# Initialize submodules
git submodule update --init --recursive

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .

# Run
./bin/cyxmake              # Linux/macOS
./bin/Release/cyxmake.exe  # Windows
```

### Optional: Install CURL for Cloud AI Providers

```bash
# Ubuntu/Debian
sudo apt install libcurl4-openssl-dev

# macOS
brew install curl

# Windows (vcpkg)
vcpkg install curl:x64-windows
```

---

## Quick Start

### 1. Launch CyxMake

```bash
cd your-project
cyxmake
```

### 2. Configure AI (Optional but Recommended)

Create `cyxmake.toml` in your project root or home directory (`~/.cyxmake/cyxmake.toml`):

```toml
[ai]
default_provider = "lmstudio"  # Change to your preferred provider
timeout = 120
max_tokens = 2048
temperature = 0.2

# ============================================
# OPTION 1: LM Studio (Recommended for Local)
# ============================================
# Easy to use with any GGUF model
[ai.providers.lmstudio]
enabled = true
type = "openai"
api_key = "not-needed"
base_url = "http://localhost:1234/v1"
model = "local-model"

# ============================================
# OPTION 2: Local llama.cpp (Direct Model)
# ============================================
# Uses bundled llama.cpp - no server needed
# Download models to ~/.cyxmake/models/
[ai.providers.local]
enabled = true
type = "llamacpp"
model_path = "~/.cyxmake/models/qwen2.5-coder-3b-q4_k_m.gguf"
context_size = 4096
gpu_layers = 0      # Set > 0 for GPU acceleration
threads = 6         # CPU threads to use

# ============================================
# OPTION 3: Ollama
# ============================================
[ai.providers.ollama]
enabled = true
type = "ollama"
base_url = "http://localhost:11434"
model = "llama2"

# ============================================
# OPTION 4: Cloud Providers
# ============================================
# OpenAI
[ai.providers.openai]
enabled = true
type = "openai"
api_key = "${OPENAI_API_KEY}"
base_url = "https://api.openai.com/v1"
model = "gpt-4o-mini"

# Anthropic Claude
[ai.providers.anthropic]
enabled = true
type = "anthropic"
api_key = "${ANTHROPIC_API_KEY}"
model = "claude-3-haiku-20240307"

# Google Gemini
[ai.providers.gemini]
enabled = true
type = "gemini"
api_key = "${GEMINI_API_KEY}"
model = "gemini-1.5-flash"

# OpenRouter (access 100+ models)
[ai.providers.openrouter]
enabled = true
type = "openai"
api_key = "${OPENROUTER_API_KEY}"
base_url = "https://openrouter.ai/api/v1"
model = "anthropic/claude-3-haiku"
```

### 3. Use CyxMake

```bash
cyxmake> /ai providers          # List available AI providers
cyxmake> /ai use ollama         # Switch to Ollama
cyxmake> /ai test               # Test AI connection

cyxmake> build                  # Build your project
cyxmake> read main.c            # View a file
cyxmake> install SDL2           # Install a dependency
cyxmake> @ai fix the errors     # Ask AI for help
```

---

## How It Works

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     User Interface (REPL)                    │
│  • Natural language input    • Slash commands (/build, etc.) │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   Command Processing                         │
│  ┌──────────────────────┐  ┌──────────────────────────────┐ │
│  │ Intent Detection     │  │ Confidence Scoring           │ │
│  │ (Local Parser)       │  │ (Route to AI if < 60%)       │ │
│  └──────────────────────┘  └──────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
      ┌───────────────┬───────┴───────┬───────────────┐
      ▼               ▼               ▼               ▼
┌───────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐
│  Local    │ │  AI Agent   │ │   Error     │ │ Multi-Agent │
│ Execution │ │  System     │ │  Recovery   │ │  System     │
│ (build,   │ │  (Complex   │ │  (Diagnosis,│ │  (Parallel  │
│  read)    │ │   queries)  │ │   fixes)    │ │   tasks)    │
└───────────┘ └─────────────┘ └─────────────┘ └─────────────┘
      │               │               │               │
      └───────────────┴───────┬───────┴───────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                 Tool Discovery & Execution                   │
│  Compilers | Build Systems | Package Managers | VCS          │
└─────────────────────────────────────────────────────────────┘
```

**CyxMake orchestrates existing tools. It doesn't replace them.**

### AI Provider Options

| Option | Pros | Cons |
|--------|------|------|
| **Ollama (Local)** | Free, private, no internet | Requires setup |
| **LM Studio** | Easy GUI, any GGUF model | Requires RAM |
| **OpenAI/Anthropic** | Best quality, easy setup | Costs money, needs internet |
| **OpenRouter** | Access to many models | Needs API key |

### Privacy

- **Local providers**: All processing happens on your machine
- **Cloud providers**: Only prompts and error messages sent (not source code)
- **Configurable**: Choose what provider to use per-query

---

## Commands Reference

### Slash Commands

| Command | Alias | Description |
|---------|-------|-------------|
| `/help` | `/h` | Show available commands |
| `/build` | `/b` | Build the project |
| `/clean` | `/c` | Clean build artifacts |
| `/init` | `/i` | Analyze project structure |
| `/status` | `/s` | Show project and tool status |
| `/ai` | - | AI status and provider management |
| `/ai providers` | - | List configured AI providers |
| `/ai use <name>` | - | Switch to a specific provider |
| `/ai test` | - | Test current AI connection |
| `/create` | - | Create project from natural language |
| `/agent` | `/a` | Multi-agent system management |
| `/agent list` | - | List all active agents |
| `/agent spawn <name> <type>` | - | Create new agent (smart/build/auto) |
| `/agent spawn ... --mock` | - | Create agent in mock mode |
| `/agent assign <name> <task>` | - | Assign task to agent |
| `/agent status <name>` | - | Show agent status |
| `/agent get <name> [key]` | - | Show agent settings |
| `/agent set <name> <key> <val>` | - | Configure agent settings |
| `/agent terminate <name>` | - | Stop an agent |
| `/agent wait <name>` | - | Wait for agent completion |
| `/exit` | `/q` | Exit CyxMake |

### CLI Commands

| Command | Description |
|---------|-------------|
| `cyxmake` | Start interactive REPL |
| `cyxmake build` | Build project (with error recovery) |
| `cyxmake auto` | **AI-powered autonomous build** |
| `cyxmake create` | **Create project from natural language** |
| `cyxmake init` | Analyze and cache project |
| `cyxmake help` | Show help |

### Natural Language

| Command | What It Does |
|---------|--------------|
| `build` | Compile the project |
| `read main.c` | Display file contents |
| `create hello.py` | Create a new file |
| `install SDL2` | Install using best package manager |
| `clean` | Remove build artifacts |
| `@ai <query>` | Force AI routing |
| `ai: <query>` | Force AI routing |
| `ask: <query>` | Force AI routing |

---

## Project Status

**Current Version**: v1.0.0 (Stable)

### Implemented

- [x] **Interactive REPL** - Full command-line interface
- [x] **Natural Language Parsing** - Intent detection with confidence scoring
- [x] **Intelligent Routing** - Complex queries go to AI, simple ones execute locally
- [x] **Multi-Provider AI** - Ollama, LM Studio, OpenAI, Anthropic, Gemini, OpenRouter, Groq, llama.cpp
- [x] **AI Agent System** - LLM-powered action execution
- [x] **AI Autonomous Build** - `cyxmake auto` for fully autonomous building
- [x] **Multi-Agent System** - Named agents, parallel execution, task assignment
- [x] **Tool Discovery** - Auto-detects compilers, build tools, package managers
- [x] **Tool Execution** - Safe command execution with output capture
- [x] **Error Recovery** - Pattern matching + AI-powered diagnosis
- [x] **Permission System** - Confirms before destructive operations
- [x] **Conversation Context** - Remembers files, errors, history
- [x] **Cross-Platform** - Windows, Linux, macOS
- [x] **Project Generation** - Create projects from natural language descriptions

### In Progress

- [ ] Interactive error fixing

### Planned

- [ ] IDE plugins (VSCode, JetBrains)
- [ ] CI/CD integration
- [ ] Plugin system for custom tools

---

## Contributing

We welcome contributions! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Setup

```bash
# Clone repository
git clone https://github.com/CYXWIZ-Lab/cyxmake.git
cd cyxmake

# Initialize submodules
git submodule update --init --recursive

# Build in debug mode
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCYXMAKE_BUILD_TESTS=ON
cmake --build .

# Run tests
ctest --output-on-failure

# Run CyxMake
./bin/Debug/cyxmake.exe  # Windows
./bin/cyxmake            # Linux/macOS
```

### Areas We Need Help

- **Testing**: Test on diverse projects across platforms
- **Documentation**: Improve docs, write tutorials
- **AI Providers**: Add support for more providers
- **Tools**: Add support for more build systems (Bazel, Buck, etc.)

---

## FAQ

### Is CyxMake a replacement for CMake/Maven/npm?

**No.** CyxMake orchestrates existing build tools with AI intelligence. It uses CMake, Maven, npm, etc. under the hood and makes them work seamlessly.

### Does it work offline?

**Yes.** Use Ollama or LM Studio to run AI models locally. No internet required.

### Is my code sent to the cloud?

**Only if you use cloud providers.** With local providers (Ollama, LM Studio, llama.cpp), everything stays on your machine. With cloud providers, only prompts and error messages are sent - not your source code.

### What languages are supported?

All languages with existing build systems: C, C++, Python, JavaScript, TypeScript, Rust, Go, Java, and more.

### Is it free?

**Yes.** CyxMake is open source (Apache 2.0). Use local AI providers for completely free usage, or bring your own API keys for cloud providers.

### How is this different from GitHub Copilot?

Copilot assists with writing code. CyxMake assists with building code. They're complementary tools.

---

## License

CyxMake is licensed under the [Apache License 2.0](LICENSE).

### Bundled Components

- **llama.cpp**: MIT License
- **cJSON**: MIT License
- **tomlc99**: MIT License

---

## Documentation

For detailed documentation, see:

- [summary.md](summary.md) - Complete system guide
- [CONTRIBUTING.md](CONTRIBUTING.md) - Contribution guidelines
- [CLAUDE.md](CLAUDE.md) - Development reference

---

## Support

- **Issues**: [GitHub Issues](https://github.com/CYXWIZ-Lab/cyxmake/issues)
- **Donate**: [See all donation options](DONATION.md)
- **GitHub Sponsors**: [Sponsor on GitHub](https://github.com/sponsors/cyxwiz-lab)
- **Patreon**: [Support on Patreon](https://www.patreon.com/posts/cyxwiz-projects-146571889)

---

**Stop fighting build systems. Start building.**
