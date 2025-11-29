/**
 * @file input.h
 * @brief Cross-platform line editing with history and tab completion
 *
 * Provides readline-like functionality for the REPL:
 * - Arrow key navigation (up/down for history, left/right for cursor)
 * - Tab completion for commands and file paths
 * - Line editing (backspace, delete, home, end)
 * - Cross-platform support (Windows Console API / POSIX termios)
 */

#ifndef CYXMAKE_INPUT_H
#define CYXMAKE_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum line length for input
 */
#define INPUT_MAX_LINE 4096

/**
 * Maximum number of completion candidates
 */
#define INPUT_MAX_COMPLETIONS 64

/**
 * Completion callback function type
 * @param input Current input text
 * @param cursor_pos Cursor position in input
 * @param completions Array to fill with completion strings (caller allocates)
 * @param max_completions Maximum number of completions to return
 * @return Number of completions added
 */
typedef int (*InputCompletionCallback)(
    const char* input,
    size_t cursor_pos,
    char** completions,
    int max_completions
);

/**
 * Input context for managing line editing state
 */
typedef struct InputContext {
    /* History */
    char** history;
    int history_count;
    int history_capacity;
    int history_index;          /* Current position when navigating history */

    /* Current line state */
    char line[INPUT_MAX_LINE];
    size_t line_len;
    size_t cursor_pos;

    /* Saved line (when navigating history) */
    char saved_line[INPUT_MAX_LINE];
    bool has_saved_line;

    /* Completion */
    InputCompletionCallback completion_callback;
    char* completions[INPUT_MAX_COMPLETIONS];
    int completion_count;
    int completion_index;       /* Current completion being shown */
    bool in_completion;         /* Currently cycling through completions */
    size_t completion_start;    /* Start position of text being completed */

    /* Terminal state */
    bool raw_mode;
    bool colors_enabled;

    /* Prompt */
    const char* prompt;
    size_t prompt_len;

#ifdef _WIN32
    void* stdin_handle;         /* HANDLE for Windows console */
    unsigned long orig_mode;    /* Original console mode */
#else
    int orig_termios_saved;
    /* struct termios stored separately to avoid header dependency */
    char orig_termios_data[64]; /* Storage for struct termios */
#endif
} InputContext;

/**
 * Create a new input context
 * @param history_capacity Maximum history size (0 for no limit)
 * @return New context or NULL on error
 */
InputContext* input_context_create(int history_capacity);

/**
 * Free an input context
 * @param ctx Context to free
 */
void input_context_free(InputContext* ctx);

/**
 * Set the completion callback
 * @param ctx Input context
 * @param callback Completion callback function
 */
void input_set_completion_callback(InputContext* ctx, InputCompletionCallback callback);

/**
 * Set whether colors are enabled
 * @param ctx Input context
 * @param enabled True to enable colors
 */
void input_set_colors(InputContext* ctx, bool enabled);

/**
 * Add a line to history
 * @param ctx Input context
 * @param line Line to add (will be copied)
 */
void input_history_add(InputContext* ctx, const char* line);

/**
 * Load history from file
 * @param ctx Input context
 * @param filename File to load from
 * @return Number of lines loaded, -1 on error
 */
int input_history_load(InputContext* ctx, const char* filename);

/**
 * Save history to file
 * @param ctx Input context
 * @param filename File to save to
 * @return 0 on success, -1 on error
 */
int input_history_save(InputContext* ctx, const char* filename);

/**
 * Clear history
 * @param ctx Input context
 */
void input_history_clear(InputContext* ctx);

/**
 * Read a line of input with editing support
 * @param ctx Input context
 * @param prompt Prompt to display
 * @return Pointer to internal buffer with input, or NULL on EOF/error
 *         The returned string is valid until the next call to input_readline
 */
char* input_readline(InputContext* ctx, const char* prompt);

/**
 * Enter raw mode for character-by-character input
 * @param ctx Input context
 * @return true on success
 */
bool input_raw_mode_enable(InputContext* ctx);

/**
 * Exit raw mode, restore terminal settings
 * @param ctx Input context
 */
void input_raw_mode_disable(InputContext* ctx);

/**
 * Clear the current line and redraw
 * @param ctx Input context
 */
void input_refresh_line(InputContext* ctx);

/**
 * Ring the terminal bell
 */
void input_beep(void);

/**
 * Get terminal width
 * @return Terminal width in columns, or 80 if unknown
 */
int input_get_terminal_width(void);

/**
 * Check if stdin is a terminal (TTY)
 * @return true if stdin is interactive, false if piped/redirected
 */
bool input_is_tty(void);

/* ============================================================================
 * Default completion functions
 * ============================================================================ */

/**
 * Slash command completion
 * @param input Current input
 * @param cursor_pos Cursor position
 * @param completions Output array
 * @param max Maximum completions
 * @return Number of completions
 */
int input_complete_slash_commands(
    const char* input,
    size_t cursor_pos,
    char** completions,
    int max
);

/**
 * File path completion
 * @param input Current input
 * @param cursor_pos Cursor position
 * @param completions Output array
 * @param max Maximum completions
 * @return Number of completions
 */
int input_complete_file_paths(
    const char* input,
    size_t cursor_pos,
    char** completions,
    int max
);

/**
 * Combined completion (commands + files)
 * @param input Current input
 * @param cursor_pos Cursor position
 * @param completions Output array
 * @param max Maximum completions
 * @return Number of completions
 */
int input_complete_combined(
    const char* input,
    size_t cursor_pos,
    char** completions,
    int max
);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_INPUT_H */
