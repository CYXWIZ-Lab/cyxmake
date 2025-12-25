# CyxMake for VS Code

AI-powered build automation extension for Visual Studio Code.

## Features

- **One-Click Builds** - Build your project with a single command
- **AI Error Recovery** - Automatically diagnose and fix build errors
- **Build Diagnostics** - See errors and warnings inline in your code
- **Integrated REPL** - Access CyxMake's natural language interface
- **Task Integration** - Use CyxMake with VS Code's task system
- **Status Bar** - Quick access to build status

## Installation

### From Marketplace

1. Open VS Code
2. Go to Extensions (Ctrl+Shift+X)
3. Search for "CyxMake"
4. Click Install

### From VSIX

```bash
code --install-extension cyxmake-0.2.0.vsix
```

### Prerequisites

CyxMake must be installed on your system:

```bash
# Clone and build
git clone https://github.com/CYXWIZ-Lab/cyxmake.git
cd cyxmake && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## Usage

### Commands

| Command | Description | Keybinding |
|---------|-------------|------------|
| `CyxMake: Build Project` | Build with default config | `Ctrl+Shift+B` |
| `CyxMake: Build Debug` | Build in Debug mode | |
| `CyxMake: Build Release` | Build in Release mode | |
| `CyxMake: Clean Build` | Clean build artifacts | |
| `CyxMake: Analyze Project` | Analyze project structure | |
| `CyxMake: Fix Build Errors` | AI-powered error fixing | `Ctrl+Shift+F` |
| `CyxMake: Open REPL` | Open interactive terminal | |

### Context Menu

Right-click on CMakeLists.txt or source files to access CyxMake commands.

### Tasks

Use CyxMake with VS Code's task runner:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "type": "cyxmake",
      "command": "build",
      "label": "CyxMake: Build",
      "group": {
        "kind": "build",
        "isDefault": true
      }
    }
  ]
}
```

### Status Bar

The status bar shows:
- $(tools) Ready
- $(sync~spin) Building...
- $(check) Success
- $(error) Failed

Click to view detailed status.

## Configuration

### Settings

| Setting | Description | Default |
|---------|-------------|---------|
| `cyxmake.enabled` | Enable extension | `true` |
| `cyxmake.executablePath` | Path to CyxMake | `cyxmake` |
| `cyxmake.defaultBuildType` | Default build type | `Debug` |
| `cyxmake.parallelJobs` | Parallel jobs (0=auto) | `0` |
| `cyxmake.autoAnalyze` | Analyze on open | `true` |
| `cyxmake.showStatusBar` | Show status bar | `true` |

### AI Settings

| Setting | Description | Default |
|---------|-------------|---------|
| `cyxmake.ai.enabled` | Enable AI features | `false` |
| `cyxmake.ai.provider` | AI provider | `none` |
| `cyxmake.ai.autoFix` | Auto-apply fixes | `false` |

### Diagnostics

| Setting | Description | Default |
|---------|-------------|---------|
| `cyxmake.diagnostics.enabled` | Show diagnostics | `true` |
| `cyxmake.diagnostics.showWarnings` | Show warnings | `true` |

## Diagnostics

Build errors appear:
- In the Problems panel
- As squiggly lines in the editor
- With hover tooltips

Errors are parsed from GCC, Clang, and MSVC output.

## AI Features

Enable AI for intelligent error fixing:

1. Set `cyxmake.ai.enabled` to `true`
2. Configure your AI provider:
   - `ollama` - Local AI (recommended)
   - `openai` - OpenAI API
   - `anthropic` - Claude API

3. Run `CyxMake: Fix Build Errors` when you encounter errors

## Troubleshooting

### CyxMake not found

Set the full path in settings:

```json
{
  "cyxmake.executablePath": "/usr/local/bin/cyxmake"
}
```

### No diagnostics shown

Check that:
1. `cyxmake.diagnostics.enabled` is `true`
2. Build has been run at least once
3. Error format matches GCC/Clang pattern

### AI not working

1. Verify AI provider is running (Ollama, etc.)
2. Check `cyxmake.toml` configuration
3. View Output panel for errors

## Development

### Building

```bash
npm install
npm run compile
```

### Packaging

```bash
npm run package
```

### Testing

```bash
npm test
```

## License

Apache-2.0
