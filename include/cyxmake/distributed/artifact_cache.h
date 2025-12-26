/**
 * @file artifact_cache.h
 * @brief Distributed artifact caching for build outputs
 *
 * Provides hash-based caching of build artifacts (object files, libraries,
 * executables) with local and distributed cache layers.
 */

#ifndef CYXMAKE_DISTRIBUTED_ARTIFACT_CACHE_H
#define CYXMAKE_DISTRIBUTED_ARTIFACT_CACHE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Cache Entry Types
 * ============================================================ */

typedef enum {
    ARTIFACT_OBJECT_FILE,         /* .o / .obj file */
    ARTIFACT_STATIC_LIB,          /* .a / .lib file */
    ARTIFACT_SHARED_LIB,          /* .so / .dll file */
    ARTIFACT_EXECUTABLE,          /* Executable binary */
    ARTIFACT_PRECOMPILED_HEADER,  /* .pch / .gch file */
    ARTIFACT_SOURCE_ARCHIVE,      /* Source archive for transfer */
    ARTIFACT_OTHER                /* Other artifact type */
} ArtifactType;

/* ============================================================
 * Cache Hit Status
 * ============================================================ */

typedef enum {
    CACHE_MISS = 0,               /* Not in cache */
    CACHE_HIT_LOCAL,              /* Found in local cache */
    CACHE_HIT_REMOTE,             /* Found in remote cache */
    CACHE_HIT_PENDING             /* Being fetched from remote */
} CacheHitStatus;

/* ============================================================
 * Artifact Entry
 * ============================================================ */

typedef struct ArtifactEntry {
    /* Identity */
    char* cache_key;              /* Unique cache key (hash) */
    char* source_hash;            /* Hash of source file(s) */
    char* compiler_hash;          /* Hash of compiler + flags */

    /* Metadata */
    ArtifactType type;            /* Artifact type */
    char* original_path;          /* Original file path */
    char* cached_path;            /* Path in cache */
    size_t size_bytes;            /* File size */
    time_t created_at;            /* When cached */
    time_t last_accessed;         /* Last access time */
    int access_count;             /* Number of accesses */

    /* Hashes */
    char* content_hash;           /* SHA-256 of content */
    char* compressed_hash;        /* Hash of compressed form */

    /* Compression */
    bool is_compressed;           /* Whether stored compressed */
    size_t compressed_size;       /* Compressed size */
    char* compression_algo;       /* Compression algorithm */

    /* Origin */
    char* producer_host;          /* Host that produced this */
    char* build_id;               /* Associated build ID */

    struct ArtifactEntry* next;   /* For linked lists */
} ArtifactEntry;

/* ============================================================
 * Cache Statistics
 * ============================================================ */

typedef struct {
    /* Hit/miss stats */
    int total_lookups;
    int local_hits;
    int remote_hits;
    int misses;

    /* Storage stats */
    size_t total_size_bytes;
    size_t compressed_size_bytes;
    int total_entries;

    /* Transfer stats */
    size_t bytes_downloaded;
    size_t bytes_uploaded;
    int remote_fetches;
    int remote_pushes;

    /* Eviction stats */
    int entries_evicted;
    size_t bytes_evicted;
} CacheStats;

/* ============================================================
 * Cache Configuration
 * ============================================================ */

typedef struct {
    /* Local cache */
    char* cache_dir;              /* Local cache directory */
    size_t max_size_bytes;        /* Maximum cache size (default: 10GB) */
    int max_entries;              /* Maximum entries (default: 100000) */
    int max_age_days;             /* Maximum age before eviction (default: 30) */

    /* Compression */
    bool enable_compression;      /* Enable artifact compression */
    char* compression_algo;       /* Algorithm: "zstd", "gzip", "lz4" */
    int compression_level;        /* Compression level (1-22 for zstd) */
    size_t compression_threshold; /* Min size to compress (default: 4KB) */

    /* Remote cache */
    bool enable_remote;           /* Enable remote cache layer */
    char* remote_url;             /* Remote cache URL */
    char* remote_auth_token;      /* Authentication token */
    int remote_timeout_sec;       /* Remote operation timeout */
    bool remote_read_only;        /* Only read from remote, no push */

    /* Eviction policy */
    char* eviction_policy;        /* "lru", "lfu", "fifo" */
    double eviction_threshold;    /* Start evicting at this % full */
} ArtifactCacheConfig;

/* ============================================================
 * Artifact Cache
 * ============================================================ */

typedef struct ArtifactCache ArtifactCache;

/* ============================================================
 * Cache Key Generation
 * ============================================================ */

/**
 * Input for cache key generation
 */
typedef struct {
    const char* source_file;      /* Source file path */
    const char* source_content;   /* Source content (optional, for hashing) */
    size_t source_size;           /* Source size */
    const char* compiler;         /* Compiler path/name */
    const char** compiler_flags;  /* Compiler flags */
    int flag_count;               /* Number of flags */
    const char** include_paths;   /* Include paths */
    int include_count;            /* Number of include paths */
    const char* target_triple;    /* Target triple (e.g., x86_64-linux-gnu) */
} CacheKeyInput;

/* ============================================================
 * Cache API
 * ============================================================ */

/**
 * Create artifact cache
 */
ArtifactCache* artifact_cache_create(const ArtifactCacheConfig* config);

/**
 * Free artifact cache
 */
void artifact_cache_free(ArtifactCache* cache);

/**
 * Initialize cache directory
 */
bool artifact_cache_init(ArtifactCache* cache);

/**
 * Generate cache key from inputs
 */
char* artifact_cache_generate_key(const CacheKeyInput* input);

/**
 * Check if artifact is in cache
 * @param cache The cache
 * @param cache_key Cache key to look up
 * @return Cache hit status
 */
CacheHitStatus artifact_cache_lookup(ArtifactCache* cache,
                                      const char* cache_key);

/**
 * Get cached artifact entry
 * @param cache The cache
 * @param cache_key Cache key
 * @return Artifact entry or NULL if not found
 */
ArtifactEntry* artifact_cache_get(ArtifactCache* cache,
                                   const char* cache_key);

/**
 * Retrieve artifact from cache to specified path
 * @param cache The cache
 * @param cache_key Cache key
 * @param output_path Where to write the artifact
 * @return true if successful
 */
bool artifact_cache_retrieve(ArtifactCache* cache,
                              const char* cache_key,
                              const char* output_path);

/**
 * Store artifact in cache
 * @param cache The cache
 * @param cache_key Cache key
 * @param file_path Path to file to cache
 * @param type Artifact type
 * @param metadata Additional metadata (optional)
 * @return Stored entry or NULL on error
 */
ArtifactEntry* artifact_cache_store(ArtifactCache* cache,
                                     const char* cache_key,
                                     const char* file_path,
                                     ArtifactType type,
                                     const char* metadata);

/**
 * Store artifact from memory buffer
 */
ArtifactEntry* artifact_cache_store_buffer(ArtifactCache* cache,
                                            const char* cache_key,
                                            const void* data,
                                            size_t size,
                                            ArtifactType type);

/**
 * Delete artifact from cache
 */
bool artifact_cache_delete(ArtifactCache* cache, const char* cache_key);

/**
 * Check if cache contains key
 */
bool artifact_cache_contains(ArtifactCache* cache, const char* cache_key);

/* ============================================================
 * Remote Cache Operations
 * ============================================================ */

/**
 * Fetch artifact from remote cache
 * @param cache The cache
 * @param cache_key Cache key
 * @return true if found and fetched
 */
bool artifact_cache_fetch_remote(ArtifactCache* cache,
                                  const char* cache_key);

/**
 * Push artifact to remote cache
 * @param cache The cache
 * @param cache_key Cache key
 * @return true if pushed successfully
 */
bool artifact_cache_push_remote(ArtifactCache* cache,
                                 const char* cache_key);

/**
 * Sync local cache with remote
 * @param cache The cache
 * @param direction "push", "pull", or "both"
 * @return Number of artifacts synced
 */
int artifact_cache_sync(ArtifactCache* cache, const char* direction);

/* ============================================================
 * Cache Maintenance
 * ============================================================ */

/**
 * Run cache eviction to free space
 * @param cache The cache
 * @param target_free_bytes How many bytes to free (0 = use policy)
 * @return Number of entries evicted
 */
int artifact_cache_evict(ArtifactCache* cache, size_t target_free_bytes);

/**
 * Remove stale/expired entries
 * @param cache The cache
 * @return Number of entries removed
 */
int artifact_cache_cleanup(ArtifactCache* cache);

/**
 * Verify cache integrity
 * @param cache The cache
 * @param fix Attempt to fix issues
 * @return Number of issues found
 */
int artifact_cache_verify(ArtifactCache* cache, bool fix);

/**
 * Clear entire cache
 */
void artifact_cache_clear(ArtifactCache* cache);

/* ============================================================
 * Cache Information
 * ============================================================ */

/**
 * Get cache statistics
 */
CacheStats artifact_cache_get_stats(ArtifactCache* cache);

/**
 * Reset statistics
 */
void artifact_cache_reset_stats(ArtifactCache* cache);

/**
 * Get total cache size
 */
size_t artifact_cache_get_size(ArtifactCache* cache);

/**
 * Get number of entries
 */
int artifact_cache_get_count(ArtifactCache* cache);

/**
 * Get cache hit rate (0.0 - 1.0)
 */
double artifact_cache_get_hit_rate(ArtifactCache* cache);

/**
 * List all entries (for debugging)
 */
ArtifactEntry* artifact_cache_list(ArtifactCache* cache);

/* ============================================================
 * Hashing Functions
 * ============================================================ */

/**
 * Compute SHA-256 hash of file
 */
char* artifact_hash_file(const char* file_path);

/**
 * Compute SHA-256 hash of buffer
 */
char* artifact_hash_buffer(const void* data, size_t size);

/**
 * Compute combined hash for cache key
 */
char* artifact_hash_combined(const char** strings, int count);

/* ============================================================
 * Compression Functions
 * ============================================================ */

/**
 * Compress data
 * @param data Input data
 * @param size Input size
 * @param out_size Output compressed size
 * @param algo Algorithm ("zstd", "gzip", "lz4")
 * @param level Compression level
 * @return Compressed data (caller frees) or NULL
 */
void* artifact_compress(const void* data, size_t size,
                        size_t* out_size,
                        const char* algo, int level);

/**
 * Decompress data
 * @param data Compressed data
 * @param size Compressed size
 * @param out_size Output decompressed size
 * @param algo Algorithm used
 * @return Decompressed data (caller frees) or NULL
 */
void* artifact_decompress(const void* data, size_t size,
                          size_t* out_size, const char* algo);

/* ============================================================
 * Utility Functions
 * ============================================================ */

/**
 * Get artifact type name
 */
const char* artifact_type_name(ArtifactType type);

/**
 * Get cache hit status name
 */
const char* cache_hit_status_name(CacheHitStatus status);

/**
 * Create default cache configuration
 */
ArtifactCacheConfig artifact_cache_config_default(void);

/**
 * Free cache configuration
 */
void artifact_cache_config_free(ArtifactCacheConfig* config);

/**
 * Free artifact entry
 */
void artifact_entry_free(ArtifactEntry* entry);

/**
 * Free artifact entry list
 */
void artifact_entry_list_free(ArtifactEntry* entries);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_DISTRIBUTED_ARTIFACT_CACHE_H */
