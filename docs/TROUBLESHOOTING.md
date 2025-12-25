# CyxMake Troubleshooting Guide

> Solutions for common problems and debugging tips

## Table of Contents

1. [Installation Issues](#installation-issues)
2. [Build Problems](#build-problems)
3. [AI Provider Issues](#ai-provider-issues)
4. [REPL Issues](#repl-issues)
5. [Error Recovery Issues](#error-recovery-issues)
6. [Multi-Agent Issues](#multi-agent-issues)
7. [Performance Issues](#performance-issues)
8. [Debugging Tips](#debugging-tips)
9. [Getting Help](#getting-help)

---

## Installation Issues

### CMake not found

**Error:**
```
CMake Error: CMake was unable to find a build program corresponding to "Unix Makefiles"
```

**Solutions:**

1. Install CMake:
   ```bash
   # Ubuntu/Debian
   sudo apt install cmake

   # macOS
   brew install cmake

   # Windows
   winget install cmake
   ```

2. Verify installation:
   ```bash
   cmake --version
   ```

---

### Build fails with missing headers

**Error:**
```
fatal error: curl/curl.h: No such file or directory
```

**Solutions:**

1. Install development packages:
   ```bash
   # Ubuntu/Debian
   sudo apt install libcurl4-openssl-dev

   # macOS
   brew install curl

   # Windows (vcpkg)
   vcpkg install curl:x64-windows
   ```

2. Set CMake prefix path if using vcpkg:
   ```bash
   cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
   ```

---

### Linker errors on Windows

**Error:**
```
LINK : fatal error LNK1104: cannot open file 'xxx.lib'
```

**Solutions:**

1. Use Visual Studio Developer Command Prompt:
   ```cmd
   "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
   ```

2. Ensure correct architecture:
   ```bash
   cmake .. -A x64
   ```

3. Check library paths:
   ```bash
   cmake .. -DCMAKE_PREFIX_PATH="C:\path\to\libraries"
   ```

---

### Permission denied during installation

**Error:**
```
CMake Error at cmake_install.cmake: file INSTALL cannot copy file
```

**Solutions:**

1. Use sudo (Linux/macOS):
   ```bash
   sudo cmake --install .
   ```

2. Install to user directory:
   ```bash
   cmake --install . --prefix ~/.local
   ```

3. Add to PATH:
   ```bash
   export PATH="$HOME/.local/bin:$PATH"
   ```

---

## Build Problems

### Project not detected

**Symptoms:**
- "Could not detect project type"
- "No build system found"

**Solutions:**

1. Verify project structure:
   ```bash
   ls -la
   # Should show CMakeLists.txt, Makefile, Cargo.toml, etc.
   ```

2. Initialize project:
   ```bash
   cyxmake init
   ```

3. Specify project type manually:
   ```toml
   # cyxmake.toml
   [project]
   build_system = "cmake"
   language = "c++"
   ```

---

### Build fails immediately

**Symptoms:**
- "Build failed: exit code 1"
- No output from build command

**Solutions:**

1. Enable debug logging:
   ```bash
   export CYXMAKE_LOG_LEVEL=debug
   cyxmake build
   ```

2. Check cache:
   ```bash
   cat .cyxmake/cache.json
   ```

3. Clear cache and retry:
   ```bash
   rm -rf .cyxmake/
   cyxmake init
   cyxmake build
   ```

4. Run build manually:
   ```bash
   mkdir build && cd build
   cmake .. && cmake --build .
   ```

---

### CMake configuration fails

**Error:**
```
CMake Error at CMakeLists.txt:5 (find_package):
  Could not find a package configuration file provided by "SDL2"
```

**Solutions:**

1. Install the missing package:
   ```bash
   # The error message often suggests what to install
   sudo apt install libsdl2-dev  # Ubuntu/Debian
   brew install sdl2             # macOS
   vcpkg install sdl2            # Windows
   ```

2. Set CMAKE_PREFIX_PATH:
   ```bash
   export CMAKE_PREFIX_PATH="/usr/local/opt/sdl2"
   cyxmake build
   ```

3. Let CyxMake auto-fix:
   ```bash
   cyxmake build  # AI will detect and suggest fixes
   ```

---

### Undefined reference errors

**Error:**
```
undefined reference to `function_name'
```

**Solutions:**

1. Check library linking order:
   - Libraries should be listed after the targets that use them

2. Add missing libraries in CMakeLists.txt:
   ```cmake
   target_link_libraries(mytarget PRIVATE missing_library)
   ```

3. Let CyxMake diagnose:
   ```bash
   cyxmake> fix the build errors
   ```

---

## AI Provider Issues

### Connection to AI provider failed

**Error:**
```
Error: Failed to connect to AI provider: Connection refused
```

**Solutions:**

1. Check if local server is running:
   ```bash
   # For Ollama
   ollama list
   ollama serve  # If not running

   # For LM Studio
   # Start the server in the UI
   ```

2. Verify URL in config:
   ```toml
   [ai.providers.ollama]
   base_url = "http://localhost:11434"  # Check port
   ```

3. Test connection:
   ```bash
   curl http://localhost:11434/api/tags
   ```

---

### API key invalid

**Error:**
```
Error: 401 Unauthorized - Invalid API key
```

**Solutions:**

1. Check environment variable:
   ```bash
   echo $OPENAI_API_KEY
   ```

2. Set environment variable:
   ```bash
   export OPENAI_API_KEY="sk-..."
   ```

3. Check config file syntax:
   ```toml
   api_key = "${OPENAI_API_KEY}"  # Use env var
   # NOT: api_key = "$OPENAI_API_KEY"  # Wrong syntax
   ```

---

### Model not found

**Error:**
```
Error: Model 'llama2' not found
```

**Solutions:**

1. Pull the model (Ollama):
   ```bash
   ollama pull llama2
   ```

2. List available models:
   ```bash
   ollama list
   ```

3. Check model name in config:
   ```toml
   model = "llama2"  # Exact name from `ollama list`
   ```

---

### Slow AI responses

**Symptoms:**
- Timeouts during processing
- Very slow responses

**Solutions:**

1. Increase timeout:
   ```toml
   [ai]
   timeout = 600  # 10 minutes
   ```

2. Use a smaller model:
   ```toml
   model = "llama2:7b"  # Instead of 70b
   ```

3. Enable GPU acceleration:
   ```toml
   [ai.providers.local]
   gpu_layers = 35  # Offload layers to GPU
   ```

4. Use faster provider:
   ```toml
   default_provider = "groq"  # Fast inference
   ```

---

## REPL Issues

### REPL doesn't start

**Error:**
```
Error: Failed to initialize REPL
```

**Solutions:**

1. Check terminal capabilities:
   ```bash
   echo $TERM
   # Should be xterm-256color or similar
   ```

2. Try with basic mode:
   ```bash
   cyxmake --no-color
   ```

3. Check for conflicting processes:
   ```bash
   ps aux | grep cyxmake
   ```

---

### Arrow keys don't work

**Symptoms:**
- ^[[A appears instead of command history
- Cursor doesn't move

**Solutions:**

1. Check terminal type:
   ```bash
   export TERM=xterm-256color
   ```

2. On Windows, use Windows Terminal instead of cmd.exe

3. Check readline is enabled (Linux):
   ```bash
   sudo apt install libreadline-dev
   # Rebuild CyxMake
   ```

---

### Tab completion not working

**Solutions:**

1. Ensure you're pressing Tab after `/`:
   ```
   cyxmake> /bu<Tab>  # Completes to /build
   ```

2. For file completion, provide partial path:
   ```
   cyxmake> /build src/<Tab>
   ```

---

## Error Recovery Issues

### Fix not applied

**Symptoms:**
- "Fix suggested but not applied"
- Error persists after fix

**Solutions:**

1. Check permissions:
   ```toml
   [permissions]
   auto_approve_build = true
   remember_choices = true
   ```

2. Apply fix manually:
   ```bash
   cyxmake> /fix apply 1  # Apply first suggestion
   ```

3. Review fix details:
   ```bash
   cyxmake> /fix details
   ```

---

### Recovery loop

**Symptoms:**
- Same error keeps recurring
- Max retries reached

**Solutions:**

1. Check fix history:
   ```bash
   cyxmake> /fix history
   ```

2. Clear fix history:
   ```bash
   rm .cyxmake/fix_history.json
   ```

3. Try manual fix:
   ```bash
   cyxmake> what's causing the error?
   # Then fix manually based on diagnosis
   ```

---

### Package installation fails

**Error:**
```
Error: Failed to install package: Permission denied
```

**Solutions:**

1. Use sudo (Linux/macOS):
   ```bash
   # CyxMake will prompt for sudo if needed
   cyxmake> install sdl2 with sudo
   ```

2. Use user package manager:
   ```bash
   # vcpkg or pip with --user
   pip install --user package_name
   ```

3. Configure package manager:
   ```toml
   [recovery]
   use_sudo = false  # Disable sudo attempts
   preferred_package_manager = "vcpkg"
   ```

---

## Multi-Agent Issues

### Agent doesn't start

**Error:**
```
Error: Failed to spawn agent 'builder'
```

**Solutions:**

1. Check thread pool:
   ```toml
   [agents]
   enabled = true
   max_concurrent = 4
   thread_pool_size = 0  # 0 = auto
   ```

2. Check agent type:
   ```bash
   cyxmake> /agent spawn builder build
   # Valid types: smart, build, auto
   ```

3. Verify AI provider:
   - Agents require working AI provider

---

### Agent conflict

**Symptoms:**
- "Conflict detected between agents"
- Operations blocked

**Solutions:**

1. Wait for resolution prompt and choose:
   ```
   Which should proceed first? [1/2/both/cancel]
   ```

2. Terminate conflicting agent:
   ```bash
   cyxmake> /agent terminate conflicting_agent
   ```

3. Use sequential execution:
   ```bash
   cyxmake> /agent wait builder
   cyxmake> /agent assign fixer "next task"
   ```

---

### Shared state corrupted

**Error:**
```
Error: Failed to load shared state
```

**Solutions:**

1. Reset shared state:
   ```bash
   rm .cyxmake/agent_memory.json
   ```

2. Restart REPL:
   ```bash
   cyxmake> exit
   cyxmake
   ```

---

## Performance Issues

### Slow project analysis

**Symptoms:**
- `cyxmake init` takes minutes
- High CPU usage during analysis

**Solutions:**

1. Exclude large directories:
   ```toml
   [analysis]
   exclude = ["node_modules", "build", ".git", "vendor"]
   ```

2. Limit file scanning:
   ```toml
   [analysis]
   max_files = 10000
   max_depth = 10
   ```

3. Use incremental analysis:
   ```bash
   cyxmake> /analyze --incremental
   ```

---

### High memory usage

**Solutions:**

1. Reduce model size:
   ```toml
   [ai.providers.local]
   context_size = 2048  # Reduce from 4096
   ```

2. Limit concurrent operations:
   ```toml
   [agents]
   max_concurrent = 2
   ```

3. Disable caching for large projects:
   ```toml
   [cache]
   max_size_mb = 50
   ```

---

### Slow builds

**Solutions:**

1. Increase parallel jobs:
   ```toml
   [build]
   parallel_jobs = 8  # Or 0 for auto
   ```

2. Use Ninja instead of Make:
   ```bash
   cmake .. -G Ninja
   ```

3. Enable ccache:
   ```bash
   export CMAKE_CXX_COMPILER_LAUNCHER=ccache
   ```

---

## Debugging Tips

### Enable debug logging

```bash
# Via environment variable
export CYXMAKE_LOG_LEVEL=debug
cyxmake build

# Via config file
[logging]
level = "debug"
```

### View detailed diagnostics

```bash
# In REPL
cyxmake> /status --verbose
cyxmake> /config show

# View cache
cat .cyxmake/cache.json | jq .

# View audit log
cat .cyxmake/audit.log
```

### Test AI connection

```bash
# Test Ollama
curl http://localhost:11434/api/generate -d '{
  "model": "llama2",
  "prompt": "Hello",
  "stream": false
}'

# Test OpenAI
curl https://api.openai.com/v1/chat/completions \
  -H "Authorization: Bearer $OPENAI_API_KEY" \
  -d '{"model": "gpt-4o-mini", "messages": [{"role": "user", "content": "Hello"}]}'
```

### Run in dry-run mode

```bash
cyxmake> /config set dry_run true
cyxmake> build
# Shows what would happen without executing
```

### Check tool discovery

```bash
# Run test to see discovered tools
./build/bin/test_tool_executor

# Manual check
which cmake
which gcc
```

---

## Getting Help

### Check logs

```bash
# View recent log entries
tail -50 .cyxmake/cyxmake.log

# Search for errors
grep -i error .cyxmake/cyxmake.log
```

### In-app help

```bash
cyxmake> help
cyxmake> /help build
cyxmake> what commands are available?
```

### Report issues

1. Gather diagnostic information:
   ```bash
   cyxmake --version
   cmake --version
   echo $CYXMAKE_LOG_LEVEL
   ```

2. Collect logs:
   ```bash
   cat .cyxmake/cyxmake.log
   cat .cyxmake/cache.json
   ```

3. Report at: https://github.com/CYXWIZ-Lab/cyxmake/issues

### Community Resources

- **Issues**: https://github.com/CYXWIZ-Lab/cyxmake/issues
- **Documentation**: https://github.com/CYXWIZ-Lab/cyxmake/tree/main/docs
- **Examples**: https://github.com/CYXWIZ-Lab/cyxmake/tree/main/examples

---

*CyxMake Troubleshooting Guide v0.2.0*
