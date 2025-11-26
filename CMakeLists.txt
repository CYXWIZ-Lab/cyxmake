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

# GPU acceleration options (passed to llama.cpp/ggml)
option(CYXMAKE_GPU_CUDA "Enable CUDA GPU acceleration (NVIDIA)" OFF)
option(CYXMAKE_GPU_VULKAN "Enable Vulkan GPU acceleration (cross-platform)" OFF)
option(CYXMAKE_GPU_METAL "Enable Metal GPU acceleration (Apple)" OFF)
option(CYXMAKE_GPU_OPENCL "Enable OpenCL GPU acceleration" OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/external)

# Find optional packages
find_package(CURL QUIET)
if(CURL_FOUND)
    message(STATUS "Found CURL: ${CURL_LIBRARIES}")
    set(HAVE_CURL TRUE)
else()
    message(STATUS "CURL not found - cloud API support will be disabled")
    set(HAVE_CURL FALSE)
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
message(STATUS "")
