# CyxMake Integrations

> IDE extensions and CI/CD integrations for CyxMake

## Available Integrations

| Integration | Status | Description |
|------------|--------|-------------|
| [GitHub Actions](github-action/) | Ready | GitHub Actions for CI/CD |
| [GitLab CI](gitlab-ci/) | Ready | GitLab CI/CD templates |
| [VS Code](vscode/) | Beta | Visual Studio Code extension |
| [JetBrains](jetbrains/) | Beta | IntelliJ/CLion/PyCharm plugin |

---

## GitHub Actions

Add AI-powered builds to your GitHub workflows.

```yaml
- uses: CYXWIZ-Lab/cyxmake/integrations/github-action@main
  with:
    command: build
    build-type: Release
```

[Full Documentation](github-action/README.md)

---

## GitLab CI

Include CyxMake in your GitLab pipelines.

```yaml
include:
  - remote: 'https://raw.githubusercontent.com/CYXWIZ-Lab/cyxmake/main/integrations/gitlab-ci/.gitlab-ci-template.yml'

build:
  extends: .cyxmake-build
```

[Full Documentation](gitlab-ci/README.md)

---

## VS Code Extension

Build automation in Visual Studio Code.

### Features
- One-click builds
- AI error recovery
- Build diagnostics
- Integrated REPL

### Installation

```bash
# From marketplace
code --install-extension cyxwiz-lab.cyxmake

# From source
cd vscode && npm install && npm run package
code --install-extension cyxmake-0.2.0.vsix
```

[Full Documentation](vscode/README.md)

---

## JetBrains Plugin

Build automation for IntelliJ IDEA, CLion, and other JetBrains IDEs.

### Features
- Build menu integration
- Tool window output
- Keyboard shortcuts
- AI features

### Installation

```bash
# From JetBrains Marketplace
# Search for "CyxMake" in Settings > Plugins

# From source
cd jetbrains && ./gradlew buildPlugin
```

[Full Documentation](jetbrains/README.md)

---

## Plugin System

Extend CyxMake with custom tools, error patterns, and AI providers.

See [Plugin Development Guide](../docs/PLUGINS.md)

---

## Contributing

We welcome contributions! To add a new integration:

1. Create directory under `integrations/`
2. Include README with setup instructions
3. Add tests if applicable
4. Submit pull request

---

*CyxMake Integrations v0.2.0*
