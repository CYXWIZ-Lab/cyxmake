/**
 * @file file_ops.h
 * @brief File operations for AI-powered CRUD
 */

#ifndef CYXMAKE_FILE_OPS_H
#define CYXMAKE_FILE_OPS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read a file and return its contents
 * @param filepath Path to the file
 * @param out_size Output parameter for content size (can be NULL)
 * @return Allocated string with file contents (caller must free), NULL on error
 */
char* file_read(const char* filepath, size_t* out_size);

/**
 * Read a file and print to stdout with line numbers
 * @param filepath Path to the file
 * @param max_lines Maximum lines to show (0 = all)
 * @return true on success
 */
bool file_read_display(const char* filepath, int max_lines);

/**
 * Write content to a file (creates or overwrites)
 * @param filepath Path to the file
 * @param content Content to write
 * @return true on success
 */
bool file_write(const char* filepath, const char* content);

/**
 * Append content to a file
 * @param filepath Path to the file
 * @param content Content to append
 * @return true on success
 */
bool file_append(const char* filepath, const char* content);

/**
 * Delete a file
 * @param filepath Path to the file
 * @return true on success
 */
bool file_delete(const char* filepath);

/**
 * Check if a file exists
 * @param filepath Path to the file
 * @return true if file exists
 */
bool file_exists(const char* filepath);

/**
 * Delete a directory recursively
 * @param dirpath Path to the directory
 * @return true on success
 */
bool dir_delete_recursive(const char* dirpath);

/**
 * Create a directory (and parent directories if needed)
 * @param dirpath Path to create
 * @return true on success
 */
bool dir_create(const char* dirpath);

/**
 * List files in a directory
 * @param dirpath Path to the directory
 * @param pattern Optional glob pattern (e.g., "*.c"), NULL for all
 * @param out_count Output parameter for file count
 * @return Array of filenames (caller must free each string and array)
 */
char** dir_list(const char* dirpath, const char* pattern, int* out_count);

/**
 * Free a directory listing
 * @param files Array from dir_list
 * @param count Number of files
 */
void dir_list_free(char** files, int count);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_FILE_OPS_H */
