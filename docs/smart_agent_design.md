# CyxMake Smart Agent Architecture

## Vision

Transform CyxMake from a "build tool with AI features" into an **intelligent build agent** that thinks, reasons, and acts like an expert developer. The agent should:

1. **Understand** - Deeply comprehend projects, not just detect patterns
2. **Reason** - Think step-by-step about problems and solutions
3. **Act** - Execute the right tools at the right time
4. **Learn** - Remember what works and adapt to each project
5. **Communicate** - Explain decisions clearly to users

---

## Core Architecture: Agent Loop

```
┌─────────────────────────────────────────────────────────────────┐
│                     SMART AGENT LOOP                             │
│                                                                  │
│   ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐ │
│   │ PERCEIVE │───▶│  THINK   │───▶│  PLAN    │───▶│   ACT    │ │
│   │          │    │          │    │          │    │          │ │
│   │ • Read   │    │ • Reason │    │ • Steps  │    │ • Execute│ │
│   │ • Analyze│    │ • Decide │    │ • Order  │    │ • Verify │ │
│   │ • Context│    │ • Options│    │ • Deps   │    │ • Report │ │
│   └──────────┘    └──────────┘    └──────────┘    └──────────┘ │
│         ▲                                               │       │
│         │              ┌──────────┐                     │       │
│         └──────────────│  LEARN   │◀────────────────────┘       │
│                        │          │                             │
│                        │ • Memory │                             │
│                        │ • Adapt  │                             │
│                        └──────────┘                             │
└─────────────────────────────────────────────────────────────────┘
```

---

## 1. PERCEIVE - Deep Project Understanding

### 1.1 File Intelligence

Current limitation: Only detects build system type.

Enhancement: **Semantic File Understanding**

```c
typedef struct {
    char* path;
    char* content_hash;
    FileType type;              // SOURCE, HEADER, CONFIG, BUILD, DOC, TEST
    char** imports;             // What this file imports/includes
    char** exports;             // What this file exports (functions, classes)
    char** dependencies;        // External dependencies referenced
    int complexity_score;       // How complex is this file
    char* purpose;              // AI-generated description of file's purpose
} SmartFileInfo;
```

**Capabilities:**
- Parse #include statements to build dependency graph
- Identify main entry points (main(), __main__, etc.)
- Detect test files vs production code
- Understand module boundaries
- Identify configuration files and their purpose

### 1.2 Project Graph

```c
typedef struct {
    SmartFileInfo** files;
    int file_count;

    // Dependency graph
    DependencyEdge** internal_deps;   // File A depends on File B
    ExternalDep** external_deps;       // External libraries needed

    // Build targets
    BuildTarget** targets;             // Executables, libraries, tests

    // Project metadata
    char* project_name;
    char* version;
    char* description;
    Language primary_language;
    BuildSystem build_system;

    // Intelligence
    char* ai_summary;                  // AI-generated project summary
    char** potential_issues;           // Detected problems
    char** suggestions;                // Improvement suggestions
} ProjectGraph;
```

### 1.3 Environment Awareness

```c
typedef struct {
    // System info
    char* os_name;
    char* os_version;
    char* architecture;

    // Available tools with versions
    ToolInfo** available_tools;

    // Package managers and their package caches
    PackageManager** package_managers;

    // Environment variables
    EnvVar** env_vars;

    // Resource constraints
    int cpu_cores;
    size_t available_memory;
    size_t disk_space;

    // Network status
    bool has_internet;
    char** accessible_registries;  // npm, pypi, crates.io, etc.
} EnvironmentContext;
```

---

## 2. THINK - Reasoning Engine

### 2.1 Chain-of-Thought Reasoning

Instead of jumping to conclusions, the agent should think step-by-step:

```c
typedef struct {
    char* observation;      // What I see
    char* interpretation;   // What it means
    char* hypothesis;       // What I think should happen
    char* action;           // What I'll do
    char* expected_result;  // What I expect
} ThoughtStep;

typedef struct {
    ThoughtStep** steps;
    int step_count;
    char* conclusion;
    float confidence;
} ReasoningChain;
```

**Example reasoning for a build error:**

```
Observation: "error: 'vector' is not a member of 'std'"
Interpretation: The code uses std::vector but the header isn't included
Hypothesis: Adding #include <vector> should fix this
Action: Check if file has #include <vector>, if not suggest adding it
Expected: Compilation should succeed after adding the include
```

### 2.2 Decision Framework

```c
typedef enum {
    DECISION_BUILD_STRATEGY,    // How to build
    DECISION_ERROR_FIX,         // How to fix an error
    DECISION_DEPENDENCY,        // Which dependency to use
    DECISION_TOOL_SELECTION,    // Which tool to use
    DECISION_OPTIMIZATION,      // How to optimize
} DecisionType;

typedef struct {
    char* description;
    float score;                // 0-1 confidence
    char** pros;
    char** cons;
    char* implementation;       // How to implement this option
} DecisionOption;

typedef struct {
    DecisionType type;
    char* context;              // What triggered this decision
    DecisionOption** options;   // Available choices
    int option_count;
    int selected_option;        // Which was chosen
    char* reasoning;            // Why this was chosen
} Decision;
```

### 2.3 Intent Detection (Enhanced)

Current: Basic keyword matching with confidence score.

Enhanced: **Semantic Intent Understanding**

```c
typedef struct {
    IntentType primary_intent;
    IntentType* secondary_intents;  // User might want multiple things

    // Extracted entities
    char** file_references;         // Files mentioned
    char** package_references;      // Packages mentioned
    char** action_modifiers;        // "quickly", "safely", "verbose"

    // Context from conversation
    char* referenced_error;         // "fix that error" -> which error?
    char* referenced_file;          // "in that file" -> which file?

    // Clarification needed?
    bool needs_clarification;
    char* clarification_question;

    // Confidence breakdown
    float semantic_confidence;      // AI understanding
    float pattern_confidence;       // Pattern matching
    float context_confidence;       // From conversation context
} SmartIntent;
```

---

## 3. PLAN - Intelligent Planning

### 3.1 Hierarchical Task Planning

```c
typedef struct BuildTask BuildTask;

struct BuildTask {
    char* id;
    char* name;
    char* description;

    TaskType type;              // CONFIGURE, COMPILE, LINK, TEST, INSTALL, etc.
    TaskPriority priority;

    // Dependencies
    char** depends_on;          // Task IDs this depends on
    char** blocks;              // Task IDs this blocks

    // Execution details
    char* command;
    char** arguments;
    char* working_dir;
    int timeout_sec;

    // Conditions
    char* condition;            // Only run if this is true
    bool can_fail;              // Is failure acceptable?

    // Sub-tasks (for hierarchical planning)
    BuildTask** subtasks;
    int subtask_count;

    // Recovery
    BuildTask* on_failure;      // What to do if this fails
    int max_retries;

    // Results
    TaskStatus status;
    char* output;
    char* error;
    int exit_code;
    double duration_sec;
};

typedef struct {
    char* name;
    char* description;

    BuildTask** tasks;
    int task_count;

    // Plan metadata
    char* ai_reasoning;         // Why this plan was chosen
    float estimated_time_sec;
    char** potential_risks;
    char** fallback_strategies;

    // Execution tracking
    int current_task;
    PlanStatus status;
} BuildPlan;
```

### 3.2 Adaptive Planning

The plan should adapt based on:
- Available resources (CPU, memory, network)
- Previous build history (what worked before)
- Current system state (what's already built)
- User preferences (fast vs thorough)

```c
typedef struct {
    // Resource-based adaptation
    bool use_parallel_build;
    int parallel_jobs;
    bool use_incremental;

    // Strategy selection
    BuildStrategy strategy;     // FAST, THOROUGH, SAFE, DEBUG

    // Optimization hints
    bool use_ccache;
    bool use_precompiled_headers;
    bool skip_tests;

    // Safety settings
    bool dry_run_first;
    bool confirm_destructive;
    bool backup_before_modify;
} PlanningOptions;
```

---

## 4. ACT - Smart Execution

### 4.1 Tool Orchestration

```c
typedef struct {
    ToolInfo* tool;
    char** args;
    char* working_dir;

    // Execution options
    bool capture_output;
    bool stream_output;         // Show output in real-time
    int timeout_sec;

    // Environment
    EnvVar** extra_env;

    // Callbacks
    void (*on_stdout)(const char* line, void* ctx);
    void (*on_stderr)(const char* line, void* ctx);
    void (*on_progress)(float percent, void* ctx);
    void* callback_ctx;
} ToolExecution;

typedef struct {
    bool success;
    int exit_code;
    char* stdout_output;
    char* stderr_output;
    double duration_sec;

    // Parsed results
    CompileError** errors;
    CompileWarning** warnings;
    int error_count;
    int warning_count;

    // AI analysis of output
    char* ai_summary;
    char** suggested_fixes;
} ToolResult;
```

### 4.2 Real-time Error Detection

Don't wait for the build to finish - detect errors as they happen:

```c
typedef struct {
    // Pattern matching
    ErrorPattern** patterns;

    // AI-based detection
    bool use_ai_detection;

    // Callbacks
    void (*on_error_detected)(const CompileError* error, void* ctx);
    void (*on_warning_detected)(const CompileWarning* warning, void* ctx);

    // Auto-fix options
    bool suggest_fixes_immediately;
    bool auto_apply_safe_fixes;
} ErrorDetectionConfig;
```

### 4.3 Verification

After each action, verify it worked:

```c
typedef struct {
    VerificationType type;

    // What to check
    char* check_file_exists;
    char* check_command;        // Run this command to verify
    char* expected_output;

    // Results
    bool passed;
    char* actual_result;
    char* failure_reason;
} Verification;
```

---

## 5. LEARN - Memory & Adaptation

### 5.1 Project Memory

Store what works for each project:

```c
typedef struct {
    char* project_hash;         // Identify this project

    // Successful builds
    BuildRecord** successful_builds;
    int success_count;

    // Failed builds and fixes
    ErrorFixRecord** error_fixes;
    int fix_count;

    // User preferences for this project
    UserPreference** preferences;

    // Performance data
    float avg_build_time;
    char* fastest_configuration;

    // Dependencies that work
    DependencyVersion** known_good_deps;
} ProjectMemory;
```

### 5.2 Global Knowledge Base

Learn from all projects:

```c
typedef struct {
    // Error -> Fix mappings
    ErrorFixMapping** error_fixes;
    int fix_count;

    // Tool compatibility
    ToolCompatibility** tool_compat;

    // Common patterns
    BuildPattern** patterns;

    // Statistics
    int total_builds;
    int successful_builds;
    float success_rate;
} GlobalKnowledge;
```

### 5.3 Learning from Failures

```c
typedef struct {
    char* error_signature;      // Unique identifier for this error type
    char* error_message;

    // Attempted fixes
    FixAttempt** attempts;
    int attempt_count;

    // What worked
    char* successful_fix;
    float fix_confidence;

    // Context that matters
    char** relevant_context;    // OS, compiler version, etc.
} LearnedFix;
```

---

## 6. COMMUNICATE - User Interaction

### 6.1 Explanatory Output

Don't just show commands - explain what's happening:

```c
typedef struct {
    OutputLevel level;          // DEBUG, INFO, IMPORTANT, ERROR

    char* message;
    char* explanation;          // Why is this happening
    char* suggestion;           // What user can do

    // Rich formatting
    bool use_color;
    char* icon;                 // Emoji or ASCII symbol

    // Progress info
    float progress;
    char* eta;
} SmartOutput;
```

**Example output:**

```
[1/3] Configuring CMake...
      Why: CMake needs to generate build files before compilation

[2/3] Building project...
      Found: 15 source files to compile
      Using: 8 parallel jobs (detected 8 CPU cores)

      ⚠ Warning in src/utils.cpp:42
        Unused variable 'temp_buffer'
        Suggestion: Remove it or use [[maybe_unused]] attribute

[3/3] Linking executable...
      Output: build/bin/myapp (2.3 MB)

✓ Build successful! (23.4 seconds)

  To run: ./build/bin/myapp
  To test: ./build/bin/myapp --test
```

### 6.2 Interactive Problem Solving

When stuck, ask smart questions:

```c
typedef struct {
    char* question;
    QuestionType type;          // YES_NO, CHOICE, TEXT, FILE_PATH

    // For choice questions
    char** options;
    char** option_descriptions;
    int default_option;

    // Context
    char* why_asking;           // Explain why this question matters
    char* consequence;          // What happens based on answer
} SmartQuestion;
```

**Example interaction:**

```
? Multiple build configurations found:

  1. Debug   - Include debug symbols, no optimization
              Best for: Development and debugging

  2. Release - Full optimization, no debug symbols
              Best for: Production deployment

  3. RelWithDebInfo - Optimization with debug symbols
              Best for: Profiling and performance analysis

  Which configuration? [1/2/3] (default: 1):
```

---

## 7. Implementation Phases

### Phase 1: Enhanced Perception (Foundation)
- [ ] Implement SmartFileInfo with import/export parsing
- [ ] Build project dependency graph
- [ ] Enhanced environment detection
- [ ] File content analysis (identify main, tests, configs)

### Phase 2: Reasoning Engine
- [ ] Chain-of-thought prompting for AI
- [ ] Decision framework with options/pros/cons
- [ ] Enhanced intent detection with entity extraction
- [ ] Context-aware reasoning (use conversation history)

### Phase 3: Intelligent Planning
- [ ] Hierarchical task breakdown
- [ ] Dependency-aware task ordering
- [ ] Adaptive planning based on resources
- [ ] Fallback strategy generation

### Phase 4: Smart Execution
- [ ] Real-time output parsing
- [ ] Streaming error detection
- [ ] Automatic verification after each step
- [ ] Progress estimation

### Phase 5: Learning System
- [ ] Project-specific memory
- [ ] Error-fix knowledge base
- [ ] Success/failure tracking
- [ ] Preference learning

### Phase 6: Communication
- [ ] Explanatory output system
- [ ] Interactive question framework
- [ ] Rich progress display
- [ ] Build reports and summaries

---

## 8. AI Prompt Engineering

### 8.1 System Prompt for Build Agent

```
You are CyxMake, an expert build system agent. Your role is to help users build software projects successfully.

CAPABILITIES:
- Analyze project structure and dependencies
- Create optimal build plans
- Diagnose and fix build errors
- Install missing dependencies
- Explain build processes clearly

BEHAVIOR:
1. Always think step-by-step before acting
2. Explain what you're doing and why
3. When uncertain, ask clarifying questions
4. Learn from each build attempt
5. Prioritize safety - never delete without confirmation

TOOLS AVAILABLE:
[List of available tools with descriptions]

CURRENT CONTEXT:
[Project summary, recent errors, conversation history]
```

### 8.2 Structured Output Format

Request JSON responses for reliable parsing:

```json
{
  "thinking": [
    "I see this is a CMake project with C++ code",
    "The CMakeLists.txt requires CMake 3.15+",
    "I detect SDL2 as an external dependency"
  ],
  "plan": {
    "steps": [
      {
        "action": "check_dependency",
        "target": "SDL2",
        "reason": "Required by CMakeLists.txt line 23"
      },
      {
        "action": "configure",
        "command": "cmake -B build -S .",
        "reason": "Standard out-of-source build"
      }
    ]
  },
  "confidence": 0.85,
  "questions": [],
  "warnings": ["SDL2 might not be installed"]
}
```

---

## 9. Example Scenarios

### Scenario 1: New Project Build

**User:** `build this project`

**Agent thinking:**
1. Analyze project structure
2. Detect CMakeLists.txt → CMake project
3. Parse CMakeLists.txt for dependencies
4. Check if dependencies are installed
5. Create build plan
6. Execute with verification

**Agent output:**
```
Analyzing project...

Project: awesome-game (C++ with CMake)
Dependencies found:
  ✓ SDL2 (2.28.0) - installed
  ✓ OpenGL - system provided
  ✗ GLEW - not found

Installing missing dependencies...
  → Installing GLEW via vcpkg... done

Building project...
  [1/4] Configuring CMake... done
  [2/4] Compiling 23 files... done (12.3s)
  [3/4] Linking... done
  [4/4] Verifying... done

✓ Build successful!

Output: build/bin/awesome-game.exe
To run: ./build/bin/awesome-game.exe
```

### Scenario 2: Build Error Recovery

**User:** `build`

**Error occurs:** `undefined reference to 'SDL_Init'`

**Agent thinking:**
1. Parse error: undefined reference to SDL_Init
2. This means SDL2 library not linked
3. Check CMakeLists.txt for SDL2 linking
4. Found: target_link_libraries missing SDL2
5. Suggest fix

**Agent output:**
```
Build failed with linking error.

Error: undefined reference to 'SDL_Init'

Analysis:
  The SDL2 library is found but not linked to your executable.

  In CMakeLists.txt line 15, you have:
    target_link_libraries(game ${SDL2_LIBRARIES})

  But SDL2_LIBRARIES is empty because find_package result
  wasn't checked.

Suggested fix:
  Replace line 12-15 with:

    find_package(SDL2 REQUIRED)
    target_link_libraries(game SDL2::SDL2)

Apply this fix? [Y/n]:
```

---

## 10. Success Metrics

How to measure if CyxMake is "smart":

1. **First-try success rate**: % of builds that succeed without intervention
2. **Error recovery rate**: % of errors automatically fixed
3. **User intervention reduction**: How often users need to manually fix things
4. **Build time optimization**: Improvement over naive builds
5. **User satisfaction**: Qualitative feedback on helpfulness

Target: 90%+ first-try success rate for standard projects.

---

## Conclusion

This design transforms CyxMake from a reactive build tool into a proactive, intelligent agent that:

- **Understands** projects deeply through semantic analysis
- **Reasons** about problems using chain-of-thought
- **Plans** intelligently with fallback strategies
- **Acts** carefully with verification
- **Learns** from every build attempt
- **Communicates** clearly with users

The key insight is that building software is not just about running commands - it's about understanding context, making decisions, and adapting to failures. By modeling CyxMake after how an expert developer thinks, we can create a truly intelligent build assistant.
