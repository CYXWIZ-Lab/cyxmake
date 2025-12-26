/**
 * @file auth.h
 * @brief Authentication for distributed builds
 *
 * Provides token-based authentication for coordinator-worker communication.
 * Supports pre-shared tokens, challenge-response, and token revocation.
 */

#ifndef CYXMAKE_DISTRIBUTED_AUTH_H
#define CYXMAKE_DISTRIBUTED_AUTH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Authentication Method
 * ============================================================ */

typedef enum {
    AUTH_METHOD_NONE = 0,         /* No authentication (development only) */
    AUTH_METHOD_TOKEN,            /* Pre-shared token */
    AUTH_METHOD_CHALLENGE,        /* Challenge-response */
    AUTH_METHOD_MTLS              /* Mutual TLS (certificate-based) */
} AuthMethod;

/* ============================================================
 * Authentication Result
 * ============================================================ */

typedef enum {
    AUTH_RESULT_SUCCESS = 0,
    AUTH_RESULT_INVALID_TOKEN,
    AUTH_RESULT_EXPIRED_TOKEN,
    AUTH_RESULT_REVOKED_TOKEN,
    AUTH_RESULT_CHALLENGE_FAILED,
    AUTH_RESULT_NOT_AUTHORIZED,
    AUTH_RESULT_INTERNAL_ERROR
} AuthResult;

/* ============================================================
 * Token Types
 * Note: Named AuthTokenType to avoid conflict with Windows SDK's TokenType
 * ============================================================ */

typedef enum {
    AUTH_TOKEN_TYPE_WORKER = 0,   /* Worker registration token */
    AUTH_TOKEN_TYPE_ADMIN,        /* Administrative access */
    AUTH_TOKEN_TYPE_CLIENT,       /* Build client token */
    AUTH_TOKEN_TYPE_SESSION       /* Temporary session token */
} AuthTokenType;

/* ============================================================
 * Token Structure
 * ============================================================ */

typedef struct AuthToken {
    char* token_id;               /* Unique token identifier */
    char* token_value;            /* The actual token string */
    AuthTokenType type;           /* Token type */
    char* issuer;                 /* Who issued this token */
    char* subject;                /* Who this token is for */
    time_t issued_at;             /* When issued */
    time_t expires_at;            /* When expires (0 = never) */
    bool revoked;                 /* Has been revoked */
    char* revocation_reason;      /* Why revoked */

    /* Permissions */
    bool can_register;            /* Can register as worker */
    bool can_submit_jobs;         /* Can submit build jobs */
    bool can_admin;               /* Administrative access */

    /* Metadata */
    char* description;            /* Human-readable description */
    char** allowed_hosts;         /* Allowed source hosts (NULL = any) */
    int allowed_hosts_count;

    struct AuthToken* next;       /* For token list */
} AuthToken;

/* ============================================================
 * Challenge-Response
 * ============================================================ */

typedef struct AuthChallenge {
    char* challenge_id;           /* Unique challenge ID */
    char* challenge_data;         /* Random challenge data (base64) */
    char* expected_response;      /* Expected response (internal) */
    time_t created_at;            /* When created */
    time_t expires_at;            /* When expires */
    bool used;                    /* Has been used */
} AuthChallenge;

/* ============================================================
 * Authentication Context
 * ============================================================ */

typedef struct AuthContext AuthContext;

/* ============================================================
 * Configuration
 * ============================================================ */

typedef struct AuthConfig {
    AuthMethod method;            /* Authentication method to use */

    /* Token settings */
    int default_token_ttl_sec;    /* Default token TTL (0 = forever) */
    int max_tokens;               /* Maximum stored tokens */
    bool allow_token_refresh;     /* Allow token refresh */

    /* Challenge settings */
    int challenge_ttl_sec;        /* Challenge validity (default: 60) */
    int max_challenge_attempts;   /* Max failed attempts before lockout */
    int lockout_duration_sec;     /* Lockout duration after failures */

    /* Secret for HMAC operations */
    char* hmac_secret;            /* Secret key for token signing */
    size_t hmac_secret_len;       /* Secret length */

    /* Token storage */
    char* token_file_path;        /* Path to token storage file */
} AuthConfig;

/* ============================================================
 * Context API
 * ============================================================ */

/**
 * Create authentication context
 */
AuthContext* auth_context_create(const AuthConfig* config);

/**
 * Free authentication context
 */
void auth_context_free(AuthContext* ctx);

/**
 * Load tokens from file
 */
bool auth_context_load_tokens(AuthContext* ctx, const char* path);

/**
 * Save tokens to file
 */
bool auth_context_save_tokens(AuthContext* ctx, const char* path);

/* ============================================================
 * Token Management API
 * ============================================================ */

/**
 * Generate a new token
 * @param ctx Auth context
 * @param type Token type
 * @param subject Who this token is for (e.g., worker name)
 * @param ttl_sec Time to live (0 = use default, -1 = never expires)
 * @return New token or NULL on error
 */
AuthToken* auth_token_generate(AuthContext* ctx,
                                AuthTokenType type,
                                const char* subject,
                                int ttl_sec);

/**
 * Validate a token
 * @param ctx Auth context
 * @param token_value The token string to validate
 * @param source_host Source host for validation (optional)
 * @return Validation result
 */
AuthResult auth_token_validate(AuthContext* ctx,
                                const char* token_value,
                                const char* source_host);

/**
 * Get token by value
 */
AuthToken* auth_token_lookup(AuthContext* ctx, const char* token_value);

/**
 * Get token by ID
 */
AuthToken* auth_token_lookup_by_id(AuthContext* ctx, const char* token_id);

/**
 * Revoke a token
 */
bool auth_token_revoke(AuthContext* ctx,
                       const char* token_id,
                       const char* reason);

/**
 * Refresh a token (extend expiration)
 */
AuthToken* auth_token_refresh(AuthContext* ctx,
                               const char* token_value,
                               int new_ttl_sec);

/**
 * List all tokens
 */
AuthToken* auth_token_list(AuthContext* ctx);

/**
 * Get token count
 */
int auth_token_count(AuthContext* ctx);

/**
 * Remove expired tokens
 */
int auth_token_cleanup_expired(AuthContext* ctx);

/* ============================================================
 * Challenge-Response API
 * ============================================================ */

/**
 * Create a new challenge
 * @param ctx Auth context
 * @return Challenge or NULL on error
 */
AuthChallenge* auth_challenge_create(AuthContext* ctx);

/**
 * Verify challenge response
 * @param ctx Auth context
 * @param challenge_id The challenge ID
 * @param response The response to verify
 * @return Verification result
 */
AuthResult auth_challenge_verify(AuthContext* ctx,
                                  const char* challenge_id,
                                  const char* response);

/**
 * Free a challenge
 */
void auth_challenge_free(AuthChallenge* challenge);

/* ============================================================
 * Token Utility Functions
 * ============================================================ */

/**
 * Generate a random token string
 * @param length Token length in bytes (will be base64 encoded)
 * @return Allocated token string or NULL
 */
char* auth_generate_random_token(size_t length);

/**
 * Hash a token for storage
 */
char* auth_hash_token(const char* token_value, const char* salt);

/**
 * Create HMAC signature
 */
char* auth_create_hmac(const void* data, size_t len,
                       const void* key, size_t key_len);

/**
 * Verify HMAC signature
 */
bool auth_verify_hmac(const void* data, size_t len,
                      const char* signature,
                      const void* key, size_t key_len);

/* ============================================================
 * Token Structure Management
 * ============================================================ */

/**
 * Free a token structure
 */
void auth_token_free(AuthToken* token);

/**
 * Free a token list
 */
void auth_token_list_free(AuthToken* tokens);

/**
 * Clone a token
 */
AuthToken* auth_token_clone(const AuthToken* token);

/* ============================================================
 * Configuration Helpers
 * ============================================================ */

/**
 * Create default configuration
 */
AuthConfig auth_config_default(void);

/**
 * Free configuration resources
 */
void auth_config_free(AuthConfig* config);

/**
 * Load configuration from TOML
 */
bool auth_config_load(AuthConfig* config, const char* path);

/* ============================================================
 * Result Helpers
 * ============================================================ */

/**
 * Get human-readable result message
 */
const char* auth_result_message(AuthResult result);

/**
 * Get token type name
 */
const char* auth_token_type_name(AuthTokenType type);

/**
 * Get auth method name
 */
const char* auth_method_name(AuthMethod method);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_DISTRIBUTED_AUTH_H */
