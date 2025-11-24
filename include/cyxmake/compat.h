/**
 * @file compat.h
 * @brief Cross-platform compatibility macros
 */

#ifndef CYXMAKE_COMPAT_H
#define CYXMAKE_COMPAT_H

/* Platform detection */
#if defined(_WIN32) || defined(_WIN64)
    #define CYXMAKE_WINDOWS
#elif defined(__linux__)
    #define CYXMAKE_LINUX
#elif defined(__APPLE__)
    #define CYXMAKE_MACOS
#endif

/* String comparison */
#ifdef CYXMAKE_WINDOWS
    #include <string.h>
    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
    #define strdup _strdup
#else
    #include <strings.h>
#endif

/* File type macros for stat */
#ifdef CYXMAKE_WINDOWS
    #include <sys/stat.h>
    /* Windows doesn't define S_ISREG/S_ISDIR macros */
    #ifndef S_ISREG
        #define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
    #endif
    #ifndef S_ISDIR
        #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
    #endif
#endif

/* Directory separator */
#ifdef CYXMAKE_WINDOWS
    #define DIR_SEP '\\'
    #define DIR_SEP_STR "\\"
#else
    #define DIR_SEP '/'
    #define DIR_SEP_STR "/"
#endif

#endif /* CYXMAKE_COMPAT_H */
