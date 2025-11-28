/**
 * @file rich_output.c
 * @brief Rich Output System implementation
 */

#include "cyxmake/rich_output.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Global color state */
static bool g_colors_enabled = true;

/* ANSI color codes */
#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_DIM     "\033[2m"
#define ANSI_ITALIC  "\033[3m"
#define ANSI_ULINE   "\033[4m"

static const char* color_codes[] = {
    "",             /* RICH_COLOR_NONE */
    "\033[31m",     /* RICH_COLOR_RED */
    "\033[32m",     /* RICH_COLOR_GREEN */
    "\033[33m",     /* RICH_COLOR_YELLOW */
    "\033[34m",     /* RICH_COLOR_BLUE */
    "\033[35m",     /* RICH_COLOR_MAGENTA */
    "\033[36m",     /* RICH_COLOR_CYAN */
    "\033[37m",     /* RICH_COLOR_WHITE */
    "\033[90m",     /* RICH_COLOR_GRAY */
};

/* ASCII symbols for cross-platform */
#define SYM_CHECK    "[OK]"
#define SYM_CROSS    "[X]"
#define SYM_WARN     "[!]"
#define SYM_INFO     "[i]"
#define SYM_THINK    "[..]"
#define SYM_ACTION   "[>]"
#define SYM_BULLET   "*"
#define SYM_ARROW    "->"

/* Spinner frames (ASCII) */
static const char* spinner_frames[] = {
    "[|]", "[/]", "[-]", "[\\]"
};
static const int spinner_frame_count = 4;

/* Box drawing characters */
static const char* box_chars_ascii[] = {
    "+", "+", "+", "+", "-", "|"  /* TL, TR, BL, BR, H, V */
};

/* ============================================================================
 * Color Management
 * ============================================================================ */

void rich_set_colors(bool enabled) {
    g_colors_enabled = enabled;
}

bool rich_colors_enabled(void) {
    return g_colors_enabled;
}

static void print_color(RichColor color) {
    if (g_colors_enabled && color > RICH_COLOR_NONE && color <= RICH_COLOR_GRAY) {
        printf("%s", color_codes[color]);
    }
}

static void print_reset(void) {
    if (g_colors_enabled) {
        printf("%s", ANSI_RESET);
    }
}

/* ============================================================================
 * Progress Bar
 * ============================================================================ */

ProgressBar* progress_bar_create(int total, int width, const char* label) {
    ProgressBar* bar = calloc(1, sizeof(ProgressBar));
    if (!bar) return NULL;

    bar->total = total > 0 ? total : 1;
    bar->width = width > 0 ? width : 40;
    bar->current = 0;
    bar->show_percentage = true;
    bar->show_count = true;
    bar->bar_color = RICH_COLOR_GREEN;
    bar->colors_enabled = g_colors_enabled;

    if (label) {
        bar->label = strdup(label);
    }

    return bar;
}

void progress_bar_free(ProgressBar* bar) {
    if (!bar) return;
    free(bar->label);
    free(bar);
}

void progress_bar_update(ProgressBar* bar, int current) {
    if (!bar) return;
    bar->current = current;
    if (bar->current > bar->total) bar->current = bar->total;
    if (bar->current < 0) bar->current = 0;
}

void progress_bar_increment(ProgressBar* bar) {
    if (!bar) return;
    progress_bar_update(bar, bar->current + 1);
}

void progress_bar_render(ProgressBar* bar) {
    if (!bar) return;

    /* Calculate fill */
    int filled = (bar->current * bar->width) / bar->total;
    int empty = bar->width - filled;

    /* Print label if present */
    if (bar->label) {
        printf("%s ", bar->label);
    }

    /* Print bar */
    printf("[");
    if (bar->colors_enabled) {
        print_color(bar->bar_color);
    }
    for (int i = 0; i < filled; i++) printf("=");
    if (bar->colors_enabled) {
        print_reset();
    }
    for (int i = 0; i < empty; i++) printf(" ");
    printf("]");

    /* Print percentage/count */
    if (bar->show_percentage) {
        int percent = (bar->current * 100) / bar->total;
        printf(" %3d%%", percent);
    }
    if (bar->show_count) {
        printf(" (%d/%d)", bar->current, bar->total);
    }

    /* Carriage return for in-place update */
    printf("\r");
    fflush(stdout);
}

void progress_bar_complete(ProgressBar* bar) {
    if (!bar) return;
    bar->current = bar->total;
    progress_bar_render(bar);
    printf("\n");
}

/* ============================================================================
 * Spinner
 * ============================================================================ */

Spinner* spinner_create(const char* message) {
    Spinner* spinner = calloc(1, sizeof(Spinner));
    if (!spinner) return NULL;

    spinner->frames = spinner_frames;
    spinner->frame_count = spinner_frame_count;
    spinner->current_frame = 0;
    spinner->colors_enabled = g_colors_enabled;
    spinner->running = true;

    if (message) {
        spinner->message = strdup(message);
    }

    return spinner;
}

void spinner_free(Spinner* spinner) {
    if (!spinner) return;
    free(spinner->message);
    free(spinner);
}

void spinner_tick(Spinner* spinner) {
    if (!spinner || !spinner->running) return;

    /* Clear line and print spinner */
    printf("\r");
    if (spinner->colors_enabled) {
        print_color(RICH_COLOR_CYAN);
    }
    printf("%s", spinner->frames[spinner->current_frame]);
    if (spinner->colors_enabled) {
        print_reset();
    }

    if (spinner->message) {
        printf(" %s", spinner->message);
    }

    fflush(stdout);

    /* Advance frame */
    spinner->current_frame = (spinner->current_frame + 1) % spinner->frame_count;
}

void spinner_set_message(Spinner* spinner, const char* message) {
    if (!spinner) return;
    free(spinner->message);
    spinner->message = message ? strdup(message) : NULL;
}

void spinner_succeed(Spinner* spinner, const char* message) {
    if (!spinner) return;
    spinner->running = false;

    printf("\r");
    if (spinner->colors_enabled) {
        print_color(RICH_COLOR_GREEN);
    }
    printf("%s", SYM_CHECK);
    if (spinner->colors_enabled) {
        print_reset();
    }
    printf(" %s\n", message ? message : (spinner->message ? spinner->message : "Done"));
}

void spinner_fail(Spinner* spinner, const char* message) {
    if (!spinner) return;
    spinner->running = false;

    printf("\r");
    if (spinner->colors_enabled) {
        print_color(RICH_COLOR_RED);
    }
    printf("%s", SYM_CROSS);
    if (spinner->colors_enabled) {
        print_reset();
    }
    printf(" %s\n", message ? message : (spinner->message ? spinner->message : "Failed"));
}

/* ============================================================================
 * Step Progress
 * ============================================================================ */

StepProgress* step_progress_create(int total_steps) {
    StepProgress* progress = calloc(1, sizeof(StepProgress));
    if (!progress) return NULL;

    progress->total_steps = total_steps;
    progress->current_step = 0;
    progress->colors_enabled = g_colors_enabled;
    progress->step_labels = calloc(total_steps, sizeof(char*));
    progress->step_completed = calloc(total_steps, sizeof(bool));

    return progress;
}

void step_progress_free(StepProgress* progress) {
    if (!progress) return;
    for (int i = 0; i < progress->total_steps; i++) {
        free(progress->step_labels[i]);
    }
    free(progress->step_labels);
    free(progress->step_completed);
    free(progress);
}

void step_progress_set_label(StepProgress* progress, int step, const char* label) {
    if (!progress || step < 0 || step >= progress->total_steps) return;
    free(progress->step_labels[step]);
    progress->step_labels[step] = label ? strdup(label) : NULL;
}

void step_progress_start(StepProgress* progress, int step) {
    if (!progress || step < 0 || step >= progress->total_steps) return;

    progress->current_step = step;

    if (progress->colors_enabled) {
        print_color(RICH_COLOR_GRAY);
    }
    printf("[ ] ");
    if (progress->colors_enabled) {
        print_reset();
    }
    printf("[%d/%d] %s...\n", step + 1, progress->total_steps,
           progress->step_labels[step] ? progress->step_labels[step] : "Processing");
}

void step_progress_complete(StepProgress* progress, int step) {
    if (!progress || step < 0 || step >= progress->total_steps) return;

    progress->step_completed[step] = true;

    /* Move up and overwrite */
    printf("\033[1A\r");  /* Move up one line */
    if (progress->colors_enabled) {
        print_color(RICH_COLOR_GREEN);
    }
    printf("%s ", SYM_CHECK);
    if (progress->colors_enabled) {
        print_reset();
    }
    printf("[%d/%d] %s\n", step + 1, progress->total_steps,
           progress->step_labels[step] ? progress->step_labels[step] : "Complete");
}

void step_progress_fail(StepProgress* progress, int step, const char* error) {
    if (!progress || step < 0 || step >= progress->total_steps) return;

    /* Move up and overwrite */
    printf("\033[1A\r");  /* Move up one line */
    if (progress->colors_enabled) {
        print_color(RICH_COLOR_RED);
    }
    printf("%s ", SYM_CROSS);
    if (progress->colors_enabled) {
        print_reset();
    }
    printf("[%d/%d] %s", step + 1, progress->total_steps,
           progress->step_labels[step] ? progress->step_labels[step] : "Failed");
    if (error) {
        printf(": %s", error);
    }
    printf("\n");
}

void step_progress_render(StepProgress* progress) {
    if (!progress) return;

    for (int i = 0; i < progress->total_steps; i++) {
        if (progress->step_completed[i]) {
            if (progress->colors_enabled) {
                print_color(RICH_COLOR_GREEN);
            }
            printf("%s ", SYM_CHECK);
        } else if (i == progress->current_step) {
            if (progress->colors_enabled) {
                print_color(RICH_COLOR_YELLOW);
            }
            printf("%s ", SYM_THINK);
        } else {
            if (progress->colors_enabled) {
                print_color(RICH_COLOR_GRAY);
            }
            printf("[ ] ");
        }
        if (progress->colors_enabled) {
            print_reset();
        }
        printf("[%d/%d] %s\n", i + 1, progress->total_steps,
               progress->step_labels[i] ? progress->step_labels[i] : "");
    }
}

/* ============================================================================
 * Message Boxes
 * ============================================================================ */

static int max_line_length(const char** lines, int line_count) {
    int max = 0;
    for (int i = 0; i < line_count; i++) {
        int len = (int)strlen(lines[i]);
        if (len > max) max = len;
    }
    return max;
}

void print_box(const char* message, BoxStyle style, RichColor color, bool colors_enabled) {
    const char* lines[1] = { message };
    print_box_lines(lines, 1, style, color, colors_enabled);
}

void print_box_lines(const char** lines, int line_count, BoxStyle style, RichColor color, bool colors_enabled) {
    (void)style;  /* Currently only ASCII style */

    int width = max_line_length(lines, line_count);
    if (width < 20) width = 20;

    /* Top border */
    if (colors_enabled) {
        printf("%s", color_codes[color]);
    }
    printf("+");
    for (int i = 0; i < width + 2; i++) printf("-");
    printf("+\n");

    /* Content */
    for (int i = 0; i < line_count; i++) {
        printf("| ");
        if (colors_enabled) {
            printf("%s", ANSI_RESET);
        }
        printf("%-*s", width, lines[i]);
        if (colors_enabled) {
            printf("%s", color_codes[color]);
        }
        printf(" |\n");
    }

    /* Bottom border */
    printf("+");
    for (int i = 0; i < width + 2; i++) printf("-");
    printf("+");
    if (colors_enabled) {
        printf("%s", ANSI_RESET);
    }
    printf("\n");
}

/* ============================================================================
 * Status Messages
 * ============================================================================ */

void rich_success(const char* format, ...) {
    if (g_colors_enabled) {
        print_color(RICH_COLOR_GREEN);
    }
    printf("%s ", SYM_CHECK);
    if (g_colors_enabled) {
        print_reset();
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void rich_error(const char* format, ...) {
    if (g_colors_enabled) {
        print_color(RICH_COLOR_RED);
    }
    printf("%s ", SYM_CROSS);
    if (g_colors_enabled) {
        print_reset();
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void rich_warning(const char* format, ...) {
    if (g_colors_enabled) {
        print_color(RICH_COLOR_YELLOW);
    }
    printf("%s ", SYM_WARN);
    if (g_colors_enabled) {
        print_reset();
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void rich_info(const char* format, ...) {
    if (g_colors_enabled) {
        print_color(RICH_COLOR_CYAN);
    }
    printf("%s ", SYM_INFO);
    if (g_colors_enabled) {
        print_reset();
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void rich_thinking(const char* format, ...) {
    if (g_colors_enabled) {
        print_color(RICH_COLOR_MAGENTA);
    }
    printf("%s ", SYM_THINK);
    if (g_colors_enabled) {
        print_reset();
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void rich_action(const char* format, ...) {
    if (g_colors_enabled) {
        print_color(RICH_COLOR_BLUE);
    }
    printf("%s ", SYM_ACTION);
    if (g_colors_enabled) {
        print_reset();
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void rich_subitem(const char* format, ...) {
    printf("   %s ", SYM_BULLET);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

/* ============================================================================
 * Explanatory Output
 * ============================================================================ */

void rich_section(const char* title) {
    printf("\n");
    if (g_colors_enabled) {
        printf("%s%s", ANSI_BOLD, color_codes[RICH_COLOR_CYAN]);
    }
    printf("=== %s ===", title);
    if (g_colors_enabled) {
        print_reset();
    }
    printf("\n\n");
}

void rich_labeled(const char* label, const char* value) {
    if (g_colors_enabled) {
        printf("%s", color_codes[RICH_COLOR_GRAY]);
    }
    printf("%s: ", label);
    if (g_colors_enabled) {
        print_reset();
    }
    printf("%s\n", value);
}

void rich_command(const char* cmd) {
    if (g_colors_enabled) {
        printf("%s", color_codes[RICH_COLOR_GRAY]);
    }
    printf("$ ");
    if (g_colors_enabled) {
        print_reset();
        printf("%s%s", ANSI_BOLD, color_codes[RICH_COLOR_WHITE]);
    }
    printf("%s", cmd);
    if (g_colors_enabled) {
        print_reset();
    }
    printf("\n");
}

void rich_explanation(const char* explanation) {
    if (g_colors_enabled) {
        printf("%s", color_codes[RICH_COLOR_GRAY]);
    }
    printf("   %s ", SYM_INFO);
    if (g_colors_enabled) {
        print_reset();
        printf("%s", ANSI_DIM);
    }
    printf("%s", explanation);
    if (g_colors_enabled) {
        print_reset();
    }
    printf("\n");
}

void rich_suggestion(const char* suggestion) {
    if (g_colors_enabled) {
        print_color(RICH_COLOR_YELLOW);
    }
    printf("   %s Suggestion: ", SYM_ARROW);
    if (g_colors_enabled) {
        print_reset();
    }
    printf("%s\n", suggestion);
}
