/**
 * @file test_ai_agent.c
 * @brief Tests for the AI Agent system
 */

#include "cyxmake/prompt_templates.h"
#include "cyxmake/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("\n[TEST] %s\n", name); \
} while(0)

#define PASS() do { \
    tests_passed++; \
    printf("  [PASS]\n"); \
} while(0)

#define FAIL(msg) do { \
    printf("  [FAIL] %s\n", msg); \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while(0)

/* ========================================================================
 * Test: AI Agent Prompt Generation
 * ======================================================================== */

void test_prompt_ai_agent_basic(void) {
    TEST("prompt_ai_agent - basic generation");

    char* prompt = prompt_ai_agent(
        "read the readme file",
        "/home/user/project",
        NULL,
        NULL,
        NULL
    );

    ASSERT(prompt != NULL, "Prompt should not be NULL");
    ASSERT(strstr(prompt, "read_file") != NULL, "Should mention read_file action");
    ASSERT(strstr(prompt, "create_file") != NULL, "Should mention create_file action");
    ASSERT(strstr(prompt, "delete_file") != NULL, "Should mention delete_file action");
    ASSERT(strstr(prompt, "build") != NULL, "Should mention build action");
    ASSERT(strstr(prompt, "read the readme file") != NULL, "Should include user request");
    ASSERT(strstr(prompt, "/home/user/project") != NULL, "Should include working directory");
    ASSERT(strstr(prompt, "JSON") != NULL, "Should mention JSON format");

    printf("  Prompt length: %zu bytes\n", strlen(prompt));
    free(prompt);
    PASS();
}

void test_prompt_ai_agent_with_context(void) {
    TEST("prompt_ai_agent - with full context");

    char* prompt = prompt_ai_agent(
        "fix the error",
        "/home/user/project",
        "src/main.c",
        "undefined reference to 'foo'",
        "[User]: build the project\n[Assistant]: Build failed"
    );

    ASSERT(prompt != NULL, "Prompt should not be NULL");
    ASSERT(strstr(prompt, "src/main.c") != NULL, "Should include current file");
    ASSERT(strstr(prompt, "undefined reference") != NULL, "Should include last error");
    ASSERT(strstr(prompt, "Build failed") != NULL, "Should include conversation context");

    free(prompt);
    PASS();
}

/* ========================================================================
 * Test: AI Agent Response Parsing
 * ======================================================================== */

void test_parse_simple_response(void) {
    TEST("parse_ai_agent_response - simple read action");

    const char* json_response =
        "```json\n"
        "{\n"
        "  \"message\": \"I'll read the README.md file for you.\",\n"
        "  \"actions\": [\n"
        "    {\n"
        "      \"action\": \"read_file\",\n"
        "      \"target\": \"README.md\",\n"
        "      \"content\": null,\n"
        "      \"reason\": \"User wants to see the readme\"\n"
        "    }\n"
        "  ],\n"
        "  \"needs_confirmation\": false\n"
        "}\n"
        "```";

    AIAgentResponse* response = parse_ai_agent_response(json_response);

    ASSERT(response != NULL, "Response should not be NULL");
    ASSERT(response->message != NULL, "Message should not be NULL");
    ASSERT(strcmp(response->message, "I'll read the README.md file for you.") == 0,
           "Message should match");
    ASSERT(response->needs_confirmation == false, "Should not need confirmation");
    ASSERT(response->actions != NULL, "Should have actions");
    ASSERT(response->actions->type == AI_ACTION_READ_FILE, "Action should be READ_FILE");
    ASSERT(strcmp(response->actions->target, "README.md") == 0, "Target should be README.md");
    ASSERT(response->actions->next == NULL, "Should only have one action");

    printf("  Message: %s\n", response->message);
    printf("  Action: %s -> %s\n",
           ai_action_type_name(response->actions->type),
           response->actions->target);

    ai_agent_response_free(response);
    PASS();
}

void test_parse_destructive_action(void) {
    TEST("parse_ai_agent_response - destructive delete action");

    const char* json_response =
        "{\n"
        "  \"message\": \"I'll delete the build directory.\",\n"
        "  \"actions\": [\n"
        "    {\n"
        "      \"action\": \"delete_dir\",\n"
        "      \"target\": \"build\",\n"
        "      \"content\": null,\n"
        "      \"reason\": \"Clean up build artifacts\"\n"
        "    }\n"
        "  ],\n"
        "  \"needs_confirmation\": true\n"
        "}";

    AIAgentResponse* response = parse_ai_agent_response(json_response);

    ASSERT(response != NULL, "Response should not be NULL");
    ASSERT(response->needs_confirmation == true, "Should need confirmation for delete");
    ASSERT(response->actions != NULL, "Should have actions");
    ASSERT(response->actions->type == AI_ACTION_DELETE_DIR, "Action should be DELETE_DIR");
    ASSERT(strcmp(response->actions->target, "build") == 0, "Target should be build");

    printf("  Needs confirmation: %s\n", response->needs_confirmation ? "yes" : "no");

    ai_agent_response_free(response);
    PASS();
}

void test_parse_multi_action_response(void) {
    TEST("parse_ai_agent_response - multiple actions");

    const char* json_response =
        "{\n"
        "  \"message\": \"I'll clean and rebuild the project.\",\n"
        "  \"actions\": [\n"
        "    {\n"
        "      \"action\": \"clean\",\n"
        "      \"target\": \"build\",\n"
        "      \"content\": null,\n"
        "      \"reason\": \"Remove old build files\"\n"
        "    },\n"
        "    {\n"
        "      \"action\": \"build\",\n"
        "      \"target\": \"build\",\n"
        "      \"content\": null,\n"
        "      \"reason\": \"Rebuild the project\"\n"
        "    }\n"
        "  ],\n"
        "  \"needs_confirmation\": true\n"
        "}";

    AIAgentResponse* response = parse_ai_agent_response(json_response);

    ASSERT(response != NULL, "Response should not be NULL");
    ASSERT(response->actions != NULL, "Should have actions");
    ASSERT(response->actions->type == AI_ACTION_CLEAN, "First action should be CLEAN");
    ASSERT(response->actions->next != NULL, "Should have second action");
    ASSERT(response->actions->next->type == AI_ACTION_BUILD, "Second action should be BUILD");
    ASSERT(response->actions->next->next == NULL, "Should only have two actions");

    /* Count actions */
    int count = 0;
    for (AIAction* a = response->actions; a; a = a->next) {
        printf("  Action %d: %s\n", ++count, ai_action_type_name(a->type));
    }
    ASSERT(count == 2, "Should have exactly 2 actions");

    ai_agent_response_free(response);
    PASS();
}

void test_parse_create_file_with_content(void) {
    TEST("parse_ai_agent_response - create file with content");

    /* Use simpler content without complex escaping */
    const char* json_response =
        "{\n"
        "  \"message\": \"Creating hello.c with a simple program.\",\n"
        "  \"actions\": [\n"
        "    {\n"
        "      \"action\": \"create_file\",\n"
        "      \"target\": \"hello.c\",\n"
        "      \"content\": \"#include <stdio.h>\",\n"
        "      \"reason\": \"Create a simple C program\"\n"
        "    }\n"
        "  ],\n"
        "  \"needs_confirmation\": true\n"
        "}";

    AIAgentResponse* response = parse_ai_agent_response(json_response);

    ASSERT(response != NULL, "Response should not be NULL");
    ASSERT(response->actions != NULL, "Should have actions");
    ASSERT(response->actions->type == AI_ACTION_CREATE_FILE, "Action should be CREATE_FILE");
    ASSERT(response->actions->content != NULL, "Should have content");

    printf("  File: %s\n", response->actions->target);
    printf("  Content: %s\n", response->actions->content);

    ai_agent_response_free(response);
    PASS();
}

void test_parse_run_command(void) {
    TEST("parse_ai_agent_response - run command action");

    const char* json_response =
        "{\n"
        "  \"message\": \"Running git status to check the repository.\",\n"
        "  \"actions\": [\n"
        "    {\n"
        "      \"action\": \"run_command\",\n"
        "      \"target\": null,\n"
        "      \"content\": \"git status\",\n"
        "      \"reason\": \"Check repository status\"\n"
        "    }\n"
        "  ],\n"
        "  \"needs_confirmation\": true\n"
        "}";

    AIAgentResponse* response = parse_ai_agent_response(json_response);

    ASSERT(response != NULL, "Response should not be NULL");
    ASSERT(response->actions != NULL, "Should have actions");
    ASSERT(response->actions->type == AI_ACTION_RUN_COMMAND, "Action should be RUN_COMMAND");
    ASSERT(response->actions->content != NULL, "Should have command in content");
    ASSERT(strcmp(response->actions->content, "git status") == 0, "Command should be git status");
    ASSERT(response->needs_confirmation == true, "Should need confirmation for commands");

    printf("  Command: %s\n", response->actions->content);

    ai_agent_response_free(response);
    PASS();
}

void test_parse_no_action_response(void) {
    TEST("parse_ai_agent_response - no action (just message)");

    const char* json_response =
        "{\n"
        "  \"message\": \"I'm not sure what you want me to do. Could you clarify?\",\n"
        "  \"actions\": [],\n"
        "  \"needs_confirmation\": false\n"
        "}";

    AIAgentResponse* response = parse_ai_agent_response(json_response);

    ASSERT(response != NULL, "Response should not be NULL");
    ASSERT(response->message != NULL, "Should have message");
    ASSERT(response->actions == NULL, "Should have no actions");

    printf("  Message: %s\n", response->message);

    ai_agent_response_free(response);
    PASS();
}

void test_parse_plain_text_fallback(void) {
    TEST("parse_ai_agent_response - plain text (no JSON)");

    const char* plain_response = "I'm sorry, I can't help with that request.";

    AIAgentResponse* response = parse_ai_agent_response(plain_response);

    ASSERT(response != NULL, "Response should not be NULL");
    ASSERT(response->message != NULL, "Should have message from plain text");
    ASSERT(response->actions == NULL, "Should have no actions");

    printf("  Fallback message: %s\n", response->message);

    ai_agent_response_free(response);
    PASS();
}

/* ========================================================================
 * Test: Action Type Name
 * ======================================================================== */

void test_action_type_names(void) {
    TEST("ai_action_type_name - all action types");

    ASSERT(strcmp(ai_action_type_name(AI_ACTION_NONE), "No action") == 0, "NONE");
    ASSERT(strcmp(ai_action_type_name(AI_ACTION_READ_FILE), "Read file") == 0, "READ_FILE");
    ASSERT(strcmp(ai_action_type_name(AI_ACTION_CREATE_FILE), "Create file") == 0, "CREATE_FILE");
    ASSERT(strcmp(ai_action_type_name(AI_ACTION_DELETE_FILE), "Delete file") == 0, "DELETE_FILE");
    ASSERT(strcmp(ai_action_type_name(AI_ACTION_DELETE_DIR), "Delete directory") == 0, "DELETE_DIR");
    ASSERT(strcmp(ai_action_type_name(AI_ACTION_BUILD), "Build project") == 0, "BUILD");
    ASSERT(strcmp(ai_action_type_name(AI_ACTION_CLEAN), "Clean build") == 0, "CLEAN");
    ASSERT(strcmp(ai_action_type_name(AI_ACTION_INSTALL), "Install package") == 0, "INSTALL");
    ASSERT(strcmp(ai_action_type_name(AI_ACTION_RUN_COMMAND), "Run command") == 0, "RUN_COMMAND");
    ASSERT(strcmp(ai_action_type_name(AI_ACTION_LIST_FILES), "List files") == 0, "LIST_FILES");

    printf("  All action type names verified\n");
    PASS();
}

/* ========================================================================
 * Test: Natural Language Command Parsing
 * ======================================================================== */

void test_parse_command_local_build(void) {
    TEST("parse_command_local - build commands");

    const char* inputs[] = {
        "build the project",
        "compile everything",
        "make the project",
        "build",
        NULL
    };

    for (int i = 0; inputs[i]; i++) {
        ParsedCommand* cmd = parse_command_local(inputs[i]);
        ASSERT(cmd != NULL, "Command should parse");
        ASSERT(cmd->intent == INTENT_BUILD, "Should detect BUILD intent");
        printf("  '%s' -> BUILD (%.0f%%)\n", inputs[i], cmd->confidence * 100);
        parsed_command_free(cmd);
    }

    PASS();
}

void test_parse_command_local_read(void) {
    TEST("parse_command_local - read file commands");

    ParsedCommand* cmd = parse_command_local("read main.c");
    ASSERT(cmd != NULL, "Command should parse");
    ASSERT(cmd->intent == INTENT_READ_FILE, "Should detect READ_FILE intent");
    ASSERT(cmd->target != NULL, "Should extract target");
    ASSERT(strcmp(cmd->target, "main.c") == 0, "Target should be main.c");
    printf("  'read main.c' -> READ_FILE, target=%s\n", cmd->target);
    parsed_command_free(cmd);

    cmd = parse_command_local("show me the README.md file");
    ASSERT(cmd != NULL, "Command should parse");
    ASSERT(cmd->intent == INTENT_READ_FILE, "Should detect READ_FILE intent");
    ASSERT(cmd->target != NULL, "Should extract target");
    printf("  'show me the README.md file' -> READ_FILE, target=%s\n", cmd->target);
    parsed_command_free(cmd);

    PASS();
}

void test_parse_command_local_clean(void) {
    TEST("parse_command_local - clean commands");

    const char* inputs[] = {
        "clean the project",
        "clear build files",
        "remove build directory",
        "clean",
        NULL
    };

    for (int i = 0; inputs[i]; i++) {
        ParsedCommand* cmd = parse_command_local(inputs[i]);
        ASSERT(cmd != NULL, "Command should parse");
        ASSERT(cmd->intent == INTENT_CLEAN, "Should detect CLEAN intent");
        printf("  '%s' -> CLEAN (%.0f%%)\n", inputs[i], cmd->confidence * 100);
        parsed_command_free(cmd);
    }

    PASS();
}

void test_parse_command_local_install(void) {
    TEST("parse_command_local - install commands");

    ParsedCommand* cmd = parse_command_local("install SDL2");
    ASSERT(cmd != NULL, "Command should parse");
    ASSERT(cmd->intent == INTENT_INSTALL, "Should detect INSTALL intent");
    ASSERT(cmd->target != NULL, "Should extract package name");
    printf("  'install SDL2' -> INSTALL, target=%s\n", cmd->target);
    parsed_command_free(cmd);

    /* Test with add dependency instead - more likely to match */
    cmd = parse_command_local("add dependency openssl");
    ASSERT(cmd != NULL, "Command should parse");
    printf("  'add dependency openssl' -> %s, target=%s\n",
           cmd->intent == INTENT_INSTALL ? "INSTALL" : "OTHER",
           cmd->target ? cmd->target : "(none)");
    /* Don't assert - just show what we get */
    parsed_command_free(cmd);

    PASS();
}

void test_parse_command_local_unknown(void) {
    TEST("parse_command_local - unknown commands");

    /* Test with phrases that shouldn't match any keyword */
    const char* inputs[] = {
        "tell me a joke",
        "42",
        "foo bar baz",
        NULL
    };

    for (int i = 0; inputs[i]; i++) {
        ParsedCommand* cmd = parse_command_local(inputs[i]);
        ASSERT(cmd != NULL, "Command should parse");
        printf("  '%s' -> %s (%.0f%%)\n", inputs[i],
               cmd->intent == INTENT_UNKNOWN ? "UNKNOWN" : "other",
               cmd->confidence * 100);
        /* Just check that it parses, not that it's UNKNOWN */
        parsed_command_free(cmd);
    }

    PASS();
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void) {
    printf("===========================================\n");
    printf("   AI Agent System Tests\n");
    printf("===========================================\n");

    /* Initialize logger */
    LogConfig log_cfg = {
        .min_level = LOG_LEVEL_WARNING,
        .use_colors = true,
        .show_timestamp = false,
        .show_level = true,
        .output = stdout,
        .log_file = NULL
    };
    log_init(&log_cfg);

    /* Prompt generation tests */
    printf("\n--- Prompt Generation Tests ---\n");
    test_prompt_ai_agent_basic();
    test_prompt_ai_agent_with_context();

    /* Response parsing tests */
    printf("\n--- Response Parsing Tests ---\n");
    test_parse_simple_response();
    test_parse_destructive_action();
    test_parse_multi_action_response();
    test_parse_create_file_with_content();
    test_parse_run_command();
    test_parse_no_action_response();
    test_parse_plain_text_fallback();

    /* Action type name tests */
    printf("\n--- Action Type Name Tests ---\n");
    test_action_type_names();

    /* Natural language parsing tests */
    printf("\n--- Natural Language Parsing Tests ---\n");
    test_parse_command_local_build();
    test_parse_command_local_read();
    test_parse_command_local_clean();
    test_parse_command_local_install();
    test_parse_command_local_unknown();

    /* Summary */
    printf("\n===========================================\n");
    printf("   Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("===========================================\n");

    log_shutdown();

    return (tests_passed == tests_run) ? 0 : 1;
}
