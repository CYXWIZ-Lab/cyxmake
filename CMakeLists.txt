cmake_minimum_required(VERSION 3.20)
project(CyxMake VERSION 0.1.0 LANGUAGES C CXX)

# Project description
set(PROJECT_DESCRIPTION "AI-Powered Build Automation System")
set(PROJECT_HOMEPAGE_URL "https://github.com/cyxmake/cyxmake")

# C/C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Options
option(CYXMAKE_BUILD_TESTS "Build test suite" ON)
option(CYXMAKE_BUILD_TOOLS "Build bundled tools" ON)
option(CYXMAKE_USE_SANITIZERS "Enable address sanitizer" OFF)
option(CYXMAKE_STATIC_LINK "Static linking" OFF)
option(CYXMAKE_REQUIRE_CURL "Require CURL for cloud AI providers (fail if not found)" OFF)
option(CYXMAKE_ENABLE_DISTRIBUTED "Enable distributed build support" OFF)

# GPU acceleration options (passed to llama.cpp/ggml)
option(CYXMAKE_GPU_CUDA "Enable CUDA GPU acceleration (NVIDIA)" OFF)
option(CYXMAKE_GPU_VULKAN "Enable Vulkan GPU acceleration (cross-platform)" OFF)
option(CYXMAKE_GPU_METAL "Enable Metal GPU acceleration (Apple)" OFF)
option(CYXMAKE_GPU_OPENCL "Enable OpenCL GPU acceleration" OFF)

# Auto-detect GPU backends if not explicitly set
if(NOT CYXMAKE_GPU_CUDA AND NOT CYXMAKE_GPU_VULKAN AND NOT CYXMAKE_GPU_METAL AND NOT CYXMAKE_GPU_OPENCL)
    # Check for CUDA
    include(CheckLanguage)
    check_language(CUDA)
    if(CMAKE_CUDA_COMPILER)
        message(STATUS "CUDA detected - enabling GPU acceleration")
        set(CYXMAKE_GPU_CUDA ON CACHE BOOL "Enable CUDA GPU acceleration" FORCE)
    endif()

    # Check for Vulkan (fallback if no CUDA)
    if(NOT CYXMAKE_GPU_CUDA)
        find_package(Vulkan QUIET)
        if(Vulkan_FOUND)
            message(STATUS "Vulkan detected - enabling GPU acceleration")
            set(CYXMAKE_GPU_VULKAN ON CACHE BOOL "Enable Vulkan GPU acceleration" FORCE)
        endif()
    endif()

    # Check for Metal (macOS only, fallback)
    if(APPLE AND NOT CYXMAKE_GPU_CUDA AND NOT CYXMAKE_GPU_VULKAN)
        message(STATUS "macOS detected - enabling Metal GPU acceleration")
        set(CYXMAKE_GPU_METAL ON CACHE BOOL "Enable Metal GPU acceleration" FORCE)
    endif()
endif()

# Report GPU backend status
if(CYXMAKE_GPU_CUDA)
    message(STATUS "GPU Backend: CUDA (NVIDIA)")
elseif(CYXMAKE_GPU_VULKAN)
    message(STATUS "GPU Backend: Vulkan")
elseif(CYXMAKE_GPU_METAL)
    message(STATUS "GPU Backend: Metal (Apple)")
elseif(CYXMAKE_GPU_OPENCL)
    message(STATUS "GPU Backend: OpenCL")
else()
    message(STATUS "GPU Backend: None (CPU only)")
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/external)

# Find CURL for cloud AI providers
find_package(CURL QUIET)
if(CURL_FOUND)
    message(STATUS "Found CURL: ${CURL_LIBRARIES}")
    set(HAVE_CURL TRUE)
else()
    set(HAVE_CURL FALSE)
    if(CYXMAKE_REQUIRE_CURL)
        message(FATAL_ERROR
            "CURL not found but CYXMAKE_REQUIRE_CURL is ON.\n"
            "Cloud AI providers (OpenAI, Anthropic, Gemini, Ollama) require CURL.\n"
            "To install CURL:\n"
            "  - Windows: vcpkg install curl:x64-windows OR winget install curl\n"
            "  - macOS:   brew install curl\n"
            "  - Ubuntu:  sudo apt install libcurl4-openssl-dev\n"
            "  - Fedora:  sudo dnf install libcurl-devel\n"
            "Or disable this requirement: cmake -DCYXMAKE_REQUIRE_CURL=OFF ..")
    else()
        message(WARNING
            "CURL not found - cloud AI providers will be disabled.\n"
            "Only local llama.cpp inference will be available.\n"
            "To enable cloud AI providers, install CURL:\n"
            "  - Windows: vcpkg install curl:x64-windows\n"
            "  - macOS:   brew install curl\n"
            "  - Linux:   sudo apt install libcurl4-openssl-dev (or equivalent)")
    endif()
endif()

# Find libwebsockets for distributed builds
if(CYXMAKE_ENABLE_DISTRIBUTED)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBWEBSOCKETS libwebsockets)
    endif()

    if(NOT LIBWEBSOCKETS_FOUND)
        # Try manual search
        find_path(LIBWEBSOCKETS_INCLUDE_DIR libwebsockets.h
            PATHS /usr/include /usr/local/include /opt/local/include
        )
        find_library(LIBWEBSOCKETS_LIBRARY websockets
            PATHS /usr/lib /usr/local/lib /opt/local/lib
        )
        if(LIBWEBSOCKETS_INCLUDE_DIR AND LIBWEBSOCKETS_LIBRARY)
            set(LIBWEBSOCKETS_FOUND TRUE)
            set(LIBWEBSOCKETS_INCLUDE_DIRS ${LIBWEBSOCKETS_INCLUDE_DIR})
            set(LIBWEBSOCKETS_LIBRARIES ${LIBWEBSOCKETS_LIBRARY})
        endif()
    endif()

    if(LIBWEBSOCKETS_FOUND)
        message(STATUS "Found libwebsockets: ${LIBWEBSOCKETS_LIBRARIES}")
        set(HAVE_LIBWEBSOCKETS TRUE)
    else()
        message(WARNING
            "libwebsockets not found - distributed builds will be disabled.\n"
            "To enable distributed builds, install libwebsockets:\n"
            "  - Windows: vcpkg install libwebsockets:x64-windows\n"
            "  - macOS:   brew install libwebsockets\n"
            "  - Ubuntu:  sudo apt install libwebsockets-dev\n"
            "  - Fedora:  sudo dnf install libwebsockets-devel")
        set(HAVE_LIBWEBSOCKETS FALSE)
        set(CYXMAKE_ENABLE_DISTRIBUTED OFF)
    endif()

    # Find OpenSSL for TLS support
    if(CYXMAKE_ENABLE_DISTRIBUTED)
        find_package(OpenSSL QUIET)
        if(OPENSSL_FOUND)
            message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
            set(HAVE_OPENSSL TRUE)
        else()
            message(WARNING "OpenSSL not found - TLS support disabled for distributed builds")
            set(HAVE_OPENSSL FALSE)
        endif()
    endif()
endif()

# Add external dependencies
add_subdirectory(external)

# Add source directories
add_subdirectory(src)

# Add tools (Python analyzer, bash executor, etc.)
if(CYXMAKE_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# Add tests
if(CYXMAKE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS cyxmake
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/cyxmake
    DESTINATION include
)

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/CyxMakeConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Print configuration summary
message(STATUS "")
message(STATUS "CyxMake Configuration Summary")
message(STATUS "=============================")
message(STATUS "Version:          ${PROJECT_VERSION}")
message(STATUS "Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler:       ${CMAKE_C_COMPILER}")
message(STATUS "C++ Compiler:     ${CMAKE_CXX_COMPILER}")
message(STATUS "Install prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Build tests:      ${CYXMAKE_BUILD_TESTS}")
message(STATUS "Build tools:      ${CYXMAKE_BUILD_TOOLS}")
message(STATUS "Use sanitizers:   ${CYXMAKE_USE_SANITIZERS}")
message(STATUS "GPU CUDA:         ${CYXMAKE_GPU_CUDA}")
message(STATUS "GPU Vulkan:       ${CYXMAKE_GPU_VULKAN}")
message(STATUS "GPU Metal:        ${CYXMAKE_GPU_METAL}")
message(STATUS "GPU OpenCL:       ${CYXMAKE_GPU_OPENCL}")
message(STATUS "CURL (HTTP):      ${HAVE_CURL}")
if(NOT HAVE_CURL)
    message(STATUS "  (Cloud AI providers disabled - install CURL to enable)")
endif()
message(STATUS "Distributed:      ${CYXMAKE_ENABLE_DISTRIBUTED}")
if(CYXMAKE_ENABLE_DISTRIBUTED)
    message(STATUS "  libwebsockets:  ${HAVE_LIBWEBSOCKETS}")
    message(STATUS "  OpenSSL (TLS):  ${HAVE_OPENSSL}")
endif()
message(STATUS "")
