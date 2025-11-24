# CyxMake LLM Integration Decisions

## Overview

This document captures the key technical decisions for integrating llama.cpp and Qwen2.5-Coder-3B into CyxMake. These decisions balance ease of implementation, user experience, performance, and maintainability.

**Status:** Decision phase (before implementation)
**Target Model:** Qwen2.5-Coder-3B-Instruct-Q4_K_M.gguf (1.8 GB)
**Target Performance:** < 2 seconds inference time

---

## Decision 1: llama.cpp Integration Method

### The Question
How should we integrate llama.cpp into our build system?

### Options Analysis

#### Option A: Git Submodule (RECOMMENDED ✅)
```bash
git submodule add https://github.com/ggerganov/llama.cpp external/llama.cpp
```

**CMake Integration:**
```cmake
# Add llama.cpp as subdirectory
add_subdirectory(external/llama.cpp)

target_link_libraries(cyxmake_core PUBLIC
    llama
    # llama.cpp provides: llama, common, ggml
)
```

**Pros:**
- Maximum control over build configuration
- Can apply patches if needed
- Always up-to-date with llama.cpp development
- CMake integration is straightforward
- Consistent across all platforms
- Developers get exact version we tested with

**Cons:**
- Increases repository clone size
- Requires git submodule commands
- Longer initial build time

**Implementation Complexity:** Medium (5/10)

#### Option B: System Package
```bash
# Users install llama.cpp themselves
sudo apt install llama.cpp  # Hypothetical
```

**Pros:**
- Smaller repository size
- Shared library benefits
- System-wide updates

**Cons:**
- llama.cpp not available in most package managers yet
- Version inconsistencies across systems
- Difficult to specify exact version requirements
- Breaks "clone and build" simplicity

**Implementation Complexity:** High (8/10)

#### Option C: Download Pre-built Binaries
**Pros:**
- Fast build times
- No compilation needed

**Cons:**
- Must maintain binaries for multiple platforms
- Security concerns (binary trust)
- Harder to debug issues
- Can't customize build flags

**Implementation Complexity:** Low (3/10) but high maintenance burden

### DECISION: Git Submodule ✅

**Rationale:**
- Provides maximum control and consistency
- llama.cpp is actively developed; submodule keeps us current
- CMake integration is well-documented
- Acceptable build time increase (llama.cpp builds in ~2-3 minutes)
- Ensures all users have identical llama.cpp version

**Implementation Steps:**
1. Add submodule: `git submodule add https://github.com/ggerganov/llama.cpp external/llama.cpp`
2. Update root CMakeLists.txt to add subdirectory
3. Link cyxmake_core against llama libraries
4. Document submodule initialization in README

---

## Decision 2: Model Download Strategy

### The Question
How should users obtain the Qwen2.5-Coder-3B-Q4_K_M model (1.8 GB)?

### Options Analysis

#### Option A: Bundle with Release (❌ Not Recommended)
```
cyxmake-v1.0.0-windows-x64.zip  (2.1 GB)
├── bin/cyxmake.exe
├── models/qwen2.5-coder-3b-q4_k_m.gguf
└── README.txt
```

**Pros:**
- Works immediately after installation
- No extra steps for users
- Guaranteed to have model

**Cons:**
- Massive download size (2+ GB for every release)
- Many users may not want AI features
- Storage waste if multiple versions installed
- Slow distribution via GitHub releases

**Implementation Complexity:** Low (2/10)

#### Option B: Separate Download Command (⚠️ Middle Ground)
```bash
cyxmake download-model qwen2.5-coder-3b
```

**Pros:**
- Keeps installer small
- Users opt-in to AI features
- Can update model independently
- Can offer multiple models

**Cons:**
- Extra step for users
- Requires internet connection
- Must implement download infrastructure

**Implementation Complexity:** Medium (6/10)

#### Option C: Automatic On-Demand Download (RECOMMENDED ✅)
```bash
$ cyxmake build
[INFO] No build system detected
[INFO] LLM assistance available - download Qwen2.5-Coder-3B? (1.8 GB) [Y/n]: y
[INFO] Downloading model... [=========>        ] 45% (820 MB / 1.8 GB)
```

**Pros:**
- Small installer (~5-10 MB)
- Users only download if needed
- Transparent user experience
- Can fallback to manual mode if declined

**Cons:**
- Requires internet on first AI feature use
- Must implement download + progress tracking
- Need to handle interrupted downloads

**Implementation Complexity:** Medium-High (7/10)

### DECISION: Automatic On-Demand Download ✅

**Rationale:**
- Best balance between installer size and user experience
- Users who never use AI features never download model
- Progressive enhancement: works without AI, better with AI
- Industry standard (similar to VS Code extensions, Python packages)

**User Experience Flow:**
```
First time needing AI:
  → Prompt user for download
  → Show size and estimated time
  → Download with progress bar
  → Cache for future use
  → Never prompt again

Manual download option also available:
  cyxmake download-model
```

**Implementation Requirements:**
1. HTTP download with libcurl (already in dependencies)
2. Progress bar with percentage + speed
3. Resume capability (handle Ctrl+C gracefully)
4. SHA256 verification (prevent corrupted downloads)
5. Atomic write (temp file → rename on success)

**Download Source:**
- Hugging Face: `https://huggingface.co/Qwen/Qwen2.5-Coder-3B-Instruct-GGUF/resolve/main/qwen2.5-coder-3b-instruct-q4_k_m.gguf`
- Fallback mirror (if primary fails)

---

## Decision 3: Model Storage Location

### The Question
Where should we store the downloaded model file?

### Options Analysis

#### Option A: User Home Directory (RECOMMENDED ✅)
```
Windows:  C:\Users\<username>\.cyxmake\models\qwen2.5-coder-3b-q4_k_m.gguf
macOS:    /Users/<username>/.cyxmake/models/qwen2.5-coder-3b-q4_k_m.gguf
Linux:    /home/<username>/.cyxmake/models/qwen2.5-coder-3b-q4_k_m.gguf
```

**Pros:**
- No admin/root privileges needed
- Per-user isolation
- Standard practice (npm, pip, cargo all do this)
- Easy to find and delete
- Works in corporate environments

**Cons:**
- Duplicated if multiple users on same system
- Consumes user quota/storage

**Implementation Complexity:** Low (3/10)

#### Option B: System-Wide Location
```
Windows:  C:\ProgramData\CyxMake\models\
macOS:    /usr/local/share/cyxmake/models/
Linux:    /usr/share/cyxmake/models/
```

**Pros:**
- Single copy for all users
- Saves disk space on multi-user systems

**Cons:**
- Requires admin/root privileges
- Installer complexity increases
- Permission issues
- Harder to update

**Implementation Complexity:** High (8/10)

#### Option C: Alongside Executable
```
/opt/cyxmake/models/qwen2.5-coder-3b-q4_k_m.gguf
```

**Pros:**
- Simple path resolution
- Self-contained installation

**Cons:**
- Requires admin privileges to update
- Messy on upgrades
- Non-standard practice

**Implementation Complexity:** Low (2/10) but poor user experience

### DECISION: User Home Directory ✅

**Rationale:**
- No privilege escalation needed
- Industry standard approach
- Users can easily manage models (delete, backup)
- Works everywhere (Windows/macOS/Linux)
- Corporate IT departments allow user home writes

**Path Resolution Implementation:**
```c
char* get_model_storage_path(void) {
#ifdef _WIN32
    // Windows: %USERPROFILE%\.cyxmake\models
    char* home = getenv("USERPROFILE");
    if (!home) home = getenv("HOMEPATH");
#else
    // POSIX: $HOME/.cyxmake/models
    char* home = getenv("HOME");
#endif

    if (!home) {
        log_error("Cannot determine home directory");
        return NULL;
    }

    char* path = malloc(PATH_MAX);
    snprintf(path, PATH_MAX, "%s/.cyxmake/models", home);

    // Create directory if it doesn't exist
    mkdir_recursive(path);

    return path;
}
```

**Directory Structure:**
```
~/.cyxmake/
├── models/
│   ├── qwen2.5-coder-3b-q4_k_m.gguf      (1.8 GB)
│   └── qwen2.5-coder-3b-q4_k_m.gguf.sha256
├── cache/
│   └── llm_context_cache/                 (for faster reloads)
└── config.toml
```

---

## Decision 4: LLM API Design

### The Question
Should the LLM API be synchronous or asynchronous?

### Options Analysis

#### Option A: Synchronous (RECOMMENDED ✅)
```c
LLMResponse* llm_query(LLMContext* ctx, const LLMRequest* req) {
    // Blocks until inference complete
    LLMResponse* resp = llama_generate(ctx->model, req->prompt);
    return resp;  // 1-2 seconds later
}
```

**Usage:**
```c
// Build error occurred
LLMRequest* req = llm_request_create();
llm_request_set_prompt(req, error_prompt);
llm_request_set_max_tokens(req, 512);

LLMResponse* resp = llm_query(llm_ctx, req);  // Blocks

if (resp && resp->success) {
    printf("Suggested fix:\n%s\n", resp->text);
}
```

**Pros:**
- Simple implementation
- Easy to reason about
- No threading/callback complexity
- Error handling is straightforward
- Sufficient for CyxMake use case (interactive CLI)

**Cons:**
- Blocks calling thread (1-2 seconds)
- Cannot show "thinking..." animation easily
- Cannot cancel mid-inference

**Implementation Complexity:** Low (3/10)

#### Option B: Asynchronous with Callbacks
```c
void llm_query_async(LLMContext* ctx, const LLMRequest* req,
                     LLMCallback callback, void* user_data) {
    // Returns immediately
    // Callback invoked when complete
}
```

**Pros:**
- Non-blocking
- Can show progress/spinner
- Better user experience

**Cons:**
- Threading complexity (mutexes, race conditions)
- Callback hell for complex flows
- Memory management trickier
- Overkill for CLI tool

**Implementation Complexity:** High (8/10)

#### Option C: Hybrid (Sync + Optional Async)
```c
// Sync version
LLMResponse* llm_query(LLMContext* ctx, const LLMRequest* req);

// Async version (added later)
LLMHandle* llm_query_async(LLMContext* ctx, const LLMRequest* req);
LLMResponse* llm_wait(LLMHandle* handle);
bool llm_is_done(LLMHandle* handle);
void llm_cancel(LLMHandle* handle);
```

**Pros:**
- Start simple, add complexity when needed
- Backwards compatible
- Can optimize hot paths later

**Cons:**
- Two APIs to maintain

**Implementation Complexity:** Medium (5/10) initially, grows to 7/10

### DECISION: Synchronous First, Async Later ✅

**Rationale:**
- CyxMake is a CLI tool, not a GUI (blocking is acceptable)
- Users expect to wait 1-2 seconds for AI suggestions
- We can show "Analyzing error..." message before blocking
- Simpler implementation = fewer bugs = ship faster
- Can add async API in Phase 2 if needed

**Phase 1 Implementation (Synchronous):**
```c
/* include/cyxmake/llm_interface.h */

typedef struct LLMContext LLMContext;

typedef struct {
    char* prompt;
    int max_tokens;
    float temperature;
    int top_k;
    float top_p;
} LLMRequest;

typedef struct {
    char* text;
    int tokens_generated;
    double duration_sec;
    bool success;
    char* error_message;
} LLMResponse;

/* Lifecycle */
LLMContext* llm_init(const char* model_path);
void llm_shutdown(LLMContext* ctx);

/* Query */
LLMResponse* llm_query(LLMContext* ctx, const LLMRequest* req);
void llm_response_free(LLMResponse* resp);

/* Helpers */
LLMRequest* llm_request_create(const char* prompt);
void llm_request_free(LLMRequest* req);
```

**Phase 2 Addition (If Needed):**
```c
/* Async API */
typedef struct LLMHandle LLMHandle;
typedef void (*LLMCallback)(LLMResponse* resp, void* user_data);

LLMHandle* llm_query_async(LLMContext* ctx, const LLMRequest* req,
                           LLMCallback callback, void* user_data);
void llm_cancel(LLMHandle* handle);
```

**User Experience:**
```
$ cyxmake build
[INFO] Build system: CMake
[INFO] Build command: cmake --build .
...
[ERROR] Build failed with exit code: 1
[INFO] Analyzing error with AI... (this may take a few seconds)
▌ Thinking...
[SUCCESS] Found likely cause: missing semicolon in main.c:42
[INFO] Suggested fix:
  Add ';' after line 42:
    printf("Hello world")
                        ^
                        ;
[PROMPT] Apply this fix automatically? [Y/n]:
```

---

## Decision 5: Error Context Level

### The Question
How much context should we send to the LLM when analyzing build errors?

### Options Analysis

#### Option A: Minimal Context
```json
{
  "error": "undefined reference to `foo'"
}
```

**Pros:**
- Fast inference (fewer tokens)
- Low memory usage

**Cons:**
- LLM lacks context to give good suggestions
- May hallucinate generic fixes
- Cannot understand project-specific issues

**Tokens:** ~50-100
**Expected Quality:** 40-60% helpful suggestions

#### Option B: Medium Context (RECOMMENDED ✅)
```json
{
  "error": {
    "type": "linker_error",
    "message": "undefined reference to `foo'",
    "file": "main.c",
    "line": 42,
    "full_output": "... last 50 lines of build output ..."
  },
  "project": {
    "language": "C",
    "build_system": "CMake",
    "dependencies": ["cjson", "curl"],
    "structure": {
      "source_files": 12,
      "header_files": 8,
      "total_lines": 3420
    }
  },
  "relevant_code": "... snippet from main.c around line 42 ..."
}
```

**Pros:**
- Good balance of context vs speed
- Enough info for LLM to understand problem
- Can make project-specific suggestions

**Cons:**
- Slightly slower than minimal
- Must implement code snippet extraction

**Tokens:** ~500-800
**Expected Quality:** 80-90% helpful suggestions

#### Option C: Maximum Context
Send entire source tree + full build log

**Pros:**
- LLM has complete picture

**Cons:**
- Way too slow (10-30 seconds)
- Exceeds model context window (8K tokens)
- Wastes tokens on irrelevant code

**Tokens:** 5000-8000+ (often exceeds limit)
**Expected Quality:** 85-95% but too slow

### DECISION: Medium Context ✅

**Rationale:**
- Best balance: 80-90% quality at 1-2 second speed
- Qwen2.5-Coder models are trained to work well with partial context
- Can iterate: if LLM needs more, we can add in Phase 2
- Fits comfortably in 8K token context window

**Context Building Strategy:**

1. **Always Include:**
   - Error message + type
   - File + line number
   - Last 50 lines of build output
   - Build system type
   - Language

2. **Include If Available:**
   - Code snippet (±10 lines around error)
   - Recent changes (from git diff)
   - Dependency list
   - Similar past errors (from history)

3. **Never Include:**
   - Full source tree
   - Binary files
   - Generated code
   - Test files (unless error is in test)

**Implementation:**
```c
typedef struct {
    /* Error info */
    char* error_type;        /* "compile_error", "linker_error", etc. */
    char* error_message;     /* Actual error text */
    char* file_path;         /* File where error occurred */
    int line_number;         /* Line number */
    char* build_output_tail; /* Last 50 lines */

    /* Project context */
    Language language;
    BuildSystem build_system;
    char** dependencies;
    int dependency_count;

    /* Code context */
    char* code_snippet;      /* ±10 lines around error */
    char* recent_changes;    /* git diff if available */
} ErrorContext;

char* build_llm_prompt(const ErrorContext* ctx) {
    // Convert ErrorContext to structured prompt
    // Use template from llm.md
}
```

**Prompt Template:**
```
You are analyzing a C build error. Here is the context:

ERROR:
Type: linker_error
Message: undefined reference to `foo'
File: main.c
Line: 42

PROJECT:
Language: C
Build System: CMake
Dependencies: cjson, curl

CODE SNIPPET (main.c around line 42):
40: int main(void) {
41:     printf("Hello world");
42:     foo();  // <-- ERROR HERE
43:     return 0;
44: }

RECENT CHANGES (git diff):
+ foo();  // Added in last commit

TASK:
1. Identify the root cause
2. Suggest a specific fix
3. Explain why this error occurred
4. Rate your confidence (1-10)

Format:
CAUSE: [one sentence]
FIX: [specific code change]
EXPLANATION: [2-3 sentences]
CONFIDENCE: [1-10]
```

---

## Summary of Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| **llama.cpp Integration** | Git Submodule | Maximum control, consistent versions |
| **Model Download** | On-Demand Prompt | Small installer, opt-in AI features |
| **Model Storage** | User Home Directory | No privileges needed, industry standard |
| **API Design** | Synchronous First | Simpler, sufficient for CLI, add async later |
| **Error Context** | Medium Context | Best balance: 80-90% quality, 1-2s speed |

---

## Implementation Roadmap

### Phase 1: Core Integration (Week 1-2)
- [ ] Add llama.cpp submodule
- [ ] Update CMakeLists.txt for llama.cpp
- [ ] Create `include/cyxmake/llm_interface.h`
- [ ] Implement `src/llm/llm_interface.c`
- [ ] Test model loading (synchronous API)

### Phase 2: Model Management (Week 3)
- [ ] Implement model download with libcurl
- [ ] Add progress bar
- [ ] SHA256 verification
- [ ] On-demand download prompt
- [ ] Manual `cyxmake download-model` command

### Phase 3: Error Analysis (Week 4-5)
- [ ] Create `src/llm/error_context.c`
- [ ] Implement context building
- [ ] Code snippet extraction
- [ ] Prompt template rendering
- [ ] Integration with build_executor

### Phase 4: Testing & Optimization (Week 6)
- [ ] Test on real build errors
- [ ] Measure inference time
- [ ] Tune context size if needed
- [ ] Add caching for common errors
- [ ] Documentation

### Phase 5: Polish (Week 7-8)
- [ ] User experience refinements
- [ ] Error message improvements
- [ ] Add `--no-ai` flag for opt-out
- [ ] Performance profiling
- [ ] Release prep

---

## Open Questions (To Revisit Later)

1. **GPU Acceleration:** Should we support GPU inference for 3-5x speedup?
   - Defer to Phase 2, offer as optional feature
   - Requires CUDA/Metal/Vulkan support in llama.cpp build

2. **Model Updates:** How to handle new Qwen model versions?
   - Add `cyxmake update-model` command
   - Notify user if new version available
   - Keep old model until new one is verified

3. **Offline Mode:** What if user has no internet?
   - Graceful fallback: "AI features unavailable (no model)"
   - Offer manual model installation instructions
   - Tool works fine without AI (manual mode)

4. **Multi-Model Support:** Should we support other models?
   - Not in Phase 1
   - Phase 2 could add: DeepSeek-Coder, CodeLlama
   - Add `cyxmake list-models` command

5. **Privacy:** What if user has sensitive code?
   - Document that local model = no data leaves machine
   - Add `--no-ai` flag for paranoid users
   - Future: add cloud API option with explicit consent

---

## Risk Mitigation

### Risk 1: Model Download Fails
**Mitigation:**
- Multiple fallback mirrors (Hugging Face + our own CDN)
- Resume capability (don't restart from 0 if interrupted)
- Clear error messages with manual download instructions

### Risk 2: Inference Too Slow
**Mitigation:**
- Quantization already addressed this (Q4_K_M = 1.5s)
- Add timeout (10 seconds max, then abort)
- Cache common error patterns
- Async API in Phase 2 if needed

### Risk 3: LLM Suggests Bad Fixes
**Mitigation:**
- Always show suggested fix to user BEFORE applying
- Require explicit confirmation (`[Y/n]` prompt)
- Add `--dry-run` mode
- Track fix success rate, improve prompts iteratively

### Risk 4: Model File Corrupted
**Mitigation:**
- SHA256 verification after download
- Checksum validation on every load
- Auto-redownload if corrupt detected
- Clear error message if validation fails

### Risk 5: Cross-Platform Build Issues
**Mitigation:**
- llama.cpp well-tested on Windows/macOS/Linux
- We'll test on all platforms in Phase 4
- CI/CD pipeline catches platform-specific bugs
- Community will report issues (good test coverage)

---

## Success Metrics

After implementation, we'll measure:

1. **Inference Speed:** < 2 seconds per query (Target: 1.5s)
2. **Fix Success Rate:** > 80% of suggestions are helpful
3. **User Satisfaction:** Positive feedback in GitHub issues
4. **Adoption Rate:** % of builds that use AI features
5. **Model Download Success:** > 95% of downloads complete

---

## Next Steps

1. **Review this document** - Are all decisions acceptable?
2. **Create GitHub issues** - One per phase in roadmap
3. **Begin Phase 1** - Add llama.cpp submodule
4. **Weekly check-ins** - Review progress, adjust as needed

**Ready to start implementation when you are! 🚀**
