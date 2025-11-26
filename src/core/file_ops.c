/**
 * @file file_ops.c
 * @brief File operations for AI-powered CRUD
 */

#include "cyxmake/file_ops.h"
#include "cyxmake/logger.h"
#include "cyxmake/compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define PATH_SEP '\\'
#else
#include <dirent.h>
#include <unistd.h>
#include <fnmatch.h>
#define PATH_SEP '/'
#endif

/* Read a file and return its contents */
char* file_read(const char* filepath, size_t* out_size) {
    if (!filepath) return NULL;

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        log_error("Cannot open file: %s (%s)", filepath, strerror(errno));
        return NULL;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        return NULL;
    }

    /* Allocate and read */
    char* content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    size_t read_size = fread(content, 1, size, f);
    content[read_size] = '\0';
    fclose(f);

    if (out_size) {
        *out_size = read_size;
    }

    return content;
}

/* Read a file and print to stdout with line numbers */
bool file_read_display(const char* filepath, int max_lines) {
    if (!filepath) return false;

    FILE* f = fopen(filepath, "r");
    if (!f) {
        log_error("Cannot open file: %s (%s)", filepath, strerror(errno));
        return false;
    }

    char line[4096];
    int line_num = 0;
    int displayed = 0;

    log_info("File: %s", filepath);
    log_plain("----------------------------------------\n");

    while (fgets(line, sizeof(line), f)) {
        line_num++;

        /* Remove trailing newline for cleaner display */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
            if (len > 1 && line[len-2] == '\r') {
                line[len-2] = '\0';
            }
        }

        log_plain("%4d | %s\n", line_num, line);
        displayed++;

        if (max_lines > 0 && displayed >= max_lines) {
            log_plain("... (%d more lines)\n", line_num);
            break;
        }
    }

    log_plain("----------------------------------------\n");
    log_info("Total: %d lines", line_num);

    fclose(f);
    return true;
}

/* Write content to a file */
bool file_write(const char* filepath, const char* content) {
    if (!filepath) return false;

    FILE* f = fopen(filepath, "wb");
    if (!f) {
        log_error("Cannot create file: %s (%s)", filepath, strerror(errno));
        return false;
    }

    if (content) {
        size_t len = strlen(content);
        size_t written = fwrite(content, 1, len, f);
        if (written != len) {
            log_error("Failed to write all content to %s", filepath);
            fclose(f);
            return false;
        }
    }

    fclose(f);
    return true;
}

/* Append content to a file */
bool file_append(const char* filepath, const char* content) {
    if (!filepath || !content) return false;

    FILE* f = fopen(filepath, "ab");
    if (!f) {
        log_error("Cannot open file for append: %s", filepath);
        return false;
    }

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);

    return written == len;
}

/* Delete a file */
bool file_delete(const char* filepath) {
    if (!filepath) return false;

    if (remove(filepath) != 0) {
        log_error("Cannot delete file: %s (%s)", filepath, strerror(errno));
        return false;
    }

    return true;
}

/* Check if a file exists */
bool file_exists(const char* filepath) {
    if (!filepath) return false;

    struct stat st;
    return stat(filepath, &st) == 0;
}

/* Delete directory recursively - Windows version */
#ifdef _WIN32
bool dir_delete_recursive(const char* dirpath) {
    if (!dirpath) return false;

    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", dirpath);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);

    if (hFind == INVALID_HANDLE_VALUE) {
        /* Try to delete as file */
        return DeleteFileA(dirpath) || RemoveDirectoryA(dirpath);
    }

    bool success = true;

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
            continue;
        }

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dirpath, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!dir_delete_recursive(full_path)) {
                success = false;
            }
        } else {
            if (!DeleteFileA(full_path)) {
                log_warning("Cannot delete: %s", full_path);
                success = false;
            }
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);

    if (!RemoveDirectoryA(dirpath)) {
        log_warning("Cannot remove directory: %s", dirpath);
        success = false;
    }

    return success;
}

bool dir_create(const char* dirpath) {
    if (!dirpath) return false;

    /* Try to create the directory */
    if (_mkdir(dirpath) == 0) {
        return true;
    }

    if (errno == EEXIST) {
        return true;  /* Already exists */
    }

    /* Try to create parent directories */
    char* path = strdup(dirpath);
    char* p = path;

    /* Skip drive letter on Windows */
    if (p[0] && p[1] == ':') {
        p += 2;
    }
    if (*p == '\\' || *p == '/') {
        p++;
    }

    while (*p) {
        if (*p == '\\' || *p == '/') {
            *p = '\0';
            _mkdir(path);
            *p = PATH_SEP;
        }
        p++;
    }

    free(path);
    return _mkdir(dirpath) == 0 || errno == EEXIST;
}

char** dir_list(const char* dirpath, const char* pattern, int* out_count) {
    if (!dirpath || !out_count) return NULL;

    char search_path[MAX_PATH];
    if (pattern) {
        snprintf(search_path, sizeof(search_path), "%s\\%s", dirpath, pattern);
    } else {
        snprintf(search_path, sizeof(search_path), "%s\\*", dirpath);
    }

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);

    if (hFind == INVALID_HANDLE_VALUE) {
        *out_count = 0;
        return NULL;
    }

    /* Count files first */
    int count = 0;
    do {
        if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
            count++;
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);

    if (count == 0) {
        *out_count = 0;
        return NULL;
    }

    /* Allocate array */
    char** files = malloc(count * sizeof(char*));
    if (!files) {
        *out_count = 0;
        return NULL;
    }

    /* Fill array */
    hFind = FindFirstFileA(search_path, &fd);
    int i = 0;

    do {
        if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
            files[i++] = strdup(fd.cFileName);
        }
    } while (FindNextFileA(hFind, &fd) && i < count);

    FindClose(hFind);
    *out_count = i;
    return files;
}

#else /* POSIX */

bool dir_delete_recursive(const char* dirpath) {
    if (!dirpath) return false;

    DIR* d = opendir(dirpath);
    if (!d) {
        /* Try to delete as file */
        return unlink(dirpath) == 0 || rmdir(dirpath) == 0;
    }

    bool success = true;
    struct dirent* entry;

    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dirpath, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (!dir_delete_recursive(full_path)) {
                success = false;
            }
        } else {
            if (unlink(full_path) != 0) {
                log_warning("Cannot delete: %s", full_path);
                success = false;
            }
        }
    }

    closedir(d);

    if (rmdir(dirpath) != 0) {
        log_warning("Cannot remove directory: %s", dirpath);
        success = false;
    }

    return success;
}

bool dir_create(const char* dirpath) {
    if (!dirpath) return false;

    /* Try to create the directory */
    if (mkdir(dirpath, 0755) == 0) {
        return true;
    }

    if (errno == EEXIST) {
        return true;
    }

    /* Try to create parent directories */
    char* path = strdup(dirpath);
    char* p = path;

    if (*p == '/') {
        p++;
    }

    while (*p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(path, 0755);
            *p = '/';
        }
        p++;
    }

    free(path);
    return mkdir(dirpath, 0755) == 0 || errno == EEXIST;
}

char** dir_list(const char* dirpath, const char* pattern, int* out_count) {
    if (!dirpath || !out_count) return NULL;

    DIR* d = opendir(dirpath);
    if (!d) {
        *out_count = 0;
        return NULL;
    }

    /* Count matching files first */
    int count = 0;
    struct dirent* entry;

    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (pattern && fnmatch(pattern, entry->d_name, 0) != 0) {
            continue;
        }
        count++;
    }

    if (count == 0) {
        closedir(d);
        *out_count = 0;
        return NULL;
    }

    /* Allocate array */
    char** files = malloc(count * sizeof(char*));
    if (!files) {
        closedir(d);
        *out_count = 0;
        return NULL;
    }

    /* Fill array */
    rewinddir(d);
    int i = 0;

    while ((entry = readdir(d)) != NULL && i < count) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (pattern && fnmatch(pattern, entry->d_name, 0) != 0) {
            continue;
        }
        files[i++] = strdup(entry->d_name);
    }

    closedir(d);
    *out_count = i;
    return files;
}

#endif

void dir_list_free(char** files, int count) {
    if (!files) return;

    for (int i = 0; i < count; i++) {
        free(files[i]);
    }
    free(files);
}
