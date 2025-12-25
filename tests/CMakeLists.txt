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

# Tool Executor test executable
add_executable(test_tool_executor test_tool_executor.c)
target_link_libraries(test_tool_executor PRIVATE cyxmake_core)
target_include_directories(test_tool_executor PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Set output directory
set_target_properties(test_tool_executor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

# Copy to bin directory for easy access
add_custom_command(TARGET test_tool_executor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:test_tool_executor>
        ${CMAKE_BINARY_DIR}/bin/test_tool_executor${CMAKE_EXECUTABLE_SUFFIX}
    COMMENT "Copying test_tool_executor to bin directory"
)

# AI Agent test executable
add_executable(test_ai_agent test_ai_agent.c)
target_link_libraries(test_ai_agent PRIVATE cyxmake_core)
target_include_directories(test_ai_agent PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Set output directory
set_target_properties(test_ai_agent PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

# Copy to bin directory for easy access
add_custom_command(TARGET test_ai_agent POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:test_ai_agent>
        ${CMAKE_BINARY_DIR}/bin/test_ai_agent${CMAKE_EXECUTABLE_SUFFIX}
    COMMENT "Copying test_ai_agent to bin directory"
)

# Recovery Integration test executable
add_executable(test_recovery_integration test_recovery_integration.c)
target_link_libraries(test_recovery_integration PRIVATE cyxmake_core)
target_include_directories(test_recovery_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Set output directory
set_target_properties(test_recovery_integration PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

# Copy to bin directory for easy access
add_custom_command(TARGET test_recovery_integration POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:test_recovery_integration>
        ${CMAKE_BINARY_DIR}/bin/test_recovery_integration${CMAKE_EXECUTABLE_SUFFIX}
    COMMENT "Copying test_recovery_integration to bin directory"
)

# Security System test executable (Phase 2)
add_executable(test_security test_security.c)
target_link_libraries(test_security PRIVATE cyxmake_core)
target_include_directories(test_security PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(test_security PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

add_custom_command(TARGET test_security POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:test_security>
        ${CMAKE_BINARY_DIR}/bin/test_security${CMAKE_EXECUTABLE_SUFFIX}
    COMMENT "Copying test_security to bin directory"
)

# Fix Validation test executable (Phase 3)
add_executable(test_fix_validation test_fix_validation.c)
target_link_libraries(test_fix_validation PRIVATE cyxmake_core)
target_include_directories(test_fix_validation PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(test_fix_validation PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

add_custom_command(TARGET test_fix_validation POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:test_fix_validation>
        ${CMAKE_BINARY_DIR}/bin/test_fix_validation${CMAKE_EXECUTABLE_SUFFIX}
    COMMENT "Copying test_fix_validation to bin directory"
)

# Register tests with CTest
add_test(NAME test_logger COMMAND test_logger)
add_test(NAME test_error_recovery COMMAND test_error_recovery)
add_test(NAME test_tool_executor COMMAND test_tool_executor)
add_test(NAME test_ai_agent COMMAND test_ai_agent)
add_test(NAME test_recovery_integration COMMAND test_recovery_integration)
add_test(NAME test_security COMMAND test_security)
add_test(NAME test_fix_validation COMMAND test_fix_validation)

message(STATUS "Tests configured: test_logger, test_error_recovery, test_tool_executor, test_ai_agent, test_recovery_integration, test_security, test_fix_validation")
