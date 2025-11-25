/**
 * @file error_patterns.c
 * @brief Error pattern matching and database
 */

#include "cyxmake/error_recovery.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
    #define strdup _strdup
    #define strncpy strncpy_s
#endif

/* Pattern definitions for missing files */
static const char* missing_file_patterns[] = {
    "No such file or directory",
    "cannot find.*file",
    "could not open.*for reading",
    "File not found",
    "does not exist",
    "cannot access.*No such file",
    NULL
};

/* Pattern definitions for missing libraries */
static const char* missing_library_patterns[] = {
    "cannot find -l",
    "undefined reference to",
    "unresolved external symbol",
    "library not found for",
    "ld: cannot find",
    "LINK : fatal error LNK1181",
    "No rule to make target.*\\.a",
    NULL
};

/* Pattern definitions for missing headers */
static const char* missing_header_patterns[] = {
    "fatal error:.*No such file or directory",
    "cannot open include file",
    "could not find include file",
    "#include.*not found",
    "error: .*\\.h.*No such file",
    NULL
};

/* Pattern definitions for permission errors */
static const char* permission_denied_patterns[] = {
    "Permission denied",
    "Access is denied",
    "cannot create.*Permission",
    "cannot write.*Permission",
    "Access denied",
    NULL
};

/* Pattern definitions for disk space errors */
static const char* disk_full_patterns[] = {
    "No space left on device",
    "Disk full",
    "insufficient disk space",
    "out of disk space",
    "There is not enough space",
    NULL
};

/* Pattern definitions for syntax errors */
static const char* syntax_error_patterns[] = {
    "syntax error",
    "expected.*before",
    "unexpected token",
    "missing terminating",
    "unterminated",
    "invalid syntax",
    NULL
};

/* Pattern definitions for undefined references */
static const char* undefined_ref_patterns[] = {
    "undefined reference to",
    "unresolved external symbol",
    "symbol.*not found",
    "cannot resolve symbol",
    NULL
};

/* Pattern definitions for version mismatches */
static const char* version_mismatch_patterns[] = {
    "version.*required",
    "version.*mismatch",
    "incompatible.*version",
    "requires.*version",
    "wrong version",
    NULL
};

/* Pattern definitions for network errors */
static const char* network_error_patterns[] = {
    "Connection refused",
    "Connection timeout",
    "Could not resolve host",
    "Network is unreachable",
    "Failed to connect",
    "Download failed",
    NULL
};

/* Pattern definitions for timeout errors */
static const char* timeout_patterns[] = {
    "timeout",
    "timed out",
    "took too long",
    "deadline exceeded",
    NULL
};

/* Error pattern database */
static ErrorPattern pattern_database[] = {
    {
        .type = ERROR_PATTERN_MISSING_FILE,
        .name = "Missing File",
        .patterns = missing_file_patterns,
        .pattern_count = 6,
        .description = "A required file could not be found",
        .priority = 10
    },
    {
        .type = ERROR_PATTERN_MISSING_LIBRARY,
        .name = "Missing Library",
        .patterns = missing_library_patterns,
        .pattern_count = 7,
        .description = "A required library is not installed or not found",
        .priority = 9
    },
    {
        .type = ERROR_PATTERN_MISSING_HEADER,
        .name = "Missing Header",
        .patterns = missing_header_patterns,
        .pattern_count = 5,
        .description = "A required header file is not found",
        .priority = 9
    },
    {
        .type = ERROR_PATTERN_PERMISSION_DENIED,
        .name = "Permission Denied",
        .patterns = permission_denied_patterns,
        .pattern_count = 5,
        .description = "Insufficient permissions to access a resource",
        .priority = 8
    },
    {
        .type = ERROR_PATTERN_DISK_FULL,
        .name = "Disk Full",
        .patterns = disk_full_patterns,
        .pattern_count = 5,
        .description = "Not enough disk space available",
        .priority = 8
    },
    {
        .type = ERROR_PATTERN_SYNTAX_ERROR,
        .name = "Syntax Error",
        .patterns = syntax_error_patterns,
        .pattern_count = 6,
        .description = "Code syntax error",
        .priority = 5
    },
    {
        .type = ERROR_PATTERN_UNDEFINED_REFERENCE,
        .name = "Undefined Reference",
        .patterns = undefined_ref_patterns,
        .pattern_count = 4,
        .description = "Symbol not defined or linked",
        .priority = 7
    },
    {
        .type = ERROR_PATTERN_VERSION_MISMATCH,
        .name = "Version Mismatch",
        .patterns = version_mismatch_patterns,
        .pattern_count = 5,
        .description = "Incompatible version of a dependency",
        .priority = 6
    },
    {
        .type = ERROR_PATTERN_NETWORK_ERROR,
        .name = "Network Error",
        .patterns = network_error_patterns,
        .pattern_count = 6,
        .description = "Network connectivity issue",
        .priority = 4
    },
    {
        .type = ERROR_PATTERN_TIMEOUT,
        .name = "Timeout",
        .patterns = timeout_patterns,
        .pattern_count = 4,
        .description = "Operation timed out",
        .priority = 3
    }
};

static const size_t pattern_database_size = sizeof(pattern_database) / sizeof(pattern_database[0]);

/* Custom patterns storage */
static ErrorPattern* custom_patterns = NULL;
static size_t custom_pattern_count = 0;
static size_t custom_pattern_capacity = 0;

/* Forward declarations */
char* extract_error_detail(const char* error_output, ErrorPatternType type);

/* Pattern comparison function for sorting by priority */
static int pattern_compare(const void* a, const void* b) {
    const ErrorPattern* pa = (const ErrorPattern*)a;
    const ErrorPattern* pb = (const ErrorPattern*)b;
    return pb->priority - pa->priority;  /* Higher priority first */
}

/* Initialize error pattern database */
bool error_patterns_init(void) {
    /* Sort patterns by priority */
    qsort(pattern_database, pattern_database_size, sizeof(ErrorPattern), pattern_compare);

    log_debug("Initialized %zu error patterns", pattern_database_size);
    return true;
}

/* Shutdown pattern database */
void error_patterns_shutdown(void) {
    /* Free custom patterns */
    if (custom_patterns) {
        free(custom_patterns);
        custom_patterns = NULL;
        custom_pattern_count = 0;
        custom_pattern_capacity = 0;
    }
}

/* Register a custom error pattern */
bool error_patterns_register(const ErrorPattern* pattern) {
    if (!pattern) return false;

    /* Grow array if needed */
    if (custom_pattern_count >= custom_pattern_capacity) {
        size_t new_capacity = custom_pattern_capacity ? custom_pattern_capacity * 2 : 8;
        ErrorPattern* new_patterns = realloc(custom_patterns,
                                             new_capacity * sizeof(ErrorPattern));
        if (!new_patterns) {
            log_error("Failed to allocate memory for custom patterns");
            return false;
        }
        custom_patterns = new_patterns;
        custom_pattern_capacity = new_capacity;
    }

    /* Copy pattern */
    custom_patterns[custom_pattern_count] = *pattern;
    custom_pattern_count++;

    log_debug("Registered custom pattern: %s", pattern->name);
    return true;
}

/* Case-insensitive string search */
static const char* stristr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;

    size_t haystack_len = strlen(haystack);
    size_t needle_len = strlen(needle);

    if (needle_len > haystack_len) return NULL;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        bool match = true;
        for (size_t j = 0; j < needle_len; j++) {
            char h = tolower((unsigned char)haystack[i + j]);
            char n = tolower((unsigned char)needle[j]);

            if (h != n) {
                match = false;
                break;
            }
        }
        if (match) return &haystack[i];
    }

    return NULL;
}

/* Match a single pattern against error output */
static bool match_pattern(const char* error_output, const char* pattern) {
    if (!error_output || !pattern) return false;

    /* For now, use simple substring matching (case-insensitive) */
    return stristr(error_output, pattern) != NULL;
}

/* Match error output against patterns */
ErrorPatternType error_patterns_match(const char* error_output) {
    if (!error_output) return ERROR_PATTERN_UNKNOWN;

    /* Check built-in patterns first (already sorted by priority) */
    for (size_t i = 0; i < pattern_database_size; i++) {
        const ErrorPattern* pattern = &pattern_database[i];

        for (size_t j = 0; pattern->patterns[j] != NULL && j < pattern->pattern_count; j++) {
            if (match_pattern(error_output, pattern->patterns[j])) {
                log_debug("Matched pattern: %s", pattern->name);
                return pattern->type;
            }
        }
    }

    /* Check custom patterns */
    for (size_t i = 0; i < custom_pattern_count; i++) {
        const ErrorPattern* pattern = &custom_patterns[i];

        for (size_t j = 0; pattern->patterns[j] != NULL && j < pattern->pattern_count; j++) {
            if (match_pattern(error_output, pattern->patterns[j])) {
                log_debug("Matched custom pattern: %s", pattern->name);
                return pattern->type;
            }
        }
    }

    return ERROR_PATTERN_UNKNOWN;
}

/* Get pattern by type */
const ErrorPattern* error_patterns_get(ErrorPatternType type) {
    /* Search built-in patterns */
    for (size_t i = 0; i < pattern_database_size; i++) {
        if (pattern_database[i].type == type) {
            return &pattern_database[i];
        }
    }

    /* Search custom patterns */
    for (size_t i = 0; i < custom_pattern_count; i++) {
        if (custom_patterns[i].type == type) {
            return &custom_patterns[i];
        }
    }

    return NULL;
}

/* Extract detail from error message (e.g., library name from "cannot find -lfoo") */
char* extract_error_detail(const char* error_output, ErrorPatternType type) {
    if (!error_output) return NULL;

    char* detail = NULL;
    char buffer[256];

    switch (type) {
        case ERROR_PATTERN_MISSING_LIBRARY: {
            /* Try to extract library name */
            const char* p = strstr(error_output, "cannot find -l");
            if (p) {
                p += strlen("cannot find -l");
                const char* end = p;
                while (*end && *end != ' ' && *end != '\n' && *end != '\r') end++;
                size_t len = end - p;
                if (len > 0 && len < sizeof(buffer)) {
                    memcpy(buffer, p, len);
                    buffer[len] = '\0';
                    detail = strdup(buffer);
                }
            } else {
                /* Try "undefined reference to" */
                p = strstr(error_output, "undefined reference to");
                if (p) {
                    p += strlen("undefined reference to");
                    while (*p == ' ' || *p == '`' || *p == '\'') p++;
                    const char* end = p;
                    while (*end && *end != '\'' && *end != '`' && *end != '(') end++;
                    size_t len = end - p;
                    if (len > 0 && len < sizeof(buffer)) {
                        memcpy(buffer, p, len);
                        buffer[len] = '\0';
                        detail = strdup(buffer);
                    }
                }
            }
            break;
        }

        case ERROR_PATTERN_MISSING_HEADER: {
            /* Try to extract header name */
            const char* start = strchr(error_output, '<');
            if (start) {
                start++;
                const char* end = strchr(start, '>');
                if (end) {
                    size_t len = end - start;
                    if (len > 0 && len < sizeof(buffer)) {
                        memcpy(buffer, start, len);
                        buffer[len] = '\0';
                        detail = strdup(buffer);
                    }
                }
            } else {
                /* Try quotes */
                start = strchr(error_output, '"');
                if (start) {
                    start++;
                    const char* end = strchr(start, '"');
                    if (end) {
                        size_t len = end - start;
                        if (len > 0 && len < sizeof(buffer)) {
                            memcpy(buffer, start, len);
                            buffer[len] = '\0';
                            detail = strdup(buffer);
                        }
                    }
                }
            }
            break;
        }

        case ERROR_PATTERN_MISSING_FILE: {
            /* Try to extract file path */
            const char* patterns[] = {
                "No such file or directory:",
                "cannot find ",
                "could not open ",
                "File not found: ",
                NULL
            };

            for (int i = 0; patterns[i]; i++) {
                const char* p = strstr(error_output, patterns[i]);
                if (p) {
                    p += strlen(patterns[i]);
                    while (*p == ' ' || *p == '\'' || *p == '"') p++;
                    const char* end = p;
                    while (*end && *end != '\n' && *end != '\r' &&
                           *end != ' ' && *end != '\'' && *end != '"') end++;
                    size_t len = end - p;
                    if (len > 0 && len < sizeof(buffer)) {
                        memcpy(buffer, p, len);
                        buffer[len] = '\0';
                        detail = strdup(buffer);
                        break;
                    }
                }
            }
            break;
        }

        default:
            /* No specific extraction for other types */
            break;
    }

    return detail;
}