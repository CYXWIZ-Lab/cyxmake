# CyxMake Core Library and Executable

# Core library sources (only implemented files for Phase 0)
set(CYXMAKE_CORE_SOURCES
    core/orchestrator.c
    core/logger.c
    core/build_executor.c
    core/file_ops.c
    core/rich_output.c
    core/config.c
    core/project_generator.c
    context/project_context.c
    context/project_analyzer.c
    context/project_graph.c
    context/cache_manager.c

    # Phase 1: LLM Integration
    llm/llm_interface.c
    llm/prompt_templates.c
    llm/error_analyzer.c
    llm/ai_provider.c
    llm/ai_build_agent.c
    llm/smart_agent.c
    llm/build_intelligence.c
    llm/autonomous_agent.c

    # Phase 1: Error Recovery System
    recovery/error_diagnosis.c
    recovery/error_patterns.c
    recovery/solution_generator.c
    recovery/fix_executor.c

    # Phase 3: Enhanced Error Recovery (validation, verification, learning)
    recovery/fix_validation.c

    # Phase 1: Tool Executor System
    tools/tool_registry.c
    tools/tool_discovery.c
    tools/tool_executor.c

    # Phase 2: Security System (permissions, audit, dry-run, rollback)
    cli/permission.c
    cli/security.c

    # Multi-Agent System
    agents/threading.c
    agents/agent_registry.c
    agents/task_queue.c
    agents/message_bus.c
    agents/shared_state.c
    agents/agent_coordinator.c

    # TODO Phase 1: Add remaining files when implemented
    # core/config.c
    # context/change_detector.c
    # llm/llm_local.c
    # llm/llm_cloud.c
)

# Create core library
add_library(cyxmake_core STATIC ${CYXMAKE_CORE_SOURCES})

target_include_directories(cyxmake_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external
)

target_link_libraries(cyxmake_core PUBLIC
    cjson
    tomlc99
)

# Link llama.cpp if available
if(TARGET llama)
    target_link_libraries(cyxmake_core PUBLIC llama)
    target_include_directories(cyxmake_core PUBLIC
        ${CMAKE_SOURCE_DIR}/external/llama.cpp/include
    )
    message(STATUS "Linking against llama.cpp")
endif()

# Link CURL if available
if(CURL_FOUND)
    target_link_libraries(cyxmake_core PUBLIC CURL::libcurl)
    target_compile_definitions(cyxmake_core PUBLIC CYXMAKE_USE_CURL)
    message(STATUS "CURL enabled - HTTP AI providers available")
endif()

# GPU backend compile definitions
if(CYXMAKE_GPU_CUDA)
    target_compile_definitions(cyxmake_core PUBLIC CYXMAKE_GPU_CUDA)
    message(STATUS "GPU backend: CUDA enabled")
elseif(CYXMAKE_GPU_VULKAN)
    target_compile_definitions(cyxmake_core PUBLIC CYXMAKE_GPU_VULKAN)
    message(STATUS "GPU backend: Vulkan enabled")
elseif(CYXMAKE_GPU_METAL)
    target_compile_definitions(cyxmake_core PUBLIC CYXMAKE_GPU_METAL)
    message(STATUS "GPU backend: Metal enabled")
elseif(CYXMAKE_GPU_OPENCL)
    target_compile_definitions(cyxmake_core PUBLIC CYXMAKE_GPU_OPENCL)
    message(STATUS "GPU backend: OpenCL enabled")
else()
    message(STATUS "GPU backend: None (CPU only)")
endif()

# Link pthreads on Unix systems (for multi-agent threading support)
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(cyxmake_core PUBLIC Threads::Threads)
    message(STATUS "Threading support enabled (pthreads)")
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(cyxmake_core PRIVATE /W4)
else()
    target_compile_options(cyxmake_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Sanitizers
if(CYXMAKE_USE_SANITIZERS AND NOT MSVC)
    target_compile_options(cyxmake_core PRIVATE -fsanitize=address,undefined)
    target_link_options(cyxmake_core PRIVATE -fsanitize=address,undefined)
endif()

# CLI sources (REPL, slash commands, permissions, context, input, planner)
set(CYXMAKE_CLI_SOURCES
    cli/main.c
    cli/repl.c
    cli/slash_commands.c
    cli/permission.c
    cli/conversation_context.c
    cli/input.c
    cli/action_planner.c
)

# Main executable
add_executable(cyxmake ${CYXMAKE_CLI_SOURCES})

target_link_libraries(cyxmake PRIVATE cyxmake_core)

# Install targets
install(TARGETS cyxmake cyxmake_core
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
