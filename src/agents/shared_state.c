/**
 * @file shared_state.c
 * @brief Thread-safe shared state implementation
 */

#include "cyxmake/agent_comm.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* For JSON parsing */
#include "cJSON.h"

#define INITIAL_BUCKETS 64
#define LOAD_FACTOR_THRESHOLD 0.75

/* ============================================================================
 * Hash Function (djb2)
 * ============================================================================ */

static unsigned long hash_string(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

/* ============================================================================
 * State Entry Management
 * ============================================================================ */

static StateEntry* entry_create(const char* key, const char* value) {
    StateEntry* entry = (StateEntry*)calloc(1, sizeof(StateEntry));
    if (!entry) return NULL;

    entry->key = strdup(key);
    entry->value = value ? strdup(value) : NULL;
    entry->created_at = time(NULL);
    entry->modified_at = entry->created_at;
    entry->locked_by = NULL;
    entry->next = NULL;

    return entry;
}

static void entry_free(StateEntry* entry) {
    if (!entry) return;
    free(entry->key);
    free(entry->value);
    free(entry->locked_by);
    free(entry);
}

/* ============================================================================
 * Shared State Lifecycle
 * ============================================================================ */

SharedState* shared_state_create(void) {
    SharedState* state = (SharedState*)calloc(1, sizeof(SharedState));
    if (!state) {
        log_error("Failed to allocate shared state");
        return NULL;
    }

    state->bucket_count = INITIAL_BUCKETS;
    state->buckets = (StateEntry**)calloc(state->bucket_count,
                                          sizeof(StateEntry*));
    if (!state->buckets) {
        log_error("Failed to allocate state buckets");
        free(state);
        return NULL;
    }

    if (!mutex_init(&state->mutex)) {
        log_error("Failed to initialize state mutex");
        free(state->buckets);
        free(state);
        return NULL;
    }

    state->entry_count = 0;
    state->persistence_path = NULL;
    state->dirty = false;

    log_debug("Shared state created");
    return state;
}

void shared_state_free(SharedState* state) {
    if (!state) return;

    /* Save if dirty */
    if (state->dirty && state->persistence_path) {
        shared_state_save(state);
    }

    /* Free all entries */
    for (size_t i = 0; i < state->bucket_count; i++) {
        StateEntry* entry = state->buckets[i];
        while (entry) {
            StateEntry* next = entry->next;
            entry_free(entry);
            entry = next;
        }
    }

    mutex_destroy(&state->mutex);
    free(state->buckets);
    free(state->persistence_path);
    free(state);

    log_debug("Shared state destroyed");
}

/* ============================================================================
 * Internal Lookup
 * ============================================================================ */

static StateEntry* find_entry(SharedState* state, const char* key,
                              StateEntry** prev_out) {
    if (!state || !key) return NULL;

    unsigned long hash = hash_string(key);
    size_t index = hash % state->bucket_count;

    StateEntry* prev = NULL;
    StateEntry* entry = state->buckets[index];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (prev_out) *prev_out = prev;
            return entry;
        }
        prev = entry;
        entry = entry->next;
    }

    return NULL;
}

/* ============================================================================
 * Core Operations
 * ============================================================================ */

bool shared_state_set(SharedState* state, const char* key, const char* value) {
    if (!state || !key) return false;

    mutex_lock(&state->mutex);

    StateEntry* entry = find_entry(state, key, NULL);

    if (entry) {
        /* Update existing entry */
        if (entry->locked_by) {
            log_warning("Cannot set locked key '%s'", key);
            mutex_unlock(&state->mutex);
            return false;
        }

        free(entry->value);
        entry->value = value ? strdup(value) : NULL;
        entry->modified_at = time(NULL);
    } else {
        /* Create new entry */
        entry = entry_create(key, value);
        if (!entry) {
            mutex_unlock(&state->mutex);
            return false;
        }

        unsigned long hash = hash_string(key);
        size_t index = hash % state->bucket_count;

        entry->next = state->buckets[index];
        state->buckets[index] = entry;
        state->entry_count++;
    }

    state->dirty = true;
    mutex_unlock(&state->mutex);
    return true;
}

char* shared_state_get(SharedState* state, const char* key) {
    if (!state || !key) return NULL;

    mutex_lock(&state->mutex);

    StateEntry* entry = find_entry(state, key, NULL);
    char* result = NULL;

    if (entry && entry->value) {
        result = strdup(entry->value);
    }

    mutex_unlock(&state->mutex);
    return result;
}

bool shared_state_exists(SharedState* state, const char* key) {
    if (!state || !key) return false;

    mutex_lock(&state->mutex);
    StateEntry* entry = find_entry(state, key, NULL);
    mutex_unlock(&state->mutex);

    return entry != NULL;
}

bool shared_state_delete(SharedState* state, const char* key) {
    if (!state || !key) return false;

    mutex_lock(&state->mutex);

    StateEntry* prev = NULL;
    StateEntry* entry = find_entry(state, key, &prev);

    if (!entry) {
        mutex_unlock(&state->mutex);
        return false;
    }

    if (entry->locked_by) {
        log_warning("Cannot delete locked key '%s'", key);
        mutex_unlock(&state->mutex);
        return false;
    }

    /* Remove from chain */
    unsigned long hash = hash_string(key);
    size_t index = hash % state->bucket_count;

    if (prev) {
        prev->next = entry->next;
    } else {
        state->buckets[index] = entry->next;
    }

    entry_free(entry);
    state->entry_count--;
    state->dirty = true;

    mutex_unlock(&state->mutex);
    return true;
}

/* ============================================================================
 * Locking Operations
 * ============================================================================ */

bool shared_state_lock(SharedState* state, const char* key, const char* agent_id) {
    if (!state || !key || !agent_id) return false;

    mutex_lock(&state->mutex);

    StateEntry* entry = find_entry(state, key, NULL);

    if (!entry) {
        /* Create entry if it doesn't exist */
        entry = entry_create(key, NULL);
        if (!entry) {
            mutex_unlock(&state->mutex);
            return false;
        }

        unsigned long hash = hash_string(key);
        size_t index = hash % state->bucket_count;
        entry->next = state->buckets[index];
        state->buckets[index] = entry;
        state->entry_count++;
    }

    if (entry->locked_by) {
        if (strcmp(entry->locked_by, agent_id) == 0) {
            /* Already locked by this agent */
            mutex_unlock(&state->mutex);
            return true;
        }
        /* Locked by someone else */
        mutex_unlock(&state->mutex);
        return false;
    }

    entry->locked_by = strdup(agent_id);
    entry->locked_at = time(NULL);
    state->dirty = true;

    mutex_unlock(&state->mutex);
    return true;
}

bool shared_state_trylock(SharedState* state, const char* key, const char* agent_id) {
    /* Same as lock for now since we don't have blocking lock */
    return shared_state_lock(state, key, agent_id);
}

bool shared_state_unlock(SharedState* state, const char* key, const char* agent_id) {
    if (!state || !key || !agent_id) return false;

    mutex_lock(&state->mutex);

    StateEntry* entry = find_entry(state, key, NULL);

    if (!entry || !entry->locked_by) {
        mutex_unlock(&state->mutex);
        return false;
    }

    if (strcmp(entry->locked_by, agent_id) != 0) {
        log_warning("Agent '%s' cannot unlock key '%s' locked by '%s'",
                   agent_id, key, entry->locked_by);
        mutex_unlock(&state->mutex);
        return false;
    }

    free(entry->locked_by);
    entry->locked_by = NULL;
    entry->locked_at = 0;
    state->dirty = true;

    mutex_unlock(&state->mutex);
    return true;
}

const char* shared_state_locked_by(SharedState* state, const char* key) {
    if (!state || !key) return NULL;

    mutex_lock(&state->mutex);

    StateEntry* entry = find_entry(state, key, NULL);
    const char* locker = entry ? entry->locked_by : NULL;

    mutex_unlock(&state->mutex);
    return locker;
}

/* ============================================================================
 * Enumeration
 * ============================================================================ */

char** shared_state_keys(SharedState* state, int* count) {
    if (!state || !count) {
        if (count) *count = 0;
        return NULL;
    }

    mutex_lock(&state->mutex);

    if (state->entry_count == 0) {
        *count = 0;
        mutex_unlock(&state->mutex);
        return NULL;
    }

    char** keys = (char**)malloc(state->entry_count * sizeof(char*));
    if (!keys) {
        *count = 0;
        mutex_unlock(&state->mutex);
        return NULL;
    }

    int idx = 0;
    for (size_t i = 0; i < state->bucket_count; i++) {
        StateEntry* entry = state->buckets[i];
        while (entry) {
            keys[idx++] = strdup(entry->key);
            entry = entry->next;
        }
    }

    *count = (int)state->entry_count;
    mutex_unlock(&state->mutex);
    return keys;
}

char** shared_state_keys_prefix(SharedState* state, const char* prefix, int* count) {
    if (!state || !prefix || !count) {
        if (count) *count = 0;
        return NULL;
    }

    size_t prefix_len = strlen(prefix);

    mutex_lock(&state->mutex);

    /* Count matching keys */
    int matching = 0;
    for (size_t i = 0; i < state->bucket_count; i++) {
        StateEntry* entry = state->buckets[i];
        while (entry) {
            if (strncmp(entry->key, prefix, prefix_len) == 0) {
                matching++;
            }
            entry = entry->next;
        }
    }

    if (matching == 0) {
        *count = 0;
        mutex_unlock(&state->mutex);
        return NULL;
    }

    char** keys = (char**)malloc(matching * sizeof(char*));
    if (!keys) {
        *count = 0;
        mutex_unlock(&state->mutex);
        return NULL;
    }

    int idx = 0;
    for (size_t i = 0; i < state->bucket_count; i++) {
        StateEntry* entry = state->buckets[i];
        while (entry) {
            if (strncmp(entry->key, prefix, prefix_len) == 0) {
                keys[idx++] = strdup(entry->key);
            }
            entry = entry->next;
        }
    }

    *count = matching;
    mutex_unlock(&state->mutex);
    return keys;
}

void shared_state_clear(SharedState* state) {
    if (!state) return;

    mutex_lock(&state->mutex);

    for (size_t i = 0; i < state->bucket_count; i++) {
        StateEntry* entry = state->buckets[i];
        while (entry) {
            StateEntry* next = entry->next;
            entry_free(entry);
            entry = next;
        }
        state->buckets[i] = NULL;
    }

    state->entry_count = 0;
    state->dirty = true;

    mutex_unlock(&state->mutex);
}

/* ============================================================================
 * Persistence
 * ============================================================================ */

void shared_state_set_persistence(SharedState* state, const char* path) {
    if (!state) return;

    mutex_lock(&state->mutex);
    free(state->persistence_path);
    state->persistence_path = path ? strdup(path) : NULL;
    mutex_unlock(&state->mutex);
}

bool shared_state_save(SharedState* state) {
    if (!state || !state->persistence_path) return false;

    mutex_lock(&state->mutex);

    /* Build JSON object */
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        mutex_unlock(&state->mutex);
        return false;
    }

    cJSON* entries = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "entries", entries);

    for (size_t i = 0; i < state->bucket_count; i++) {
        StateEntry* entry = state->buckets[i];
        while (entry) {
            if (entry->value) {
                cJSON_AddStringToObject(entries, entry->key, entry->value);
            }
            entry = entry->next;
        }
    }

    /* Write to file */
    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);

    if (!json_str) {
        mutex_unlock(&state->mutex);
        return false;
    }

    FILE* fp = fopen(state->persistence_path, "w");
    if (!fp) {
        log_error("Failed to open state file: %s", state->persistence_path);
        free(json_str);
        mutex_unlock(&state->mutex);
        return false;
    }

    fputs(json_str, fp);
    fclose(fp);
    free(json_str);

    state->dirty = false;
    mutex_unlock(&state->mutex);

    log_debug("Shared state saved to: %s", state->persistence_path);
    return true;
}

bool shared_state_load(SharedState* state) {
    if (!state || !state->persistence_path) return false;

    mutex_lock(&state->mutex);

    /* Read file */
    FILE* fp = fopen(state->persistence_path, "r");
    if (!fp) {
        /* File doesn't exist - not an error */
        mutex_unlock(&state->mutex);
        return true;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 0) {
        fclose(fp);
        mutex_unlock(&state->mutex);
        return true;
    }

    char* json_str = (char*)malloc(size + 1);
    if (!json_str) {
        fclose(fp);
        mutex_unlock(&state->mutex);
        return false;
    }

    size_t read = fread(json_str, 1, size, fp);
    fclose(fp);
    json_str[read] = '\0';

    /* Parse JSON */
    cJSON* root = cJSON_Parse(json_str);
    free(json_str);

    if (!root) {
        log_warning("Failed to parse state file");
        mutex_unlock(&state->mutex);
        return false;
    }

    /* Load entries */
    cJSON* entries = cJSON_GetObjectItem(root, "entries");
    if (entries && cJSON_IsObject(entries)) {
        cJSON* entry = NULL;
        cJSON_ArrayForEach(entry, entries) {
            if (cJSON_IsString(entry)) {
                /* Unlock mutex temporarily to use set function */
                mutex_unlock(&state->mutex);
                shared_state_set(state, entry->string, entry->valuestring);
                mutex_lock(&state->mutex);
            }
        }
    }

    cJSON_Delete(root);
    state->dirty = false;
    mutex_unlock(&state->mutex);

    log_debug("Shared state loaded from: %s", state->persistence_path);
    return true;
}
