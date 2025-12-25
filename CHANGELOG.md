# Changelog

All notable changes to CyxMake are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2024-12-26

### 🎉 First Stable Release

CyxMake v1.0.0 marks the first production-ready release of the AI-powered build automation system.

### Added

#### Core Features
- **Natural Language Interface** - Build projects using plain English commands
- **Project Auto-Detection** - Automatically detects C/C++, Rust, Python, Node.js projects
- **Multi-Build System Support** - CMake, Make, Meson, Ninja, Cargo, npm
- **Interactive REPL** - Full command-line interface with history, tab completion, colors
- **Cross-Platform** - Windows, Linux, and macOS support

#### AI Integration
- **Local LLM Support** - llama.cpp integration for offline AI
- **Cloud AI Providers** - OpenAI, Anthropic Claude, Google Gemini, Groq, OpenRouter
- **Provider Fallback** - Automatic fallback between providers
- **Offline Mode** - Graceful degradation when AI unavailable

#### Security System (Phase 2)
- **Permission System** - SAFE, ASK, DANGEROUS, BLOCKED permission levels
- **Audit Logging** - Comprehensive action logging with export (JSON, CSV, text)
- **Dry-Run Mode** - Preview actions without execution
- **Rollback Support** - Undo file modifications
- **Sandboxed Execution** - Resource limits via OS primitives

#### Error Recovery (Phase 3)
- **Pattern Matching** - Detects 12+ common error types
- **AI Diagnosis** - LLM-powered error analysis for complex errors
- **Fix Validation** - Validates fixes before applying
- **Risk Assessment** - NONE, LOW, MEDIUM, HIGH, CRITICAL risk levels
- **Incremental Fixes** - Apply fixes one at a time with verification
- **Fix Learning** - Records successful fixes for future suggestions

#### Multi-Agent System
- **Named Agents** - Create agents with custom names and types
- **Agent Types** - Smart, Build, Autonomous, Coordinator
- **Task Assignment** - Assign tasks via `/agent assign`
- **Message Passing** - Inter-agent communication
- **Conflict Resolution** - User-prompted conflict handling
- **Shared State** - Persistent JSON storage for coordination

#### Testing & Quality (Phase 4)
- **Test Framework** - Custom lightweight C test framework
- **Security Tests** - 17 tests for audit, dry-run, rollback, permissions
- **Fix Validation Tests** - 17 tests for validation, risk, history
- **CI/CD Pipeline** - GitHub Actions for Windows, Linux, macOS
- **Memory Safety** - AddressSanitizer and Valgrind integration
- **Benchmarks** - Performance benchmarking support

#### Documentation (Phase 5)
- **User Guide** - Comprehensive tutorials and feature guides
- **API Reference** - Complete C API documentation
- **Configuration Guide** - All settings and options
- **Troubleshooting Guide** - Common problems and solutions
- **Video Tutorial Outlines** - Scripts for 5 tutorial videos

#### Ecosystem (Phase 6)
- **GitHub Actions** - Reusable action for CI/CD workflows
- **GitLab CI** - Template for GitLab pipelines
- **VS Code Extension** - Full extension with commands, diagnostics, REPL
- **JetBrains Plugin** - Plugin for IntelliJ, CLion, PyCharm
- **Plugin System** - Extensible architecture for custom tools

### Tool Discovery
- Compilers: GCC, G++, Clang, Clang++, MSVC (cl)
- Build Systems: CMake, Make, Ninja, MSBuild, Cargo, npm
- Package Managers: apt, yum, dnf, pacman, brew, vcpkg, conan, npm, yarn, pip, cargo, choco, winget
- Version Control: Git
- Linters/Formatters: clang-format, clang-tidy

### Configuration
- TOML configuration file support
- Environment variable substitution
- Per-project and user-level settings
- AI provider configuration with API keys

### REPL Commands
| Command | Alias | Description |
|---------|-------|-------------|
| `/help` | `/h` | Show help |
| `/build` | `/b` | Build project |
| `/clean` | `/c` | Clean build |
| `/status` | `/s` | Show status |
| `/analyze` | `/a` | Analyze project |
| `/config` | | Show/set configuration |
| `/agent` | | Manage agents |
| `/fix` | | Fix management |
| `/audit` | | Audit log |
| `/rollback` | | Rollback changes |
| `/exit` | `/q` | Exit REPL |

---

## [0.7.0-rc] - 2024-12-26

### Added
- Phase 6: Ecosystem integrations
- GitHub Actions integration
- GitLab CI templates
- VS Code extension scaffolding
- JetBrains plugin scaffolding
- Plugin system API

## [0.6.0] - 2024-12-26

### Added
- Phase 5: Complete documentation
- User guide with tutorials
- API reference documentation
- Configuration guide
- Troubleshooting guide

## [0.5.0] - 2024-12-26

### Added
- Phase 4: Testing & Quality
- Test framework with assertions and benchmarks
- Security system tests
- Fix validation tests
- GitHub Actions CI/CD pipeline
- Valgrind suppressions

## [0.4.0] - 2024-12-26

### Added
- Phase 3: Error Recovery enhancements
- Fix validation before applying
- Risk assessment system
- Incremental fix application
- Fix verification with rebuild
- Fix history and learning

## [0.3.0] - 2024-12-25

### Added
- Phase 2: Security & Safety
- Permission system with levels
- Audit logging
- Dry-run mode
- Rollback support
- Sandboxed execution

## [0.2.0] - 2024-12-24

### Added
- Phase 1: AI Stability
- Multiple AI provider support
- Provider health checks
- Fallback between providers
- Offline mode

### Added
- Multi-agent system
- Named agents with lifecycle
- Task queue with priorities
- Message bus for communication
- Shared state persistence
- Conflict resolution

## [0.1.0] - 2024-12-20

### Added
- Initial release
- Core orchestrator
- Project analysis
- Build execution
- Tool discovery
- Error pattern matching
- REPL interface
- Natural language parsing

---

## Migration Guide

### From v0.x to v1.0

No breaking changes. All v0.x configurations and caches are compatible.

### New Features to Try

1. **AI Error Recovery**
   ```bash
   cyxmake build  # Errors are automatically diagnosed and fixed
   ```

2. **Multi-Agent Workflows**
   ```bash
   cyxmake> /agent spawn builder build
   cyxmake> /agent assign builder "Build release"
   ```

3. **Security Features**
   ```bash
   cyxmake> /config set dry_run true
   cyxmake> build  # Preview without executing
   ```

---

[1.0.0]: https://github.com/CYXWIZ-Lab/cyxmake/releases/tag/v1.0.0
[0.7.0-rc]: https://github.com/CYXWIZ-Lab/cyxmake/releases/tag/v0.7.0-rc
[0.6.0]: https://github.com/CYXWIZ-Lab/cyxmake/releases/tag/v0.6.0
[0.5.0]: https://github.com/CYXWIZ-Lab/cyxmake/releases/tag/v0.5.0
[0.4.0]: https://github.com/CYXWIZ-Lab/cyxmake/releases/tag/v0.4.0
[0.3.0]: https://github.com/CYXWIZ-Lab/cyxmake/releases/tag/v0.3.0
[0.2.0]: https://github.com/CYXWIZ-Lab/cyxmake/releases/tag/v0.2.0
[0.1.0]: https://github.com/CYXWIZ-Lab/cyxmake/releases/tag/v0.1.0
