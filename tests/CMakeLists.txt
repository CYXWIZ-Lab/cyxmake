# CyxMake Tests

# Logger test executable
add_executable(test_logger test_logger.c)
target_link_libraries(test_logger PRIVATE cyxmake_core)
target_include_directories(test_logger PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Set output directory
set_target_properties(test_logger PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

# Copy to bin directory for easy access
add_custom_command(TARGET test_logger POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:test_logger>
        ${CMAKE_BINARY_DIR}/bin/test_logger${CMAKE_EXECUTABLE_SUFFIX}
    COMMENT "Copying test_logger to bin directory"
)

# Error Recovery test executable
add_executable(test_error_recovery test_error_recovery.c)
target_link_libraries(test_error_recovery PRIVATE cyxmake_core)
target_include_directories(test_error_recovery PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Set output directory
set_target_properties(test_error_recovery PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

# Copy to bin directory for easy access
add_custom_command(TARGET test_error_recovery POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:test_error_recovery>
        ${CMAKE_BINARY_DIR}/bin/test_error_recovery${CMAKE_EXECUTABLE_SUFFIX}
    COMMENT "Copying test_error_recovery to bin directory"
)

message(STATUS "Tests configured: test_logger, test_error_recovery")

# TODO: Add more unit tests
# TODO: Add integration tests
