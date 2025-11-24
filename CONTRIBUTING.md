# Contributing to CyxMake

Thank you for your interest in contributing to CyxMake! This document provides guidelines and information for contributors.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How Can I Contribute?](#how-can-i-contribute)
- [Development Setup](#development-setup)
- [Project Structure](#project-structure)
- [Coding Standards](#coding-standards)
- [Submitting Changes](#submitting-changes)
- [Areas We Need Help](#areas-we-need-help)

---

## Code of Conduct

This project adheres to a code of conduct. By participating, you are expected to uphold this code:

- Be respectful and inclusive
- Welcome newcomers
- Focus on what is best for the community
- Show empathy towards others

---

## How Can I Contribute?

### Reporting Bugs

Before creating bug reports, please check existing issues. When you create a bug report, include:

- **Clear title and description**
- **Steps to reproduce**
- **Expected vs actual behavior**
- **Environment** (OS, CyxMake version, etc.)
- **Logs** (if applicable)

### Suggesting Features

Feature suggestions are welcome! Please:

- Check if the feature is already requested
- Provide clear use cases
- Explain why this would be useful
- Consider implementation complexity

### Contributing Code

1. **Fork the repository**
2. **Create a feature branch** (`git checkout -b feature/amazing-feature`)
3. **Write your code** (follow coding standards)
4. **Write tests** for your changes
5. **Commit your changes** (`git commit -m 'Add amazing feature'`)
6. **Push to your fork** (`git push origin feature/amazing-feature`)
7. **Open a Pull Request**

### Contributing Tools

CyxMake's power comes from its tool ecosystem. You can contribute new tools:

- See [Tool Development Guide](docs/tool-development.md)
- Tools can be written in any language (C, Python, Go, Rust, etc.)
- Each tool must have a `tool.toml` manifest
- Follow the standard tool interface (JSON stdin/stdout)

### Contributing Documentation

Documentation improvements are always welcome:

- Fix typos
- Clarify confusing sections
- Add examples
- Translate to other languages

---

## Development Setup

### Prerequisites

- **C Compiler**: GCC 9+, Clang 10+, or MSVC 2019+
- **CMake**: 3.20 or later
- **Git**: For version control
- **libcurl**: HTTP client library

### Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install build-essential cmake git libcurl4-openssl-dev
```

### macOS

```bash
brew install cmake git curl
```

### Windows

Install Visual Studio 2019+ with C++ tools, or:

```bash
choco install cmake git curl
```

### Building from Source

```bash
# Clone repository
git clone https://github.com/cyxmake/cyxmake.git
cd cyxmake

# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCYXMAKE_BUILD_TESTS=ON

# Build
cmake --build .

# Run tests
ctest --output-on-failure

# Install (optional)
sudo cmake --install .
```

### Running Tests

```bash
cd build
ctest --verbose

# Run specific test
./tests/unit/test_project_context

# With sanitizers
cmake .. -DCYXMAKE_USE_SANITIZERS=ON
cmake --build .
ctest
```

---

## Project Structure

```
cyxmake/
├── src/
│   ├── core/           # Core orchestrator, config, logging
│   ├── context/        # Project context manager, cache
│   ├── recovery/       # Error diagnosis and recovery
│   ├── llm/            # LLM integration (local + cloud)
│   ├── tools/          # Tool registry and execution
│   └── cli/            # Command-line interface
├── include/
│   └── cyxmake/        # Public headers
├── tests/
│   ├── unit/           # Unit tests
│   └── integration/    # Integration tests
├── tools/              # Bundled tools
│   ├── python_analyzer/
│   ├── bash_executor/
│   └── cmake_parser/
├── docs/               # Documentation
├── external/           # Third-party dependencies
└── cmake/              # CMake modules
```

---

## Coding Standards

### C Code Style

- **Indentation**: 4 spaces (no tabs)
- **Naming**:
  - Functions: `snake_case` (e.g., `project_analyze`)
  - Types: `PascalCase` (e.g., `ProjectContext`)
  - Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_PATH_LEN`)
  - File scope: `static` with `s_` prefix (e.g., `s_tool_count`)
- **Line length**: 100 characters max
- **Braces**: K&R style (opening brace on same line)
- **Comments**: Doxygen style for public APIs

### Example

```c
/**
 * @brief Analyze project structure
 * @param root_path Path to project root
 * @return ProjectContext or NULL on failure
 */
ProjectContext* project_analyze(const char* root_path) {
    if (!root_path) {
        log_error("Invalid root_path");
        return NULL;
    }

    ProjectContext* ctx = calloc(1, sizeof(ProjectContext));
    if (!ctx) {
        log_error("Memory allocation failed");
        return NULL;
    }

    // Implementation...
    return ctx;
}
```

### Error Handling

- Use return codes for errors (see `CyxMakeError`)
- Log errors with context
- Clean up resources on error paths
- Never return uninitialized values

### Memory Management

- Use `calloc` for zero-initialization
- Always check allocation results
- Free resources in reverse allocation order
- Use RAII pattern where applicable (cleanup functions)

### Testing

- Write unit tests for new functions
- Aim for 80%+ code coverage
- Test edge cases and error paths
- Use descriptive test names

---

## Submitting Changes

### Pull Request Process

1. **Update documentation** if you changed APIs
2. **Add tests** for new functionality
3. **Ensure all tests pass** locally
4. **Follow commit message conventions**
5. **Request review** from maintainers

### Commit Message Format

```
type(scope): brief description

Detailed description of the change (optional).

Fixes #123
```

**Types**:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation only
- `refactor`: Code change without feature/bug
- `test`: Adding or updating tests
- `chore`: Maintenance tasks

**Example**:
```
feat(llm): add local model support via llama.cpp

Integrate llama.cpp for local LLM inference. Supports
GGUF quantized models with 4-bit and 8-bit quantization.

Closes #45
```

### Code Review Checklist

Before requesting review, ensure:

- [ ] Code follows project style guide
- [ ] Tests are included and passing
- [ ] Documentation is updated
- [ ] Commit messages are clear
- [ ] No unrelated changes included
- [ ] Branch is up to date with main

---

## Areas We Need Help

### High Priority

1. **Tool Development**
   - Gradle tool (Java/Kotlin)
   - Bazel tool (Google's build system)
   - Meson tool (modern build system)
   - Go build tool

2. **Platform Support**
   - Windows native (currently WSL only)
   - macOS ARM (Apple Silicon) optimization
   - FreeBSD support

3. **LLM Integration**
   - Fine-tuning dataset collection
   - Prompt engineering
   - Model evaluation benchmarks

### Medium Priority

4. **Testing**
   - Test on diverse projects
   - Add integration tests
   - Performance benchmarks

5. **Documentation**
   - Tutorials for different languages
   - Video guides
   - API documentation

6. **Features**
   - Dependency vulnerability scanning
   - Build caching strategies
   - Parallel build optimization

### Good First Issues

Look for issues tagged `good-first-issue`:

- Documentation improvements
- Add new error patterns
- Write tool tests
- Fix compiler warnings

---

## Getting Help

- **Discord**: [discord.gg/cyxmake](https://discord.gg/cyxmake)
- **Forum**: [forum.cyxmake.com](https://forum.cyxmake.com)
- **GitHub Discussions**: Ask questions, share ideas
- **Email**: [dev@cyxmake.com](mailto:dev@cyxmake.com)

---

## Recognition

Contributors are recognized in:

- [CONTRIBUTORS.md](CONTRIBUTORS.md)
- Project README
- Release notes

Significant contributions may result in:

- Maintainer status
- CyxMake swag
- Conference travel sponsorship (for major contributions)

---

## License

By contributing, you agree that your contributions will be licensed under the Apache License 2.0, the same license as the project.

---

**Thank you for contributing to CyxMake!**

Together, we're ending build system suffering. 🚀
