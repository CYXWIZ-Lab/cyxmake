# External Dependencies for CyxMake

# cJSON - JSON parser
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cJSON")
    message(STATUS "Downloading cJSON...")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/cJSON/cJSON.c"
    )
    file(DOWNLOAD
        "https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/cJSON/cJSON.h"
    )
endif()

add_library(cjson STATIC cJSON/cJSON.c)
target_include_directories(cjson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cJSON)

# tomlc99 - TOML parser
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tomlc99")
    message(STATUS "Downloading tomlc99...")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/cktan/tomlc99/master/toml.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/tomlc99/toml.c"
    )
    file(DOWNLOAD
        "https://raw.githubusercontent.com/cktan/tomlc99/master/toml.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/tomlc99/toml.h"
    )
endif()

add_library(tomlc99 STATIC tomlc99/toml.c)
target_include_directories(tomlc99 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tomlc99)

# llama.cpp - LLM inference engine
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp/CMakeLists.txt")
    message(STATUS "Adding llama.cpp...")

    # Configure llama.cpp build options
    set(LLAMA_BUILD_TESTS OFF CACHE BOOL "Disable llama.cpp tests" FORCE)
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "Disable llama.cpp examples" FORCE)
    set(LLAMA_BUILD_SERVER OFF CACHE BOOL "Disable llama.cpp server" FORCE)
    set(LLAMA_BUILD_COMMON OFF CACHE BOOL "Disable llama.cpp common library" FORCE)
    set(LLAMA_NATIVE OFF CACHE BOOL "Disable native optimizations for portability" FORCE)

    # GPU acceleration options - pass through from parent CMake
    # CUDA (NVIDIA GPUs)
    if(CYXMAKE_GPU_CUDA)
        set(GGML_CUDA ON CACHE BOOL "Enable CUDA" FORCE)
        message(STATUS "  CUDA GPU acceleration: ENABLED")
    else()
        set(GGML_CUDA OFF CACHE BOOL "Enable CUDA" FORCE)
    endif()

    # Vulkan (cross-platform GPU)
    if(CYXMAKE_GPU_VULKAN)
        set(GGML_VULKAN ON CACHE BOOL "Enable Vulkan" FORCE)
        message(STATUS "  Vulkan GPU acceleration: ENABLED")
    else()
        set(GGML_VULKAN OFF CACHE BOOL "Enable Vulkan" FORCE)
    endif()

    # Metal (Apple GPUs)
    if(CYXMAKE_GPU_METAL)
        set(GGML_METAL ON CACHE BOOL "Enable Metal" FORCE)
        message(STATUS "  Metal GPU acceleration: ENABLED")
    else()
        set(GGML_METAL OFF CACHE BOOL "Enable Metal" FORCE)
    endif()

    # OpenCL
    if(CYXMAKE_GPU_OPENCL)
        set(GGML_OPENCL ON CACHE BOOL "Enable OpenCL" FORCE)
        message(STATUS "  OpenCL GPU acceleration: ENABLED")
    else()
        set(GGML_OPENCL OFF CACHE BOOL "Enable OpenCL" FORCE)
    endif()

    add_subdirectory(llama.cpp)

    message(STATUS "llama.cpp integration complete")
else()
    message(WARNING "llama.cpp submodule not found. Run: git submodule update --init --recursive")
endif()
