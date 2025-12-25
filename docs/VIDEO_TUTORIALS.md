# CyxMake Video Tutorials

> Script outlines for video tutorial series

## Tutorial Series Overview

| # | Title | Duration | Level |
|---|-------|----------|-------|
| 1 | Getting Started with CyxMake | 5 min | Beginner |
| 2 | Natural Language Builds | 7 min | Beginner |
| 3 | Error Recovery in Action | 8 min | Intermediate |
| 4 | Multi-Agent Workflows | 10 min | Advanced |
| 5 | Security and Audit Features | 6 min | Intermediate |

---

## Tutorial 1: Getting Started with CyxMake

**Duration:** ~5 minutes
**Level:** Beginner
**Goal:** Install CyxMake and run your first build

### Script Outline

**0:00 - Introduction (30s)**
- "CyxMake is an AI-powered build automation system"
- "Today we'll install it and build our first project"

**0:30 - Prerequisites (45s)**
- Show required tools: CMake, C compiler
- Windows: Visual Studio or MinGW
- macOS: Xcode Command Line Tools
- Linux: build-essential

**1:15 - Installation (1:30)**
- Clone repository
  ```bash
  git clone https://github.com/CYXWIZ-Lab/cyxmake.git
  cd cyxmake
  ```
- Build from source
  ```bash
  mkdir build && cd build
  cmake .. -DCMAKE_BUILD_TYPE=Release
  cmake --build . --config Release
  ```
- Verify installation
  ```bash
  ./bin/cyxmake --version
  ```

**2:45 - First Project (1:30)**
- Navigate to sample project
- Initialize CyxMake
  ```bash
  cyxmake init
  ```
- Show project analysis output
- Run build
  ```bash
  cyxmake build
  ```

**4:15 - REPL Introduction (45s)**
- Start interactive mode
  ```bash
  cyxmake
  ```
- Show prompt and basic commands
- Type `help` to see options
- Type `exit` to quit

**5:00 - Wrap Up**
- "You've installed CyxMake and built your first project"
- "Next: Natural language commands"

---

## Tutorial 2: Natural Language Builds

**Duration:** ~7 minutes
**Level:** Beginner
**Goal:** Use natural language to control builds

### Script Outline

**0:00 - Introduction (30s)**
- "No need to remember CMake syntax"
- "Just describe what you want in plain English"

**0:30 - Starting the REPL (30s)**
- ```bash
  cyxmake
  ```
- Show the prompt

**1:00 - Basic Commands (2:00)**
- "Build the project"
- "Compile in release mode"
- "Clean and rebuild"
- Show how CyxMake interprets each command
- Display confidence scores

**3:00 - Slash Commands (1:30)**
- `/build` - Quick build
- `/build release` - Release configuration
- `/clean` - Clean build artifacts
- `/status` - Show project status
- Tab completion demonstration

**4:30 - Intent Detection (1:30)**
- Show how CyxMake detects intent
- Examples:
  - "make a fresh build" → CLEAN + BUILD
  - "compile with 4 threads" → BUILD with parallel option
  - "what's in this project?" → ANALYZE

**6:00 - Configuration Options (45s)**
- ```
  cyxmake> /config show
  ```
- Show how to change options
- ```
  cyxmake> /config set build_type Release
  ```

**6:45 - Wrap Up**
- "Natural language makes builds intuitive"
- "Next: Automatic error recovery"

---

## Tutorial 3: Error Recovery in Action

**Duration:** ~8 minutes
**Level:** Intermediate
**Goal:** See AI-powered error diagnosis and fixing

### Script Outline

**0:00 - Introduction (30s)**
- "Build errors happen - CyxMake fixes them automatically"
- "Let's see error recovery in action"

**0:30 - Setup Broken Build (1:00)**
- Create intentional errors:
  - Missing library dependency
  - Missing header file
  - Wrong CMake version

**1:30 - Error Detection (1:30)**
- Run build with missing SDL2
  ```bash
  cyxmake> build
  ```
- Show error output
- Show diagnosis:
  - Pattern matched: MISSING_LIBRARY
  - Confidence: 0.92
  - Suggested fixes

**3:00 - Fix Suggestions (1:30)**
- Show fix options:
  1. Install SDL2 library [HIGH RISK]
  2. Set library path [LOW RISK]
  3. Clean and retry [SAFE]
- Explain risk levels
- Permission prompt demonstration

**4:30 - Automatic Fixing (2:00)**
- Apply fix
- Show package installation
- Automatic rebuild
- Success verification
- Fix recorded to history

**6:30 - Fix History and Learning (1:00)**
- ```
  cyxmake> /fix history
  ```
- Show how CyxMake remembers successful fixes
- Next time: suggests based on history

**7:30 - Wrap Up**
- "CyxMake learns from your fixes"
- "Most common errors are fixed automatically"

---

## Tutorial 4: Multi-Agent Workflows

**Duration:** ~10 minutes
**Level:** Advanced
**Goal:** Use multiple agents for complex tasks

### Script Outline

**0:00 - Introduction (30s)**
- "Complex projects need parallel processing"
- "CyxMake agents work together autonomously"

**0:30 - Agent Concepts (1:30)**
- Agent types: smart, build, auto
- Agent states: idle, running, completed
- Shared state and message passing

**2:00 - Spawning Agents (2:00)**
- Create build agent:
  ```
  /agent spawn builder build
  ```
- Create analyzer agent:
  ```
  /agent spawn analyzer smart
  ```
- List agents:
  ```
  /agent list
  ```

**4:00 - Assigning Tasks (2:00)**
- Assign build task:
  ```
  /agent assign builder "Build release configuration"
  ```
- Assign analysis task:
  ```
  /agent assign analyzer "Find unused dependencies"
  ```
- Show parallel execution

**6:00 - Monitoring and Waiting (1:30)**
- Check status:
  ```
  /agent status builder
  ```
- Wait for completion:
  ```
  /agent wait builder
  ```
- Show results

**7:30 - Conflict Resolution (1:30)**
- Demonstrate conflict scenario
- Show user prompt for resolution
- Choose which agent proceeds

**9:00 - Cleanup (30s)**
- Terminate agents:
  ```
  /agent terminate builder
  /agent terminate analyzer
  ```

**9:30 - Wrap Up**
- "Agents enable parallel workflows"
- "Great for CI/CD and complex projects"

---

## Tutorial 5: Security and Audit Features

**Duration:** ~6 minutes
**Level:** Intermediate
**Goal:** Use security features for safe automation

### Script Outline

**0:00 - Introduction (30s)**
- "Build automation needs security"
- "CyxMake provides audit, rollback, and sandboxing"

**0:30 - Permission System (1:30)**
- Show permission levels: SAFE, ASK, DANGEROUS, BLOCKED
- Demonstrate prompts:
  ```
  Action: Install package 'sdl2'
  Risk Level: DANGEROUS
  Proceed? [Y/n/a/v]
  ```
- Options: Yes, No, Always, View details

**2:00 - Audit Logging (1:30)**
- Enable audit:
  ```
  /config set audit_enabled true
  ```
- Show audit log:
  ```
  /audit show
  ```
- Export options:
  ```
  /audit export json
  ```

**3:30 - Dry-Run Mode (1:00)**
- Enable dry-run:
  ```
  /config set dry_run true
  ```
- Run build (shows what would happen)
- No actual changes made

**4:30 - Rollback Support (1:00)**
- Show rollback history:
  ```
  /rollback list
  ```
- Undo last change:
  ```
  /rollback last 1
  ```
- Verify restoration

**5:30 - Wrap Up**
- "Security features protect your system"
- "Use dry-run for unfamiliar projects"
- "Audit logs for compliance"

---

## Production Notes

### Recording Setup
- Terminal: Windows Terminal / iTerm2
- Font: JetBrains Mono, 14pt
- Theme: Dark mode, high contrast
- Resolution: 1920x1080

### Post-Production
- Add captions for all commands
- Highlight key areas with zoom
- Include chapter markers
- Add intro/outro music

### Distribution
- YouTube channel
- Project website
- GitHub repository links

---

*Video tutorial outlines v0.2.0*
