# CyxMake GitLab CI Integration

AI-powered build automation for GitLab CI/CD pipelines.

## Quick Start

Add to your `.gitlab-ci.yml`:

```yaml
include:
  - remote: 'https://raw.githubusercontent.com/CYXWIZ-Lab/cyxmake/main/integrations/gitlab-ci/.gitlab-ci-template.yml'

stages:
  - build

build:
  extends: .cyxmake-build
  stage: build
```

## Available Templates

### .cyxmake-build

Main build template with caching and artifact collection.

```yaml
my-build:
  extends: .cyxmake-build
  variables:
    CYXMAKE_BUILD_TYPE: "Release"
```

### .cyxmake-analyze

Project analysis template.

```yaml
analyze:
  extends: .cyxmake-analyze
  stage: analyze
```

### .cyxmake-clean

Clean build artifacts.

```yaml
clean:
  extends: .cyxmake-clean
  stage: clean
```

## Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `CYXMAKE_BUILD_TYPE` | Build type (Debug, Release) | `Release` |
| `CYXMAKE_AI_PROVIDER` | AI provider (none, openai, anthropic) | `none` |
| `CYXMAKE_COMMAND` | CyxMake command to run | `build` |
| `OPENAI_API_KEY` | OpenAI API key (for AI features) | |
| `ANTHROPIC_API_KEY` | Anthropic API key (for AI features) | |

## Examples

### Basic Build

```yaml
include:
  - remote: 'https://raw.githubusercontent.com/CYXWIZ-Lab/cyxmake/main/integrations/gitlab-ci/.gitlab-ci-template.yml'

stages:
  - build

build:
  extends: .cyxmake-build
```

### Debug and Release Builds

```yaml
include:
  - remote: 'https://raw.githubusercontent.com/CYXWIZ-Lab/cyxmake/main/integrations/gitlab-ci/.gitlab-ci-template.yml'

stages:
  - build

build:debug:
  extends: .cyxmake-build
  variables:
    CYXMAKE_BUILD_TYPE: "Debug"

build:release:
  extends: .cyxmake-build
  variables:
    CYXMAKE_BUILD_TYPE: "Release"
```

### With AI Error Recovery

```yaml
include:
  - remote: 'https://raw.githubusercontent.com/CYXWIZ-Lab/cyxmake/main/integrations/gitlab-ci/.gitlab-ci-template.yml'

stages:
  - build

build:
  extends: .cyxmake-build
  variables:
    CYXMAKE_AI_PROVIDER: "openai"
  # Set OPENAI_API_KEY in GitLab CI/CD Variables
```

### Multi-Platform Matrix

```yaml
include:
  - remote: 'https://raw.githubusercontent.com/CYXWIZ-Lab/cyxmake/main/integrations/gitlab-ci/.gitlab-ci-template.yml'

stages:
  - build

build:
  extends: .cyxmake-build
  parallel:
    matrix:
      - CYXMAKE_BUILD_TYPE: ["Debug", "Release"]
```

### Complete Pipeline

```yaml
include:
  - remote: 'https://raw.githubusercontent.com/CYXWIZ-Lab/cyxmake/main/integrations/gitlab-ci/.gitlab-ci-template.yml'

stages:
  - analyze
  - build
  - test

analyze:
  extends: .cyxmake-analyze
  stage: analyze
  only:
    - merge_requests

build:debug:
  extends: .cyxmake-build
  stage: build
  variables:
    CYXMAKE_BUILD_TYPE: "Debug"

build:release:
  extends: .cyxmake-build
  stage: build
  variables:
    CYXMAKE_BUILD_TYPE: "Release"

test:
  stage: test
  needs: ["build:debug"]
  script:
    - cd build && ctest --output-on-failure
```

### Custom Docker Image

```yaml
build:
  extends: .cyxmake-build
  image: ubuntu:22.04
  before_script:
    # Override setup for custom image
    - apt-get update && apt-get install -y cmake build-essential git
    - !reference [.cyxmake-build, before_script]
```

## Caching

The template automatically caches:
- `.cyxmake/` - Project analysis cache
- `build/` - Build artifacts

Cache key is based on the branch name.

## Artifacts

Collected artifacts:
- `.cyxmake/cyxmake.log` - Main log
- `.cyxmake/audit.log` - Audit trail

Artifacts expire after 1 week.

## Setting Up AI Features

1. Go to **Settings > CI/CD > Variables**
2. Add your API key:
   - Name: `OPENAI_API_KEY` or `ANTHROPIC_API_KEY`
   - Value: Your API key
   - Protected: Yes
   - Masked: Yes

3. Set the provider in your job:
   ```yaml
   variables:
     CYXMAKE_AI_PROVIDER: "openai"
   ```

## License

Apache-2.0
