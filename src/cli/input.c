/**
 * @file input.c
 * @brief Cross-platform line editing implementation
 *
 * Provides readline-like functionality:
 * - Windows: Uses Console API for raw input
 * - POSIX: Uses termios for raw input
 */

#include "cyxmake/input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <io.h>
#include <direct.h>
#define F_OK 0
#define access _access
#else
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <dirent.h>
#endif

/* ANSI escape codes */
#define ESC "\033"
#define CSI ESC "["

/* Key codes */
#ifdef _WIN32
/* Windows extended key scan codes (from _getch after 0xE0 prefix) */
#define KEY_SPECIAL_PREFIX 0xE0
#define KEY_SPECIAL_PREFIX2 0x00
#define SCAN_UP_ARROW    72
#define SCAN_DOWN_ARROW  80
#define SCAN_LEFT_ARROW  75
#define SCAN_RIGHT_ARROW 77
#define SCAN_HOME        71
#define SCAN_END         79
#define SCAN_DELETE      83
#define KEY_BACKSPACE    8
#define KEY_TAB          9
#define KEY_ENTER        13
#define KEY_ESCAPE       27
#else
/* ANSI escape sequences (after ESC [) */
#define ANSI_UP    'A'
#define ANSI_DOWN  'B'
#define ANSI_RIGHT 'C'
#define ANSI_LEFT  'D'
#define ANSI_HOME  'H'
#define ANSI_END   'F'
#endif

/* Forward declarations */
static void clear_completions(InputContext* ctx);
static void handle_tab(InputContext* ctx);
static void handle_backspace(InputContext* ctx);
static void handle_delete(InputContext* ctx);
static void handle_left(InputContext* ctx);
static void handle_right(InputContext* ctx);
static void handle_home(InputContext* ctx);
static void handle_end(InputContext* ctx);
static void handle_up(InputContext* ctx);
static void handle_down(InputContext* ctx);
static void insert_char(InputContext* ctx, char c);

/* ============================================================================
 * Context management
 * ============================================================================ */

InputContext* input_context_create(int history_capacity) {
    InputContext* ctx = calloc(1, sizeof(InputContext));
    if (!ctx) return NULL;

    ctx->history_capacity = history_capacity > 0 ? history_capacity : 1000;
    ctx->history = calloc(ctx->history_capacity, sizeof(char*));
    if (!ctx->history) {
        free(ctx);
        return NULL;
    }

    ctx->colors_enabled = true;
    ctx->history_index = -1;

#ifdef _WIN32
    ctx->stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
#endif

    return ctx;
}

void input_context_free(InputContext* ctx) {
    if (!ctx) return;

    /* Restore terminal mode if needed */
    if (ctx->raw_mode) {
        input_raw_mode_disable(ctx);
    }

    /* Free history */
    input_history_clear(ctx);
    free(ctx->history);

    /* Free completions */
    clear_completions(ctx);

    free(ctx);
}

void input_set_completion_callback(InputContext* ctx, InputCompletionCallback callback) {
    if (ctx) {
        ctx->completion_callback = callback;
    }
}

void input_set_colors(InputContext* ctx, bool enabled) {
    if (ctx) {
        ctx->colors_enabled = enabled;
    }
}

/* ============================================================================
 * History management
 * ============================================================================ */

void input_history_add(InputContext* ctx, const char* line) {
    if (!ctx || !line || strlen(line) == 0) return;

    /* Don't add duplicates of last entry */
    if (ctx->history_count > 0 &&
        strcmp(ctx->history[ctx->history_count - 1], line) == 0) {
        return;
    }

    /* If at capacity, remove oldest */
    if (ctx->history_count >= ctx->history_capacity) {
        free(ctx->history[0]);
        memmove(ctx->history, ctx->history + 1,
                (ctx->history_capacity - 1) * sizeof(char*));
        ctx->history_count--;
    }

    ctx->history[ctx->history_count++] = strdup(line);
}

int input_history_load(InputContext* ctx, const char* filename) {
    if (!ctx || !filename) return -1;

    FILE* f = fopen(filename, "r");
    if (!f) return -1;

    char line[INPUT_MAX_LINE];
    int count = 0;

    while (fgets(line, sizeof(line), f)) {
        /* Remove trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        if (len > 0) {
            input_history_add(ctx, line);
            count++;
        }
    }

    fclose(f);
    return count;
}

int input_history_save(InputContext* ctx, const char* filename) {
    if (!ctx || !filename) return -1;

    FILE* f = fopen(filename, "w");
    if (!f) return -1;

    for (int i = 0; i < ctx->history_count; i++) {
        fprintf(f, "%s\n", ctx->history[i]);
    }

    fclose(f);
    return 0;
}

void input_history_clear(InputContext* ctx) {
    if (!ctx) return;

    for (int i = 0; i < ctx->history_count; i++) {
        free(ctx->history[i]);
        ctx->history[i] = NULL;
    }
    ctx->history_count = 0;
    ctx->history_index = -1;
}

/* ============================================================================
 * Terminal control
 * ============================================================================ */

int input_get_terminal_width(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80;
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && ws.ws_col != 0) {
        return ws.ws_col;
    }
    return 80;
#endif
}

void input_beep(void) {
#ifdef _WIN32
    /* Simple beep - just print bell character */
    putchar('\a');
    fflush(stdout);
#else
    write(STDOUT_FILENO, "\a", 1);
#endif
}

bool input_is_tty(void) {
#ifdef _WIN32
    /* On Windows, check if stdin is a console (not a pipe or file) */
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD mode;
    /* GetConsoleMode fails if handle is not a console */
    return GetConsoleMode(h, &mode) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

bool input_raw_mode_enable(InputContext* ctx) {
    if (!ctx || ctx->raw_mode) return true;

#ifdef _WIN32
    HANDLE h = ctx->stdin_handle;
    if (h == INVALID_HANDLE_VALUE) return false;

    /* Save original mode */
    if (!GetConsoleMode(h, &ctx->orig_mode)) return false;

    /* Set raw mode: disable line input, echo */
    DWORD mode = ctx->orig_mode;
    mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;

    if (!SetConsoleMode(h, mode)) {
        /* Try without virtual terminal input (older Windows) */
        mode &= ~ENABLE_VIRTUAL_TERMINAL_INPUT;
        if (!SetConsoleMode(h, mode)) return false;
    }

    /* Enable ANSI output */
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD out_mode;
    if (GetConsoleMode(out, &out_mode)) {
        SetConsoleMode(out, out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    ctx->raw_mode = true;
    return true;
#else
    struct termios raw;
    struct termios* orig = (struct termios*)ctx->orig_termios_data;

    if (tcgetattr(STDIN_FILENO, orig) == -1) return false;
    ctx->orig_termios_saved = 1;

    raw = *orig;
    /* Input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* Output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* Control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* Local modes - echoing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* Control chars - set return condition: min number of bytes and timer. */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) return false;

    ctx->raw_mode = true;
    return true;
#endif
}

void input_raw_mode_disable(InputContext* ctx) {
    if (!ctx || !ctx->raw_mode) return;

#ifdef _WIN32
    SetConsoleMode(ctx->stdin_handle, ctx->orig_mode);
#else
    if (ctx->orig_termios_saved) {
        struct termios* orig = (struct termios*)ctx->orig_termios_data;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
    }
#endif

    ctx->raw_mode = false;
}

/* ============================================================================
 * Line display
 * ============================================================================ */

/**
 * Calculate visible length of string (excluding ANSI escape codes)
 */
static size_t visible_strlen(const char* str) {
    if (!str) return 0;

    size_t len = 0;
    bool in_escape = false;

    for (const char* p = str; *p; p++) {
        if (*p == '\033') {
            in_escape = true;
        } else if (in_escape) {
            /* End of escape sequence at letter */
            if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {
                in_escape = false;
            }
        } else {
            len++;
        }
    }

    return len;
}

void input_refresh_line(InputContext* ctx) {
    if (!ctx) return;

    /* Move cursor to start of line */
    printf("\r");

    /* Print prompt */
    if (ctx->prompt) {
        printf("%s", ctx->prompt);
    }

    /* Print current line */
    printf("%s", ctx->line);

    /* Erase to end of line */
    printf(CSI "K");

    /* Move cursor to correct position */
    size_t cursor_col = ctx->prompt_len + ctx->cursor_pos;
    printf("\r" CSI "%zuC", cursor_col);

    fflush(stdout);
}

/* ============================================================================
 * Completion helpers
 * ============================================================================ */

static void clear_completions(InputContext* ctx) {
    for (int i = 0; i < ctx->completion_count; i++) {
        free(ctx->completions[i]);
        ctx->completions[i] = NULL;
    }
    ctx->completion_count = 0;
    ctx->completion_index = 0;
    ctx->in_completion = false;
}

static void handle_tab(InputContext* ctx) {
    if (!ctx->completion_callback) {
        input_beep();
        return;
    }

    if (!ctx->in_completion) {
        /* Start new completion */
        clear_completions(ctx);

        ctx->completion_count = ctx->completion_callback(
            ctx->line,
            ctx->cursor_pos,
            ctx->completions,
            INPUT_MAX_COMPLETIONS
        );

        if (ctx->completion_count == 0) {
            input_beep();
            return;
        }

        /* Find start of word being completed */
        ctx->completion_start = ctx->cursor_pos;
        while (ctx->completion_start > 0 &&
               ctx->line[ctx->completion_start - 1] != ' ' &&
               ctx->line[ctx->completion_start - 1] != '/') {
            ctx->completion_start--;
        }

        ctx->in_completion = true;
        ctx->completion_index = 0;
    } else {
        /* Cycle to next completion */
        ctx->completion_index = (ctx->completion_index + 1) % ctx->completion_count;
    }

    /* Apply current completion */
    const char* completion = ctx->completions[ctx->completion_index];
    size_t comp_len = strlen(completion);

    /* Remove old text from completion_start to cursor */
    size_t remove_len = ctx->cursor_pos - ctx->completion_start;
    memmove(ctx->line + ctx->completion_start,
            ctx->line + ctx->cursor_pos,
            ctx->line_len - ctx->cursor_pos + 1);
    ctx->line_len -= remove_len;
    ctx->cursor_pos = ctx->completion_start;

    /* Insert completion */
    if (ctx->line_len + comp_len < INPUT_MAX_LINE - 1) {
        memmove(ctx->line + ctx->cursor_pos + comp_len,
                ctx->line + ctx->cursor_pos,
                ctx->line_len - ctx->cursor_pos + 1);
        memcpy(ctx->line + ctx->cursor_pos, completion, comp_len);
        ctx->line_len += comp_len;
        ctx->cursor_pos += comp_len;
    }

    input_refresh_line(ctx);

    /* If only one completion, add space and end completion mode */
    if (ctx->completion_count == 1) {
        clear_completions(ctx);
        /* Add trailing space if not a path ending in / */
        if (comp_len > 0 && completion[comp_len - 1] != '/' &&
            completion[comp_len - 1] != '\\') {
            insert_char(ctx, ' ');
        }
    }
}

/* ============================================================================
 * Line editing
 * ============================================================================ */

static void insert_char(InputContext* ctx, char c) {
    if (ctx->line_len >= INPUT_MAX_LINE - 1) {
        input_beep();
        return;
    }

    /* Clear completion state on any character input */
    if (ctx->in_completion && c != '\t') {
        clear_completions(ctx);
    }

    /* Insert at cursor position */
    memmove(ctx->line + ctx->cursor_pos + 1,
            ctx->line + ctx->cursor_pos,
            ctx->line_len - ctx->cursor_pos + 1);
    ctx->line[ctx->cursor_pos] = c;
    ctx->line_len++;
    ctx->cursor_pos++;

    input_refresh_line(ctx);
}

static void handle_backspace(InputContext* ctx) {
    if (ctx->cursor_pos == 0) {
        input_beep();
        return;
    }

    clear_completions(ctx);

    memmove(ctx->line + ctx->cursor_pos - 1,
            ctx->line + ctx->cursor_pos,
            ctx->line_len - ctx->cursor_pos + 1);
    ctx->cursor_pos--;
    ctx->line_len--;

    input_refresh_line(ctx);
}

static void handle_delete(InputContext* ctx) {
    if (ctx->cursor_pos >= ctx->line_len) {
        input_beep();
        return;
    }

    clear_completions(ctx);

    memmove(ctx->line + ctx->cursor_pos,
            ctx->line + ctx->cursor_pos + 1,
            ctx->line_len - ctx->cursor_pos);
    ctx->line_len--;

    input_refresh_line(ctx);
}

static void handle_left(InputContext* ctx) {
    if (ctx->cursor_pos > 0) {
        ctx->cursor_pos--;
        input_refresh_line(ctx);
    } else {
        input_beep();
    }
}

static void handle_right(InputContext* ctx) {
    if (ctx->cursor_pos < ctx->line_len) {
        ctx->cursor_pos++;
        input_refresh_line(ctx);
    } else {
        input_beep();
    }
}

static void handle_home(InputContext* ctx) {
    if (ctx->cursor_pos > 0) {
        ctx->cursor_pos = 0;
        input_refresh_line(ctx);
    }
}

static void handle_end(InputContext* ctx) {
    if (ctx->cursor_pos < ctx->line_len) {
        ctx->cursor_pos = ctx->line_len;
        input_refresh_line(ctx);
    }
}

static void handle_up(InputContext* ctx) {
    if (ctx->history_count == 0) {
        input_beep();
        return;
    }

    /* Save current line if this is the first up press */
    if (ctx->history_index == -1) {
        strcpy(ctx->saved_line, ctx->line);
        ctx->has_saved_line = true;
        ctx->history_index = ctx->history_count;
    }

    if (ctx->history_index > 0) {
        ctx->history_index--;
        strcpy(ctx->line, ctx->history[ctx->history_index]);
        ctx->line_len = strlen(ctx->line);
        ctx->cursor_pos = ctx->line_len;
        input_refresh_line(ctx);
    } else {
        input_beep();
    }
}

static void handle_down(InputContext* ctx) {
    if (ctx->history_index == -1) {
        input_beep();
        return;
    }

    ctx->history_index++;

    if (ctx->history_index >= ctx->history_count) {
        /* Restore saved line */
        ctx->history_index = -1;
        if (ctx->has_saved_line) {
            strcpy(ctx->line, ctx->saved_line);
            ctx->has_saved_line = false;
        } else {
            ctx->line[0] = '\0';
        }
    } else {
        strcpy(ctx->line, ctx->history[ctx->history_index]);
    }

    ctx->line_len = strlen(ctx->line);
    ctx->cursor_pos = ctx->line_len;
    input_refresh_line(ctx);
}

/* ============================================================================
 * Main readline function
 * ============================================================================ */

char* input_readline(InputContext* ctx, const char* prompt) {
    if (!ctx) return NULL;

    /* Initialize line state */
    ctx->line[0] = '\0';
    ctx->line_len = 0;
    ctx->cursor_pos = 0;
    ctx->history_index = -1;
    ctx->has_saved_line = false;
    clear_completions(ctx);

    /* Store prompt info (use visible length for cursor positioning) */
    ctx->prompt = prompt;
    ctx->prompt_len = prompt ? visible_strlen(prompt) : 0;

    /* Print prompt */
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }

    /* Enter raw mode */
    /* First check if stdin is a TTY - if not, use simple fgets */
    if (!input_is_tty() || !input_raw_mode_enable(ctx)) {
        /* Fallback to simple fgets */
        if (fgets(ctx->line, INPUT_MAX_LINE, stdin) == NULL) {
            return NULL;
        }
        /* Remove trailing newline */
        size_t len = strlen(ctx->line);
        while (len > 0 && (ctx->line[len-1] == '\n' || ctx->line[len-1] == '\r')) {
            ctx->line[--len] = '\0';
        }
        return ctx->line;
    }

    /* Read characters until Enter or EOF */
    while (1) {
#ifdef _WIN32
        int c = _getch();

        if (c == EOF || c == 3) {  /* Ctrl+C */
            input_raw_mode_disable(ctx);
            printf("\n");
            return NULL;
        }

        if (c == 4) {  /* Ctrl+D */
            if (ctx->line_len == 0) {
                input_raw_mode_disable(ctx);
                printf("\n");
                return NULL;
            }
            continue;
        }

        if (c == KEY_ENTER || c == '\n') {
            input_raw_mode_disable(ctx);
            printf("\n");
            return ctx->line;
        }

        if (c == KEY_TAB) {
            handle_tab(ctx);
            continue;
        }

        if (c == KEY_BACKSPACE || c == 127) {
            handle_backspace(ctx);
            continue;
        }

        if (c == KEY_ESCAPE) {
            /* Clear line */
            ctx->line[0] = '\0';
            ctx->line_len = 0;
            ctx->cursor_pos = 0;
            clear_completions(ctx);
            input_refresh_line(ctx);
            continue;
        }

        /* Handle special keys (arrow keys, etc.) */
        if (c == KEY_SPECIAL_PREFIX || c == KEY_SPECIAL_PREFIX2) {
            c = _getch();
            switch (c) {
                case SCAN_UP_ARROW:    handle_up(ctx); break;
                case SCAN_DOWN_ARROW:  handle_down(ctx); break;
                case SCAN_LEFT_ARROW:  handle_left(ctx); break;
                case SCAN_RIGHT_ARROW: handle_right(ctx); break;
                case SCAN_HOME:        handle_home(ctx); break;
                case SCAN_END:         handle_end(ctx); break;
                case SCAN_DELETE:      handle_delete(ctx); break;
                default: break;
            }
            continue;
        }

        /* Regular printable character */
        if (c >= 32 && c < 127) {
            insert_char(ctx, (char)c);
        }

#else /* POSIX */
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);

        if (n <= 0) {
            input_raw_mode_disable(ctx);
            printf("\n");
            return NULL;
        }

        if (c == 3) {  /* Ctrl+C */
            input_raw_mode_disable(ctx);
            printf("\n");
            return NULL;
        }

        if (c == 4) {  /* Ctrl+D */
            if (ctx->line_len == 0) {
                input_raw_mode_disable(ctx);
                printf("\n");
                return NULL;
            }
            continue;
        }

        if (c == '\r' || c == '\n') {
            input_raw_mode_disable(ctx);
            printf("\n");
            return ctx->line;
        }

        if (c == '\t') {
            handle_tab(ctx);
            continue;
        }

        if (c == 127 || c == 8) {  /* Backspace */
            handle_backspace(ctx);
            continue;
        }

        if (c == 27) {  /* Escape sequence */
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) == 0) continue;
            if (read(STDIN_FILENO, &seq[1], 1) == 0) continue;

            if (seq[0] == '[') {
                switch (seq[1]) {
                    case ANSI_UP:    handle_up(ctx); break;
                    case ANSI_DOWN:  handle_down(ctx); break;
                    case ANSI_LEFT:  handle_left(ctx); break;
                    case ANSI_RIGHT: handle_right(ctx); break;
                    case ANSI_HOME:  handle_home(ctx); break;
                    case ANSI_END:   handle_end(ctx); break;
                    case '3':  /* Delete key sends ESC[3~ */
                        read(STDIN_FILENO, &seq[2], 1);
                        handle_delete(ctx);
                        break;
                    case '1':  /* Home can also be ESC[1~ */
                        read(STDIN_FILENO, &seq[2], 1);
                        handle_home(ctx);
                        break;
                    case '4':  /* End can also be ESC[4~ */
                        read(STDIN_FILENO, &seq[2], 1);
                        handle_end(ctx);
                        break;
                    default: break;
                }
            }
            continue;
        }

        /* Regular printable character */
        if (c >= 32 && c < 127) {
            insert_char(ctx, c);
        }
#endif
    }
}

/* ============================================================================
 * Default completion functions
 * ============================================================================ */

/* List of slash commands for completion */
static const char* slash_commands[] = {
    "/help", "/h", "/?",
    "/init", "/i",
    "/build", "/b",
    "/clean", "/c",
    "/test", "/t",
    "/run", "/r",
    "/config", "/cfg",
    "/status", "/s",
    "/history", "/hist",
    "/clear", "/cls",
    "/model", "/m",
    "/verbose", "/v",
    "/exit", "/quit", "/q",
    "/ai",
    NULL
};

int input_complete_slash_commands(
    const char* input,
    size_t cursor_pos,
    char** completions,
    int max
) {
    if (!input || cursor_pos == 0 || input[0] != '/') return 0;

    int count = 0;
    size_t input_len = cursor_pos;

    for (int i = 0; slash_commands[i] && count < max; i++) {
        if (strncmp(slash_commands[i], input, input_len) == 0) {
            completions[count++] = strdup(slash_commands[i]);
        }
    }

    return count;
}

int input_complete_file_paths(
    const char* input,
    size_t cursor_pos,
    char** completions,
    int max
) {
    if (!input || cursor_pos == 0) return 0;

    /* Find start of the path (after last space) */
    size_t start = cursor_pos;
    while (start > 0 && input[start - 1] != ' ') {
        start--;
    }

    /* Extract the partial path */
    char partial[512];
    size_t partial_len = cursor_pos - start;
    if (partial_len >= sizeof(partial)) return 0;
    strncpy(partial, input + start, partial_len);
    partial[partial_len] = '\0';

    /* Find directory and prefix */
    char dir[512] = ".";
    char prefix[256] = "";
    const char* last_sep = strrchr(partial, '/');
#ifdef _WIN32
    const char* last_sep_win = strrchr(partial, '\\');
    if (last_sep_win && (!last_sep || last_sep_win > last_sep)) {
        last_sep = last_sep_win;
    }
#endif

    if (last_sep) {
        size_t dir_len = last_sep - partial;
        if (dir_len > 0) {
            strncpy(dir, partial, dir_len);
            dir[dir_len] = '\0';
        }
        strcpy(prefix, last_sep + 1);
    } else {
        strcpy(prefix, partial);
    }

    size_t prefix_len = strlen(prefix);
    int count = 0;

#ifdef _WIN32
    WIN32_FIND_DATAA ffd;
    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir);

    HANDLE hFind = FindFirstFileA(search_path, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return 0;

    do {
        if (count >= max) break;

        /* Skip . and .. */
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) {
            continue;
        }

        /* Check if prefix matches */
        if (prefix_len > 0 && strncmp(ffd.cFileName, prefix, prefix_len) != 0) {
            continue;
        }

        /* Build full completion */
        char full_path[1024];
        if (last_sep) {
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, ffd.cFileName);
        } else {
            strcpy(full_path, ffd.cFileName);
        }

        /* Add trailing slash for directories */
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            strcat(full_path, "/");
        }

        completions[count++] = strdup(full_path);
    } while (FindNextFileA(hFind, &ffd) != 0);

    FindClose(hFind);
#else
    DIR* d = opendir(dir);
    if (!d) return 0;

    struct dirent* entry;
    while ((entry = readdir(d)) != NULL && count < max) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* Check if prefix matches */
        if (prefix_len > 0 && strncmp(entry->d_name, prefix, prefix_len) != 0) {
            continue;
        }

        /* Build full completion */
        char full_path[1024];
        if (last_sep) {
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);
        } else {
            strcpy(full_path, entry->d_name);
        }

        /* Add trailing slash for directories */
        if (entry->d_type == DT_DIR) {
            strcat(full_path, "/");
        }

        completions[count++] = strdup(full_path);
    }

    closedir(d);
#endif

    return count;
}

int input_complete_combined(
    const char* input,
    size_t cursor_pos,
    char** completions,
    int max
) {
    if (!input || cursor_pos == 0) return 0;

    int count = 0;

    /* If starts with /, try slash commands first */
    if (input[0] == '/') {
        count = input_complete_slash_commands(input, cursor_pos, completions, max);
        if (count > 0) return count;
    }

    /* Otherwise try file paths */
    return input_complete_file_paths(input, cursor_pos, completions, max);
}
