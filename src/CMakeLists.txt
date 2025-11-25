# CyxMake Core Library and Executable

# Core library sources (only implemented files for Phase 0)
set(CYXMAKE_CORE_SOURCES
    core/orchestrator.c
    core/logger.c
    core/build_executor.c
    context/project_context.c
    context/project_analyzer.c
    context/cache_manager.c

    # Phase 1: LLM Integration
    llm/llm_interface.c
    llm/prompt_templates.c
    llm/error_analyzer.c

    # Phase 1: Error Recovery System
    recovery/error_diagnosis.c
    recovery/error_patterns.c
    recovery/solution_generator.c
    recovery/fix_executor.c

    # Phase 1: Tool Executor System
    tools/tool_registry.c
    tools/tool_discovery.c
    tools/tool_executor.c

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

# Main executable
add_executable(cyxmake cli/main.c)

target_link_libraries(cyxmake PRIVATE cyxmake_core)

# Install targets
install(TARGETS cyxmake cyxmake_core
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
