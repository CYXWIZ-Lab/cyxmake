cmake_minimum_required(VERSION 3.16)
project(hello_plugin VERSION 1.0.0 LANGUAGES C)

# Find CyxMake headers
# Adjust this path to your CyxMake installation
set(CYXMAKE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../include")

# Create shared library
add_library(hello_plugin SHARED hello_plugin.c)

# Include CyxMake headers
target_include_directories(hello_plugin PRIVATE ${CYXMAKE_INCLUDE_DIR})

# Set output name without 'lib' prefix on Unix
set_target_properties(hello_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "hello_plugin"
)

# Platform-specific settings
if(WIN32)
    set_target_properties(hello_plugin PROPERTIES
        SUFFIX ".dll"
    )
elseif(APPLE)
    set_target_properties(hello_plugin PROPERTIES
        SUFFIX ".dylib"
    )
else()
    set_target_properties(hello_plugin PROPERTIES
        SUFFIX ".so"
    )
    # Enable position-independent code
    target_compile_options(hello_plugin PRIVATE -fPIC)
endif()

# Install
install(TARGETS hello_plugin
    LIBRARY DESTINATION lib/cyxmake/plugins
    RUNTIME DESTINATION lib/cyxmake/plugins
)
