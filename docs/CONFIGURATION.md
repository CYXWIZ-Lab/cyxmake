# CyxMake Configuration Guide

> Complete guide to configuring CyxMake for your environment

## Table of Contents

1. [Configuration File Location](#configuration-file-location)
2. [AI Provider Configuration](#ai-provider-configuration)
3. [Project Settings](#project-settings)
4. [Build Settings](#build-settings)
5. [Permission Settings](#permission-settings)
6. [Logging Settings](#logging-settings)
7. [Multi-Agent Configuration](#multi-agent-configuration)
8. [Security Configuration](#security-configuration)
9. [Environment Variables](#environment-variables)

---

## Configuration File Location

CyxMake searches for configuration in these locations (in order):

1. `./cyxmake.toml` - Project-specific configuration
2. `./.cyxmake/config.toml` - Project configuration directory
3. `~/.cyxmake/config.toml` - User-specific configuration
4. `~/.config/cyxmake/config.toml` - XDG-compliant location

### Creating a Configuration File

```bash
# Create project-specific configuration
cp /path/to/cyxmake/cyxmake.toml.example ./cyxmake.toml

# Or create a minimal configuration
cat > cyxmake.toml << 'EOF'
[ai]
default_provider = "ollama"

[ai.providers.ollama]
enabled = true
type = "ollama"
base_url = "http://localhost:11434"
model = "llama2"

[build]
type = "Debug"
build_dir = "build"
EOF
```

---

## AI Provider Configuration

CyxMake supports multiple AI providers for natural language processing and error analysis.

### Basic AI Settings

```toml
[ai]
# Default provider to use (must match a provider name below)
default_provider = "ollama"

# Fallback provider if the default fails (optional)
fallback_provider = "openai"

# Request timeout in seconds (increase for slow local models)
timeout = 300

# Maximum tokens for responses
max_tokens = 1024

# Temperature for generation (0.0 = deterministic, 1.0 = creative)
temperature = 0.7
```

### Provider: Ollama (Local)

Ollama runs models locally with no API key required.

**Installation:** https://ollama.ai

```bash
# Install a model
ollama pull llama2
ollama pull codellama
```

```toml
[ai.providers.ollama]
enabled = true
type = "ollama"
base_url = "http://localhost:11434"
model = "llama2"
```

**Recommended Models:**
| Model | Use Case | Memory |
|-------|----------|--------|
| llama2 | General purpose | ~4GB |
| codellama | Code generation | ~4GB |
| mistral | Fast, high quality | ~4GB |
| mixtral | Complex reasoning | ~26GB |

---

### Provider: OpenAI

**Get API Key:** https://platform.openai.com/api-keys

```toml
[ai.providers.openai]
enabled = true
type = "openai"
api_key = "${OPENAI_API_KEY}"  # Use environment variable
base_url = "https://api.openai.com/v1"
model = "gpt-4o-mini"
```

**Available Models:**
| Model | Speed | Cost |
|-------|-------|------|
| gpt-4o | Medium | High |
| gpt-4o-mini | Fast | Low |
| gpt-4-turbo | Medium | Medium |

---

### Provider: Anthropic Claude

**Get API Key:** https://console.anthropic.com/

```toml
[ai.providers.anthropic]
enabled = true
type = "anthropic"
api_key = "${ANTHROPIC_API_KEY}"
base_url = "https://api.anthropic.com/v1"
model = "claude-3-haiku-20240307"
```

**Available Models:**
| Model | Best For |
|-------|----------|
| claude-3-haiku | Fast responses |
| claude-3-5-sonnet | Code and reasoning |
| claude-3-opus | Complex analysis |

---

### Provider: Google Gemini

**Get API Key:** https://makersuite.google.com/app/apikey

```toml
[ai.providers.gemini]
enabled = true
type = "gemini"
api_key = "${GEMINI_API_KEY}"
base_url = "https://generativelanguage.googleapis.com/v1beta"
model = "gemini-1.5-flash"
```

---

### Provider: OpenRouter (Multi-Model)

OpenRouter provides access to many models through one API.

**Get API Key:** https://openrouter.ai/keys

```toml
[ai.providers.openrouter]
enabled = true
type = "openai"  # OpenRouter uses OpenAI-compatible API
api_key = "${OPENROUTER_API_KEY}"
base_url = "https://openrouter.ai/api/v1"
model = "meta-llama/llama-3.1-8b-instruct:free"

# Optional: Add site info for rankings
[ai.providers.openrouter.headers]
"HTTP-Referer" = "https://myproject.dev"
"X-Title" = "My Project"
```

**Popular Models:**
- `anthropic/claude-3.5-sonnet`
- `google/gemini-pro-1.5`
- `meta-llama/llama-3.1-70b-instruct`
- `openai/gpt-4o-mini`

---

### Provider: Groq (Fast Inference)

**Get API Key:** https://console.groq.com/

```toml
[ai.providers.groq]
enabled = true
type = "openai"
api_key = "${GROQ_API_KEY}"
base_url = "https://api.groq.com/openai/v1"
model = "llama-3.1-70b-versatile"
```

---

### Provider: Local llama.cpp

Run a local GGUF model with llama.cpp.

```toml
[ai.providers.local]
enabled = true
type = "llamacpp"
model_path = "/path/to/model.gguf"
context_size = 4096
gpu_layers = 0  # 0 = CPU only
threads = 4
```

**Getting Models:**
```bash
# Download from Hugging Face
# Example: https://huggingface.co/TheBloke/Llama-2-7B-Chat-GGUF
# Download a Q4_K_M or Q5_K_M quantization for good quality/speed balance
```

---

### Provider: Custom OpenAI-Compatible Server

Works with LM Studio, vLLM, text-generation-webui, etc.

```toml
[ai.providers.custom]
enabled = true
type = "openai"
api_key = "not-needed"  # Many local servers don't need a key
base_url = "http://localhost:1234/v1"  # LM Studio default
model = "local-model"
```

---

## Project Settings

```toml
[project]
# Project name (auto-detected from directory if not set)
name = "my-project"

# Primary language (auto-detected from files if not set)
# Options: "c", "c++", "rust", "python", "javascript", "typescript"
language = "c++"

# Build system (auto-detected if not set)
# Options: "cmake", "make", "meson", "cargo", "npm"
build_system = "cmake"
```

**Auto-Detection:**

CyxMake automatically detects:
- Language from file extensions (.c, .cpp, .rs, .py, .js)
- Build system from configuration files (CMakeLists.txt, Makefile, Cargo.toml)
- Dependencies from package files

---

## Build Settings

```toml
[build]
# Default build type
# Options: "Debug", "Release", "RelWithDebInfo", "MinSizeRel"
type = "Debug"

# Build directory (relative to project root)
build_dir = "build"

# Number of parallel jobs
# 0 = auto (uses number of CPU cores)
# n = fixed number of parallel jobs
parallel_jobs = 0

# Clean before building
# true = always run clean first
# false = incremental build
clean_first = false
```

**CMake-Specific Options:**

```toml
[build.cmake]
# Additional CMake arguments
args = ["-DENABLE_TESTS=ON", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]

# CMake generator (auto-detected if not set)
# Options: "Ninja", "Unix Makefiles", "Visual Studio 17 2022"
generator = "Ninja"
```

---

## Permission Settings

Control what actions require user confirmation.

```toml
[permissions]
# Auto-approve safe operations without asking
auto_approve_read = true      # Reading files
auto_approve_build = true     # Running build commands
auto_approve_list = true      # Listing directories

# Always ask for dangerous operations
always_confirm_delete = true   # Deleting files
always_confirm_install = true  # Installing packages
always_confirm_command = true  # Running shell commands

# Remember user choices for this session
# true = remember "Yes to all" / "No to all" choices
remember_choices = true
```

**Permission Levels:**

| Level | Description | Examples |
|-------|-------------|----------|
| SAFE | No confirmation needed | Read files, analyze |
| ASK | Y/N prompt | Create/modify files, run commands |
| DANGEROUS | Warning + confirm | Delete files, install packages |
| BLOCKED | Always denied | System file access |

---

## Logging Settings

```toml
[logging]
# Log level: trace, debug, info, warning, error
level = "info"

# Enable colored output (recommended for terminals)
colors = true

# Show timestamps in logs
timestamps = false

# Log file path (optional)
# If set, logs are also written to this file
file = ".cyxmake/cyxmake.log"
```

**Log Levels:**

| Level | Description |
|-------|-------------|
| trace | Maximum detail (very verbose) |
| debug | Debugging information |
| info | Normal operation messages |
| warning | Potential issues |
| error | Errors only |

---

## Multi-Agent Configuration

Configure the multi-agent system for complex workflows.

```toml
[agents]
# Enable multi-agent system
enabled = true

# Maximum concurrent agents
max_concurrent = 4

# Thread pool size (0 = auto based on CPU cores)
thread_pool_size = 0

# Shared memory file for agent coordination
shared_memory_path = ".cyxmake/agent_memory.json"

[agents.defaults]
# Default timeout for agent tasks (seconds)
timeout_sec = 300

# Verbose agent output
verbose = false

# Pre-defined named agents
[agents.definitions.builder]
type = "build"
description = "Primary build agent"

[agents.definitions.fixer]
type = "smart"
description = "Error diagnosis specialist"
```

**Agent Types:**

| Type | Description | Capabilities |
|------|-------------|--------------|
| build | Build orchestration | Build, execute commands |
| smart | Reasoning and planning | Analyze, reason, read/write |
| auto | Autonomous operation | All capabilities |
| coordinator | Manages other agents | Spawn, coordinate |

---

## Security Configuration

```toml
[security]
# Enable permission prompts
enable_permissions = true

# Enable audit logging
enable_audit = true

# Enable dry-run mode by default
enable_dry_run = false

# Enable rollback support
enable_rollback = true

[security.audit]
# Audit log file
log_file = ".cyxmake/audit.log"

# Log to console as well
log_to_console = false

# Minimum severity to log
# Options: "debug", "info", "warning", "action", "denied", "error", "security"
min_severity = "action"

# Include timestamps in audit log
include_timestamps = true

# Include user information
include_user = false

[security.rollback]
# Backup directory for rollback files
backup_dir = ".cyxmake/backups"

# Maximum number of rollback entries
max_entries = 100

# Maximum file size to backup in memory (bytes)
max_file_size = 1048576  # 1MB

# Backup large files to disk
backup_large_files = true

# How long to keep backups (hours, 0 = forever)
retention_hours = 24

[security.sandbox]
# Default sandbox level for command execution
# Options: "none", "light", "medium", "strict"
default_level = "light"

# Allow network access in sandbox
allow_network = true

# Allow spawning subprocesses
allow_subprocesses = true

# Resource limits
max_memory_mb = 1024      # 0 = unlimited
max_cpu_sec = 300         # 0 = unlimited
```

---

## Environment Variables

CyxMake supports environment variable substitution in configuration files.

**Syntax:** `${VARIABLE_NAME}`

```toml
[ai.providers.openai]
api_key = "${OPENAI_API_KEY}"
```

**Common Environment Variables:**

| Variable | Description |
|----------|-------------|
| `OPENAI_API_KEY` | OpenAI API key |
| `ANTHROPIC_API_KEY` | Anthropic API key |
| `GEMINI_API_KEY` | Google Gemini API key |
| `OPENROUTER_API_KEY` | OpenRouter API key |
| `GROQ_API_KEY` | Groq API key |
| `CYXMAKE_LOG_LEVEL` | Override log level |
| `CYXMAKE_CONFIG` | Override config file path |

**Setting Environment Variables:**

```bash
# Linux/macOS
export OPENAI_API_KEY="sk-..."

# Windows (Command Prompt)
set OPENAI_API_KEY=sk-...

# Windows (PowerShell)
$env:OPENAI_API_KEY = "sk-..."
```

---

## Example Configurations

### Minimal Local Setup

```toml
[ai]
default_provider = "ollama"

[ai.providers.ollama]
enabled = true
type = "ollama"
base_url = "http://localhost:11434"
model = "codellama"

[build]
type = "Debug"
```

### Production Setup with Fallback

```toml
[ai]
default_provider = "openai"
fallback_provider = "ollama"
timeout = 60
max_tokens = 2048

[ai.providers.openai]
enabled = true
type = "openai"
api_key = "${OPENAI_API_KEY}"
model = "gpt-4o-mini"

[ai.providers.ollama]
enabled = true
type = "ollama"
base_url = "http://localhost:11434"
model = "llama2"

[build]
type = "Release"
parallel_jobs = 8

[security]
enable_audit = true
enable_rollback = true

[logging]
level = "warning"
file = ".cyxmake/cyxmake.log"
```

### Security-Focused Setup

```toml
[ai]
default_provider = "local"

[ai.providers.local]
enabled = true
type = "llamacpp"
model_path = "/models/codellama-7b-Q5_K_M.gguf"

[permissions]
auto_approve_read = true
auto_approve_build = false  # Ask before building
always_confirm_delete = true
always_confirm_install = true
always_confirm_command = true

[security]
enable_permissions = true
enable_audit = true
enable_dry_run = true  # Preview all actions
enable_rollback = true

[security.sandbox]
default_level = "strict"
allow_network = false
max_memory_mb = 512
```

---

*CyxMake Configuration Guide v0.2.0*
