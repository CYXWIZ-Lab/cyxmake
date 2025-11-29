# Autonomous Agent Design

## The Problem

The current CyxMake system is NOT an intelligent agent. It's just:
- Pattern matching for intent classification
- Disconnected AI that answers questions but can't ACT
- No ability to read files, write files, or execute commands autonomously

## The Solution: True Autonomous Agent

The agent should work EXACTLY like Claude Code:
1. Receive a natural language request
2. THINK about what needs to be done
3. USE TOOLS to gather information and take action
4. OBSERVE the results
5. ITERATE if something fails - try different approaches
6. COMPLETE the task or explain why it can't be done

## Architecture

```
User Request: "Read the README and build this project"
                    |
                    v
            +---------------+
            |  Agent Core   |
            |  (Reasoning)  |
            +---------------+
                    |
        +-----------+-----------+
        |           |           |
        v           v           v
    +-------+   +-------+   +--------+
    | Read  |   | Write |   | Execute|
    | Tool  |   | Tool  |   | Tool   |
    +-------+   +-------+   +--------+
        |           |           |
        v           v           v
    Files       Files       Commands
```

## Tools the Agent Needs

### 1. read_file(path)
- Read any file in the project
- Returns file content as string
- Used to understand project structure, read configs, etc.

### 2. write_file(path, content)
- Write/create any file
- Used to create project files, configs, fix code, etc.

### 3. execute(command)
- Run any shell command
- Capture stdout, stderr, exit code
- Used for: npm init, cmake, make, git, etc.

### 4. list_directory(path)
- List files in a directory
- Used to explore project structure

### 5. search_files(pattern, path)
- Search for files matching pattern
- Used to find specific files

### 6. search_content(regex, path)
- Search file contents
- Used to find code patterns, errors, etc.

## Agent Loop (ReAct Pattern)

```
while not done:
    # 1. THINK - Reason about current state
    thought = ai.generate(
        system="You are an autonomous build agent...",
        messages=conversation,
        tools=available_tools
    )

    # 2. Check if AI wants to use a tool
    if thought.has_tool_call:
        tool_name = thought.tool_call.name
        tool_args = thought.tool_call.arguments

        # 3. ACT - Execute the tool
        result = execute_tool(tool_name, tool_args)

        # 4. OBSERVE - Add result to conversation
        conversation.append({
            "role": "tool",
            "content": result
        })

    # 5. Check if AI wants to respond (task complete or needs clarification)
    elif thought.has_response:
        print(thought.response)
        if thought.is_complete:
            done = True
```

## Example Flow

**User:** "Read the README and build this project"

**Agent Thought 1:** "I need to first read the README to understand the project."

**Agent Action 1:** `read_file("README.md")`

**Observation 1:** "# SynthgenParticles\n\nThis is a GPU particle system demo...\n\n## Building\n\nOpen as CMake project in Visual Studio..."

**Agent Thought 2:** "This is a CMake project. Let me check for CMakeLists.txt and understand the structure."

**Agent Action 2:** `list_directory(".")`

**Observation 2:** ["CMakeLists.txt", "src/", "bin/", "README.md", ...]

**Agent Thought 3:** "CMakeLists.txt exists. Let me configure and build with CMake."

**Agent Action 3:** `execute("cmake -B build -S .")`

**Observation 3:** "CMake Error: Compatibility with CMake < 3.5 has been removed..."

**Agent Thought 4:** "CMake version error. I need to fix CMakeLists.txt to use a newer version."

**Agent Action 4:** `read_file("CMakeLists.txt")`

**Observation 4:** "cmake_minimum_required(VERSION 3.0)..."

**Agent Action 5:** `write_file("CMakeLists.txt", "cmake_minimum_required(VERSION 3.10)...")`

**Agent Thought 5:** "Fixed. Let me retry the build."

**Agent Action 6:** `execute("cmake -B build -S .")`

**Observation 6:** "-- Configuring done..."

**Agent Action 7:** `execute("cmake --build build")`

**Observation 7:** "Build successful!"

**Agent Response:** "I've successfully built the project! Here's what I did:
1. Read the README to understand this is a CMake project
2. Found CMake version was outdated (3.0), updated to 3.10
3. Configured and built with CMake
4. Build completed successfully - executable at build/Debug/SynthgenParticles.exe"

## Implementation in C

### Tool Definition Structure
```c
typedef struct {
    const char* name;
    const char* description;
    const char* parameters_schema;  // JSON schema
    ToolHandler handler;
} AgentTool;

typedef char* (*ToolHandler)(const char* args_json);
```

### Agent State
```c
typedef struct {
    AIProvider* ai;
    AgentTool* tools;
    int tool_count;
    char** conversation;  // JSON messages
    int message_count;
    int max_iterations;
    bool verbose;
} AutonomousAgent;
```

### Main Loop
```c
char* agent_run(AutonomousAgent* agent, const char* user_request) {
    // Add user message
    agent_add_message(agent, "user", user_request);

    for (int i = 0; i < agent->max_iterations; i++) {
        // Generate AI response with tool definitions
        AIResponse* resp = ai_complete_with_tools(
            agent->ai,
            agent->conversation,
            agent->tools
        );

        if (resp->has_tool_calls) {
            // Execute each tool call
            for (int j = 0; j < resp->tool_call_count; j++) {
                ToolCall* call = &resp->tool_calls[j];
                char* result = execute_tool(agent, call->name, call->arguments);
                agent_add_tool_result(agent, call->id, result);
                free(result);
            }
        } else {
            // AI responded without tool use - task complete
            return strdup(resp->content);
        }
    }

    return strdup("Max iterations reached");
}
```

## Key Differences from Current System

| Current System | Autonomous Agent |
|----------------|------------------|
| Pattern matching for intent | AI decides what to do |
| Hard-coded actions | Dynamic tool selection |
| No file access | Full read/write access |
| No command execution | Can run any command |
| Single response | Iterative problem solving |
| Fails on first error | Retries with different approaches |
| No context awareness | Remembers full conversation |

## Security Considerations

1. **Sandboxing** - Agent should only access project directory
2. **Command allowlist** - Limit dangerous commands
3. **User confirmation** - Ask before destructive actions
4. **Rollback** - Track changes for undo capability

## Next Steps

1. Implement tool system in C
2. Create agent loop with ReAct pattern
3. Integrate with OpenAI-compatible tool calling API
4. Add to REPL as primary command handler
5. Test with real projects
