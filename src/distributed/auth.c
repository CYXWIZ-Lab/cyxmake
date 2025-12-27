/**
 * @file auth.c
 * @brief Authentication implementation for distributed builds
 *
 * Provides secure token-based authentication with HMAC signing,
 * challenge-response support, and token lifecycle management.
 */

#include "cyxmake/distributed/auth.h"
#include "cyxmake/logger.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
#include "cyxmake/threading.h"
#endif

/* ============================================================
 * Constants
 * ============================================================ */

#define DEFAULT_TOKEN_TTL_SEC (7 * 24 * 3600)  /* 7 days */
#define DEFAULT_CHALLENGE_TTL_SEC 60
#define DEFAULT_MAX_TOKENS 1000
#define DEFAULT_LOCKOUT_DURATION_SEC 300
#define DEFAULT_MAX_CHALLENGE_ATTEMPTS 5
#define TOKEN_RANDOM_BYTES 32
#define CHALLENGE_RANDOM_BYTES 32
#define MAX_CHALLENGES 100

/* Base64 encoding table */
static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* ============================================================
 * Internal Structures
 * ============================================================ */

struct AuthContext {
    AuthConfig config;
    AuthToken* tokens;            /* Token list */
    int token_count;
    AuthChallenge* challenges[MAX_CHALLENGES];
    int challenge_count;

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    MutexHandle mutex;
#endif
};

/* ============================================================
 * Helper Functions
 * ============================================================ */

static void context_lock(AuthContext* ctx) {
#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    mutex_lock(&ctx->mutex);
#else
    (void)ctx;
#endif
}

static void context_unlock(AuthContext* ctx) {
#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    mutex_unlock(&ctx->mutex);
#else
    (void)ctx;
#endif
}

static void get_random_bytes(void* buf, size_t len) {
    unsigned char* p = (unsigned char*)buf;

#ifdef _WIN32
    /* Windows: Use RtlGenRandom */
    HMODULE advapi = LoadLibraryA("advapi32.dll");
    if (advapi) {
        typedef BOOLEAN (WINAPI *RtlGenRandomFunc)(PVOID, ULONG);
        RtlGenRandomFunc RtlGenRandom =
            (RtlGenRandomFunc)GetProcAddress(advapi, "SystemFunction036");
        if (RtlGenRandom) {
            RtlGenRandom(buf, (ULONG)len);
            FreeLibrary(advapi);
            return;
        }
        FreeLibrary(advapi);
    }
#else
    /* Unix: Use /dev/urandom */
    FILE* f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t read = fread(buf, 1, len, f);
        fclose(f);
        if (read == len) {
            return;
        }
    }
#endif

    /* Fallback: Use time and rand (not cryptographically secure!) */
    log_warning("Using insecure random fallback - install OpenSSL for better security");
    srand((unsigned int)time(NULL) ^ (unsigned int)(size_t)buf);
    for (size_t i = 0; i < len; i++) {
        p[i] = (unsigned char)(rand() & 0xFF);
    }
}

static char* base64_encode(const void* data, size_t len) {
    const unsigned char* input = (const unsigned char*)data;
    size_t output_len = 4 * ((len + 2) / 3) + 1;
    char* output = malloc(output_len);
    if (!output) return NULL;

    size_t i, j;
    for (i = 0, j = 0; i < len;) {
        uint32_t octet_a = i < len ? input[i++] : 0;
        uint32_t octet_b = i < len ? input[i++] : 0;
        uint32_t octet_c = i < len ? input[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        output[j++] = base64_table[(triple >> 18) & 0x3F];
        output[j++] = base64_table[(triple >> 12) & 0x3F];
        output[j++] = base64_table[(triple >> 6) & 0x3F];
        output[j++] = base64_table[triple & 0x3F];
    }

    /* Padding */
    size_t mod = len % 3;
    if (mod > 0) {
        output[j - 1] = '=';
        if (mod == 1) {
            output[j - 2] = '=';
        }
    }

    output[j] = '\0';
    return output;
}

static char* generate_uuid(void) {
    unsigned char bytes[16];
    get_random_bytes(bytes, sizeof(bytes));

    /* Set version 4 */
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    /* Set variant */
    bytes[8] = (bytes[8] & 0x3F) | 0x80;

    char* uuid = malloc(37);
    if (!uuid) return NULL;

    snprintf(uuid, 37,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             bytes[0], bytes[1], bytes[2], bytes[3],
             bytes[4], bytes[5], bytes[6], bytes[7],
             bytes[8], bytes[9], bytes[10], bytes[11],
             bytes[12], bytes[13], bytes[14], bytes[15]);

    return uuid;
}

/* Simple HMAC-like hash (not cryptographically secure without OpenSSL) */
static char* simple_hash(const char* data, const char* key) {
    if (!data || !key) return NULL;

    /* XOR-based hash (for demonstration - use OpenSSL in production) */
    size_t data_len = strlen(data);
    size_t key_len = strlen(key);
    unsigned char hash[32] = {0};

    for (size_t i = 0; i < data_len; i++) {
        hash[i % 32] ^= (unsigned char)data[i];
        hash[(i + 1) % 32] ^= (unsigned char)key[i % key_len];
        hash[(i + 2) % 32] ^= (unsigned char)(i & 0xFF);
    }

    return base64_encode(hash, 32);
}

/* ============================================================
 * Configuration
 * ============================================================ */

AuthConfig auth_config_default(void) {
    AuthConfig config = {
        .method = AUTH_METHOD_TOKEN,
        .default_token_ttl_sec = DEFAULT_TOKEN_TTL_SEC,
        .max_tokens = DEFAULT_MAX_TOKENS,
        .allow_token_refresh = true,
        .challenge_ttl_sec = DEFAULT_CHALLENGE_TTL_SEC,
        .max_challenge_attempts = DEFAULT_MAX_CHALLENGE_ATTEMPTS,
        .lockout_duration_sec = DEFAULT_LOCKOUT_DURATION_SEC,
        .hmac_secret = NULL,
        .hmac_secret_len = 0,
        .token_file_path = NULL
    };
    return config;
}

void auth_config_free(AuthConfig* config) {
    if (!config) return;
    free(config->hmac_secret);
    free(config->token_file_path);
    config->hmac_secret = NULL;
    config->token_file_path = NULL;
}

/* ============================================================
 * Context API
 * ============================================================ */

AuthContext* auth_context_create(const AuthConfig* config) {
    AuthContext* ctx = calloc(1, sizeof(AuthContext));
    if (!ctx) {
        log_error("Failed to allocate auth context");
        return NULL;
    }

    if (config) {
        ctx->config = *config;
        /* Deep copy strings */
        if (config->hmac_secret) {
            ctx->config.hmac_secret = malloc(config->hmac_secret_len);
            if (ctx->config.hmac_secret) {
                memcpy(ctx->config.hmac_secret, config->hmac_secret,
                       config->hmac_secret_len);
            }
        }
        if (config->token_file_path) {
            ctx->config.token_file_path = strdup(config->token_file_path);
        }
    } else {
        ctx->config = auth_config_default();
    }

    /* Generate default secret if none provided */
    if (!ctx->config.hmac_secret) {
        ctx->config.hmac_secret = malloc(32);
        if (ctx->config.hmac_secret) {
            get_random_bytes(ctx->config.hmac_secret, 32);
            ctx->config.hmac_secret_len = 32;
            log_debug("Generated random HMAC secret");
        }
    }

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    if (!mutex_init(&ctx->mutex)) {
        log_error("Failed to create auth context mutex");
        auth_context_free(ctx);
        return NULL;
    }
#endif

    log_debug("Auth context created (method: %s)",
              auth_method_name(ctx->config.method));

    return ctx;
}

void auth_context_free(AuthContext* ctx) {
    if (!ctx) return;

    /* Free all tokens */
    auth_token_list_free(ctx->tokens);

    /* Free all challenges */
    for (int i = 0; i < ctx->challenge_count; i++) {
        auth_challenge_free(ctx->challenges[i]);
    }

    /* Free config */
    auth_config_free(&ctx->config);

#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    mutex_destroy(&ctx->mutex);
#endif

    free(ctx);
    log_debug("Auth context freed");
}

/* ============================================================
 * Token Management
 * ============================================================ */

void auth_token_free(AuthToken* token) {
    if (!token) return;

    free(token->token_id);
    free(token->token_value);
    free(token->issuer);
    free(token->subject);
    free(token->revocation_reason);
    free(token->description);

    if (token->allowed_hosts) {
        for (int i = 0; i < token->allowed_hosts_count; i++) {
            free(token->allowed_hosts[i]);
        }
        free(token->allowed_hosts);
    }

    free(token);
}

void auth_token_list_free(AuthToken* tokens) {
    while (tokens) {
        AuthToken* next = tokens->next;
        auth_token_free(tokens);
        tokens = next;
    }
}

AuthToken* auth_token_clone(const AuthToken* token) {
    if (!token) return NULL;

    AuthToken* clone = calloc(1, sizeof(AuthToken));
    if (!clone) return NULL;

    clone->type = token->type;
    clone->issued_at = token->issued_at;
    clone->expires_at = token->expires_at;
    clone->revoked = token->revoked;
    clone->can_register = token->can_register;
    clone->can_submit_jobs = token->can_submit_jobs;
    clone->can_admin = token->can_admin;

    if (token->token_id) clone->token_id = strdup(token->token_id);
    if (token->token_value) clone->token_value = strdup(token->token_value);
    if (token->issuer) clone->issuer = strdup(token->issuer);
    if (token->subject) clone->subject = strdup(token->subject);
    if (token->revocation_reason) clone->revocation_reason = strdup(token->revocation_reason);
    if (token->description) clone->description = strdup(token->description);

    if (token->allowed_hosts && token->allowed_hosts_count > 0) {
        clone->allowed_hosts = malloc(sizeof(char*) * token->allowed_hosts_count);
        if (clone->allowed_hosts) {
            clone->allowed_hosts_count = token->allowed_hosts_count;
            for (int i = 0; i < token->allowed_hosts_count; i++) {
                clone->allowed_hosts[i] = strdup(token->allowed_hosts[i]);
            }
        }
    }

    return clone;
}

AuthToken* auth_token_generate(AuthContext* ctx,
                                AuthTokenType type,
                                const char* subject,
                                int ttl_sec) {
    if (!ctx) return NULL;

    context_lock(ctx);

    /* Check capacity */
    if (ctx->token_count >= ctx->config.max_tokens) {
        log_warning("Token limit reached (%d)", ctx->config.max_tokens);
        context_unlock(ctx);
        return NULL;
    }

    AuthToken* token = calloc(1, sizeof(AuthToken));
    if (!token) {
        context_unlock(ctx);
        return NULL;
    }

    /* Generate token ID and value */
    token->token_id = generate_uuid();
    token->token_value = auth_generate_random_token(TOKEN_RANDOM_BYTES);

    if (!token->token_id || !token->token_value) {
        auth_token_free(token);
        context_unlock(ctx);
        return NULL;
    }

    /* Set properties */
    token->type = type;
    token->subject = subject ? strdup(subject) : NULL;
    token->issuer = strdup("cyxmake-coordinator");
    token->issued_at = time(NULL);

    /* Set expiration */
    if (ttl_sec == 0) {
        ttl_sec = ctx->config.default_token_ttl_sec;
    }
    if (ttl_sec > 0) {
        token->expires_at = token->issued_at + ttl_sec;
    } else {
        token->expires_at = 0;  /* Never expires */
    }

    /* Set default permissions based on type */
    switch (type) {
        case AUTH_TOKEN_TYPE_WORKER:
            token->can_register = true;
            token->can_submit_jobs = false;
            token->can_admin = false;
            break;
        case AUTH_TOKEN_TYPE_CLIENT:
            token->can_register = false;
            token->can_submit_jobs = true;
            token->can_admin = false;
            break;
        case AUTH_TOKEN_TYPE_ADMIN:
            token->can_register = true;
            token->can_submit_jobs = true;
            token->can_admin = true;
            break;
        case AUTH_TOKEN_TYPE_SESSION:
            token->can_register = false;
            token->can_submit_jobs = true;
            token->can_admin = false;
            break;
    }

    /* Add to list */
    token->next = ctx->tokens;
    ctx->tokens = token;
    ctx->token_count++;

    log_info("Token generated: %s (type: %s, subject: %s)",
             token->token_id,
             auth_token_type_name(type),
             subject ? subject : "none");

    context_unlock(ctx);

    return token;
}

AuthResult auth_token_validate(AuthContext* ctx,
                                const char* token_value,
                                const char* source_host) {
    if (!ctx || !token_value) {
        return AUTH_RESULT_INVALID_TOKEN;
    }

    if (ctx->config.method == AUTH_METHOD_NONE) {
        return AUTH_RESULT_SUCCESS;
    }

    context_lock(ctx);

    AuthToken* token = NULL;
    for (AuthToken* t = ctx->tokens; t; t = t->next) {
        if (strcmp(t->token_value, token_value) == 0) {
            token = t;
            break;
        }
    }

    if (!token) {
        context_unlock(ctx);
        log_warning("Token validation failed: invalid token");
        return AUTH_RESULT_INVALID_TOKEN;
    }

    /* Check if revoked */
    if (token->revoked) {
        context_unlock(ctx);
        log_warning("Token validation failed: revoked");
        return AUTH_RESULT_REVOKED_TOKEN;
    }

    /* Check expiration */
    if (token->expires_at > 0 && time(NULL) > token->expires_at) {
        context_unlock(ctx);
        log_warning("Token validation failed: expired");
        return AUTH_RESULT_EXPIRED_TOKEN;
    }

    /* Check source host if restricted */
    if (source_host && token->allowed_hosts && token->allowed_hosts_count > 0) {
        bool host_allowed = false;
        for (int i = 0; i < token->allowed_hosts_count; i++) {
            if (strcmp(source_host, token->allowed_hosts[i]) == 0) {
                host_allowed = true;
                break;
            }
        }
        if (!host_allowed) {
            context_unlock(ctx);
            log_warning("Token validation failed: host not allowed");
            return AUTH_RESULT_NOT_AUTHORIZED;
        }
    }

    context_unlock(ctx);
    return AUTH_RESULT_SUCCESS;
}

AuthToken* auth_token_lookup(AuthContext* ctx, const char* token_value) {
    if (!ctx || !token_value) return NULL;

    context_lock(ctx);

    for (AuthToken* t = ctx->tokens; t; t = t->next) {
        if (strcmp(t->token_value, token_value) == 0) {
            context_unlock(ctx);
            return t;
        }
    }

    context_unlock(ctx);
    return NULL;
}

AuthToken* auth_token_lookup_by_id(AuthContext* ctx, const char* token_id) {
    if (!ctx || !token_id) return NULL;

    context_lock(ctx);

    for (AuthToken* t = ctx->tokens; t; t = t->next) {
        if (strcmp(t->token_id, token_id) == 0) {
            context_unlock(ctx);
            return t;
        }
    }

    context_unlock(ctx);
    return NULL;
}

bool auth_token_revoke(AuthContext* ctx,
                       const char* token_id,
                       const char* reason) {
    if (!ctx || !token_id) return false;

    context_lock(ctx);

    for (AuthToken* t = ctx->tokens; t; t = t->next) {
        if (strcmp(t->token_id, token_id) == 0) {
            t->revoked = true;
            free(t->revocation_reason);
            t->revocation_reason = reason ? strdup(reason) : NULL;

            log_info("Token revoked: %s (%s)", token_id,
                     reason ? reason : "no reason");

            context_unlock(ctx);
            return true;
        }
    }

    context_unlock(ctx);
    log_warning("Token not found for revocation: %s", token_id);
    return false;
}

AuthToken* auth_token_refresh(AuthContext* ctx,
                               const char* token_value,
                               int new_ttl_sec) {
    if (!ctx || !token_value) return NULL;

    if (!ctx->config.allow_token_refresh) {
        log_warning("Token refresh not allowed");
        return NULL;
    }

    context_lock(ctx);

    AuthToken* token = NULL;
    for (AuthToken* t = ctx->tokens; t; t = t->next) {
        if (strcmp(t->token_value, token_value) == 0) {
            token = t;
            break;
        }
    }

    if (!token || token->revoked) {
        context_unlock(ctx);
        return NULL;
    }

    /* Update expiration */
    if (new_ttl_sec <= 0) {
        new_ttl_sec = ctx->config.default_token_ttl_sec;
    }
    token->expires_at = time(NULL) + new_ttl_sec;

    log_info("Token refreshed: %s (new TTL: %d sec)", token->token_id, new_ttl_sec);

    context_unlock(ctx);
    return token;
}

AuthToken* auth_token_list(AuthContext* ctx) {
    return ctx ? ctx->tokens : NULL;
}

int auth_token_count(AuthContext* ctx) {
    return ctx ? ctx->token_count : 0;
}

int auth_token_cleanup_expired(AuthContext* ctx) {
    if (!ctx) return 0;

    context_lock(ctx);

    int removed = 0;
    time_t now = time(NULL);
    AuthToken* prev = NULL;
    AuthToken* token = ctx->tokens;

    while (token) {
        AuthToken* next = token->next;

        if (token->expires_at > 0 && now > token->expires_at) {
            /* Remove expired token */
            if (prev) {
                prev->next = next;
            } else {
                ctx->tokens = next;
            }
            ctx->token_count--;
            removed++;

            log_debug("Removed expired token: %s", token->token_id);
            auth_token_free(token);
        } else {
            prev = token;
        }

        token = next;
    }

    context_unlock(ctx);

    if (removed > 0) {
        log_info("Cleaned up %d expired tokens", removed);
    }

    return removed;
}

/* ============================================================
 * Challenge-Response API
 * ============================================================ */

AuthChallenge* auth_challenge_create(AuthContext* ctx) {
    if (!ctx) return NULL;

    context_lock(ctx);

    /* Clean up old challenges first */
    time_t now = time(NULL);
    for (int i = 0; i < ctx->challenge_count; i++) {
        if (ctx->challenges[i]->expires_at < now || ctx->challenges[i]->used) {
            auth_challenge_free(ctx->challenges[i]);
            /* Move last to this position */
            ctx->challenges[i] = ctx->challenges[ctx->challenge_count - 1];
            ctx->challenge_count--;
            i--;  /* Recheck this position */
        }
    }

    /* Check capacity */
    if (ctx->challenge_count >= MAX_CHALLENGES) {
        log_warning("Challenge limit reached");
        context_unlock(ctx);
        return NULL;
    }

    AuthChallenge* challenge = calloc(1, sizeof(AuthChallenge));
    if (!challenge) {
        context_unlock(ctx);
        return NULL;
    }

    /* Generate challenge */
    challenge->challenge_id = generate_uuid();
    unsigned char random_data[CHALLENGE_RANDOM_BYTES];
    get_random_bytes(random_data, sizeof(random_data));
    challenge->challenge_data = base64_encode(random_data, sizeof(random_data));

    if (!challenge->challenge_id || !challenge->challenge_data) {
        auth_challenge_free(challenge);
        context_unlock(ctx);
        return NULL;
    }

    /* Compute expected response */
    challenge->expected_response = simple_hash(challenge->challenge_data,
                                                ctx->config.hmac_secret);

    challenge->created_at = now;
    challenge->expires_at = now + ctx->config.challenge_ttl_sec;
    challenge->used = false;

    /* Store challenge */
    ctx->challenges[ctx->challenge_count++] = challenge;

    log_debug("Challenge created: %s", challenge->challenge_id);

    context_unlock(ctx);
    return challenge;
}

AuthResult auth_challenge_verify(AuthContext* ctx,
                                  const char* challenge_id,
                                  const char* response) {
    if (!ctx || !challenge_id || !response) {
        return AUTH_RESULT_CHALLENGE_FAILED;
    }

    context_lock(ctx);

    AuthChallenge* challenge = NULL;
    int challenge_idx = -1;

    for (int i = 0; i < ctx->challenge_count; i++) {
        if (strcmp(ctx->challenges[i]->challenge_id, challenge_id) == 0) {
            challenge = ctx->challenges[i];
            challenge_idx = i;
            break;
        }
    }

    if (!challenge) {
        context_unlock(ctx);
        log_warning("Challenge not found: %s", challenge_id);
        return AUTH_RESULT_CHALLENGE_FAILED;
    }

    /* Check if expired */
    if (time(NULL) > challenge->expires_at) {
        context_unlock(ctx);
        log_warning("Challenge expired: %s", challenge_id);
        return AUTH_RESULT_EXPIRED_TOKEN;
    }

    /* Check if already used */
    if (challenge->used) {
        context_unlock(ctx);
        log_warning("Challenge already used: %s", challenge_id);
        return AUTH_RESULT_CHALLENGE_FAILED;
    }

    /* Mark as used regardless of result */
    challenge->used = true;

    /* Verify response */
    bool valid = (challenge->expected_response &&
                  strcmp(response, challenge->expected_response) == 0);

    context_unlock(ctx);

    if (valid) {
        log_debug("Challenge verified: %s", challenge_id);
        return AUTH_RESULT_SUCCESS;
    } else {
        log_warning("Challenge verification failed: %s", challenge_id);
        return AUTH_RESULT_CHALLENGE_FAILED;
    }
}

void auth_challenge_free(AuthChallenge* challenge) {
    if (!challenge) return;

    free(challenge->challenge_id);
    free(challenge->challenge_data);
    free(challenge->expected_response);
    free(challenge);
}

/* ============================================================
 * Token Utility Functions
 * ============================================================ */

char* auth_generate_random_token(size_t length) {
    unsigned char* bytes = malloc(length);
    if (!bytes) return NULL;

    get_random_bytes(bytes, length);
    char* token = base64_encode(bytes, length);
    free(bytes);

    return token;
}

char* auth_hash_token(const char* token_value, const char* salt) {
    if (!token_value) return NULL;

    /* Combine token and salt */
    size_t token_len = strlen(token_value);
    size_t salt_len = salt ? strlen(salt) : 0;
    char* combined = malloc(token_len + salt_len + 1);
    if (!combined) return NULL;

    strcpy(combined, token_value);
    if (salt) {
        strcat(combined, salt);
    }

    char* hash = simple_hash(combined, "cyxmake-token-hash");
    free(combined);

    return hash;
}

char* auth_create_hmac(const void* data, size_t len,
                       const void* key, size_t key_len) {
    if (!data || !key) return NULL;

    /* Simple HMAC approximation */
    char* data_str = base64_encode(data, len);
    char* key_str = base64_encode(key, key_len);

    if (!data_str || !key_str) {
        free(data_str);
        free(key_str);
        return NULL;
    }

    char* hmac = simple_hash(data_str, key_str);

    free(data_str);
    free(key_str);

    return hmac;
}

bool auth_verify_hmac(const void* data, size_t len,
                      const char* signature,
                      const void* key, size_t key_len) {
    if (!data || !signature || !key) return false;

    char* computed = auth_create_hmac(data, len, key, key_len);
    if (!computed) return false;

    bool valid = (strcmp(computed, signature) == 0);
    free(computed);

    return valid;
}

/* ============================================================
 * Result Helpers
 * ============================================================ */

const char* auth_result_message(AuthResult result) {
    switch (result) {
        case AUTH_RESULT_SUCCESS: return "Authentication successful";
        case AUTH_RESULT_INVALID_TOKEN: return "Invalid token";
        case AUTH_RESULT_EXPIRED_TOKEN: return "Token expired";
        case AUTH_RESULT_REVOKED_TOKEN: return "Token revoked";
        case AUTH_RESULT_CHALLENGE_FAILED: return "Challenge verification failed";
        case AUTH_RESULT_NOT_AUTHORIZED: return "Not authorized";
        case AUTH_RESULT_INTERNAL_ERROR: return "Internal error";
        default: return "Unknown error";
    }
}

const char* auth_token_type_name(AuthTokenType type) {
    switch (type) {
        case AUTH_TOKEN_TYPE_WORKER: return "WORKER";
        case AUTH_TOKEN_TYPE_ADMIN: return "ADMIN";
        case AUTH_TOKEN_TYPE_CLIENT: return "CLIENT";
        case AUTH_TOKEN_TYPE_SESSION: return "SESSION";
        default: return "UNKNOWN";
    }
}

const char* auth_method_name(AuthMethod method) {
    switch (method) {
        case AUTH_METHOD_NONE: return "NONE";
        case AUTH_METHOD_TOKEN: return "TOKEN";
        case AUTH_METHOD_CHALLENGE: return "CHALLENGE";
        case AUTH_METHOD_MTLS: return "MTLS";
        default: return "UNKNOWN";
    }
}

/* ============================================================
 * Token Storage (Simple JSON format)
 * ============================================================ */

bool auth_context_load_tokens(AuthContext* ctx, const char* path) {
    if (!ctx || !path) return false;

    /* TODO: Implement JSON token loading */
    log_debug("Token loading not yet implemented");
    (void)path;

    return true;
}

bool auth_context_save_tokens(AuthContext* ctx, const char* path) {
    if (!ctx || !path) return false;

    /* TODO: Implement JSON token saving */
    log_debug("Token saving not yet implemented");
    (void)path;

    return true;
}

bool auth_config_load(AuthConfig* config, const char* path) {
    if (!config || !path) return false;

    /* TODO: Implement TOML config loading */
    log_debug("Auth config loading not yet implemented");
    (void)path;

    return true;
}
