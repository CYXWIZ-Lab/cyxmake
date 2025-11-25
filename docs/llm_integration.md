# CyxMake LLM Integration

## Overview

CyxMake integrates Large Language Models (LLMs) to provide intelligent build assistance, error diagnosis, and automated fix suggestions. The system uses llama.cpp for local inference with quantized models, ensuring privacy and fast response times.

## Features

### 1. Build Error Analysis
- **Automatic error type detection** (compilation, linker, missing dependencies)
- **Context-aware suggestions** based on project language and build system
- **Smart prompt generation** that adapts to error patterns
- **Installation commands** for missing dependencies

### 2. Build Configuration Generation
- Generate CMakeLists.txt, Makefiles, or other build configs
- Support for multiple languages and build systems
- Modern best practices included

### 3. Build Optimization
- Analyze build performance bottlenecks
- Suggest parallelization strategies
- Recommend caching improvements
- Incremental build optimization

## Architecture

```
┌─────────────────────────────────────────┐
│         Error Analyzer Module           │
│  • Error type detection                 │
│  • Interactive analysis                 │
│  • Fix suggestions                      │
└─────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────┐
│       Prompt Templates Module           │
│  • Smart prompt generation              │
│  • Error-specific templates             │
│  • Context injection                    │
└─────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────┐
│         LLM Interface Module            │
│  • Model loading (llama.cpp)            │
│  • Token generation                     │
│  • Response streaming                   │
└─────────────────────────────────────────┘
```

## Model Requirements

### Recommended Model
- **Model**: Qwen2.5-Coder-3B-Instruct-Q4_K_M.gguf
- **Size**: ~1.8 GB (quantized)
- **Context**: 8K tokens
- **Performance**: 20-30 tokens/sec on CPU

### Download Models
```bash
# Create models directory
mkdir -p ~/.cyxmake/models

# Download recommended model
wget https://huggingface.co/Qwen/Qwen2.5-Coder-3B-Instruct-GGUF/resolve/main/qwen2.5-coder-3b-instruct-q4_k_m.gguf \
     -O ~/.cyxmake/models/qwen2.5-coder-3b-instruct-q4_k_m.gguf
```

## Usage

### Command Line Interface

#### Test LLM Integration
```bash
cyxmake test-llm [model_path]
```

#### Analyze Build Errors
When a build fails, CyxMake can automatically analyze the error:

```bash
cyxmake build --analyze-errors
```

Example output:
```
Build failed with exit code: 1

AI Analysis:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
The error indicates SDL2 is not found.

Install SDL2 on your system:
- Ubuntu/Debian: sudo apt-get install libsdl2-dev
- macOS: brew install sdl2
- Windows: Download from https://www.libsdl.org/download-2.0.php

Then add to your CMakeLists.txt:
find_package(SDL2 REQUIRED)
target_link_libraries(your_target ${SDL2_LIBRARIES})
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

### API Usage

#### Basic Error Analysis
```c
#include "cyxmake/llm_interface.h"
#include "cyxmake/error_analyzer.h"

// Initialize LLM
LLMConfig* config = llm_config_default();
config->model_path = "~/.cyxmake/models/qwen2.5-coder-3b-instruct-q4_k_m.gguf";
LLMContext* llm = llm_init(config);

// Create analyzer
ErrorAnalyzer* analyzer = error_analyzer_create(llm, project_ctx);

// Analyze build error
BuildResult* result = build_execute(project_ctx, NULL);
if (!result->success) {
    char* suggestion = error_analyzer_analyze(analyzer, result);
    printf("AI Suggestion:\n%s\n", suggestion);
    free(suggestion);
}

// Cleanup
error_analyzer_free(analyzer);
llm_shutdown(llm);
llm_config_free(config);
```

#### Generate Build Configuration
```c
// Generate CMakeLists.txt for a C++ project
char* config = error_analyzer_generate_config(
    analyzer,
    "application",  // project type
    "C++",          // language
    "SDL2,OpenGL"   // dependencies
);

if (config) {
    // Save to file
    FILE* f = fopen("CMakeLists.txt", "w");
    fprintf(f, "%s", config);
    fclose(f);
    free(config);
}
```

#### Get Installation Commands
```c
// Get install command for missing dependency
char* cmd = error_analyzer_get_install_cmd(analyzer, "boost");
if (cmd) {
    printf("Install with: %s\n", cmd);
    free(cmd);
}
```

## Prompt Templates

### Error Type Detection
The system automatically detects error types:
- **Compilation errors**: Syntax, type mismatches, undefined references
- **Linker errors**: Missing symbols, libraries not found
- **Missing headers**: Include file not found
- **Missing libraries**: Cannot find -lXXX
- **Configuration errors**: CMake/build system issues

### Smart Prompt Generation
Based on error type, different prompts are generated:

```c
// For linker errors
"I'm getting a linker error with undefined symbols:
 undefined reference to 'SDL_Init'
 What libraries am I missing and how do I link them?"

// For missing dependencies
"I need to install 'boost' for my CMake project on Linux.
 Please provide the package manager command and verification steps."

// For compilation errors
"File: main.cpp, Line: 42
 Error: 'vector' is not a member of 'std'
 Show the fix and explain the issue."
```

## Performance

### Inference Speed
- **First token**: 1-2 seconds (model loading)
- **Generation**: 20-30 tokens/sec on modern CPU
- **Typical response**: 2-5 seconds total

### Memory Usage
- **Model in RAM**: ~2 GB
- **Context buffer**: ~100 MB
- **Total**: ~2.5 GB when active

### Optimization Tips
1. **Use memory mapping** (enabled by default)
2. **Adjust thread count** based on CPU cores
3. **Lower temperature** for factual responses (0.3)
4. **Limit max tokens** for faster responses

## Configuration

### LLM Configuration Options
```c
typedef struct {
    const char* model_path;    // Path to GGUF model
    int n_ctx;                 // Context size (default: 8192)
    int n_threads;             // CPU threads (0 = auto)
    int n_gpu_layers;          // GPU layers (0 = CPU only)
    bool use_mmap;             // Memory mapping (default: true)
    bool use_mlock;            // Lock in RAM (default: false)
    bool verbose;              // Debug output (default: false)
} LLMConfig;
```

### Request Configuration
```c
typedef struct {
    const char* prompt;        // Input prompt
    int max_tokens;            // Max generation (default: 512)
    float temperature;         // Creativity (default: 0.7)
    int top_k;                 // Top-K sampling (default: 40)
    float top_p;               // Nucleus sampling (default: 0.9)
    float repeat_penalty;      // Repetition penalty (default: 1.1)
    const char* stop_sequence; // Stop token (optional)
} LLMRequest;
```

## Error Handling

### Common Issues

#### Model Not Found
```
Error: Model validation failed
Please provide a valid GGUF model file:
1. Download from: https://huggingface.co/Qwen/Qwen2.5-Coder-3B-Instruct-GGUF
2. Place at: ~/.cyxmake/models/qwen2.5-coder-3b-instruct-q4_k_m.gguf
```

#### Out of Memory
- Use smaller model (1B instead of 3B)
- Reduce context size (4096 instead of 8192)
- Enable memory mapping
- Close other applications

#### Slow Inference
- Check CPU usage (should be near 100%)
- Adjust thread count to CPU cores - 1
- Use quantized models (Q4_K_M or Q5_K_M)
- Disable verbose logging

## Privacy & Security

### Local Inference
- **All processing happens locally** - no cloud API calls
- **No code sent to external servers**
- **Model runs entirely on your machine**
- **No telemetry or usage tracking**

### Cloud Fallback (Optional)
- Can be configured for complex errors
- Only error messages sent (no source code)
- Requires explicit opt-in and API key
- Disabled by default

## Supported Error Patterns

### C/C++ Errors
- Undefined references
- Missing headers (stdio.h, vector, etc.)
- Template errors
- Linking errors
- Missing libraries

### Build System Errors
- CMake configuration issues
- Missing Find modules
- Compiler detection failures
- Platform-specific issues

### Dependency Errors
- Package not found
- Version mismatches
- Missing system libraries
- pkg-config failures

## Examples

### Example 1: Missing Library
```
Error: undefined reference to 'pthread_create'

AI Analysis:
You're missing the pthread library. Add this to your build:
- GCC/Clang: Add -lpthread to your link flags
- CMake: Add find_package(Threads REQUIRED) and link ${CMAKE_THREAD_LIBS_INIT}
- Make: LDFLAGS += -lpthread
```

### Example 2: CMake Error
```
Error: Could not find a package configuration file for "Qt5"

AI Analysis:
Qt5 is not found by CMake. Solutions:
1. Install Qt5: sudo apt-get install qt5-default
2. Set CMAKE_PREFIX_PATH: -DCMAKE_PREFIX_PATH=/path/to/qt5
3. Or set Qt5_DIR: -DQt5_DIR=/path/to/qt5/lib/cmake/Qt5
```

### Example 3: Compilation Error
```
Error: 'cout' was not declared in this scope

AI Analysis:
Missing namespace or header. Fix:
1. Add: #include <iostream>
2. Use: std::cout instead of cout
3. Or add: using namespace std;
```

## API Reference

See individual header files for detailed API documentation:
- `include/cyxmake/llm_interface.h` - Core LLM interface
- `include/cyxmake/prompt_templates.h` - Prompt generation
- `include/cyxmake/error_analyzer.h` - Error analysis API

## Future Enhancements

### Planned Features
1. **Multi-model support** - Switch between different models
2. **Streaming responses** - Real-time token generation
3. **Fine-tuning** - Custom model training on build errors
4. **Context caching** - Faster repeated queries
5. **Batch processing** - Analyze multiple errors at once

### Model Improvements
- Support for larger models (7B, 13B)
- GPU acceleration (CUDA, Metal)
- Custom fine-tuned models for build systems
- Multi-language model support

## Contributing

To contribute to LLM integration:

1. **Add new error patterns** in `prompt_templates.c`
2. **Improve prompt engineering** for better responses
3. **Test with different models** and report performance
4. **Add language-specific error handling**
5. **Optimize inference performance**

## License

The LLM integration uses:
- **llama.cpp**: MIT License
- **Qwen models**: Apache 2.0 License
- **CyxMake LLM code**: Apache 2.0 License