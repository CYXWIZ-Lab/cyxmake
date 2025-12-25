# CyxMake GitHub Action

AI-powered build automation for your GitHub workflows.

## Quick Start

```yaml
name: Build with CyxMake

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Build with CyxMake
        uses: CYXWIZ-Lab/cyxmake/integrations/github-action@main
        with:
          command: build
          build-type: Release
```

## Inputs

| Input | Description | Default |
|-------|-------------|---------|
| `command` | CyxMake command (build, analyze, clean) | `build` |
| `project-path` | Path to project directory | `.` |
| `build-type` | Build type (Debug, Release, etc.) | `Release` |
| `parallel-jobs` | Number of parallel jobs (0 = auto) | `0` |
| `ai-provider` | AI provider (ollama, openai, anthropic, none) | `none` |
| `ai-api-key` | API key for cloud AI | |
| `dry-run` | Run without making changes | `false` |
| `auto-fix` | Automatically fix errors | `false` |
| `cyxmake-version` | Version to use | `latest` |
| `cache` | Enable build caching | `true` |

## Outputs

| Output | Description |
|--------|-------------|
| `success` | Whether the build succeeded |
| `build-time` | Total build time in seconds |
| `errors-fixed` | Number of errors automatically fixed |
| `cache-hit` | Whether cache was hit |

## Examples

### Basic Build

```yaml
- uses: CYXWIZ-Lab/cyxmake/integrations/github-action@main
  with:
    command: build
```

### Release Build with Caching

```yaml
- uses: CYXWIZ-Lab/cyxmake/integrations/github-action@main
  with:
    command: build
    build-type: Release
    cache: true
```

### Build with AI Error Recovery

```yaml
- uses: CYXWIZ-Lab/cyxmake/integrations/github-action@main
  with:
    command: build
    ai-provider: openai
    ai-api-key: ${{ secrets.OPENAI_API_KEY }}
    auto-fix: true
```

### Multi-Platform Build

```yaml
jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
        build-type: [Debug, Release]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - uses: CYXWIZ-Lab/cyxmake/integrations/github-action@main
        with:
          build-type: ${{ matrix.build-type }}
```

### Analyze Project

```yaml
- uses: CYXWIZ-Lab/cyxmake/integrations/github-action@main
  with:
    command: analyze
```

### Dry Run (Preview)

```yaml
- uses: CYXWIZ-Lab/cyxmake/integrations/github-action@main
  with:
    command: build
    dry-run: true
```

## Complete Workflow Example

```yaml
name: CyxMake CI

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  analyze:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Analyze Project
        uses: CYXWIZ-Lab/cyxmake/integrations/github-action@main
        with:
          command: analyze

  build:
    needs: analyze
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
        build-type: [Debug, Release]
    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v4

      - name: Build
        id: build
        uses: CYXWIZ-Lab/cyxmake/integrations/github-action@main
        with:
          command: build
          build-type: ${{ matrix.build-type }}
          cache: true

      - name: Report
        run: |
          echo "Build succeeded: ${{ steps.build.outputs.success }}"
          echo "Build time: ${{ steps.build.outputs.build-time }}s"

  test:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Build and Test
        uses: CYXWIZ-Lab/cyxmake/integrations/github-action@main
        with:
          command: build
          build-type: Debug

      - name: Run Tests
        run: ctest --test-dir build --output-on-failure
```

## Caching

The action automatically caches:
- `.cyxmake/` directory (project analysis)
- `build/` directory (build artifacts)

Cache key is based on:
- Operating system
- Build type
- Hash of build configuration files (CMakeLists.txt, Cargo.toml, package.json)

## Artifacts

Build logs are automatically uploaded as artifacts:
- `cyxmake.log` - Main log file
- `audit.log` - Audit trail

## License

Apache-2.0
