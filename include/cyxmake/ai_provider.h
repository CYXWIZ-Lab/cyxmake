/**
 * @file ai_provider.h
 * @brief AI Provider abstraction layer for multiple LLM backends
 *
 * Supports:
 * - OpenAI and OpenAI-compatible APIs (GPT, Grok, OpenRouter, Groq, Together)
 * - Google Gemini
 * - Anthropic Claude
 * - Ollama (local)
 * - llama.cpp (local GGUF models)
 * - Custom providers
 */

#ifndef CYXMAKE_AI_PROVIDER_H
#define CYXMAKE_AI_PROVIDER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration for llama.cpp context */
typedef struct LLMContext LLMContext;

/* ========================================================================
 * Provider Types
 * ======================================================================== */

/**
 * AI provider types
 */
typedef enum {
    AI_PROVIDER_NONE = 0,
    AI_PROVIDER_OPENAI,      /* OpenAI and compatible (Grok, Groq, OpenRouter, etc.) */
    AI_PROVIDER_GEMINI,      /* Google Gemini */
    AI_PROVIDER_ANTHROPIC,   /* Anthropic Claude */
    AI_PROVIDER_OLLAMA,      /* Ollama local server */
    AI_PROVIDER_LLAMACPP,    /* Local llama.cpp */
    AI_PROVIDER_CUSTOM       /* Custom provider */
} AIProviderType;

/**
 * Provider status
 */
typedef enum {
    PROVIDER_STATUS_UNKNOWN,
    PROVIDER_STATUS_READY,
    PROVIDER_STATUS_LOADING,
    PROVIDER_STATUS_ERROR,
    PROVIDER_STATUS_DISABLED
} AIProviderStatus;

/* ========================================================================
 * Provider Configuration
 * ======================================================================== */

/**
 * Custom HTTP headers for provider requests
 */
typedef struct {
    char* name;
    char* value;
} AIProviderHeader;

/**
 * Provider configuration
 */
typedef struct {
    char* name;              /* Provider name (e.g., "openai", "ollama") */
    AIProviderType type;     /* Provider type */
    bool enabled;            /* Is this provider enabled */

    /* API settings */
    char* api_key;           /* API key (can be "${ENV_VAR}" for env lookup) */
    char* base_url;          /* Base URL for API */
    char* model;             /* Model name/ID */

    /* Local model settings (for llamacpp) */
    char* model_path;        /* Path to local model file */
    int context_size;        /* Context window size */
    int gpu_layers;          /* Layers to offload to GPU */
    int threads;             /* CPU threads */

    /* Request settings */
    int timeout_sec;         /* Request timeout */
    int max_tokens;          /* Max response tokens */
    float temperature;       /* Generation temperature */

    /* Custom headers */
    AIProviderHeader* headers;
    int header_count;
} AIProviderConfig;

/* ========================================================================
 * AI Request/Response
 * ======================================================================== */

/**
 * Message role for chat completions
 */
typedef enum {
    AI_ROLE_SYSTEM,
    AI_ROLE_USER,
    AI_ROLE_ASSISTANT
} AIMessageRole;

/**
 * Chat message
 */
typedef struct {
    AIMessageRole role;
    char* content;
} AIMessage;

/**
 * Tool call from AI
 */
typedef struct {
    char* id;                /* Tool call ID */
    char* name;              /* Function name */
    char* arguments;         /* JSON arguments string */
} AIToolCall;

/**
 * AI completion request
 */
typedef struct {
    AIMessage* messages;     /* Array of messages */
    int message_count;       /* Number of messages */

    /* Override provider defaults (0 = use default) */
    int max_tokens;
    float temperature;

    /* Optional parameters */
    char* system_prompt;     /* System prompt (added as first message) */
    bool stream;             /* Enable streaming (not yet supported) */

    /* Tool calling support */
    char* tools_json;        /* JSON array of tool definitions (OpenAI format) */
} AIRequest;

/**
 * AI completion response
 */
typedef struct {
    bool success;            /* Request succeeded */
    char* content;           /* Response content */
    char* error;             /* Error message if failed */

    /* Tool calls (if AI wants to use tools) */
    AIToolCall* tool_calls;  /* Array of tool calls */
    int tool_call_count;     /* Number of tool calls */

    /* Usage info (if available) */
    int prompt_tokens;
    int completion_tokens;
    int total_tokens;

    /* Timing */
    double duration_sec;
} AIResponse;

/* ========================================================================
 * Provider Instance
 * ======================================================================== */

/**
 * Provider instance (opaque)
 */
typedef struct AIProvider AIProvider;

/**
 * Health check result
 */
typedef struct {
    bool healthy;            /* Provider is operational */
    int latency_ms;          /* Response time in milliseconds */
    char* message;           /* Status message or error */
    AIProviderStatus status; /* Current provider status */
} AIHealthCheckResult;

/**
 * Provider function table (vtable)
 */
typedef struct {
    /* Initialize provider */
    bool (*init)(AIProvider* provider);

    /* Shutdown provider */
    void (*shutdown)(AIProvider* provider);

    /* Check if provider is ready */
    bool (*is_ready)(AIProvider* provider);

    /* Send completion request */
    AIResponse* (*complete)(AIProvider* provider, const AIRequest* request);

    /* Get provider status */
    AIProviderStatus (*get_status)(AIProvider* provider);

    /* Get last error message */
    const char* (*get_error)(AIProvider* provider);

    /* Health check (optional) */
    AIHealthCheckResult* (*health_check)(AIProvider* provider);
} AIProviderVTable;

/**
 * Provider instance structure
 */
struct AIProvider {
    AIProviderConfig config;
    AIProviderVTable* vtable;
    AIProviderStatus status;
    char* last_error;
    void* internal;          /* Provider-specific data */
};

/* ========================================================================
 * Provider Registry
 * ======================================================================== */

/**
 * Provider registry (manages multiple providers)
 */
typedef struct AIProviderRegistry AIProviderRegistry;

/**
 * Create provider registry
 * @return New registry (caller must free with ai_registry_free)
 */
AIProviderRegistry* ai_registry_create(void);

/**
 * Free provider registry
 * @param registry Registry to free
 */
void ai_registry_free(AIProviderRegistry* registry);

/**
 * Load providers from config file
 * @param registry Provider registry
 * @param config_path Path to cyxmake.toml (NULL for default locations)
 * @return Number of providers loaded, -1 on error
 */
int ai_registry_load_config(AIProviderRegistry* registry, const char* config_path);

/**
 * Add a provider to the registry
 * @param registry Provider registry
 * @param config Provider configuration
 * @return true on success
 */
bool ai_registry_add(AIProviderRegistry* registry, const AIProviderConfig* config);

/**
 * Get provider by name
 * @param registry Provider registry
 * @param name Provider name
 * @return Provider or NULL if not found
 */
AIProvider* ai_registry_get(AIProviderRegistry* registry, const char* name);

/**
 * Get the default provider
 * @param registry Provider registry
 * @return Default provider or NULL
 */
AIProvider* ai_registry_get_default(AIProviderRegistry* registry);

/**
 * Set the default provider
 * @param registry Provider registry
 * @param name Provider name
 * @return true on success
 */
bool ai_registry_set_default(AIProviderRegistry* registry, const char* name);

/**
 * Get list of available providers
 * @param registry Provider registry
 * @param names Output array of provider names (caller provides array)
 * @param max_names Maximum names to return
 * @return Number of providers
 */
int ai_registry_list(AIProviderRegistry* registry, const char** names, int max_names);

/**
 * Get number of enabled providers
 * @param registry Provider registry
 * @return Number of enabled providers
 */
int ai_registry_count(AIProviderRegistry* registry);

/* ========================================================================
 * Provider Operations
 * ======================================================================== */

/**
 * Create a provider from configuration
 * @param config Provider configuration
 * @return New provider (caller must free with ai_provider_free)
 */
AIProvider* ai_provider_create(const AIProviderConfig* config);

/**
 * Free a provider
 * @param provider Provider to free
 */
void ai_provider_free(AIProvider* provider);

/**
 * Initialize a provider (connect, load model, etc.)
 * @param provider Provider to initialize
 * @return true on success
 */
bool ai_provider_init(AIProvider* provider);

/**
 * Check if provider is ready for requests
 * @param provider Provider to check
 * @return true if ready
 */
bool ai_provider_is_ready(AIProvider* provider);

/**
 * Get provider status
 * @param provider Provider
 * @return Provider status
 */
AIProviderStatus ai_provider_status(AIProvider* provider);

/**
 * Get last error message
 * @param provider Provider
 * @return Error message or NULL
 */
const char* ai_provider_error(AIProvider* provider);

/**
 * Send a completion request
 * @param provider Provider to use
 * @param request Request to send
 * @return Response (caller must free with ai_response_free)
 */
AIResponse* ai_provider_complete(AIProvider* provider, const AIRequest* request);

/**
 * Simple query (single user message)
 * @param provider Provider to use
 * @param prompt User prompt
 * @param max_tokens Maximum tokens (0 for default)
 * @return Response content (caller must free) or NULL on error
 */
char* ai_provider_query(AIProvider* provider, const char* prompt, int max_tokens);

/**
 * Query with system prompt
 * @param provider Provider to use
 * @param system_prompt System prompt
 * @param user_prompt User prompt
 * @param max_tokens Maximum tokens (0 for default)
 * @return Response content (caller must free) or NULL on error
 */
char* ai_provider_query_with_system(AIProvider* provider,
                                     const char* system_prompt,
                                     const char* user_prompt,
                                     int max_tokens);

/* ========================================================================
 * Request/Response Helpers
 * ======================================================================== */

/**
 * Create a request
 * @return New request (caller must free with ai_request_free)
 */
AIRequest* ai_request_create(void);

/**
 * Add a message to a request
 * @param request Request
 * @param role Message role
 * @param content Message content
 */
void ai_request_add_message(AIRequest* request, AIMessageRole role, const char* content);

/**
 * Set system prompt for request
 * @param request Request
 * @param system_prompt System prompt
 */
void ai_request_set_system(AIRequest* request, const char* system_prompt);

/**
 * Free a request
 * @param request Request to free
 */
void ai_request_free(AIRequest* request);

/**
 * Free a response
 * @param response Response to free
 */
void ai_response_free(AIResponse* response);

/* ========================================================================
 * Configuration Helpers
 * ======================================================================== */

/**
 * Create default provider config
 * @param name Provider name
 * @param type Provider type
 * @return New config (caller must free with ai_config_free)
 */
AIProviderConfig* ai_config_create(const char* name, AIProviderType type);

/**
 * Free provider config
 * @param config Config to free
 */
void ai_config_free(AIProviderConfig* config);

/**
 * Set API key (handles environment variable expansion)
 * @param config Config
 * @param api_key API key or "${ENV_VAR}"
 */
void ai_config_set_api_key(AIProviderConfig* config, const char* api_key);

/**
 * Add custom header
 * @param config Config
 * @param name Header name
 * @param value Header value
 */
void ai_config_add_header(AIProviderConfig* config, const char* name, const char* value);

/**
 * Get provider type from string
 * @param type_str Type string ("openai", "gemini", etc.)
 * @return Provider type
 */
AIProviderType ai_provider_type_from_string(const char* type_str);

/**
 * Get string name for provider type
 * @param type Provider type
 * @return Type name string
 */
const char* ai_provider_type_to_string(AIProviderType type);

/**
 * Get string name for provider status
 * @param status Provider status
 * @return Status name string
 */
const char* ai_provider_status_to_string(AIProviderStatus status);

/* ========================================================================
 * Quick Setup Helpers
 * ======================================================================== */

/**
 * Create OpenAI provider with API key
 * @param api_key OpenAI API key
 * @param model Model name (NULL for default "gpt-4o-mini")
 * @return New provider (caller must free)
 */
AIProvider* ai_provider_openai(const char* api_key, const char* model);

/**
 * Create Ollama provider
 * @param model Model name (NULL for default "llama2")
 * @param base_url Base URL (NULL for default "http://localhost:11434")
 * @return New provider (caller must free)
 */
AIProvider* ai_provider_ollama(const char* model, const char* base_url);

/**
 * Create Gemini provider with API key
 * @param api_key Gemini API key
 * @param model Model name (NULL for default "gemini-1.5-flash")
 * @return New provider (caller must free)
 */
AIProvider* ai_provider_gemini(const char* api_key, const char* model);

/**
 * Create Anthropic provider with API key
 * @param api_key Anthropic API key
 * @param model Model name (NULL for default "claude-3-haiku-20240307")
 * @return New provider (caller must free)
 */
AIProvider* ai_provider_anthropic(const char* api_key, const char* model);

/**
 * Create local llama.cpp provider
 * @param model_path Path to GGUF model file
 * @return New provider (caller must free)
 */
AIProvider* ai_provider_llamacpp(const char* model_path);

/**
 * Create provider from environment
 * Checks for API keys in environment variables and creates appropriate provider
 * Priority: OPENAI_API_KEY, ANTHROPIC_API_KEY, GEMINI_API_KEY, then Ollama
 * @return New provider or NULL if no provider available
 */
AIProvider* ai_provider_from_env(void);

/* ========================================================================
 * HTTP Support Detection
 * ======================================================================== */

/**
 * Check if HTTP support is available
 * HTTP support is required for cloud AI providers (OpenAI, Anthropic, Gemini, Ollama)
 * @return true if HTTP (CURL) is compiled in
 */
bool ai_provider_has_http_support(void);

/**
 * Get information about HTTP support status
 * @return Human-readable string describing HTTP support status
 */
const char* ai_provider_http_status_message(void);

/**
 * Check if a provider type requires HTTP support
 * @param type Provider type to check
 * @return true if provider requires HTTP (cloud providers)
 */
bool ai_provider_type_requires_http(AIProviderType type);

/* ========================================================================
 * Retry and Fallback Support
 * ======================================================================== */

/**
 * Retry configuration for AI provider requests
 */
typedef struct {
    int max_retries;           /* Maximum retry attempts (default: 3) */
    int initial_delay_ms;      /* Initial delay between retries in ms (default: 1000) */
    int max_delay_ms;          /* Maximum delay between retries in ms (default: 30000) */
    float backoff_multiplier;  /* Exponential backoff multiplier (default: 2.0) */
    bool retry_on_timeout;     /* Retry on timeout errors (default: true) */
    bool retry_on_rate_limit;  /* Retry on rate limit errors (default: true) */
    bool retry_on_server_error; /* Retry on 5xx errors (default: true) */
} AIRetryConfig;

/**
 * Get default retry configuration
 * @return Default retry config (3 retries, 1s initial delay, 2x backoff)
 */
AIRetryConfig ai_retry_config_default(void);

/**
 * Send completion request with retry support
 * @param provider Provider to use
 * @param request Request to send
 * @param retry_config Retry configuration (NULL for defaults)
 * @return Response (caller must free with ai_response_free)
 */
AIResponse* ai_provider_complete_with_retry(AIProvider* provider,
                                             const AIRequest* request,
                                             const AIRetryConfig* retry_config);

/**
 * Send completion request with fallback to alternative providers
 * Tries providers in order: primary, fallback from registry, then all enabled providers
 * @param registry Provider registry
 * @param request Request to send
 * @param primary_provider Primary provider name (NULL for default)
 * @param retry_config Retry configuration for each provider (NULL for defaults)
 * @return Response (caller must free with ai_response_free)
 */
AIResponse* ai_registry_complete_with_fallback(AIProviderRegistry* registry,
                                                const AIRequest* request,
                                                const char* primary_provider,
                                                const AIRetryConfig* retry_config);

/**
 * Set the fallback provider in registry
 * @param registry Provider registry
 * @param name Fallback provider name
 * @return true on success
 */
bool ai_registry_set_fallback(AIProviderRegistry* registry, const char* name);

/**
 * Get the fallback provider from registry
 * @param registry Provider registry
 * @return Fallback provider or NULL
 */
AIProvider* ai_registry_get_fallback(AIProviderRegistry* registry);

/* ========================================================================
 * Health Check Support
 * ======================================================================== */

/**
 * Free health check result
 * @param result Result to free
 */
void ai_health_check_free(AIHealthCheckResult* result);

/**
 * Perform health check on a provider
 * Sends a minimal request to verify the provider is operational
 * @param provider Provider to check
 * @return Health check result (caller must free with ai_health_check_free)
 */
AIHealthCheckResult* ai_provider_health_check(AIProvider* provider);

/**
 * Check health of all providers in registry
 * @param registry Provider registry
 * @param results Output array of health check results (caller provides array)
 * @param names Output array of provider names (caller provides array)
 * @param max_count Maximum number of providers to check
 * @return Number of providers checked
 */
int ai_registry_health_check_all(AIProviderRegistry* registry,
                                  AIHealthCheckResult** results,
                                  const char** names,
                                  int max_count);

/**
 * Find first healthy provider in registry
 * @param registry Provider registry
 * @return First healthy provider or NULL if none healthy
 */
AIProvider* ai_registry_find_healthy(AIProviderRegistry* registry);

/**
 * Print health status report for all providers
 * @param registry Provider registry
 */
void ai_registry_print_health_report(AIProviderRegistry* registry);

/* ========================================================================
 * Offline Mode Support
 * ======================================================================== */

/**
 * Offline mode status
 */
typedef enum {
    AI_NETWORK_ONLINE,       /* Network available, providers working */
    AI_NETWORK_DEGRADED,     /* Some providers failing, fallbacks in use */
    AI_NETWORK_OFFLINE,      /* All cloud providers unavailable */
    AI_NETWORK_LOCAL_ONLY    /* Only local llama.cpp available */
} AINetworkStatus;

/**
 * Offline mode configuration
 */
typedef struct {
    bool enabled;                    /* Enable offline mode graceful degradation */
    bool use_cached_responses;       /* Use cached responses when available */
    bool provide_generic_help;       /* Provide generic help responses */
    int cache_ttl_sec;               /* Cache time-to-live in seconds */
    const char* cache_path;          /* Path to response cache file */
} AIOfflineModeConfig;

/**
 * Get default offline mode configuration
 * @return Default config with offline mode enabled
 */
AIOfflineModeConfig ai_offline_config_default(void);

/**
 * Check network/provider status
 * @param registry Provider registry
 * @return Network status
 */
AINetworkStatus ai_registry_check_network_status(AIProviderRegistry* registry);

/**
 * Get human-readable network status message
 * @param status Network status
 * @return Status message string
 */
const char* ai_network_status_message(AINetworkStatus status);

/**
 * Complete request with offline mode support
 * Falls back to cached/generic responses when providers unavailable
 * @param registry Provider registry
 * @param request Request to send
 * @param offline_config Offline mode configuration (NULL for defaults)
 * @param retry_config Retry configuration (NULL for defaults)
 * @return Response (caller must free with ai_response_free)
 */
AIResponse* ai_registry_complete_offline_aware(AIProviderRegistry* registry,
                                                const AIRequest* request,
                                                const AIOfflineModeConfig* offline_config,
                                                const AIRetryConfig* retry_config);

/**
 * Generate offline fallback response
 * Returns a helpful response explaining the offline situation
 * @param request Original request
 * @param status Network status
 * @return Response with offline message (caller must free)
 */
AIResponse* ai_generate_offline_response(const AIRequest* request,
                                          AINetworkStatus status);

/* ========================================================================
 * GPU Acceleration Support
 * ======================================================================== */

/**
 * GPU backend type
 */
typedef enum {
    AI_GPU_NONE = 0,     /* CPU only */
    AI_GPU_CUDA,         /* NVIDIA CUDA */
    AI_GPU_VULKAN,       /* Vulkan (cross-platform) */
    AI_GPU_METAL,        /* Apple Metal */
    AI_GPU_OPENCL        /* OpenCL */
} AIGPUBackend;

/**
 * GPU information
 */
typedef struct {
    AIGPUBackend backend;        /* Active GPU backend */
    bool available;              /* GPU is available for use */
    char* device_name;           /* GPU device name (if available) */
    int memory_mb;               /* GPU memory in MB (if available) */
    int recommended_layers;      /* Recommended layers to offload */
} AIGPUInfo;

/**
 * Check which GPU backend is compiled in
 * @return Active GPU backend type
 */
AIGPUBackend ai_get_gpu_backend(void);

/**
 * Get GPU backend name as string
 * @param backend GPU backend type
 * @return Backend name string
 */
const char* ai_gpu_backend_name(AIGPUBackend backend);

/**
 * Check if GPU acceleration is available
 * @return true if GPU support is compiled in
 */
bool ai_has_gpu_support(void);

/**
 * Get GPU information
 * @return GPU info (caller must free with ai_gpu_info_free)
 */
AIGPUInfo* ai_get_gpu_info(void);

/**
 * Free GPU info
 * @param info GPU info to free
 */
void ai_gpu_info_free(AIGPUInfo* info);

/**
 * Get recommended number of GPU layers for a model
 * Based on available GPU memory and model size
 * @param model_size_mb Approximate model size in MB
 * @return Recommended number of layers to offload (0 for CPU only)
 */
int ai_recommend_gpu_layers(int model_size_mb);

/**
 * Print GPU status report
 */
void ai_print_gpu_status(void);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_AI_PROVIDER_H */
