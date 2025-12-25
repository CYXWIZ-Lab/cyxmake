# CyxMake for JetBrains IDEs

AI-powered build automation plugin for IntelliJ IDEA, CLion, and other JetBrains IDEs.

## Features

- **One-Click Builds** - Build your project from the IDE
- **AI Error Recovery** - Automatically fix build errors
- **Tool Window** - Integrated build output console
- **Menu Integration** - Access from Build menu, context menus, and toolbar
- **Keyboard Shortcuts** - Quick access to common actions

## Supported IDEs

- IntelliJ IDEA (Community & Ultimate)
- CLion
- PyCharm
- WebStorm
- Other JetBrains IDEs (2023.3+)

## Installation

### From JetBrains Marketplace

1. Open IDE Settings (Ctrl+Alt+S)
2. Go to Plugins
3. Search for "CyxMake"
4. Click Install

### From Disk

1. Build the plugin: `./gradlew buildPlugin`
2. Open IDE Settings > Plugins
3. Click gear icon > Install Plugin from Disk
4. Select `build/distributions/cyxmake-0.2.0.zip`

### Prerequisites

CyxMake must be installed on your system:

```bash
git clone https://github.com/CYXWIZ-Lab/cyxmake.git
cd cyxmake && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## Usage

### Menu Actions

**Build Menu:**
- CyxMake > Build Project (Ctrl+Shift+F9)
- CyxMake > Build Release
- CyxMake > Build Debug
- CyxMake > Clean Build
- CyxMake > Analyze Project
- CyxMake > Fix Build Errors (Ctrl+Shift+Alt+F)
- CyxMake > Open REPL

### Context Menu

Right-click in editor or project view:
- Build with CyxMake
- Fix Build Errors

### Tool Window

View > Tool Windows > CyxMake

Shows:
- Build output
- Error messages
- Fix suggestions

## Configuration

Settings > Tools > CyxMake

| Setting | Description | Default |
|---------|-------------|---------|
| Executable Path | Path to CyxMake | `cyxmake` |
| Default Build Type | Debug or Release | `Debug` |
| Parallel Jobs | Number of jobs (0=auto) | `0` |
| Auto Analyze | Analyze on project open | `true` |

### AI Settings

| Setting | Description | Default |
|---------|-------------|---------|
| Enable AI | Enable AI features | `false` |
| AI Provider | Provider to use | `none` |
| Auto Fix | Auto-apply fixes | `false` |

## Building

### Requirements

- JDK 17+
- Gradle 8.0+

### Build Plugin

```bash
./gradlew buildPlugin
```

Output: `build/distributions/cyxmake-0.2.0.zip`

### Run in Development

```bash
./gradlew runIde
```

### Publish

```bash
export PUBLISH_TOKEN=your_token
./gradlew publishPlugin
```

## Project Structure

```
jetbrains/
├── build.gradle.kts           # Gradle build configuration
├── src/main/
│   ├── kotlin/com/cyxwiz/cyxmake/
│   │   ├── actions/           # Menu and toolbar actions
│   │   ├── services/          # Project services
│   │   ├── settings/          # Plugin settings
│   │   └── ui/                # Tool windows
│   └── resources/
│       ├── META-INF/plugin.xml  # Plugin descriptor
│       └── icons/             # Plugin icons
```

## License

Apache-2.0
