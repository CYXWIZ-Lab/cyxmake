/**
 * @file slash_commands.h
 * @brief Slash command handlers for REPL
 */

#ifndef CYXMAKE_SLASH_COMMANDS_H
#define CYXMAKE_SLASH_COMMANDS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Include ReplSession definition */
#include "cyxmake/repl.h"

/**
 * Slash command handler function type
 * @param session REPL session
 * @param args Command arguments (after the command name)
 * @return true to continue REPL, false to exit
 */
typedef bool (*SlashCommandHandler)(ReplSession* session, const char* args);

/**
 * Slash command definition
 */
typedef struct {
    const char* name;           /* Command name without slash (e.g., "help") */
    const char* alias;          /* Short alias (e.g., "h") */
    const char* description;    /* Help text */
    SlashCommandHandler handler;
} SlashCommand;

/**
 * Check if input is a slash command
 * @param input User input
 * @return true if starts with '/'
 */
bool is_slash_command(const char* input);

/**
 * Execute a slash command
 * @param session REPL session
 * @param input Full input including the slash
 * @return true to continue REPL, false to exit
 */
bool execute_slash_command(ReplSession* session, const char* input);

/**
 * Get all available slash commands
 * @param out_count Output: number of commands
 * @return Array of slash commands
 */
const SlashCommand* get_slash_commands(int* out_count);

/* Individual command handlers */
bool cmd_help(ReplSession* session, const char* args);
bool cmd_exit(ReplSession* session, const char* args);
bool cmd_clear(ReplSession* session, const char* args);
bool cmd_init(ReplSession* session, const char* args);
bool cmd_build(ReplSession* session, const char* args);
bool cmd_clean(ReplSession* session, const char* args);
bool cmd_status(ReplSession* session, const char* args);
bool cmd_config(ReplSession* session, const char* args);
bool cmd_history(ReplSession* session, const char* args);
bool cmd_version(ReplSession* session, const char* args);
bool cmd_context(ReplSession* session, const char* args);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_SLASH_COMMANDS_H */
