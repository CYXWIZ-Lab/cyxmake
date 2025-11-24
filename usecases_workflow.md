# CyxMake: User Workflows and Use Cases

## Overview

This document describes the complete user experience from installation to daily usage of CyxMake. It covers initialization, caching, and common workflows that developers encounter.

---

## Installation and Setup

### Installing CyxMake

**Windows**:
```bash
# Option 1: Installer
> cyxmake-installer.exe

# Option 2: Chocolatey
> choco install cyxmake

# Option 3: Manual (add to PATH)
> curl -O https://releases.cyxmake.com/cyxmake-windows-x64.zip
> unzip cyxmake-windows-x64.zip -d C:\Program Files\CyxMake
> setx PATH "%PATH%;C:\Program Files\CyxMake\bin"
```

**Linux**:
```bash
# Option 1: Package manager
$ sudo apt install cyxmake        # Debian/Ubuntu
$ sudo dnf install cyxmake        # Fedora
$ sudo pacman -S cyxmake          # Arch

# Option 2: Install script
$ curl -sSL https://install.cyxmake.com | bash

# Option 3: Manual
$ wget https://releases.cyxmake.com/cyxmake-linux-x64.tar.gz
$ tar -xzf cyxmake-linux-x64.tar.gz
$ sudo mv cyxmake /usr/local/bin/
```

**macOS**:
```bash
# Option 1: Homebrew
$ brew install cyxmake

# Option 2: Install script
$ curl -sSL https://install.cyxmake.com | bash
```

### First-Time Configuration

After installation, CyxMake creates configuration directories:

```
~/.cyxmake/
├── config.toml          # User configuration
├── cache/               # Project caches
├── tools/               # Installed tools
├── models/              # Downloaded LLM models
└── logs/                # Execution logs
```

**Initialize user settings**:
```bash
$ cyxmake config init

CyxMake Configuration Setup
===========================

1. Download local AI model? (Recommended: 3GB)
   [Y/n]: Y

   Downloading codellama-7b-instruct-q4.gguf...
   [████████████████████] 100% (3.2 GB)

   ✓ Model installed to ~/.cyxmake/models/

2. Enable cloud LLM fallback for complex errors?
   [Y/n]: Y

   Choose provider:
   1) Anthropic Claude (recommended)
   2) OpenAI GPT
   3) None (local only)

   Selection [1]: 1

   Enter Anthropic API key (or press Enter to set later):
   > sk-ant-*********************

   ✓ API key saved (encrypted)

3. Enable anonymous telemetry to improve CyxMake?
   [Y/n]: n

   ✓ Telemetry disabled

Configuration complete! Run 'cyxmake init' in your project directory to get started.
```

---

## Core Workflow: Project Initialization

### Use Case 1: Initialize Existing Project

**Scenario**: Sarah clones an unfamiliar Python project and wants CyxMake to understand it.

```bash
$ cd ~/projects/neural-network-trainer
$ ls
README.md  requirements.txt  setup.py  src/  tests/  data/

$ cyxmake init

CyxMake Project Initialization
===============================

Analyzing project structure...
[████████████████████] 100%

✓ Detected project type: Python package
✓ Build system: setuptools (setup.py)
✓ Dependencies: 23 packages (requirements.txt)
✓ Python version: >=3.8 (from setup.py)
✓ Tests detected: pytest in tests/
✓ Entry points: 3 console scripts

Project Summary:
────────────────
Name:           neural-network-trainer
Type:           Python package
Build System:   setuptools
Languages:      Python (98%), Shell (2%)
Source Files:   127 files
Dependencies:   23 (15 installed, 8 missing)
Documentation:  README.md, docs/

Build Instructions (from README.md):
────────────────────────────────────
1. Create virtual environment: python -m venv venv
2. Activate: source venv/bin/activate
3. Install dependencies: pip install -r requirements.txt
4. Install package: pip install -e .
5. Run tests: pytest tests/

Cache saved to: .cyxmake/cache.json

Next steps:
  • Run 'cyxmake build' to build the project
  • Run 'cyxmake doctor' to check for issues
  • Run 'cyxmake show' to view detailed project information
```

**What happened behind the scenes?**

CyxMake performed deep project analysis:

1. **File system scan**: Discovered all files, detected language ratios
2. **Build system detection**: Found `setup.py`, identified setuptools
3. **Dependency analysis**: Parsed `requirements.txt`, checked installed packages
4. **README parsing**: Extracted build instructions using NLP
5. **Environment detection**: Detected Python version requirements
6. **Test discovery**: Found pytest configuration
7. **Cache creation**: Saved all findings to `.cyxmake/cache.json`

### Cache Structure: `.cyxmake/cache.json`

```json
{
  "version": "1.0",
  "created_at": "2025-11-24T14:30:00Z",
  "updated_at": "2025-11-24T14:30:00Z",
  "project": {
    "name": "neural-network-trainer",
    "root": "/home/sarah/projects/neural-network-trainer",
    "type": "python_package",
    "primary_language": "python",
    "languages": {
      "python": {"files": 120, "lines": 15420, "percentage": 98.2},
      "shell": {"files": 7, "lines": 234, "percentage": 1.8}
    }
  },
  "build_system": {
    "type": "setuptools",
    "config_files": ["setup.py", "setup.cfg"],
    "build_commands": [
      "pip install -e .",
      "python setup.py install"
    ]
  },
  "dependencies": {
    "requirements_file": "requirements.txt",
    "count": 23,
    "packages": [
      {
        "name": "numpy",
        "version_spec": ">=1.21.0",
        "installed": true,
        "installed_version": "1.24.3"
      },
      {
        "name": "torch",
        "version_spec": ">=2.0.0",
        "installed": false,
        "required": true
      }
      // ... more packages
    ],
    "missing": ["torch", "torchvision", "tensorboard", "wandb", "pytest-cov", "sphinx", "black", "mypy"],
    "outdated": ["requests"]
  },
  "environment": {
    "python": {
      "required_version": ">=3.8",
      "current_version": "3.11.5",
      "virtual_env": null,
      "venv_recommended": true
    },
    "system": {
      "os": "linux",
      "arch": "x86_64",
      "available_memory_gb": 16,
      "available_disk_gb": 250
    }
  },
  "readme": {
    "path": "README.md",
    "has_build_instructions": true,
    "extracted_steps": [
      {"step": 1, "description": "Create virtual environment", "command": "python -m venv venv"},
      {"step": 2, "description": "Activate", "command": "source venv/bin/activate"},
      {"step": 3, "description": "Install dependencies", "command": "pip install -r requirements.txt"},
      {"step": 4, "description": "Install package", "command": "pip install -e ."},
      {"step": 5, "description": "Run tests", "command": "pytest tests/"}
    ],
    "prerequisites": ["Python 3.8+", "pip", "virtualenv"],
    "known_issues": []
  },
  "structure": {
    "source_dirs": ["src/", "neural_network_trainer/"],
    "test_dirs": ["tests/"],
    "doc_dirs": ["docs/"],
    "config_files": ["setup.py", "setup.cfg", "requirements.txt", ".gitignore", "pyproject.toml"],
    "entry_points": [
      {"name": "nnt-train", "module": "neural_network_trainer.cli:train"},
      {"name": "nnt-eval", "module": "neural_network_trainer.cli:evaluate"},
      {"name": "nnt-export", "module": "neural_network_trainer.cli:export_model"}
    ]
  },
  "tests": {
    "framework": "pytest",
    "test_files": 45,
    "config": "pytest.ini",
    "coverage_tool": "pytest-cov"
  },
  "git": {
    "repository": true,
    "remote": "https://github.com/user/neural-network-trainer.git",
    "branch": "main",
    "uncommitted_changes": false
  },
  "metadata": {
    "total_files": 127,
    "total_lines": 15654,
    "estimated_complexity": "medium",
    "cyxmake_confidence": 0.95
  }
}
```

This cache enables:
- **Fast subsequent operations** (no re-scanning)
- **Intelligent build planning** (knows dependencies, order)
- **Error context** (understands project structure when errors occur)
- **Incremental updates** (only re-analyze changed files)

---

## Use Case 2: Building a Project After Init

**Continuing from Use Case 1**:

```bash
$ cyxmake build

CyxMake Build Process
=====================

Reading project cache... ✓

Build Plan:
───────────
1. Create virtual environment (venv)
2. Activate virtual environment
3. Install 8 missing dependencies
4. Install package in development mode
5. Verify installation

Proceed? [Y/n]: Y

[1/5] Creating virtual environment...
  $ python -m venv venv
  ✓ Virtual environment created (2.3s)

[2/5] Activating virtual environment...
  ✓ Activated venv

[3/5] Installing missing dependencies...
  $ pip install torch torchvision tensorboard wandb pytest-cov sphinx black mypy

  Installing torch (2.1.0)... ████████████████ 850 MB
  Installing torchvision (0.16.0)... ████ 150 MB
  ...

  ⚠ Warning: torch 2.1.0 requires CUDA 11.8, but CUDA 12.1 detected

  Attempting fix: Installing torch with CUDA 12.1 support...
  $ pip install torch --index-url https://download.pytorch.org/whl/cu121

  ✓ torch 2.1.0+cu121 installed successfully
  ✓ All dependencies installed (142.5s)

[4/5] Installing package in development mode...
  $ pip install -e .
  ✓ neural-network-trainer 0.3.1 installed (3.2s)

[5/5] Verifying installation...
  $ python -c "import neural_network_trainer; print(neural_network_trainer.__version__)"
  Output: 0.3.1
  ✓ Package verified

Build Summary:
──────────────
Status:         SUCCESS
Duration:       2m 28s
Dependencies:   23 installed
Warnings:       1 (CUDA version mismatch, auto-fixed)
Errors Fixed:   1 (torch CUDA compatibility)

Next steps:
  • Run 'cyxmake test' to run the test suite
  • Run 'nnt-train --help' to see available commands
  • Run 'cyxmake doctor' for health check

Build log saved to: .cyxmake/build.log
```

**Key features demonstrated**:
1. **Cache utilization**: No re-scanning, instant build plan
2. **Autonomous error recovery**: Detected CUDA mismatch, fixed automatically
3. **Progress tracking**: Clear visibility into each step
4. **Verification**: Confirmed installation works
5. **User guidance**: Suggested next steps

---

## Use Case 3: Generating a New Project

**Scenario**: John wants to create a C++ project from scratch using natural language.

```bash
$ mkdir ~/projects/game-engine
$ cd ~/projects/game-engine

$ cyxmake init --create

CyxMake Project Creator
=======================

What kind of project would you like to create?

You: I want to create a C++ game engine using SDL2 and OpenGL.
     It should compile on Windows and Linux, and use CMake.
     I want a library and an example executable.

Analyzing requirements...

Project Plan:
─────────────
Type:            C++ library + executable
Build System:    CMake (3.20+)
Dependencies:    SDL2, OpenGL (via glad), GLM (math)
Target Platforms: Windows (MSVC), Linux (GCC/Clang)
Output:
  • libgameengine.a (static library)
  • gameengine-demo (example executable)

Additional files:
  • README.md with build instructions
  • .gitignore for C++ projects
  • CMakeLists.txt (root + subdirectories)
  • Basic source structure

Proceed with generation? [Y/n]: Y

Generating project structure...

[1/8] Creating directory structure...
  ✓ src/ (library source)
  ✓ include/ (public headers)
  ✓ examples/ (demo application)
  ✓ tests/ (unit tests with Google Test)
  ✓ external/ (third-party dependencies)

[2/8] Generating CMakeLists.txt...
  ✓ Root CMakeLists.txt (project configuration)
  ✓ src/CMakeLists.txt (library target)
  ✓ examples/CMakeLists.txt (demo executable)
  ✓ tests/CMakeLists.txt (test suite)

[3/8] Generating source files...
  ✓ include/gameengine/core.h
  ✓ include/gameengine/renderer.h
  ✓ src/core.cpp
  ✓ src/renderer.cpp
  ✓ examples/demo.cpp

[4/8] Generating README.md...
  ✓ Build instructions
  ✓ Dependency installation guide
  ✓ Usage examples

[5/8] Generating .gitignore...
  ✓ C++ build artifacts
  ✓ IDE files
  ✓ System files

[6/8] Setting up dependency management...
  ✓ vcpkg integration (Windows)
  ✓ pkg-config support (Linux)
  ✓ Dependency manifest (vcpkg.json)

[7/8] Initializing Git repository...
  $ git init
  $ git add .
  $ git commit -m "Initial project structure generated by CyxMake"
  ✓ Git repository initialized

[8/8] Creating project cache...
  ✓ Analyzing generated project
  ✓ Cache saved (.cyxmake/cache.json)

Project created successfully!

Project Structure:
──────────────────
game-engine/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── vcpkg.json
├── include/
│   └── gameengine/
│       ├── core.h
│       └── renderer.h
├── src/
│   ├── CMakeLists.txt
│   ├── core.cpp
│   └── renderer.cpp
├── examples/
│   ├── CMakeLists.txt
│   └── demo.cpp
├── tests/
│   └── CMakeLists.txt
├── external/
└── .cyxmake/
    └── cache.json

Next steps:
  1. Review generated files
  2. Run 'cyxmake build' to build the project
  3. Run 'cyxmake doctor' to check for missing dependencies
  4. Customize the generated code in src/

Time saved: ~2-3 hours of manual setup!
```

**Generated CMakeLists.txt** (root):

```cmake
cmake_minimum_required(VERSION 3.20)
project(GameEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_EXAMPLES "Build example applications" ON)
option(BUILD_TESTS "Build test suite" ON)

# Dependencies
find_package(SDL2 REQUIRED CONFIG)
find_package(OpenGL REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# Library
add_subdirectory(src)

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

**Generated README.md**:

```markdown
# Game Engine

A C++ game engine using SDL2 and OpenGL.

## Building

### Prerequisites

**Windows:**
```bash
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg && bootstrap-vcpkg.bat

# Install dependencies
vcpkg install sdl2 glad glm
```

**Linux:**
```bash
# Ubuntu/Debian
sudo apt install libsdl2-dev libglm-dev

# Fedora
sudo dnf install SDL2-devel glm-devel
```

### Build Steps

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run demo
./build/examples/gameengine-demo
```

---

*Project generated by [CyxMake](https://cyxmake.com) - AI-powered build automation*
```

---

## Use Case 4: Cache Updates and Intelligence

### Scenario: File Changes Trigger Cache Update

```bash
$ cyxmake init
# ... initial cache created ...

# Developer adds a new dependency to requirements.txt
$ echo "scikit-learn>=1.3.0" >> requirements.txt

# CyxMake detects the change
$ cyxmake build

Detected changes since last build:
  • requirements.txt (modified)

Updating project cache... ✓

New dependencies detected:
  + scikit-learn>=1.3.0 (not installed)
  + scipy>=1.9.0 (transitive dependency, not installed)

Updated build plan includes installing 2 new packages.

Proceed? [Y/n]: Y
```

### Cache Invalidation Rules

Cache is invalidated when:
- `requirements.txt`, `package.json`, `Cargo.toml`, etc. are modified
- Build configuration files change (`CMakeLists.txt`, `Makefile`)
- `.cyxmake/cache.json` is older than 7 days
- User runs `cyxmake init --force`
- Git branch changes (different dependencies per branch)

### Incremental Cache Updates

```bash
$ cyxmake status

Project Status:
───────────────
Cache:          Valid (updated 2 hours ago)
Dependencies:   23/23 installed ✓
Build System:   setuptools (configured)
Last Build:     Success (2 hours ago)
Git Status:     Clean

Recent Changes:
  • src/neural_net.py (modified 5 minutes ago)
  • tests/test_training.py (modified 5 minutes ago)

Cache Status: UP TO DATE
No rebuild required unless you've changed dependencies.
```

---

## Use Case 5: Doctor Command (Health Check)

**Scenario**: Developer encounters issues, runs doctor to diagnose.

```bash
$ cyxmake doctor

CyxMake Health Check
====================

Analyzing project and environment...

[✓] Project Cache
    • Cache exists and is valid
    • Last updated: 2 hours ago

[✓] Build System
    • Type: setuptools
    • Configuration files: setup.py ✓

[⚠] Dependencies
    • 23/23 required packages installed
    • 1 package has security vulnerability:
      - requests 2.28.0 → upgrade to 2.31.0+ (CVE-2023-32681)

    Fix: pip install --upgrade requests

[✓] Python Environment
    • Version: 3.11.5 (meets requirement: >=3.8)
    • Virtual environment: active
    • Pip version: 23.2.1 (latest)

[✗] System Tools
    • gcc: not found (required for compiling native extensions)

    Fix (Ubuntu/Debian): sudo apt install build-essential
    Fix (macOS): xcode-select --install

[⚠] Disk Space
    • Available: 8.2 GB
    • Recommended: 10+ GB for ML dependencies
    • Warning: Low disk space may cause build failures

[✓] Network
    • PyPI reachable ✓
    • GitHub reachable ✓

[✓] Git Repository
    • Initialized ✓
    • Remote configured ✓
    • No uncommitted changes ✓

Health Score: 7/10

Issues Found: 2
  1. [MEDIUM] Security vulnerability in requests package
  2. [HIGH] Missing GCC compiler (required for torch native extensions)

Recommended Actions:
  1. Install build tools: sudo apt install build-essential
  2. Update requests: pip install --upgrade requests
  3. Free up disk space (consider cleaning pip cache)

Run 'cyxmake doctor --fix' to automatically fix solvable issues.
```

**Auto-fix mode**:

```bash
$ cyxmake doctor --fix

CyxMake Auto-Fix
================

Applying fixes for 2 issues...

[1/2] Installing build-essential...
  $ sudo apt install build-essential
  [sudo] password for user: ********

  ✓ build-essential installed (45s)

[2/2] Updating vulnerable packages...
  $ pip install --upgrade requests

  ✓ requests upgraded to 2.31.0 (3.2s)

All fixes applied successfully!

Re-running health check...

Health Score: 10/10 ✓

Your project is healthy and ready to build!
```

---

## Use Case 6: Interactive Shell Mode

```bash
$ cyxmake shell

CyxMake Interactive Shell
=========================
Type 'help' for commands, 'exit' to quit.

Project: neural-network-trainer (Python package)
Status: Ready to build

cyxmake> build

Building project...
[...build output...]
✓ Build successful

cyxmake> test

Running tests...
[...test output...]
✓ 45/45 tests passed

cyxmake> What dependencies does this project need?

Analyzing dependencies...

This project requires 23 Python packages:

Core Dependencies:
  • numpy >=1.21.0 - Numerical computing
  • torch >=2.0.0 - Deep learning framework
  • torchvision >=0.15.0 - Computer vision utilities
  • tensorboard >=2.14.0 - Training visualization

Development Dependencies:
  • pytest >=7.0.0 - Testing framework
  • pytest-cov - Test coverage
  • black - Code formatter
  • mypy - Type checker

All dependencies are currently installed ✓

cyxmake> How do I add a new dependency?

To add a new dependency:

1. Add it to requirements.txt:
   $ echo "scipy>=1.9.0" >> requirements.txt

2. Install it:
   $ cyxmake install scipy

   Or let CyxMake handle it automatically:
   $ cyxmake build

CyxMake will detect the new dependency and install it.

cyxmake> show cache

Project Cache:
──────────────
Location: .cyxmake/cache.json
Size: 45 KB
Created: 2 hours ago
Last Updated: 2 hours ago
Status: Valid

Contains:
  • Project metadata
  • 23 dependency specifications
  • 127 source file mappings
  • Build instructions (5 steps)
  • Test configuration

Use 'cyxmake cache --export' to export as human-readable report.

cyxmake> exit

Goodbye!
```

---

## Use Case 7: CI/CD Integration

### GitHub Actions Integration

```yaml
# .github/workflows/build.yml
name: Build with CyxMake

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v3

      - name: Install CyxMake
        run: |
          curl -sSL https://install.cyxmake.com | bash
          echo "$HOME/.cyxmake/bin" >> $GITHUB_PATH

      - name: Initialize project
        run: cyxmake init

      - name: Build project
        run: cyxmake build --verbose
        env:
          CYXMAKE_CLOUD_ENABLED: false  # Use local model only in CI

      - name: Run tests
        run: cyxmake test --coverage

      - name: Upload build artifacts
        uses: actions/upload-artifact@v3
        with:
          name: build-artifacts
          path: dist/
```

**Benefits in CI/CD**:
- Consistent builds across environments
- Automatic error recovery (no manual debugging)
- Detailed structured logs
- Fast (uses cache from previous runs)

---

## Use Case 8: Multi-Project Workspace

### Scenario: Monorepo with Multiple Languages

```bash
$ cd ~/projects/company-monorepo
$ tree -L 2
.
├── backend/         (Python FastAPI)
├── frontend/        (TypeScript React)
├── mobile/          (React Native)
├── shared/          (TypeScript shared code)
└── infrastructure/  (Terraform)

$ cyxmake init --workspace

CyxMake Workspace Initialization
=================================

Scanning workspace...

Found 5 sub-projects:

1. backend/          Python (FastAPI, PostgreSQL)
2. frontend/         TypeScript (React, Vite)
3. mobile/           TypeScript (React Native)
4. shared/           TypeScript (library)
5. infrastructure/   Terraform (AWS)

Initialize all? [Y/n]: Y

Analyzing projects in parallel...
[████████████████████] 100%

✓ backend initialized (14 dependencies)
✓ frontend initialized (87 dependencies)
✓ mobile initialized (112 dependencies)
✓ shared initialized (8 dependencies)
✓ infrastructure initialized (3 providers)

Workspace cache created: .cyxmake/workspace.json

Build order determined:
  1. shared/ (no dependencies)
  2. backend/ (depends on shared/)
  3. frontend/ (depends on shared/)
  4. mobile/ (depends on shared/)
  5. infrastructure/ (deploys all)

Use 'cyxmake build --all' to build entire workspace.
Use 'cyxmake build backend' to build specific project.
```

**Building specific projects**:

```bash
$ cyxmake build backend

Building: backend
=================

Dependencies: shared/ (building first)

[1/2] Building shared/...
  $ npm run build
  ✓ shared built (12.3s)

[2/2] Building backend/...
  $ pip install -r requirements.txt
  $ pip install -e .
  ✓ backend built (45.2s)

Total time: 57.5s
```

**Building everything**:

```bash
$ cyxmake build --all

Building all projects in workspace...

Build Order:
  1. shared/
  2. backend/, frontend/, mobile/ (parallel)
  3. infrastructure/

[1/5] Building shared/...
  ✓ Completed (12.3s)

[2/5] Building backend/...
[3/5] Building frontend/...
[4/5] Building mobile/...
  ✓ backend completed (45.2s)
  ✓ frontend completed (62.1s)
  ✓ mobile completed (78.4s)

[5/5] Validating infrastructure/...
  ✓ Terraform configuration valid (5.1s)

Workspace build successful!
Total time: 1m 38s (parallelized from potential 3m 15s)
```

---

## Cache Management Commands

### View Cache Contents

```bash
$ cyxmake cache show

Project Cache Summary
=====================
Project: neural-network-trainer
Type: Python package
Cache Size: 45 KB
Last Updated: 2 hours ago

Cached Information:
  • Project structure (127 files)
  • Dependencies (23 packages)
  • Build system (setuptools)
  • Test configuration (pytest)
  • README instructions (5 steps)
  • Git metadata

$ cyxmake cache export --format json > project-info.json
$ cyxmake cache export --format yaml > project-info.yaml
$ cyxmake cache export --format text > project-info.txt
```

### Clear Cache

```bash
$ cyxmake cache clear

Clear project cache?
This will force re-analysis on next build.
Cache location: .cyxmake/cache.json

Continue? [y/N]: y

✓ Cache cleared
Run 'cyxmake init' to rebuild cache.

$ cyxmake cache clear --global

Clear ALL CyxMake caches?
This includes:
  • Project caches (~/.cyxmake/cache/)
  • Tool caches
  • Model caches (LARGE: 3.2 GB)
  • Build logs

Continue? [y/N]: n

Cancelled.
```

### Cache Statistics

```bash
$ cyxmake cache stats --global

CyxMake Global Cache Statistics
================================

Project Caches:
  • Active projects: 12
  • Total cache size: 2.3 MB
  • Oldest cache: 14 days (project: old-script)

Tool Caches:
  • Installed tools: 8
  • Cache size: 45 MB

Model Caches:
  • Downloaded models: 2
    - codellama-7b-instruct-q4.gguf (3.2 GB)
    - phi-3-mini-q4.gguf (1.8 GB)
  • Total size: 5.0 GB

Build Logs:
  • Logs: 145 files
  • Size: 23 MB
  • Oldest: 30 days

Recommendations:
  • Clean old project caches (>30 days): cyxmake cache clean --old
  • Archive old logs: cyxmake logs archive
  • Free up 4.8 GB by removing unused models
```

---

## Advanced Features

### 1. Watch Mode (Auto-rebuild on Changes)

```bash
$ cyxmake watch

CyxMake Watch Mode
==================
Watching for file changes...

Changes detected:
  • src/neural_net.py (modified)

Rebuilding...
✓ Build successful (3.2s)

Watching for file changes...

[Press Ctrl+C to stop]
```

### 2. Explain Mode (Educational)

```bash
$ cyxmake build --explain

CyxMake Build (with explanations)
==================================

Step 1: Creating virtual environment
────────────────────────────────────
Command: python -m venv venv

Why: Python projects should use isolated environments to avoid
     dependency conflicts between projects. This creates a
     "venv" directory with its own Python interpreter and packages.

Executing...
✓ Completed (2.3s)

Step 2: Activating virtual environment
───────────────────────────────────────
Command: source venv/bin/activate

Why: Activating the venv ensures all subsequent pip installs
     and python commands use the isolated environment.

Executing...
✓ Completed (0.1s)

[...continues with explanations...]
```

### 3. Dry-run Mode

```bash
$ cyxmake build --dry-run

CyxMake Build Plan (Dry Run)
=============================

The following actions would be performed:

1. Create virtual environment
   Command: python -m venv venv
   Estimated time: 2-5 seconds

2. Activate virtual environment
   Command: source venv/bin/activate
   Estimated time: < 1 second

3. Install missing dependencies (8 packages)
   Command: pip install torch torchvision ...
   Estimated time: 2-3 minutes
   Download size: ~1.2 GB

4. Install package in development mode
   Command: pip install -e .
   Estimated time: 3-5 seconds

5. Verify installation
   Command: python -c "import neural_network_trainer"
   Estimated time: 1-2 seconds

Total estimated time: 3-4 minutes
Total download size: 1.2 GB

No actions performed (dry run).
Run 'cyxmake build' to execute.
```

---

## Best Practices

### 1. Initialize Early

```bash
# Good: Initialize right after cloning
$ git clone <repo>
$ cd <repo>
$ cyxmake init
$ cyxmake build

# Avoid: Trying to build without init
$ git clone <repo>
$ cd <repo>
$ cyxmake build  # Will auto-init, but slower
```

### 2. Commit the Cache (Optional)

```bash
# .gitignore
.cyxmake/logs/
.cyxmake/temp/
# .cyxmake/cache.json  <- Don't ignore this

# Benefits of committing cache:
# - Faster CI builds (no re-analysis)
# - Consistent understanding across team
# - Historical record of project structure
```

### 3. Regular Doctor Checks

```bash
# Add to your routine
$ cyxmake doctor

# Or set up as git pre-push hook
# .git/hooks/pre-push
#!/bin/bash
cyxmake doctor --strict || exit 1
```

### 4. Use Workspaces for Monorepos

```bash
# Instead of multiple inits:
$ cd backend && cyxmake init
$ cd ../frontend && cyxmake init
$ cd ../mobile && cyxmake init

# Use workspace:
$ cd project-root
$ cyxmake init --workspace
```

---

## Comparison with Traditional Workflows

### Traditional Workflow (CMake Example)

```bash
# 1. Read README
$ cat README.md

# 2. Install dependencies manually
$ sudo apt install libsdl2-dev libglm-dev
$ git clone https://github.com/g-truc/glm.git external/glm

# 3. Configure CMake
$ mkdir build && cd build
$ cmake ..
# ERROR: Could not find SDL2

# 4. Debug (Google search: "cmake find SDL2 ubuntu")
$ cmake .. -DSDL2_DIR=/usr/lib/x86_64-linux-gnu/cmake/SDL2
# ERROR: OpenGL not found

# 5. More debugging...
$ sudo apt install libgl1-mesa-dev
$ cmake ..
# SUCCESS

# 6. Build
$ make -j$(nproc)
# ERROR: undefined reference to 'glad_init'

# 7. Even more debugging...
[30 minutes later...]

# Total time: 45+ minutes
```

### CyxMake Workflow

```bash
$ cyxmake init
$ cyxmake build

# If errors occur, they're fixed automatically
# Total time: 3-5 minutes
```

**Time saved per project**: 30-40 minutes

---

## Conclusion

CyxMake's init and caching system provides:

1. **Instant project understanding** - Deep analysis in seconds
2. **Persistent intelligence** - Cache enables fast subsequent operations
3. **Autonomous adaptation** - Detects changes, updates cache automatically
4. **Error context** - Rich project knowledge helps diagnose issues
5. **Team consistency** - Same cache = same understanding across developers
6. **CI/CD efficiency** - Cache commits enable zero-config builds

The init workflow transforms from:
```
Clone → Read docs → Install tools → Debug → Build (30-60 min)
```

To:
```
Clone → cyxmake init → cyxmake build (2-5 min)
```

**Next**: Try CyxMake on your next project and experience AI-powered build automation!

---

**Document Version**: 1.0
**Last Updated**: 2025-11-24
**Status**: Ready for User Testing
