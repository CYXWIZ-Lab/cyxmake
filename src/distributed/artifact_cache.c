/**
 * @file artifact_cache.c
 * @brief Distributed artifact caching implementation
 *
 * Provides local and distributed caching of build artifacts with
 * content-addressable storage, compression, and LRU eviction.
 */

#include "cyxmake/distributed/artifact_cache.h"
#include "cyxmake/logger.h"
#include "cyxmake/compat.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define mkdir(path, mode) _mkdir(path)
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#include <dirent.h>
#endif

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
#include "cyxmake/threading.h"
#endif

/* ============================================================
 * Constants
 * ============================================================ */

#define DEFAULT_MAX_SIZE_BYTES (10ULL * 1024 * 1024 * 1024)  /* 10GB */
#define DEFAULT_MAX_ENTRIES 100000
#define DEFAULT_MAX_AGE_DAYS 30
#define DEFAULT_COMPRESSION_THRESHOLD 4096
#define DEFAULT_EVICTION_THRESHOLD 0.9
#define HASH_HEX_LENGTH 64  /* SHA-256 = 32 bytes = 64 hex chars */

/* ============================================================
 * Internal Structures
 * ============================================================ */

struct ArtifactCache {
    ArtifactCacheConfig config;
    ArtifactEntry* entries;       /* Linked list of entries */
    int entry_count;
    size_t total_size;

    CacheStats stats;

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    MutexHandle mutex;
#endif
};

/* ============================================================
 * Helper Functions
 * ============================================================ */

static void cache_lock(ArtifactCache* cache) {
#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    mutex_lock(&cache->mutex);
#else
    (void)cache;
#endif
}

static void cache_unlock(ArtifactCache* cache) {
#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    mutex_unlock(&cache->mutex);
#else
    (void)cache;
#endif
}

static bool ensure_directory(const char* path) {
    if (!path) return false;

#ifdef _WIN32
    struct _stat st;
    if (_stat(path, &st) == 0) {
        return (st.st_mode & _S_IFDIR) != 0;
    }
    return _mkdir(path) == 0;
#else
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return mkdir(path, 0755) == 0;
#endif
}

static bool file_exists(const char* path) {
    return access(path, F_OK) == 0;
}

static size_t get_file_size(const char* path) {
#ifdef _WIN32
    struct _stat st;
    if (_stat(path, &st) != 0) return 0;
#else
    struct stat st;
    if (stat(path, &st) != 0) return 0;
#endif
    return (size_t)st.st_size;
}

static bool copy_file(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if (!in) return false;

    FILE* out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return false;
    }

    char buffer[8192];
    size_t bytes;
    bool success = true;

    while ((bytes = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, bytes, out) != bytes) {
            success = false;
            break;
        }
    }

    fclose(in);
    fclose(out);
    return success;
}

/* Simple SHA-256-like hash (not cryptographically secure) */
static void simple_hash(const void* data, size_t len, unsigned char* out) {
    const unsigned char* input = (const unsigned char*)data;
    unsigned char state[32] = {
        0x6a, 0x09, 0xe6, 0x67, 0xbb, 0x67, 0xae, 0x85,
        0x3c, 0x6e, 0xf3, 0x72, 0xa5, 0x4f, 0xf5, 0x3a,
        0x51, 0x0e, 0x52, 0x7f, 0x9b, 0x05, 0x68, 0x8c,
        0x1f, 0x83, 0xd9, 0xab, 0x5b, 0xe0, 0xcd, 0x19
    };

    for (size_t i = 0; i < len; i++) {
        state[i % 32] ^= input[i];
        state[(i + 1) % 32] += state[i % 32];
        state[(i + 7) % 32] ^= (state[(i + 3) % 32] >> 3);
    }

    memcpy(out, state, 32);
}

static char* bytes_to_hex(const unsigned char* bytes, size_t len) {
    char* hex = malloc(len * 2 + 1);
    if (!hex) return NULL;

    for (size_t i = 0; i < len; i++) {
        sprintf(hex + i * 2, "%02x", bytes[i]);
    }
    hex[len * 2] = '\0';

    return hex;
}

/* ============================================================
 * Configuration
 * ============================================================ */

ArtifactCacheConfig artifact_cache_config_default(void) {
    ArtifactCacheConfig config = {
        .cache_dir = NULL,
        .max_size_bytes = DEFAULT_MAX_SIZE_BYTES,
        .max_entries = DEFAULT_MAX_ENTRIES,
        .max_age_days = DEFAULT_MAX_AGE_DAYS,
        .enable_compression = false,  /* Requires zstd */
        .compression_algo = NULL,
        .compression_level = 3,
        .compression_threshold = DEFAULT_COMPRESSION_THRESHOLD,
        .enable_remote = false,
        .remote_url = NULL,
        .remote_auth_token = NULL,
        .remote_timeout_sec = 30,
        .remote_read_only = false,
        .eviction_policy = NULL,
        .eviction_threshold = DEFAULT_EVICTION_THRESHOLD
    };
    return config;
}

void artifact_cache_config_free(ArtifactCacheConfig* config) {
    if (!config) return;
    free(config->cache_dir);
    free(config->compression_algo);
    free(config->remote_url);
    free(config->remote_auth_token);
    free(config->eviction_policy);
}

/* ============================================================
 * Entry Management
 * ============================================================ */

void artifact_entry_free(ArtifactEntry* entry) {
    if (!entry) return;

    free(entry->cache_key);
    free(entry->source_hash);
    free(entry->compiler_hash);
    free(entry->original_path);
    free(entry->cached_path);
    free(entry->content_hash);
    free(entry->compressed_hash);
    free(entry->compression_algo);
    free(entry->producer_host);
    free(entry->build_id);

    free(entry);
}

void artifact_entry_list_free(ArtifactEntry* entries) {
    while (entries) {
        ArtifactEntry* next = entries->next;
        artifact_entry_free(entries);
        entries = next;
    }
}

/* ============================================================
 * Cache API Implementation
 * ============================================================ */

ArtifactCache* artifact_cache_create(const ArtifactCacheConfig* config) {
    ArtifactCache* cache = calloc(1, sizeof(ArtifactCache));
    if (!cache) {
        log_error("Failed to allocate artifact cache");
        return NULL;
    }

    if (config) {
        cache->config = *config;
        if (config->cache_dir) {
            cache->config.cache_dir = strdup(config->cache_dir);
        }
        if (config->compression_algo) {
            cache->config.compression_algo = strdup(config->compression_algo);
        }
        if (config->remote_url) {
            cache->config.remote_url = strdup(config->remote_url);
        }
        if (config->remote_auth_token) {
            cache->config.remote_auth_token = strdup(config->remote_auth_token);
        }
        if (config->eviction_policy) {
            cache->config.eviction_policy = strdup(config->eviction_policy);
        }
    } else {
        cache->config = artifact_cache_config_default();
    }

    /* Default cache directory */
    if (!cache->config.cache_dir) {
#ifdef _WIN32
        const char* appdata = getenv("LOCALAPPDATA");
        if (appdata) {
            size_t len = strlen(appdata) + 32;
            cache->config.cache_dir = malloc(len);
            snprintf(cache->config.cache_dir, len, "%s\\cyxmake\\cache", appdata);
        }
#else
        const char* home = getenv("HOME");
        if (home) {
            size_t len = strlen(home) + 32;
            cache->config.cache_dir = malloc(len);
            snprintf(cache->config.cache_dir, len, "%s/.cyxmake/cache", home);
        }
#endif
    }

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    if (!mutex_init(&cache->mutex)) {
        log_error("Failed to create cache mutex");
        artifact_cache_free(cache);
        return NULL;
    }
#endif

    log_info("Artifact cache created (dir: %s, max: %zu MB)",
             cache->config.cache_dir ? cache->config.cache_dir : "default",
             cache->config.max_size_bytes / (1024 * 1024));

    return cache;
}

void artifact_cache_free(ArtifactCache* cache) {
    if (!cache) return;

    /* Free all entries */
    artifact_entry_list_free(cache->entries);

    /* Free config */
    artifact_cache_config_free(&cache->config);

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    mutex_destroy(&cache->mutex);
#endif

    free(cache);
    log_debug("Artifact cache freed");
}

bool artifact_cache_init(ArtifactCache* cache) {
    if (!cache || !cache->config.cache_dir) return false;

    /* Create cache directory structure */
    if (!ensure_directory(cache->config.cache_dir)) {
        log_error("Failed to create cache directory: %s", cache->config.cache_dir);
        return false;
    }

    /* Create subdirectories for two-level hash storage */
    char subdir[512];
    for (int i = 0; i < 256; i++) {
        snprintf(subdir, sizeof(subdir), "%s/%02x", cache->config.cache_dir, i);
        ensure_directory(subdir);
    }

    log_info("Artifact cache initialized: %s", cache->config.cache_dir);
    return true;
}

char* artifact_cache_generate_key(const CacheKeyInput* input) {
    if (!input || !input->source_file) return NULL;

    /* Build combined string for hashing */
    size_t total_len = 0;
    total_len += strlen(input->source_file) + 1;
    if (input->compiler) total_len += strlen(input->compiler) + 1;
    if (input->target_triple) total_len += strlen(input->target_triple) + 1;

    for (int i = 0; i < input->flag_count && input->compiler_flags; i++) {
        if (input->compiler_flags[i]) {
            total_len += strlen(input->compiler_flags[i]) + 1;
        }
    }

    char* combined = malloc(total_len + 1);
    if (!combined) return NULL;

    char* p = combined;
    p += sprintf(p, "%s|", input->source_file);
    if (input->compiler) p += sprintf(p, "%s|", input->compiler);
    if (input->target_triple) p += sprintf(p, "%s|", input->target_triple);

    for (int i = 0; i < input->flag_count && input->compiler_flags; i++) {
        if (input->compiler_flags[i]) {
            p += sprintf(p, "%s|", input->compiler_flags[i]);
        }
    }

    /* Hash the combined string */
    unsigned char hash[32];
    simple_hash(combined, strlen(combined), hash);
    free(combined);

    return bytes_to_hex(hash, 32);
}

CacheHitStatus artifact_cache_lookup(ArtifactCache* cache,
                                      const char* cache_key) {
    if (!cache || !cache_key) return CACHE_MISS;

    cache_lock(cache);
    cache->stats.total_lookups++;

    /* Search local entries */
    for (ArtifactEntry* e = cache->entries; e; e = e->next) {
        if (strcmp(e->cache_key, cache_key) == 0) {
            e->last_accessed = time(NULL);
            e->access_count++;
            cache->stats.local_hits++;
            cache_unlock(cache);
            return CACHE_HIT_LOCAL;
        }
    }

    cache->stats.misses++;
    cache_unlock(cache);

    /* TODO: Check remote cache if enabled */

    return CACHE_MISS;
}

ArtifactEntry* artifact_cache_get(ArtifactCache* cache,
                                   const char* cache_key) {
    if (!cache || !cache_key) return NULL;

    cache_lock(cache);

    for (ArtifactEntry* e = cache->entries; e; e = e->next) {
        if (strcmp(e->cache_key, cache_key) == 0) {
            e->last_accessed = time(NULL);
            e->access_count++;
            cache_unlock(cache);
            return e;
        }
    }

    cache_unlock(cache);
    return NULL;
}

bool artifact_cache_retrieve(ArtifactCache* cache,
                              const char* cache_key,
                              const char* output_path) {
    if (!cache || !cache_key || !output_path) return false;

    ArtifactEntry* entry = artifact_cache_get(cache, cache_key);
    if (!entry || !entry->cached_path) return false;

    /* Copy from cache to output path */
    if (!copy_file(entry->cached_path, output_path)) {
        log_error("Failed to retrieve artifact: %s", cache_key);
        return false;
    }

    log_debug("Retrieved artifact: %s -> %s", cache_key, output_path);
    return true;
}

ArtifactEntry* artifact_cache_store(ArtifactCache* cache,
                                     const char* cache_key,
                                     const char* file_path,
                                     ArtifactType type,
                                     const char* metadata) {
    if (!cache || !cache_key || !file_path) return NULL;

    if (!file_exists(file_path)) {
        log_error("File not found: %s", file_path);
        return NULL;
    }

    cache_lock(cache);

    /* Check if already exists */
    for (ArtifactEntry* e = cache->entries; e; e = e->next) {
        if (strcmp(e->cache_key, cache_key) == 0) {
            e->last_accessed = time(NULL);
            cache_unlock(cache);
            return e;
        }
    }

    /* Check capacity */
    if (cache->entry_count >= cache->config.max_entries ||
        cache->total_size >= cache->config.max_size_bytes) {
        /* Trigger eviction */
        cache_unlock(cache);
        artifact_cache_evict(cache, cache->config.max_size_bytes / 10);
        cache_lock(cache);
    }

    /* Create entry */
    ArtifactEntry* entry = calloc(1, sizeof(ArtifactEntry));
    if (!entry) {
        cache_unlock(cache);
        return NULL;
    }

    entry->cache_key = strdup(cache_key);
    entry->type = type;
    entry->original_path = strdup(file_path);
    entry->size_bytes = get_file_size(file_path);
    entry->created_at = time(NULL);
    entry->last_accessed = entry->created_at;
    entry->access_count = 1;

    /* Compute content hash */
    entry->content_hash = artifact_hash_file(file_path);

    /* Determine cached path */
    char cached_path[512];
    const char* key = cache_key;
    snprintf(cached_path, sizeof(cached_path), "%s/%c%c/%s",
             cache->config.cache_dir,
             key[0], key[1], cache_key);

    entry->cached_path = strdup(cached_path);

    /* Copy file to cache */
    if (!copy_file(file_path, cached_path)) {
        log_error("Failed to store artifact: %s", cache_key);
        artifact_entry_free(entry);
        cache_unlock(cache);
        return NULL;
    }

    /* Add to list */
    entry->next = cache->entries;
    cache->entries = entry;
    cache->entry_count++;
    cache->total_size += entry->size_bytes;

    (void)metadata;  /* TODO: Store metadata */

    log_debug("Stored artifact: %s (%zu bytes)", cache_key, entry->size_bytes);

    cache_unlock(cache);
    return entry;
}

ArtifactEntry* artifact_cache_store_buffer(ArtifactCache* cache,
                                            const char* cache_key,
                                            const void* data,
                                            size_t size,
                                            ArtifactType type) {
    if (!cache || !cache_key || !data || size == 0) return NULL;

    /* Write buffer to temporary file */
    char temp_path[512];
    snprintf(temp_path, sizeof(temp_path), "%s/temp_%s",
             cache->config.cache_dir, cache_key);

    FILE* f = fopen(temp_path, "wb");
    if (!f) return NULL;

    size_t written = fwrite(data, 1, size, f);
    fclose(f);

    if (written != size) {
        remove(temp_path);
        return NULL;
    }

    /* Store using file API */
    ArtifactEntry* entry = artifact_cache_store(cache, cache_key,
                                                 temp_path, type, NULL);

    remove(temp_path);
    return entry;
}

bool artifact_cache_delete(ArtifactCache* cache, const char* cache_key) {
    if (!cache || !cache_key) return false;

    cache_lock(cache);

    ArtifactEntry* prev = NULL;
    ArtifactEntry* entry = cache->entries;

    while (entry) {
        if (strcmp(entry->cache_key, cache_key) == 0) {
            /* Remove from list */
            if (prev) {
                prev->next = entry->next;
            } else {
                cache->entries = entry->next;
            }
            cache->entry_count--;
            cache->total_size -= entry->size_bytes;

            /* Delete cached file */
            if (entry->cached_path) {
                remove(entry->cached_path);
            }

            artifact_entry_free(entry);
            cache_unlock(cache);
            return true;
        }
        prev = entry;
        entry = entry->next;
    }

    cache_unlock(cache);
    return false;
}

bool artifact_cache_contains(ArtifactCache* cache, const char* cache_key) {
    return artifact_cache_lookup(cache, cache_key) != CACHE_MISS;
}

/* ============================================================
 * Remote Cache Operations (Stubs)
 * ============================================================ */

bool artifact_cache_fetch_remote(ArtifactCache* cache,
                                  const char* cache_key) {
    if (!cache || !cache_key || !cache->config.enable_remote) {
        return false;
    }

    /* TODO: Implement HTTP fetch from remote cache */
    log_debug("Remote cache fetch not implemented");
    return false;
}

bool artifact_cache_push_remote(ArtifactCache* cache,
                                 const char* cache_key) {
    if (!cache || !cache_key || !cache->config.enable_remote ||
        cache->config.remote_read_only) {
        return false;
    }

    /* TODO: Implement HTTP push to remote cache */
    log_debug("Remote cache push not implemented");
    return false;
}

int artifact_cache_sync(ArtifactCache* cache, const char* direction) {
    if (!cache || !cache->config.enable_remote) return 0;

    (void)direction;
    log_debug("Remote cache sync not implemented");
    return 0;
}

/* ============================================================
 * Cache Maintenance
 * ============================================================ */

int artifact_cache_evict(ArtifactCache* cache, size_t target_free_bytes) {
    if (!cache) return 0;

    cache_lock(cache);

    int evicted = 0;
    size_t freed = 0;

    /* Simple LRU eviction: remove oldest accessed entries */
    while (freed < target_free_bytes && cache->entries) {
        /* Find oldest entry */
        ArtifactEntry* oldest = cache->entries;
        ArtifactEntry* oldest_prev = NULL;
        ArtifactEntry* prev = NULL;

        for (ArtifactEntry* e = cache->entries; e; e = e->next) {
            if (e->last_accessed < oldest->last_accessed) {
                oldest = e;
                oldest_prev = prev;
            }
            prev = e;
        }

        /* Remove oldest */
        if (oldest_prev) {
            oldest_prev->next = oldest->next;
        } else {
            cache->entries = oldest->next;
        }

        freed += oldest->size_bytes;
        cache->entry_count--;
        cache->total_size -= oldest->size_bytes;
        evicted++;

        cache->stats.entries_evicted++;
        cache->stats.bytes_evicted += oldest->size_bytes;

        /* Delete file */
        if (oldest->cached_path) {
            remove(oldest->cached_path);
        }

        artifact_entry_free(oldest);
    }

    cache_unlock(cache);

    if (evicted > 0) {
        log_info("Evicted %d artifacts (freed %zu bytes)", evicted, freed);
    }

    return evicted;
}

int artifact_cache_cleanup(ArtifactCache* cache) {
    if (!cache) return 0;

    cache_lock(cache);

    time_t now = time(NULL);
    time_t max_age = cache->config.max_age_days * 24 * 3600;
    int removed = 0;

    ArtifactEntry* prev = NULL;
    ArtifactEntry* entry = cache->entries;

    while (entry) {
        ArtifactEntry* next = entry->next;

        if ((now - entry->created_at) > max_age) {
            /* Remove expired entry */
            if (prev) {
                prev->next = next;
            } else {
                cache->entries = next;
            }
            cache->entry_count--;
            cache->total_size -= entry->size_bytes;
            removed++;

            if (entry->cached_path) {
                remove(entry->cached_path);
            }

            artifact_entry_free(entry);
        } else {
            prev = entry;
        }

        entry = next;
    }

    cache_unlock(cache);

    if (removed > 0) {
        log_info("Cleaned up %d expired artifacts", removed);
    }

    return removed;
}

int artifact_cache_verify(ArtifactCache* cache, bool fix) {
    if (!cache) return 0;

    cache_lock(cache);

    int issues = 0;

    for (ArtifactEntry* e = cache->entries; e; e = e->next) {
        if (e->cached_path && !file_exists(e->cached_path)) {
            log_warning("Missing cached file: %s", e->cache_key);
            issues++;

            if (fix) {
                /* Mark for removal */
                e->size_bytes = 0;
            }
        }
    }

    cache_unlock(cache);
    return issues;
}

void artifact_cache_clear(ArtifactCache* cache) {
    if (!cache) return;

    cache_lock(cache);

    /* Delete all cached files */
    for (ArtifactEntry* e = cache->entries; e; e = e->next) {
        if (e->cached_path) {
            remove(e->cached_path);
        }
    }

    /* Free all entries */
    artifact_entry_list_free(cache->entries);
    cache->entries = NULL;
    cache->entry_count = 0;
    cache->total_size = 0;

    cache_unlock(cache);

    log_info("Artifact cache cleared");
}

/* ============================================================
 * Cache Information
 * ============================================================ */

CacheStats artifact_cache_get_stats(ArtifactCache* cache) {
    if (!cache) {
        CacheStats empty = {0};
        return empty;
    }
    return cache->stats;
}

void artifact_cache_reset_stats(ArtifactCache* cache) {
    if (!cache) return;
    memset(&cache->stats, 0, sizeof(CacheStats));
}

size_t artifact_cache_get_size(ArtifactCache* cache) {
    return cache ? cache->total_size : 0;
}

int artifact_cache_get_count(ArtifactCache* cache) {
    return cache ? cache->entry_count : 0;
}

double artifact_cache_get_hit_rate(ArtifactCache* cache) {
    if (!cache || cache->stats.total_lookups == 0) return 0.0;

    int hits = cache->stats.local_hits + cache->stats.remote_hits;
    return (double)hits / cache->stats.total_lookups;
}

ArtifactEntry* artifact_cache_list(ArtifactCache* cache) {
    return cache ? cache->entries : NULL;
}

/* ============================================================
 * Hashing Functions
 * ============================================================ */

char* artifact_hash_file(const char* file_path) {
    if (!file_path) return NULL;

    FILE* f = fopen(file_path, "rb");
    if (!f) return NULL;

    unsigned char hash[32] = {0};
    unsigned char buffer[8192];
    size_t bytes;

    while ((bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        for (size_t i = 0; i < bytes; i++) {
            hash[i % 32] ^= buffer[i];
            hash[(i + 1) % 32] += hash[i % 32];
        }
    }

    fclose(f);
    return bytes_to_hex(hash, 32);
}

char* artifact_hash_buffer(const void* data, size_t size) {
    if (!data || size == 0) return NULL;

    unsigned char hash[32];
    simple_hash(data, size, hash);
    return bytes_to_hex(hash, 32);
}

char* artifact_hash_combined(const char** strings, int count) {
    if (!strings || count == 0) return NULL;

    /* Concatenate all strings */
    size_t total = 0;
    for (int i = 0; i < count; i++) {
        if (strings[i]) total += strlen(strings[i]) + 1;
    }

    char* combined = malloc(total + 1);
    if (!combined) return NULL;

    char* p = combined;
    for (int i = 0; i < count; i++) {
        if (strings[i]) {
            size_t len = strlen(strings[i]);
            memcpy(p, strings[i], len);
            p += len;
            *p++ = '|';
        }
    }
    *p = '\0';

    char* hash = artifact_hash_buffer(combined, strlen(combined));
    free(combined);

    return hash;
}

/* ============================================================
 * Compression Functions (Stubs - requires zstd/gzip)
 * ============================================================ */

void* artifact_compress(const void* data, size_t size,
                        size_t* out_size,
                        const char* algo, int level) {
    (void)data;
    (void)size;
    (void)out_size;
    (void)algo;
    (void)level;

    log_debug("Compression not implemented (requires zstd library)");
    return NULL;
}

void* artifact_decompress(const void* data, size_t size,
                          size_t* out_size, const char* algo) {
    (void)data;
    (void)size;
    (void)out_size;
    (void)algo;

    log_debug("Decompression not implemented (requires zstd library)");
    return NULL;
}

/* ============================================================
 * Utility Functions
 * ============================================================ */

const char* artifact_type_name(ArtifactType type) {
    switch (type) {
        case ARTIFACT_OBJECT_FILE: return "OBJECT_FILE";
        case ARTIFACT_STATIC_LIB: return "STATIC_LIB";
        case ARTIFACT_SHARED_LIB: return "SHARED_LIB";
        case ARTIFACT_EXECUTABLE: return "EXECUTABLE";
        case ARTIFACT_PRECOMPILED_HEADER: return "PCH";
        case ARTIFACT_SOURCE_ARCHIVE: return "SOURCE_ARCHIVE";
        case ARTIFACT_OTHER: return "OTHER";
        default: return "UNKNOWN";
    }
}

const char* cache_hit_status_name(CacheHitStatus status) {
    switch (status) {
        case CACHE_MISS: return "MISS";
        case CACHE_HIT_LOCAL: return "HIT_LOCAL";
        case CACHE_HIT_REMOTE: return "HIT_REMOTE";
        case CACHE_HIT_PENDING: return "HIT_PENDING";
        default: return "UNKNOWN";
    }
}
