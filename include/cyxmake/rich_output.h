/**
 * @file rich_output.h
 * @brief Rich Output System - Progress indicators and explanatory messages
 *
 * This module provides rich terminal output capabilities:
 * - Progress bars with percentage
 * - Spinner animations for long operations
 * - Boxed messages for important info
 * - Step-by-step progress indicators
 * - Color-coded status messages
 */

#ifndef CYXMAKE_RICH_OUTPUT_H
#define CYXMAKE_RICH_OUTPUT_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Color Definitions
 * ============================================================================ */

typedef enum {
    RICH_COLOR_NONE = 0,
    RICH_COLOR_RED,
    RICH_COLOR_GREEN,
    RICH_COLOR_YELLOW,
    RICH_COLOR_BLUE,
    RICH_COLOR_MAGENTA,
    RICH_COLOR_CYAN,
    RICH_COLOR_WHITE,
    RICH_COLOR_GRAY,
} RichColor;

typedef enum {
    RICH_STYLE_NORMAL = 0,
    RICH_STYLE_BOLD = 1,
    RICH_STYLE_DIM = 2,
    RICH_STYLE_ITALIC = 4,
    RICH_STYLE_UNDERLINE = 8,
} RichStyle;

/* ============================================================================
 * Progress Bar
 * ============================================================================ */

typedef struct {
    int current;
    int total;
    int width;              /* Width in characters */
    char* label;            /* Optional label */
    bool show_percentage;
    bool show_count;        /* e.g. "5/10" */
    RichColor bar_color;
    bool colors_enabled;
} ProgressBar;

/**
 * Create a new progress bar
 * @param total Total number of items
 * @param width Bar width in characters
 * @param label Optional label (can be NULL)
 * @return New progress bar
 */
ProgressBar* progress_bar_create(int total, int width, const char* label);

/**
 * Free a progress bar
 */
void progress_bar_free(ProgressBar* bar);

/**
 * Update progress bar
 * @param bar Progress bar
 * @param current Current progress value
 */
void progress_bar_update(ProgressBar* bar, int current);

/**
 * Increment progress bar by 1
 */
void progress_bar_increment(ProgressBar* bar);

/**
 * Render progress bar to stdout
 * @param bar Progress bar
 */
void progress_bar_render(ProgressBar* bar);

/**
 * Complete and close the progress bar
 */
void progress_bar_complete(ProgressBar* bar);

/* ============================================================================
 * Spinner Animation
 * ============================================================================ */

typedef struct {
    const char** frames;    /* Animation frames */
    int frame_count;
    int current_frame;
    char* message;
    bool colors_enabled;
    bool running;
} Spinner;

/**
 * Create a new spinner
 * @param message Message to show next to spinner
 * @return New spinner
 */
Spinner* spinner_create(const char* message);

/**
 * Free a spinner
 */
void spinner_free(Spinner* spinner);

/**
 * Render next frame of spinner
 */
void spinner_tick(Spinner* spinner);

/**
 * Update spinner message
 */
void spinner_set_message(Spinner* spinner, const char* message);

/**
 * Stop spinner with success indicator
 */
void spinner_succeed(Spinner* spinner, const char* message);

/**
 * Stop spinner with failure indicator
 */
void spinner_fail(Spinner* spinner, const char* message);

/* ============================================================================
 * Step Progress
 * ============================================================================ */

typedef struct {
    int current_step;
    int total_steps;
    char** step_labels;
    bool* step_completed;
    bool colors_enabled;
} StepProgress;

/**
 * Create step progress tracker
 * @param total_steps Number of steps
 * @return New step progress
 */
StepProgress* step_progress_create(int total_steps);

/**
 * Free step progress
 */
void step_progress_free(StepProgress* progress);

/**
 * Set label for a step
 */
void step_progress_set_label(StepProgress* progress, int step, const char* label);

/**
 * Start a step (renders "[ ] Step N: ...")
 */
void step_progress_start(StepProgress* progress, int step);

/**
 * Complete current step (renders "[+] Step N: ..." with checkmark)
 */
void step_progress_complete(StepProgress* progress, int step);

/**
 * Fail current step (renders "[X] Step N: ..." with X)
 */
void step_progress_fail(StepProgress* progress, int step, const char* error);

/**
 * Render current step status
 */
void step_progress_render(StepProgress* progress);

/* ============================================================================
 * Message Boxes
 * ============================================================================ */

typedef enum {
    BOX_STYLE_SINGLE,       /* Single line box */
    BOX_STYLE_DOUBLE,       /* Double line box */
    BOX_STYLE_ROUNDED,      /* Rounded corners */
    BOX_STYLE_ASCII,        /* ASCII only (+-|) */
} BoxStyle;

/**
 * Print a message in a box
 * @param message Message to display
 * @param style Box style
 * @param color Box color
 * @param colors_enabled Whether to use colors
 */
void print_box(const char* message, BoxStyle style, RichColor color, bool colors_enabled);

/**
 * Print a multi-line message in a box
 * @param lines Array of strings
 * @param line_count Number of lines
 * @param style Box style
 * @param color Box color
 * @param colors_enabled Whether to use colors
 */
void print_box_lines(const char** lines, int line_count, BoxStyle style, RichColor color, bool colors_enabled);

/* ============================================================================
 * Status Messages
 * ============================================================================ */

/**
 * Print a success message with checkmark
 */
void rich_success(const char* format, ...);

/**
 * Print an error message with X
 */
void rich_error(const char* format, ...);

/**
 * Print a warning message with warning symbol
 */
void rich_warning(const char* format, ...);

/**
 * Print an info message with info symbol
 */
void rich_info(const char* format, ...);

/**
 * Print a thinking/processing message
 */
void rich_thinking(const char* format, ...);

/**
 * Print an action being taken
 */
void rich_action(const char* format, ...);

/**
 * Print a sub-item (indented)
 */
void rich_subitem(const char* format, ...);

/* ============================================================================
 * Explanatory Output
 * ============================================================================ */

/**
 * Print a section header
 */
void rich_section(const char* title);

/**
 * Print a labeled value
 */
void rich_labeled(const char* label, const char* value);

/**
 * Print a command that is being executed
 */
void rich_command(const char* cmd);

/**
 * Print agent explanation/reasoning
 */
void rich_explanation(const char* explanation);

/**
 * Print a suggestion
 */
void rich_suggestion(const char* suggestion);

/**
 * Enable/disable colors globally
 */
void rich_set_colors(bool enabled);

/**
 * Check if colors are enabled
 */
bool rich_colors_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_RICH_OUTPUT_H */
