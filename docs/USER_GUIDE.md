# CyxMake User Guide

> AI-powered build automation that eliminates traditional build system complexity

## Table of Contents

1. [Introduction](#introduction)
2. [Installation](#installation)
3. [Quick Start](#quick-start)
4. [REPL Interface](#repl-interface)
5. [Natural Language Commands](#natural-language-commands)
6. [Project Types](#project-types)
7. [Error Recovery](#error-recovery)
8. [Multi-Agent System](#multi-agent-system)
9. [Security Features](#security-features)
10. [Best Practices](#best-practices)

---

## Introduction

CyxMake is an AI-powered build automation system that lets you use plain English instead of learning complex build system syntax. It automatically:

- Detects your project type and structure
- Selects appropriate build tools
- Diagnoses and fixes build errors
- Learns from successful fixes

### Key Features

| Feature | Description |
|---------|-------------|
| Natural Language | Use plain English like "build in release mode" |
| Auto-Detection | Automatically detects C/C++, Rust, Node.js, Python projects |
| Error Recovery | AI-powered error diagnosis and automatic fixes |
| Multi-Agent | Parallel task execution with named agents |
| Security | Permission prompts, audit logging, dry-run mode |

---

## Installation

### Prerequisites

- CMake 3.16 or later
- C compiler (GCC, Clang, or MSVC)
- Git (for cloning)

### Building from Source

```bash
# Clone the repository
git clone https://github.com/CYXWIZ-Lab/cyxmake.git
cd cyxmake

# Create build directory
mkdir build && cd build

# Configure and build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Optional: Install system-wide
cmake --install .
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `CYXMAKE_BUILD_TESTS` | ON | Build test suite |
| `CYXMAKE_BUILD_TOOLS` | ON | Build bundled tools |
| `CYXMAKE_USE_SANITIZERS` | OFF | Enable AddressSanitizer |
| `CYXMAKE_STATIC_LINK` | OFF | Static linking |

### Platform-Specific Notes

**Windows:**
```bash
# Use Visual Studio generator
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

**macOS:**
```bash
# Install dependencies
brew install cmake

# Build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

**Linux:**
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install cmake build-essential libcurl4-openssl-dev

# Build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## Quick Start

### Tutorial 1: Building Your First Project

```bash
# Navigate to any project with a build system
cd /path/to/your/project

# Initialize CyxMake (analyzes project structure)
cyxmake init

# Build the project
cyxmake build

# Or use natural language
cyxmake "build in release mode"
```

### Tutorial 2: Creating a New Project

```bash
# Create a new C++ project with natural language
cyxmake create "C++ console application with unit tests"

# Create a game project
cyxmake create "C++ game with SDL2 and OpenGL"

# Create a web server
cyxmake create "C REST API server with SQLite"
```

### Tutorial 3: Using the Interactive REPL

```bash
# Start interactive mode
cyxmake

# You'll see the CyxMake prompt
cyxmake> help
cyxmake> /build
cyxmake> /status
cyxmake> exit
```

---

## REPL Interface

The REPL (Read-Eval-Print Loop) provides an interactive shell for working with CyxMake.

### Starting the REPL

```bash
cyxmake          # Start REPL in current directory
cyxmake -d /path # Start REPL in specified directory
```

### Slash Commands

| Command | Alias | Description |
|---------|-------|-------------|
| `/help` | `/h` | Show help |
| `/build` | `/b` | Build project |
| `/clean` | `/c` | Clean build artifacts |
| `/status` | `/s` | Show project status |
| `/analyze` | `/a` | Analyze project structure |
| `/config` | | Show configuration |
| `/agent` | | Manage agents |
| `/exit` | `/q` | Exit REPL |

### Examples

```bash
cyxmake> /build              # Build with defaults
cyxmake> /build release      # Build release configuration
cyxmake> /clean              # Clean build directory
cyxmake> /status             # Show build status
cyxmake> /analyze            # Re-analyze project
```

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Up/Down | Navigate command history |
| Tab | Auto-complete commands |
| Ctrl+C | Cancel current operation |
| Ctrl+D | Exit REPL |

---

## Natural Language Commands

CyxMake understands natural language for common operations.

### Build Commands

```bash
cyxmake> build the project
cyxmake> compile in debug mode
cyxmake> make a release build
cyxmake> rebuild everything
cyxmake> build with 4 parallel jobs
```

### Clean Commands

```bash
cyxmake> clean the build
cyxmake> remove build artifacts
cyxmake> fresh build
```

### Analysis Commands

```bash
cyxmake> what's in this project?
cyxmake> analyze the codebase
cyxmake> show dependencies
cyxmake> check for errors
```

### Fix Commands

```bash
cyxmake> fix the build errors
cyxmake> install missing dependencies
cyxmake> resolve the SDL2 issue
```

### Intent Detection

CyxMake uses confidence scoring to understand your intent:

```
Intent: BUILD (confidence: 0.95)
Action: Running cmake --build . --config Debug
```

If confidence is low, CyxMake will ask for clarification.

---

## Project Types

CyxMake automatically detects and supports various project types.

### C/C++ Projects

**Supported Build Systems:**
- CMake (CMakeLists.txt)
- Make (Makefile)
- Meson (meson.build)
- Ninja (build.ninja)

**Auto-Detection:**
- Scans for `.c`, `.cpp`, `.h`, `.hpp` files
- Identifies CMakeLists.txt or Makefile
- Detects compiler requirements

### Rust Projects

**Detection:**
- Cargo.toml present
- `.rs` source files

**Commands:**
```bash
cyxmake> build          # cargo build
cyxmake> build release  # cargo build --release
cyxmake> test           # cargo test
```

### Node.js Projects

**Detection:**
- package.json present
- `.js`, `.ts` source files

**Commands:**
```bash
cyxmake> build          # npm run build
cyxmake> install deps   # npm install
cyxmake> test           # npm test
```

### Python Projects

**Detection:**
- setup.py or pyproject.toml
- `.py` source files

**Commands:**
```bash
cyxmake> install        # pip install .
cyxmake> test           # pytest
```

---

## Error Recovery

CyxMake automatically diagnoses and fixes common build errors.

### How It Works

1. **Detection**: CyxMake monitors build output for error patterns
2. **Diagnosis**: AI analyzes the error and identifies the cause
3. **Solution**: Generates fix actions (install package, modify file, etc.)
4. **Validation**: Validates fixes before applying
5. **Verification**: Rebuilds to verify the fix worked
6. **Learning**: Records successful fixes for future reference

### Supported Error Types

| Error Type | Example | Auto-Fix |
|------------|---------|----------|
| Missing Library | `cannot find -lSDL2` | Install package |
| Missing Header | `SDL2/SDL.h: No such file` | Install dev package |
| CMake Package | `Could not find SDL2` | Install + set CMAKE_PREFIX_PATH |
| Permission Denied | `Permission denied` | chmod or sudo |
| Syntax Error | `expected ';'` | Manual fix required |
| Undefined Reference | `undefined reference to 'foo'` | Link library |

### Example Recovery Session

```
Building project...
ERROR: cannot find -lSDL2

[Diagnosis] Missing library: SDL2
[Confidence] 0.92

Suggested fixes:
  1. Install SDL2 library [HIGH RISK - requires confirmation]
  2. Set LD_LIBRARY_PATH [LOW RISK]
  3. Clean and rebuild [LOW RISK]

Apply fix #1? [Y/n/a/v] y
Installing SDL2...
Package installed successfully.

Rebuilding...
Build successful!

[Fix recorded to history]
```

### Fix History

CyxMake remembers successful fixes:

```bash
cyxmake> /fix history     # Show fix history
cyxmake> /fix suggest     # Get suggestions based on history
```

---

## Multi-Agent System

CyxMake supports parallel task execution with named agents.

### Spawning Agents

```bash
cyxmake> /agent spawn builder build
Created agent 'builder' (type: build, state: idle)

cyxmake> /agent spawn analyzer smart
Created agent 'analyzer' (type: smart, state: idle)
```

### Agent Types

| Type | Description |
|------|-------------|
| `build` | Executes build commands |
| `smart` | AI-powered analysis and fixes |
| `auto` | Autonomous operation |

### Assigning Tasks

```bash
cyxmake> /agent assign builder "Build release configuration"
Task assigned to 'builder'. Running...

cyxmake> /agent assign analyzer "Find unused dependencies"
Task assigned to 'analyzer'. Running...
```

### Managing Agents

```bash
cyxmake> /agent list              # List all agents
cyxmake> /agent status builder    # Check agent status
cyxmake> /agent wait builder      # Wait for completion
cyxmake> /agent terminate builder # Stop agent
```

### Conflict Resolution

When agents conflict, CyxMake prompts for resolution:

```
CONFLICT: Agents 'builder' and 'fixer' both want to modify CMakeLists.txt

Agent 'builder': Add find_package(SDL2)
Agent 'fixer': Update cmake_minimum_required

Which should proceed first?
  [1] builder
  [2] fixer
  [3] both (sequential)
  [4] cancel

Choice:
```

---

## Security Features

CyxMake includes comprehensive security features.

### Permission System

Operations are classified by risk level:

| Level | Actions | Prompt |
|-------|---------|--------|
| SAFE | Read files, analyze | No prompt |
| ASK | Create/modify files, run commands | Y/N prompt |
| DANGEROUS | Delete files, install packages | Warning + confirm |
| BLOCKED | System files | Always denied |

**Permission Prompt:**
```
Action: Install package 'sdl2'
Risk Level: DANGEROUS

This action will:
  - Run: vcpkg install sdl2
  - Modify system packages

Proceed? [Y/n/a/v]
  Y = Yes (once)
  n = No
  a = Always (this session)
  v = View details
```

### Dry-Run Mode

Preview actions without executing:

```bash
cyxmake> /config set dry_run true
cyxmake> /build

[DRY-RUN] Would execute: cmake --build . --config Debug
[DRY-RUN] Would create: build/CMakeCache.txt
[DRY-RUN] No changes were made.
```

### Audit Logging

All actions are logged for review:

```bash
cyxmake> /audit show          # Recent actions
cyxmake> /audit export json   # Export to file
```

**Log Format:**
```
2024-12-26 10:30:45 [ACTION] RUN_COMMAND cmake --build . SUCCESS
2024-12-26 10:30:46 [ACTION] CREATE_FILE build/output.txt SUCCESS
2024-12-26 10:30:47 [DENIED] DELETE_FILE /etc/passwd BLOCKED
```

### Rollback Support

Undo file modifications:

```bash
cyxmake> /rollback list       # Show rollback history
cyxmake> /rollback last 1     # Undo last change
cyxmake> /rollback all        # Undo all changes
```

---

## Best Practices

### 1. Start with Analysis

Always analyze a project before building:

```bash
cyxmake init
cyxmake /analyze
```

### 2. Use Dry-Run for New Projects

Preview actions on unfamiliar projects:

```bash
cyxmake /config set dry_run true
cyxmake build
cyxmake /config set dry_run false
```

### 3. Review Fix Suggestions

Always review AI-suggested fixes:

```bash
# Don't auto-approve dangerous fixes
cyxmake /config set auto_approve_dangerous false
```

### 4. Use Named Agents for Complex Tasks

Break down complex builds:

```bash
cyxmake> /agent spawn deps build
cyxmake> /agent spawn main build
cyxmake> /agent assign deps "Install all dependencies"
cyxmake> /agent wait deps
cyxmake> /agent assign main "Build main project"
```

### 5. Check Fix History

Before fixing errors manually, check history:

```bash
cyxmake> /fix suggest
# Shows fixes that worked before for similar errors
```

### 6. Keep Audit Logs

Enable audit logging for compliance:

```bash
cyxmake /config set audit_enabled true
cyxmake /config set audit_file "./cyxmake_audit.log"
```

---

## Getting Help

- **In REPL**: Type `/help` or `help`
- **Command Line**: `cyxmake --help`
- **Issues**: https://github.com/CYXWIZ-Lab/cyxmake/issues
- **Documentation**: https://github.com/CYXWIZ-Lab/cyxmake/tree/main/docs

---

*CyxMake - Build smarter, not harder.*
