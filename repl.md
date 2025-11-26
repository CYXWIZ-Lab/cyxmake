# CyxMake Interactive REPL Architecture

## Vision

Transform CyxMake from a one-shot CLI tool into an **interactive AI agent** similar to Claude Code CLI, GitHub Copilot CLI, or OpenAI Codex CLI. Users open a terminal, type `cyxmake`, and enter an interactive session where they can communicate with an AI build assistant using natural language.

## Current State vs Target State

### Current (v0.1.0)
```bash
$ cyxmake "build the project"    # One-shot, exits after execution
$ cyxmake build                   # Traditional command
$ cyxmake init                    # Traditional command
```

### Target (v0.2.0)
```bash
$ cyxmake                         # Opens interactive REPL
cyxmake> build this project       # Natural language
cyxmake> /init                    # Slash command
cyxmake> read readme.md and summarize
cyxmake> fix the error in main.c
cyxmake> /exit
```

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              CYXMAKE REPL                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐                   │
│  │   Terminal   │    │    Agent     │    │    Tools     │                   │
│  │   Layer      │───▶│    Core      │───▶│    Layer     │                   │
│  └──────────────┘    └──────────────┘    └──────────────┘                   │
│         │                   │                   │                            │
│         ▼                   ▼                   ▼                            │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐                   │
│  │ • REPL Loop  │    │ • Intent     │    │ • File CRUD  │                   │
│  │ • Input      │    │   Parser     │    │ • Build Exec │                   │
│  │ • Output     │    │ • LLM Query  │    │ • Pkg Manager│                   │
│  │ • History    │    │ • Context    │    │ • Error Diag │                   │
│  │ • Completion │    │ • Planner    │    │ • Git Ops    │                   │
│  └──────────────┘    └──────────────┘    └──────────────┘                   │
│                              │                                               │
│                              ▼                                               │
│                      ┌──────────────┐                                        │
│                      │  Permission  │                                        │
│                      │   System     │                                        │
│                      │              │                                        │
│                      │ • Ask Y/N    │                                        │
│                      │ • Auto-allow │                                        │
│                      │ • Audit log  │                                        │
│                      └──────────────┘                                        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Components

### 1. Terminal Layer (`src/cli/`)

**Files:**
- `repl.c` / `repl.h` - Main REPL loop
- `terminal.c` / `terminal.h` - Terminal utilities (colors, cursor, clear)
- `input.c` / `input.h` - Input handling with history and completion
- `output.c` / `output.h` - Formatted output (boxes, tables, progress)

**Responsibilities:**
- Read user input line-by-line
- Handle arrow keys for history navigation
- Tab completion for commands and file paths
- Colored output and formatting
- Clear screen, cursor positioning

### 2. Slash Command System (`src/cli/`)

**Files:**
- `slash_commands.c` / `slash_commands.h`

**Commands:**

| Command | Alias | Description |
|---------|-------|-------------|
| `/help` | `/h`, `/?` | Show available commands |
| `/init` | `/i` | Initialize/analyze project |
| `/build` | `/b` | Build the project |
| `/clean` | `/c` | Clean build artifacts |
| `/test` | `/t` | Run tests |
| `/run` | `/r` | Run the built executable |
| `/config` | `/cfg` | Show/edit configuration |
| `/status` | `/s` | Show project & AI status |
| `/history` | `/hist` | Show conversation history |
| `/clear` | `/cls` | Clear screen |
| `/model` | `/m` | Switch AI model |
| `/verbose` | `/v` | Toggle verbose mode |
| `/exit` | `/quit`, `/q` | Exit CyxMake |

### 3. Agent Core (`src/agent/`)

**Files:**
- `agent.c` / `agent.h` - Main agent orchestration
- `conversation.c` / `conversation.h` - Conversation state management
- `action_planner.c` / `action_planner.h` - Multi-step action planning

**Conversation Context:**
```c
typedef struct {
    /* Message history */
    struct {
        char* role;      /* "user" or "assistant" */
        char* content;
        time_t timestamp;
    }* messages;
    int message_count;
    int message_capacity;

    /* Current context */
    char* working_directory;
    char* current_file;      /* File being discussed */
    char* last_error;        /* Last build error */
    char* last_command;      /* Last executed command */

    /* Project info (cached) */
    ProjectContext* project;

    /* Session settings */
    bool verbose;
    bool auto_approve_safe;  /* Auto-approve read operations */
} ConversationContext;
```

**Action Types:**
```c
typedef enum {
    ACTION_READ_FILE,       /* Safe - no permission needed */
    ACTION_BUILD,           /* Safe */
    ACTION_ANALYZE,         /* Safe */
    ACTION_STATUS,          /* Safe */

    ACTION_CREATE_FILE,     /* Ask permission */
    ACTION_MODIFY_FILE,     /* Ask permission */
    ACTION_DELETE_FILE,     /* Ask permission */
    ACTION_INSTALL_PKG,     /* Ask permission */
    ACTION_RUN_COMMAND,     /* Ask permission */

    ACTION_DELETE_PROJECT,  /* Dangerous - explicit confirm */
    ACTION_SYSTEM_MODIFY,   /* Dangerous */
} ActionType;
```

### 4. Permission System (`src/cli/`)

**Files:**
- `permission.c` / `permission.h`

**Permission Levels:**
```
SAFE        → Execute immediately, no prompt
ASK         → Show prompt, wait for Y/N
DANGEROUS   → Show warning, require explicit confirmation
BLOCKED     → Never allow (system files, etc.)
```

**Permission Prompt:**
```
┌─────────────────────────────────────────────────────────────┐
│ ⚠ Permission Required                                       │
├─────────────────────────────────────────────────────────────┤
│ Action:  Create file                                        │
│ Path:    src/utils/helper.c                                 │
│ Reason:  You asked to create a helper utility               │
├─────────────────────────────────────────────────────────────┤
│ [Y]es  [N]o  [A]lways  [V]iew  [?]Help                     │
└─────────────────────────────────────────────────────────────┘
```

**Response Options:**
- `Y` / `y` / Enter → Yes, allow this action
- `N` / `n` → No, deny this action
- `A` / `a` → Always allow this action type
- `V` / `v` → View more details (file contents, diff, etc.)
- `?` → Show help

---

## Implementation Phases

### Phase 1: Basic REPL (Foundation)
**Goal:** Get a working interactive loop

**Tasks:**
- [ ] Create `repl.c` with basic input loop
- [ ] Implement simple line reading (no fancy features yet)
- [ ] Detect slash commands vs natural language
- [ ] Implement core slash commands: `/help`, `/exit`, `/clear`
- [ ] Connect to existing NL parser for natural language
- [ ] Basic prompt: `cyxmake> `

**Deliverable:** Can type `cyxmake`, enter REPL, type commands, exit

```bash
$ cyxmake
cyxmake> /help
Available commands: /help, /exit, /clear, /build, /init
cyxmake> build this
[executes build]
cyxmake> /exit
$
```

### Phase 2: Slash Commands
**Goal:** Full slash command system

**Tasks:**
- [ ] Implement all slash commands from table above
- [ ] Add command aliases (`/b` for `/build`)
- [ ] Add argument parsing for commands (`/config set key value`)
- [ ] Command not found error handling
- [ ] `/status` shows project and AI info
- [ ] `/config` shows and modifies settings

**Deliverable:** All slash commands working

### Phase 3: Permission System
**Goal:** Ask before dangerous operations

**Tasks:**
- [ ] Create permission classification for all actions
- [ ] Implement permission prompt UI
- [ ] Add "Always allow" memory (per-session)
- [ ] Add permission for: create, modify, delete, install, run
- [ ] Audit log of permitted actions

**Deliverable:** Agent asks before creating/deleting files

```bash
cyxmake> create a readme file
⚠ May I create README.md? [Y/n] y
✓ Created README.md
```

### Phase 4: Conversation Context
**Goal:** Remember conversation history

**Tasks:**
- [ ] Store message history
- [ ] Pass context to LLM for better responses
- [ ] Remember current file being discussed
- [ ] Remember last error for "fix it" commands
- [ ] `/history` command to view past messages

**Deliverable:** AI remembers context

```bash
cyxmake> read main.c
[shows main.c]
cyxmake> what does line 42 do?
[AI explains line 42 of main.c - remembers context]
```

### Phase 5: Enhanced Terminal
**Goal:** Better UX with colors, history, completion

**Tasks:**
- [ ] Add colored output (errors red, success green)
- [ ] Input history with arrow keys
- [ ] Tab completion for file paths
- [ ] Tab completion for slash commands
- [ ] Progress indicators for long operations
- [ ] Spinner for AI thinking

**Deliverable:** Polished terminal experience

### Phase 6: Action Planning
**Goal:** Multi-step actions with approval

**Tasks:**
- [ ] Detect complex requests needing multiple steps
- [ ] Generate action plan
- [ ] Show plan to user for approval
- [ ] Execute steps one by one
- [ ] Rollback on failure

**Deliverable:** Complex commands work

```bash
cyxmake> fix all compilation errors
● Planning actions:
  1. Build project to find errors
  2. Read src/main.c (3 errors)
  3. Fix line 42: missing semicolon
  4. Fix line 67: undefined variable
  5. Fix line 89: type mismatch
  6. Rebuild to verify

Execute plan? [Y/n/step]
```

---

## Data Flow

### User Input Flow
```
User types: "fix the error in main.c"
                    │
                    ▼
            ┌───────────────┐
            │ Input Handler │
            └───────────────┘
                    │
        ┌───────────┴───────────┐
        │                       │
        ▼                       ▼
┌───────────────┐       ┌───────────────┐
│ Starts with / │       │ Natural Lang  │
│ Slash Command │       │ Parse Intent  │
└───────────────┘       └───────────────┘
        │                       │
        ▼                       ▼
┌───────────────┐       ┌───────────────┐
│ Execute Cmd   │       │ Plan Actions  │
└───────────────┘       └───────────────┘
                                │
                                ▼
                        ┌───────────────┐
                        │ Check Perms   │
                        └───────────────┘
                                │
                    ┌───────────┴───────────┐
                    │                       │
                    ▼                       ▼
            ┌───────────────┐       ┌───────────────┐
            │ Safe: Execute │       │ Ask: Prompt   │
            └───────────────┘       └───────────────┘
                    │                       │
                    └───────────┬───────────┘
                                │
                                ▼
                        ┌───────────────┐
                        │ Execute Tool  │
                        │ (File/Build)  │
                        └───────────────┘
                                │
                                ▼
                        ┌───────────────┐
                        │ Show Result   │
                        └───────────────┘
                                │
                                ▼
                        ┌───────────────┐
                        │ Update Context│
                        └───────────────┘
```

---

## File Structure (Final)

```
src/
├── cli/
│   ├── main.c                 # Entry point, mode detection
│   ├── repl.c                 # REPL loop                    [Phase 1]
│   ├── repl.h
│   ├── slash_commands.c       # Slash command handlers       [Phase 2]
│   ├── slash_commands.h
│   ├── permission.c           # Permission system            [Phase 3]
│   ├── permission.h
│   ├── terminal.c             # Terminal utilities           [Phase 5]
│   ├── terminal.h
│   ├── input.c                # Input with history           [Phase 5]
│   └── input.h
│
├── agent/                      # NEW DIRECTORY
│   ├── agent.c                # Agent orchestration          [Phase 4]
│   ├── agent.h
│   ├── conversation.c         # Conversation context         [Phase 4]
│   ├── conversation.h
│   ├── action_planner.c       # Multi-step planning          [Phase 6]
│   └── action_planner.h
│
├── core/
│   ├── orchestrator.c         # (existing)
│   ├── logger.c               # (existing)
│   ├── build_executor.c       # (existing)
│   └── file_ops.c             # (existing)
│
├── llm/
│   ├── llm_interface.c        # (existing)
│   ├── prompt_templates.c     # (existing) - add REPL prompts
│   └── error_analyzer.c       # (existing)
│
└── tools/
    ├── tool_registry.c        # (existing)
    ├── tool_discovery.c       # (existing)
    └── tool_executor.c        # (existing)
```

---

## Configuration

### Default Settings (`~/.cyxmake/config.toml`)
```toml
[repl]
prompt = "cyxmake> "
history_size = 1000
save_history = true

[permissions]
auto_approve_read = true
auto_approve_build = true
ask_create = true
ask_modify = true
ask_delete = true
ask_install = true

[display]
colors = true
verbose = false
show_timing = true
```

---

## Example Session

```
$ cyxmake

╭────────────────────────────────────────────────────────────────╮
│  CyxMake v0.2.0 - AI Build Assistant                           │
│  Project: my-app (C++, CMake)                                  │
│  Model: qwen2.5-coder-3b (local, CPU)                          │
│                                                                 │
│  Type naturally or /help for commands                          │
╰────────────────────────────────────────────────────────────────╯

cyxmake> /init
● Analyzing project...
  Found: 23 source files, 12 headers
  Build system: CMake 3.20
  Dependencies: fmt, spdlog, boost
✓ Project initialized

cyxmake> build
● Building with CMake...
✗ Build failed (3 errors)

  src/main.cpp:42: error: 'string_view' not found
  src/utils.cpp:17: error: missing semicolon
  src/utils.cpp:89: error: undefined 'logger'

cyxmake> fix these errors
● Planning fixes:
  1. Add #include <string_view> to main.cpp
  2. Add semicolon to utils.cpp:17
  3. Add logger declaration to utils.cpp

⚠ May I modify these files? [Y/n/v] y

● Fixing src/main.cpp... ✓
● Fixing src/utils.cpp... ✓
● Rebuilding...
✓ Build successful!

cyxmake> run it
● Running: ./build/my-app
Hello, World!
Program exited with code 0

cyxmake> /exit
Goodbye!

$
```

---

## Next Steps

1. **Review this document** - Any changes to the design?
2. **Start Phase 1** - Basic REPL loop
3. **Iterate** - Each phase builds on the previous

---

## Questions to Resolve

1. **History persistence** - Save between sessions? (Yes, like bash)
2. **Multi-line input** - Support for pasting code blocks?
3. **Streaming output** - Show AI response as it generates?
4. **Interrupt handling** - Ctrl+C behavior? (Cancel current, not exit)
5. **Windows support** - Use Windows Console API or cross-platform lib?

---

*Document created: 2025-01-XX*
*Last updated: 2025-01-XX*
