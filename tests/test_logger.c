/**
 * @file test_logger.c
 * @brief Test program for the logger system
 */

#include "cyxmake/logger.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== CyxMake Logger Test Suite ===\n\n");

    /* Test 1: Default initialization */
    printf("Test 1: Default initialization\n");
    log_init(NULL);
    log_info("Logger initialized with defaults");
    printf("\n");

    /* Test 2: All log levels */
    printf("Test 2: All log levels\n");
    log_debug("This is a DEBUG message");
    log_info("This is an INFO message");
    log_success("This is a SUCCESS message");
    log_warning("This is a WARNING message");
    log_error("This is an ERROR message");
    printf("\n");

    /* Test 3: Step logging */
    printf("Test 3: Step logging (simulating project analysis)\n");
    log_info("Analyzing project...");
    log_step(1, 5, "Detecting primary language...");
    log_step(2, 5, "Detecting build system...");
    log_step(3, 5, "Scanning source files...");
    log_step(4, 5, "Calculating statistics...");
    log_step(5, 5, "Generating cache...");
    log_success("Analysis complete!");
    printf("\n");

    /* Test 4: Custom prefix */
    printf("Test 4: Custom prefix\n");
    log_with_prefix("[BUILD]", "Compiling main.c...");
    log_with_prefix("[BUILD]", "Linking executable...");
    log_with_prefix("[TEST] ", "Running unit tests...");
    printf("\n");

    /* Test 5: Plain output */
    printf("Test 5: Plain output (no formatting)\n");
    log_plain("Plain message without any formatting\n");
    log_plain("Useful for raw output: stdout, stderr redirection\n");
    printf("\n");

    /* Test 6: Different log levels */
    printf("Test 6: Setting minimum log level to WARNING\n");
    log_set_level(LOG_LEVEL_WARNING);
    log_debug("This DEBUG should NOT appear");
    log_info("This INFO should NOT appear");
    log_warning("This WARNING SHOULD appear");
    log_error("This ERROR SHOULD appear");
    printf("\n");

    /* Test 7: Disable colors */
    printf("Test 7: Disabling colors\n");
    log_set_level(LOG_LEVEL_INFO);
    log_set_colors(false);
    log_info("This message has no colors");
    log_success("Success without green");
    log_error("Error without red");
    printf("\n");

    /* Test 8: Re-enable colors */
    printf("Test 8: Re-enabling colors\n");
    log_set_colors(true);
    log_info("Colors are back!");
    log_success("Green is back!");
    log_error("Red is back!");
    printf("\n");

    /* Test 9: Custom configuration */
    printf("Test 9: Custom configuration with timestamps\n");
    LogConfig custom_config = {
        .min_level = LOG_LEVEL_DEBUG,
        .use_colors = true,
        .show_timestamp = true,
        .show_level = true,
        .output = stdout
    };
    log_init(&custom_config);
    log_debug("Debug with timestamp");
    log_info("Info with timestamp");
    log_success("Success with timestamp");
    printf("\n");

    /* Test 10: Level string conversion */
    printf("Test 10: Log level string conversion\n");
    log_plain("DEBUG   -> %s\n", log_level_to_string(LOG_LEVEL_DEBUG));
    log_plain("INFO    -> %s\n", log_level_to_string(LOG_LEVEL_INFO));
    log_plain("SUCCESS -> %s\n", log_level_to_string(LOG_LEVEL_SUCCESS));
    log_plain("WARNING -> %s\n", log_level_to_string(LOG_LEVEL_WARNING));
    log_plain("ERROR   -> %s\n", log_level_to_string(LOG_LEVEL_ERROR));
    log_plain("NONE    -> %s\n", log_level_to_string(LOG_LEVEL_NONE));
    printf("\n");

    /* Test 11: Long messages */
    printf("Test 11: Long messages\n");
    log_info("This is a very long message that spans multiple words and "
             "contains lots of information about the current operation being "
             "performed by the system during the build process");
    printf("\n");

    /* Test 12: Messages with format specifiers */
    printf("Test 12: Format specifiers\n");
    int files = 29;
    const char* language = "C";
    float confidence = 85.5f;
    log_info("Found %d %s files", files, language);
    log_success("Analysis confidence: %.1f%%", confidence);
    log_warning("Memory usage: %zu bytes", (size_t)1024 * 1024);
    printf("\n");

    /* Test 13: File logging */
    printf("Test 13: File logging\n");
    const char* log_file_path = "test_output.log";

    /* Remove old log file if exists */
    remove(log_file_path);

    /* Enable file logging */
    if (log_set_file(log_file_path)) {
        log_info("File logging enabled to: %s", log_file_path);
        log_debug("Debug message to file");
        log_info("Info message to file");
        log_success("Success message to file");
        log_warning("Warning message to file");
        log_error("Error message to file");
        log_step(1, 3, "Step message to file");

        /* Check that file was created */
        FILE* check = fopen(log_file_path, "r");
        if (check) {
            log_success("Log file created successfully!");
            fclose(check);

            /* Show file contents */
            printf("\nLog file contents:\n");
            printf("------------------\n");
            check = fopen(log_file_path, "r");
            char line[256];
            while (fgets(line, sizeof(line), check)) {
                printf("%s", line);
            }
            fclose(check);
            printf("------------------\n");
        } else {
            log_error("Failed to create log file!");
        }

        /* Disable file logging */
        log_set_file(NULL);
        log_info("File logging disabled");
    } else {
        log_error("Failed to enable file logging!");
    }
    printf("\n");

    /* Cleanup */
    log_info("All logger tests completed successfully!");
    log_shutdown();

    /* Clean up test log file */
    remove(log_file_path);

    return 0;
}
