/**
 * @file ai_provider.c
 * @brief AI Provider abstraction layer implementation
 */

#include "cyxmake/ai_provider.h"
#include "cyxmake/logger.h"
#include "tomlc99/toml.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define strdup _strdup
#define getcwd _getcwd
#else
#include <sys/time.h>
#include <unistd.h>
#endif

/* Maximum providers in registry */
#define MAX_PROVIDERS 16

/* ========================================================================
 * Provider Registry Implementation
 * ======================================================================== */

struct AIProviderRegistry {
    AIProvider* providers[MAX_PROVIDERS];
    int count;
    char* default_provider;
    char* fallback_provider;
};

AIProviderRegistry* ai_registry_create(void) {
    AIProviderRegistry* registry = calloc(1, sizeof(AIProviderRegistry));
    return registry;
}

void ai_registry_free(AIProviderRegistry* registry) {
    if (!registry) return;

    for (int i = 0; i < registry->count; i++) {
        ai_provider_free(registry->providers[i]);
    }

    free(registry->default_provider);
    free(registry->fallback_provider);
    free(registry);
}

bool ai_registry_add(AIProviderRegistry* registry, const AIProviderConfig* config) {
    if (!registry || !config || registry->count >= MAX_PROVIDERS) {
        return false;
    }

    AIProvider* provider = ai_provider_create(config);
    if (!provider) {
        return false;
    }

    registry->providers[registry->count++] = provider;

    /* Set as default if it's the first enabled provider */
    if (!registry->default_provider && config->enabled) {
        registry->default_provider = strdup(config->name);
    }

    return true;
}

AIProvider* ai_registry_get(AIProviderRegistry* registry, const char* name) {
    if (!registry || !name) return NULL;

    for (int i = 0; i < registry->count; i++) {
        if (registry->providers[i] &&
            strcmp(registry->providers[i]->config.name, name) == 0) {
            return registry->providers[i];
        }
    }
    return NULL;
}

AIProvider* ai_registry_get_default(AIProviderRegistry* registry) {
    if (!registry) return NULL;

    if (registry->default_provider) {
        AIProvider* provider = ai_registry_get(registry, registry->default_provider);
        if (provider && provider->config.enabled) {
            return provider;
        }
    }

    /* Fall back to first enabled provider */
    for (int i = 0; i < registry->count; i++) {
        if (registry->providers[i] && registry->providers[i]->config.enabled) {
            return registry->providers[i];
        }
    }

    return NULL;
}

bool ai_registry_set_default(AIProviderRegistry* registry, const char* name) {
    if (!registry || !name) return false;

    AIProvider* provider = ai_registry_get(registry, name);
    if (!provider) return false;

    free(registry->default_provider);
    registry->default_provider = strdup(name);
    return true;
}

int ai_registry_list(AIProviderRegistry* registry, const char** names, int max_names) {
    if (!registry || !names) return 0;

    int count = 0;
    for (int i = 0; i < registry->count && count < max_names; i++) {
        if (registry->providers[i]) {
            names[count++] = registry->providers[i]->config.name;
        }
    }
    return count;
}

int ai_registry_count(AIProviderRegistry* registry) {
    if (!registry) return 0;

    int count = 0;
    for (int i = 0; i < registry->count; i++) {
        if (registry->providers[i] && registry->providers[i]->config.enabled) {
            count++;
        }
    }
    return count;
}

/* ========================================================================
 * TOML Configuration Loading
 * ======================================================================== */

/**
 * Load providers from TOML config file
 */
int ai_registry_load_config(AIProviderRegistry* registry, const char* config_path) {
    if (!registry) return -1;

    /* Get current working directory for relative paths */
    char cwd[1024];
    char full_path[2048];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        cwd[0] = '.';
        cwd[1] = '\0';
    }

    /* Debug output removed - config loading works */

    /* Try to find config file - count non-NULL paths */
    const char* paths[4];
    int path_count = 0;

    if (config_path) paths[path_count++] = config_path;
    paths[path_count++] = "cyxmake.toml";
    paths[path_count++] = ".cyxmake/config.toml";

    FILE* fp = NULL;
    const char* used_path = NULL;

    for (int i = 0; i < path_count; i++) {
        if (paths[i]) {
            /* Try absolute path first */
            fp = fopen(paths[i], "r");
            if (fp) {
                used_path = paths[i];
                log_debug("Found config at direct path: %s", paths[i]);
                break;
            }

            /* Try relative to CWD */
#ifdef _WIN32
            snprintf(full_path, sizeof(full_path), "%s\\%s", cwd, paths[i]);
#else
            snprintf(full_path, sizeof(full_path), "%s/%s", cwd, paths[i]);
#endif
            fp = fopen(full_path, "r");
            if (fp) {
                used_path = full_path;
                log_debug("Found config at CWD path: %s", full_path);
                break;
            }
        }
    }

    if (!fp) {
        log_info("No config file found (tried CWD: %s)", cwd);
        return 0;
    }

    log_info("Loading AI config from: %s", used_path);

    /* Parse TOML */
    char errbuf[256];
    toml_table_t* conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!conf) {
        log_error("Failed to parse config: %s", errbuf);
        return -1;
    }

    /* Get [ai] section */
    toml_table_t* ai = toml_table_in(conf, "ai");
    if (!ai) {
        log_debug("No [ai] section in config");
        toml_free(conf);
        return 0;
    }

    /* Get default provider */
    toml_datum_t default_provider = toml_string_in(ai, "default_provider");
    if (default_provider.ok) {
        free(registry->default_provider);
        registry->default_provider = strdup(default_provider.u.s);
        free(default_provider.u.s);
    }

    /* Get fallback provider */
    toml_datum_t fallback_provider = toml_string_in(ai, "fallback_provider");
    if (fallback_provider.ok) {
        free(registry->fallback_provider);
        registry->fallback_provider = strdup(fallback_provider.u.s);
        free(fallback_provider.u.s);
    }

    /* Get global settings */
    int global_timeout = 120;  /* 2 minutes default for AI responses */
    int global_max_tokens = 2048;
    float global_temperature = 0.7f;

    toml_datum_t timeout = toml_int_in(ai, "timeout");
    if (timeout.ok) global_timeout = (int)timeout.u.i;

    toml_datum_t max_tokens = toml_int_in(ai, "max_tokens");
    if (max_tokens.ok) global_max_tokens = (int)max_tokens.u.i;

    toml_datum_t temperature = toml_double_in(ai, "temperature");
    if (temperature.ok) global_temperature = (float)temperature.u.d;

    /* Get [ai.providers] section */
    toml_table_t* providers = toml_table_in(ai, "providers");
    if (!providers) {
        log_debug("No [ai.providers] section in config");
        toml_free(conf);
        return 0;
    }

    /* Iterate over providers */
    int loaded = 0;
    for (int i = 0; ; i++) {
        const char* name = toml_key_in(providers, i);
        if (!name) break;

        toml_table_t* provider = toml_table_in(providers, name);
        if (!provider) continue;

        /* Check if enabled */
        toml_datum_t enabled = toml_bool_in(provider, "enabled");
        if (enabled.ok && !enabled.u.b) {
            log_debug("Provider '%s' is disabled, skipping", name);
            continue;
        }

        /* Get provider type */
        toml_datum_t type_str = toml_string_in(provider, "type");
        if (!type_str.ok) {
            log_warning("Provider '%s' missing type, skipping", name);
            continue;
        }

        AIProviderType type = ai_provider_type_from_string(type_str.u.s);
        free(type_str.u.s);

        if (type == AI_PROVIDER_NONE) {
            log_warning("Provider '%s' has unknown type, skipping", name);
            continue;
        }

        /* Create config */
        AIProviderConfig* config = ai_config_create(name, type);
        if (!config) continue;

        config->enabled = !enabled.ok || enabled.u.b;
        config->timeout_sec = global_timeout;
        config->max_tokens = global_max_tokens;
        config->temperature = global_temperature;

        /* Get API key */
        toml_datum_t api_key = toml_string_in(provider, "api_key");
        if (api_key.ok) {
            ai_config_set_api_key(config, api_key.u.s);
            free(api_key.u.s);
        }

        /* Get base URL */
        toml_datum_t base_url = toml_string_in(provider, "base_url");
        if (base_url.ok) {
            config->base_url = strdup(base_url.u.s);
            free(base_url.u.s);
        }

        /* Get model */
        toml_datum_t model = toml_string_in(provider, "model");
        if (model.ok) {
            config->model = strdup(model.u.s);
            free(model.u.s);
        }

        /* Get model path (for llama.cpp) */
        toml_datum_t model_path = toml_string_in(provider, "model_path");
        if (model_path.ok) {
            config->model_path = strdup(model_path.u.s);
            free(model_path.u.s);
        }

        /* Get context size */
        toml_datum_t context_size = toml_int_in(provider, "context_size");
        if (context_size.ok) config->context_size = (int)context_size.u.i;

        /* Get GPU layers */
        toml_datum_t gpu_layers = toml_int_in(provider, "gpu_layers");
        if (gpu_layers.ok) config->gpu_layers = (int)gpu_layers.u.i;

        /* Get threads */
        toml_datum_t threads = toml_int_in(provider, "threads");
        if (threads.ok) config->threads = (int)threads.u.i;

        /* Get custom headers */
        toml_table_t* headers = toml_table_in(provider, "headers");
        if (headers) {
            for (int j = 0; ; j++) {
                const char* header_name = toml_key_in(headers, j);
                if (!header_name) break;

                toml_datum_t header_value = toml_string_in(headers, header_name);
                if (header_value.ok) {
                    ai_config_add_header(config, header_name, header_value.u.s);
                    free(header_value.u.s);
                }
            }
        }

        /* Add provider to registry */
        if (ai_registry_add(registry, config)) {
            log_info("Loaded provider: %s (%s)", name, ai_provider_type_to_string(type));
            loaded++;

            /* Initialize the provider */
            AIProvider* prov = ai_registry_get(registry, name);
            if (prov) {
                ai_provider_init(prov);
            }
        }

        ai_config_free(config);
    }

    /* Set default provider if specified */
    if (registry->default_provider) {
        ai_registry_set_default(registry, registry->default_provider);
    }

    toml_free(conf);
    log_info("Loaded %d AI providers", loaded);
    return loaded;
}

/* ========================================================================
 * Configuration Helpers
 * ======================================================================== */

AIProviderConfig* ai_config_create(const char* name, AIProviderType type) {
    AIProviderConfig* config = calloc(1, sizeof(AIProviderConfig));
    if (!config) return NULL;

    config->name = name ? strdup(name) : NULL;
    config->type = type;
    config->enabled = true;
    config->timeout_sec = 120;  /* 2 minutes default */
    config->max_tokens = 2048;
    config->temperature = 0.7f;
    config->context_size = 4096;
    config->threads = 4;

    return config;
}

void ai_config_free(AIProviderConfig* config) {
    if (!config) return;

    free(config->name);
    free(config->api_key);
    free(config->base_url);
    free(config->model);
    free(config->model_path);

    if (config->headers) {
        for (int i = 0; i < config->header_count; i++) {
            free(config->headers[i].name);
            free(config->headers[i].value);
        }
        free(config->headers);
    }

    free(config);
}

/* Expand environment variables in string */
static char* expand_env_var(const char* str) {
    if (!str) return NULL;

    /* Check for ${VAR} pattern */
    if (strncmp(str, "${", 2) == 0 && str[strlen(str) - 1] == '}') {
        /* Extract variable name */
        size_t len = strlen(str) - 3;
        char* var_name = malloc(len + 1);
        strncpy(var_name, str + 2, len);
        var_name[len] = '\0';

        /* Get environment variable */
        const char* value = getenv(var_name);
        free(var_name);

        if (value) {
            return strdup(value);
        }
        return NULL;
    }

    return strdup(str);
}

void ai_config_set_api_key(AIProviderConfig* config, const char* api_key) {
    if (!config) return;

    free(config->api_key);
    config->api_key = expand_env_var(api_key);
}

void ai_config_add_header(AIProviderConfig* config, const char* name, const char* value) {
    if (!config || !name || !value) return;

    config->headers = realloc(config->headers,
                               (config->header_count + 1) * sizeof(AIProviderHeader));
    config->headers[config->header_count].name = strdup(name);
    config->headers[config->header_count].value = strdup(value);
    config->header_count++;
}

AIProviderType ai_provider_type_from_string(const char* type_str) {
    if (!type_str) return AI_PROVIDER_NONE;

    if (strcmp(type_str, "openai") == 0) return AI_PROVIDER_OPENAI;
    if (strcmp(type_str, "gemini") == 0) return AI_PROVIDER_GEMINI;
    if (strcmp(type_str, "anthropic") == 0) return AI_PROVIDER_ANTHROPIC;
    if (strcmp(type_str, "ollama") == 0) return AI_PROVIDER_OLLAMA;
    if (strcmp(type_str, "llamacpp") == 0) return AI_PROVIDER_LLAMACPP;
    if (strcmp(type_str, "custom") == 0) return AI_PROVIDER_CUSTOM;

    return AI_PROVIDER_NONE;
}

const char* ai_provider_type_to_string(AIProviderType type) {
    switch (type) {
        case AI_PROVIDER_OPENAI:    return "openai";
        case AI_PROVIDER_GEMINI:    return "gemini";
        case AI_PROVIDER_ANTHROPIC: return "anthropic";
        case AI_PROVIDER_OLLAMA:    return "ollama";
        case AI_PROVIDER_LLAMACPP:  return "llamacpp";
        case AI_PROVIDER_CUSTOM:    return "custom";
        default:                    return "none";
    }
}

const char* ai_provider_status_to_string(AIProviderStatus status) {
    switch (status) {
        case PROVIDER_STATUS_READY:    return "ready";
        case PROVIDER_STATUS_LOADING:  return "loading";
        case PROVIDER_STATUS_ERROR:    return "error";
        case PROVIDER_STATUS_DISABLED: return "disabled";
        default:                       return "unknown";
    }
}

/* ========================================================================
 * Request/Response Helpers
 * ======================================================================== */

AIRequest* ai_request_create(void) {
    return calloc(1, sizeof(AIRequest));
}

void ai_request_add_message(AIRequest* request, AIMessageRole role, const char* content) {
    if (!request || !content) return;

    request->messages = realloc(request->messages,
                                 (request->message_count + 1) * sizeof(AIMessage));
    request->messages[request->message_count].role = role;
    request->messages[request->message_count].content = strdup(content);
    request->message_count++;
}

void ai_request_set_system(AIRequest* request, const char* system_prompt) {
    if (!request) return;
    free(request->system_prompt);
    request->system_prompt = system_prompt ? strdup(system_prompt) : NULL;
}

void ai_request_free(AIRequest* request) {
    if (!request) return;

    for (int i = 0; i < request->message_count; i++) {
        free(request->messages[i].content);
    }
    free(request->messages);
    free(request->system_prompt);
    free(request);
}

void ai_response_free(AIResponse* response) {
    if (!response) return;
    free(response->content);
    free(response->error);
    free(response);
}

/* ========================================================================
 * Provider Base Implementation
 * ======================================================================== */

/* Forward declarations for provider implementations */
static AIProviderVTable* get_openai_vtable(void);
static AIProviderVTable* get_ollama_vtable(void);
static AIProviderVTable* get_gemini_vtable(void);
static AIProviderVTable* get_anthropic_vtable(void);
static AIProviderVTable* get_llamacpp_vtable(void);

AIProvider* ai_provider_create(const AIProviderConfig* config) {
    if (!config) return NULL;

    AIProvider* provider = calloc(1, sizeof(AIProvider));
    if (!provider) return NULL;

    /* Copy configuration */
    provider->config.name = config->name ? strdup(config->name) : NULL;
    provider->config.type = config->type;
    provider->config.enabled = config->enabled;
    provider->config.api_key = config->api_key ? strdup(config->api_key) : NULL;
    provider->config.base_url = config->base_url ? strdup(config->base_url) : NULL;
    provider->config.model = config->model ? strdup(config->model) : NULL;
    provider->config.model_path = config->model_path ? strdup(config->model_path) : NULL;
    provider->config.context_size = config->context_size;
    provider->config.gpu_layers = config->gpu_layers;
    provider->config.threads = config->threads;
    provider->config.timeout_sec = config->timeout_sec > 0 ? config->timeout_sec : 60;
    provider->config.max_tokens = config->max_tokens > 0 ? config->max_tokens : 1024;
    provider->config.temperature = config->temperature;

    /* Copy headers */
    if (config->headers && config->header_count > 0) {
        provider->config.headers = malloc(config->header_count * sizeof(AIProviderHeader));
        provider->config.header_count = config->header_count;
        for (int i = 0; i < config->header_count; i++) {
            provider->config.headers[i].name = strdup(config->headers[i].name);
            provider->config.headers[i].value = strdup(config->headers[i].value);
        }
    }

    /* Set vtable based on provider type */
    switch (config->type) {
        case AI_PROVIDER_OPENAI:
            provider->vtable = get_openai_vtable();
            break;
        case AI_PROVIDER_OLLAMA:
            provider->vtable = get_ollama_vtable();
            break;
        case AI_PROVIDER_GEMINI:
            provider->vtable = get_gemini_vtable();
            break;
        case AI_PROVIDER_ANTHROPIC:
            provider->vtable = get_anthropic_vtable();
            break;
        case AI_PROVIDER_LLAMACPP:
            provider->vtable = get_llamacpp_vtable();
            break;
        default:
            provider->vtable = get_openai_vtable(); /* Default to OpenAI-compatible */
            break;
    }

    provider->status = config->enabled ? PROVIDER_STATUS_UNKNOWN : PROVIDER_STATUS_DISABLED;

    return provider;
}

void ai_provider_free(AIProvider* provider) {
    if (!provider) return;

    /* Shutdown if needed */
    if (provider->vtable && provider->vtable->shutdown) {
        provider->vtable->shutdown(provider);
    }

    /* Free config */
    free(provider->config.name);
    free(provider->config.api_key);
    free(provider->config.base_url);
    free(provider->config.model);
    free(provider->config.model_path);

    if (provider->config.headers) {
        for (int i = 0; i < provider->config.header_count; i++) {
            free(provider->config.headers[i].name);
            free(provider->config.headers[i].value);
        }
        free(provider->config.headers);
    }

    free(provider->last_error);
    free(provider);
}

bool ai_provider_init(AIProvider* provider) {
    if (!provider || !provider->vtable || !provider->vtable->init) {
        return false;
    }
    return provider->vtable->init(provider);
}

bool ai_provider_is_ready(AIProvider* provider) {
    if (!provider) return false;
    if (provider->vtable && provider->vtable->is_ready) {
        return provider->vtable->is_ready(provider);
    }
    return provider->status == PROVIDER_STATUS_READY;
}

AIProviderStatus ai_provider_status(AIProvider* provider) {
    if (!provider) return PROVIDER_STATUS_UNKNOWN;
    if (provider->vtable && provider->vtable->get_status) {
        return provider->vtable->get_status(provider);
    }
    return provider->status;
}

const char* ai_provider_error(AIProvider* provider) {
    if (!provider) return NULL;
    if (provider->vtable && provider->vtable->get_error) {
        return provider->vtable->get_error(provider);
    }
    return provider->last_error;
}

AIResponse* ai_provider_complete(AIProvider* provider, const AIRequest* request) {
    if (!provider || !request) return NULL;

    if (!provider->vtable || !provider->vtable->complete) {
        AIResponse* response = calloc(1, sizeof(AIResponse));
        response->success = false;
        response->error = strdup("Provider not implemented");
        return response;
    }

    return provider->vtable->complete(provider, request);
}

char* ai_provider_query(AIProvider* provider, const char* prompt, int max_tokens) {
    if (!provider || !prompt) return NULL;

    AIRequest* request = ai_request_create();
    if (!request) return NULL;

    ai_request_add_message(request, AI_ROLE_USER, prompt);
    if (max_tokens > 0) {
        request->max_tokens = max_tokens;
    }

    AIResponse* response = ai_provider_complete(provider, request);
    ai_request_free(request);

    if (!response) return NULL;

    char* result = NULL;
    if (response->success && response->content) {
        result = strdup(response->content);
    }

    ai_response_free(response);
    return result;
}

char* ai_provider_query_with_system(AIProvider* provider,
                                     const char* system_prompt,
                                     const char* user_prompt,
                                     int max_tokens) {
    if (!provider || !user_prompt) return NULL;

    AIRequest* request = ai_request_create();
    if (!request) return NULL;

    if (system_prompt) {
        ai_request_add_message(request, AI_ROLE_SYSTEM, system_prompt);
    }
    ai_request_add_message(request, AI_ROLE_USER, user_prompt);
    if (max_tokens > 0) {
        request->max_tokens = max_tokens;
    }

    AIResponse* response = ai_provider_complete(provider, request);
    ai_request_free(request);

    if (!response) return NULL;

    char* result = NULL;
    if (response->success && response->content) {
        result = strdup(response->content);
    }

    ai_response_free(response);
    return result;
}

/* ========================================================================
 * Quick Setup Helpers
 * ======================================================================== */

AIProvider* ai_provider_openai(const char* api_key, const char* model) {
    AIProviderConfig* config = ai_config_create("openai", AI_PROVIDER_OPENAI);
    if (!config) return NULL;

    ai_config_set_api_key(config, api_key);
    config->base_url = strdup("https://api.openai.com/v1");
    config->model = strdup(model ? model : "gpt-4o-mini");

    AIProvider* provider = ai_provider_create(config);
    ai_config_free(config);

    if (provider) {
        ai_provider_init(provider);
    }

    return provider;
}

AIProvider* ai_provider_ollama(const char* model, const char* base_url) {
    AIProviderConfig* config = ai_config_create("ollama", AI_PROVIDER_OLLAMA);
    if (!config) return NULL;

    config->base_url = strdup(base_url ? base_url : "http://localhost:11434");
    config->model = strdup(model ? model : "llama2");

    AIProvider* provider = ai_provider_create(config);
    ai_config_free(config);

    if (provider) {
        ai_provider_init(provider);
    }

    return provider;
}

AIProvider* ai_provider_gemini(const char* api_key, const char* model) {
    AIProviderConfig* config = ai_config_create("gemini", AI_PROVIDER_GEMINI);
    if (!config) return NULL;

    ai_config_set_api_key(config, api_key);
    config->base_url = strdup("https://generativelanguage.googleapis.com/v1beta");
    config->model = strdup(model ? model : "gemini-1.5-flash");

    AIProvider* provider = ai_provider_create(config);
    ai_config_free(config);

    if (provider) {
        ai_provider_init(provider);
    }

    return provider;
}

AIProvider* ai_provider_anthropic(const char* api_key, const char* model) {
    AIProviderConfig* config = ai_config_create("anthropic", AI_PROVIDER_ANTHROPIC);
    if (!config) return NULL;

    ai_config_set_api_key(config, api_key);
    config->base_url = strdup("https://api.anthropic.com/v1");
    config->model = strdup(model ? model : "claude-3-haiku-20240307");

    AIProvider* provider = ai_provider_create(config);
    ai_config_free(config);

    if (provider) {
        ai_provider_init(provider);
    }

    return provider;
}

AIProvider* ai_provider_llamacpp(const char* model_path) {
    AIProviderConfig* config = ai_config_create("local", AI_PROVIDER_LLAMACPP);
    if (!config) return NULL;

    config->model_path = strdup(model_path);

    AIProvider* provider = ai_provider_create(config);
    ai_config_free(config);

    if (provider) {
        ai_provider_init(provider);
    }

    return provider;
}

AIProvider* ai_provider_from_env(void) {
    const char* api_key;

    /* Check OpenAI */
    api_key = getenv("OPENAI_API_KEY");
    if (api_key && strlen(api_key) > 0) {
        log_info("Using OpenAI provider from environment");
        return ai_provider_openai(api_key, NULL);
    }

    /* Check Anthropic */
    api_key = getenv("ANTHROPIC_API_KEY");
    if (api_key && strlen(api_key) > 0) {
        log_info("Using Anthropic provider from environment");
        return ai_provider_anthropic(api_key, NULL);
    }

    /* Check Gemini */
    api_key = getenv("GEMINI_API_KEY");
    if (api_key && strlen(api_key) > 0) {
        log_info("Using Gemini provider from environment");
        return ai_provider_gemini(api_key, NULL);
    }

    /* Try Ollama as fallback (no API key needed) */
    log_info("Trying Ollama provider (localhost)");
    return ai_provider_ollama(NULL, NULL);
}

/* ========================================================================
 * Provider-Specific Implementations
 * ======================================================================== */

/*
 * Note: The actual HTTP implementations will use libcurl.
 * For now, we provide stub implementations that can be filled in.
 */

/* Helper to set error on provider */
static void set_provider_error(AIProvider* provider, const char* error) {
    free(provider->last_error);
    provider->last_error = error ? strdup(error) : NULL;
    provider->status = PROVIDER_STATUS_ERROR;
}

/* ========================================================================
 * OpenAI Provider (and compatible APIs)
 * ======================================================================== */

static bool openai_init(AIProvider* provider) {
    if (!provider) return false;

    /* Verify we have required config */
    if (!provider->config.api_key || strlen(provider->config.api_key) == 0) {
        /* Some OpenAI-compatible APIs don't need a key */
        if (provider->config.type == AI_PROVIDER_OPENAI) {
            log_warning("OpenAI provider: No API key set");
        }
    }

    if (!provider->config.base_url) {
        provider->config.base_url = strdup("https://api.openai.com/v1");
    }

    if (!provider->config.model) {
        provider->config.model = strdup("gpt-4o-mini");
    }

    provider->status = PROVIDER_STATUS_READY;
    log_debug("OpenAI provider initialized: %s", provider->config.model);
    return true;
}

static void openai_shutdown(AIProvider* provider) {
    if (provider) {
        provider->status = PROVIDER_STATUS_UNKNOWN;
    }
}

static bool openai_is_ready(AIProvider* provider) {
    return provider && provider->status == PROVIDER_STATUS_READY;
}

static AIProviderStatus openai_get_status(AIProvider* provider) {
    return provider ? provider->status : PROVIDER_STATUS_UNKNOWN;
}

static const char* openai_get_error(AIProvider* provider) {
    return provider ? provider->last_error : NULL;
}

/* Build OpenAI API request JSON */
static char* build_openai_request_json(AIProvider* provider, const AIRequest* request) {
    /* Calculate buffer size needed */
    size_t size = 4096;
    for (int i = 0; i < request->message_count; i++) {
        size += strlen(request->messages[i].content) + 100;
    }
    if (request->system_prompt) {
        size += strlen(request->system_prompt) + 100;
    }

    char* json = malloc(size);
    if (!json) return NULL;

    int offset = 0;
    offset += snprintf(json + offset, size - offset, "{\"model\":\"%s\",\"messages\":[",
                       provider->config.model);

    /* Add system prompt if present */
    if (request->system_prompt) {
        offset += snprintf(json + offset, size - offset,
                           "{\"role\":\"system\",\"content\":\"%s\"},",
                           request->system_prompt);
    }

    /* Add messages */
    for (int i = 0; i < request->message_count; i++) {
        const char* role = request->messages[i].role == AI_ROLE_SYSTEM ? "system" :
                          request->messages[i].role == AI_ROLE_USER ? "user" : "assistant";

        /* Escape content for JSON */
        const char* content = request->messages[i].content;
        offset += snprintf(json + offset, size - offset,
                           "{\"role\":\"%s\",\"content\":\"", role);

        /* Simple JSON string escaping */
        for (const char* p = content; *p && offset < (int)size - 10; p++) {
            if (*p == '"') {
                offset += snprintf(json + offset, size - offset, "\\\"");
            } else if (*p == '\\') {
                offset += snprintf(json + offset, size - offset, "\\\\");
            } else if (*p == '\n') {
                offset += snprintf(json + offset, size - offset, "\\n");
            } else if (*p == '\r') {
                offset += snprintf(json + offset, size - offset, "\\r");
            } else if (*p == '\t') {
                offset += snprintf(json + offset, size - offset, "\\t");
            } else {
                json[offset++] = *p;
            }
        }

        offset += snprintf(json + offset, size - offset, "\"}");
        if (i < request->message_count - 1) {
            offset += snprintf(json + offset, size - offset, ",");
        }
    }

    /* Add parameters */
    int max_tokens = request->max_tokens > 0 ? request->max_tokens : provider->config.max_tokens;
    float temp = request->temperature > 0 ? request->temperature : provider->config.temperature;

    offset += snprintf(json + offset, size - offset,
                       "],\"max_tokens\":%d,\"temperature\":%.2f}",
                       max_tokens, temp);

    return json;
}

/* Parse OpenAI API response */
static AIResponse* parse_openai_response(const char* response_body) {
    AIResponse* response = calloc(1, sizeof(AIResponse));
    if (!response) return NULL;

    if (!response_body) {
        response->success = false;
        response->error = strdup("Empty response");
        return response;
    }

    /* Look for content in choices[0].message.content */
    const char* content_start = strstr(response_body, "\"content\":");
    if (content_start) {
        content_start = strchr(content_start, ':');
        if (content_start) {
            content_start++;
            while (*content_start == ' ' || *content_start == '\t') content_start++;

            if (*content_start == '"') {
                content_start++;
                const char* content_end = content_start;

                /* Find end of string (handle escapes) */
                while (*content_end && !(*content_end == '"' && *(content_end - 1) != '\\')) {
                    content_end++;
                }

                size_t len = content_end - content_start;
                response->content = malloc(len + 1);

                /* Unescape */
                size_t j = 0;
                for (size_t i = 0; i < len; i++) {
                    if (content_start[i] == '\\' && i + 1 < len) {
                        i++;
                        switch (content_start[i]) {
                            case 'n': response->content[j++] = '\n'; break;
                            case 't': response->content[j++] = '\t'; break;
                            case 'r': response->content[j++] = '\r'; break;
                            case '"': response->content[j++] = '"'; break;
                            case '\\': response->content[j++] = '\\'; break;
                            default: response->content[j++] = content_start[i]; break;
                        }
                    } else {
                        response->content[j++] = content_start[i];
                    }
                }
                response->content[j] = '\0';
                response->success = true;
            }
        }
    }

    /* Check for error */
    if (!response->success) {
        const char* error_start = strstr(response_body, "\"error\":");
        if (error_start) {
            const char* msg_start = strstr(error_start, "\"message\":");
            if (msg_start) {
                msg_start = strchr(msg_start + 10, '"');
                if (msg_start) {
                    msg_start++;
                    const char* msg_end = strchr(msg_start, '"');
                    if (msg_end) {
                        size_t len = msg_end - msg_start;
                        response->error = malloc(len + 1);
                        strncpy(response->error, msg_start, len);
                        response->error[len] = '\0';
                    }
                }
            }
        }
        if (!response->error) {
            response->error = strdup("Failed to parse response");
        }
    }

    /* Try to parse usage */
    const char* usage = strstr(response_body, "\"usage\":");
    if (usage) {
        const char* prompt_tokens = strstr(usage, "\"prompt_tokens\":");
        const char* completion_tokens = strstr(usage, "\"completion_tokens\":");
        const char* total_tokens = strstr(usage, "\"total_tokens\":");

        if (prompt_tokens) {
            sscanf(prompt_tokens + 16, "%d", &response->prompt_tokens);
        }
        if (completion_tokens) {
            sscanf(completion_tokens + 20, "%d", &response->completion_tokens);
        }
        if (total_tokens) {
            sscanf(total_tokens + 15, "%d", &response->total_tokens);
        }
    }

    return response;
}

#ifdef CYXMAKE_USE_CURL
#include <curl/curl.h>

/* CURL write callback */
struct CurlBuffer {
    char* data;
    size_t size;
};

static size_t cyxmake_curl_write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    struct CurlBuffer* buf = (struct CurlBuffer*)userp;

    char* ptr = realloc(buf->data, buf->size + realsize + 1);
    if (!ptr) return 0;

    buf->data = ptr;
    memcpy(&(buf->data[buf->size]), contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = '\0';

    return realsize;
}

static AIResponse* openai_complete(AIProvider* provider, const AIRequest* request) {
    if (!provider || !request) return NULL;

    CURL* curl = curl_easy_init();
    if (!curl) {
        AIResponse* response = calloc(1, sizeof(AIResponse));
        response->error = strdup("Failed to initialize CURL");
        return response;
    }

    /* Build request JSON */
    char* json = build_openai_request_json(provider, request);
    if (!json) {
        curl_easy_cleanup(curl);
        AIResponse* response = calloc(1, sizeof(AIResponse));
        response->error = strdup("Failed to build request");
        return response;
    }

    /* Build URL */
    char url[512];
    snprintf(url, sizeof(url), "%s/chat/completions", provider->config.base_url);

    /* Set up headers */
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (provider->config.api_key) {
        char auth[512];
        snprintf(auth, sizeof(auth), "Authorization: Bearer %s", provider->config.api_key);
        headers = curl_slist_append(headers, auth);
    }

    /* Add custom headers */
    for (int i = 0; i < provider->config.header_count; i++) {
        char header[512];
        snprintf(header, sizeof(header), "%s: %s",
                 provider->config.headers[i].name,
                 provider->config.headers[i].value);
        headers = curl_slist_append(headers, header);
    }

    /* Response buffer */
    struct CurlBuffer response_buf = {0};

    /* Configure CURL */
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cyxmake_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, provider->config.timeout_sec);

    /* Perform request */
    CURLcode res = curl_easy_perform(curl);

    AIResponse* response = NULL;
    if (res == CURLE_OK) {
        response = parse_openai_response(response_buf.data);
    } else {
        response = calloc(1, sizeof(AIResponse));
        response->error = strdup(curl_easy_strerror(res));
    }

    /* Cleanup */
    free(json);
    free(response_buf.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return response;
}

#else /* No CURL */

static AIResponse* openai_complete(AIProvider* provider, const AIRequest* request) {
    (void)provider;
    (void)request;

    AIResponse* response = calloc(1, sizeof(AIResponse));
    response->success = false;
    response->error = strdup("HTTP support not compiled (CURL not available)");
    return response;
}

#endif /* CYXMAKE_USE_CURL */

static AIProviderVTable openai_vtable = {
    .init = openai_init,
    .shutdown = openai_shutdown,
    .is_ready = openai_is_ready,
    .complete = openai_complete,
    .get_status = openai_get_status,
    .get_error = openai_get_error
};

static AIProviderVTable* get_openai_vtable(void) {
    return &openai_vtable;
}

/* ========================================================================
 * Ollama Provider
 * ======================================================================== */

static bool ollama_init(AIProvider* provider) {
    if (!provider) return false;

    if (!provider->config.base_url) {
        provider->config.base_url = strdup("http://localhost:11434");
    }

    if (!provider->config.model) {
        provider->config.model = strdup("llama2");
    }

    provider->status = PROVIDER_STATUS_READY;
    log_debug("Ollama provider initialized: %s at %s",
             provider->config.model, provider->config.base_url);
    return true;
}

/* Ollama uses a different API format */
static char* build_ollama_request_json(AIProvider* provider, const AIRequest* request) {
    size_t size = 4096;
    for (int i = 0; i < request->message_count; i++) {
        size += strlen(request->messages[i].content) + 100;
    }

    char* json = malloc(size);
    if (!json) return NULL;

    int offset = 0;
    offset += snprintf(json + offset, size - offset,
                       "{\"model\":\"%s\",\"messages\":[",
                       provider->config.model);

    /* Add system prompt if present */
    if (request->system_prompt) {
        offset += snprintf(json + offset, size - offset,
                           "{\"role\":\"system\",\"content\":\"%s\"},",
                           request->system_prompt);
    }

    /* Add messages */
    for (int i = 0; i < request->message_count; i++) {
        const char* role = request->messages[i].role == AI_ROLE_SYSTEM ? "system" :
                          request->messages[i].role == AI_ROLE_USER ? "user" : "assistant";

        offset += snprintf(json + offset, size - offset,
                           "{\"role\":\"%s\",\"content\":\"", role);

        /* Escape content */
        const char* content = request->messages[i].content;
        for (const char* p = content; *p && offset < (int)size - 10; p++) {
            if (*p == '"') offset += snprintf(json + offset, size - offset, "\\\"");
            else if (*p == '\\') offset += snprintf(json + offset, size - offset, "\\\\");
            else if (*p == '\n') offset += snprintf(json + offset, size - offset, "\\n");
            else json[offset++] = *p;
        }

        offset += snprintf(json + offset, size - offset, "\"}");
        if (i < request->message_count - 1) {
            offset += snprintf(json + offset, size - offset, ",");
        }
    }

    offset += snprintf(json + offset, size - offset, "],\"stream\":false}");

    return json;
}

#ifdef CYXMAKE_USE_CURL

static AIResponse* ollama_complete(AIProvider* provider, const AIRequest* request) {
    if (!provider || !request) return NULL;

    CURL* curl = curl_easy_init();
    if (!curl) {
        AIResponse* response = calloc(1, sizeof(AIResponse));
        response->error = strdup("Failed to initialize CURL");
        return response;
    }

    char* json = build_ollama_request_json(provider, request);
    if (!json) {
        curl_easy_cleanup(curl);
        AIResponse* response = calloc(1, sizeof(AIResponse));
        response->error = strdup("Failed to build request");
        return response;
    }

    char url[512];
    snprintf(url, sizeof(url), "%s/api/chat", provider->config.base_url);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    struct CurlBuffer response_buf = {0};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cyxmake_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, provider->config.timeout_sec);

    CURLcode res = curl_easy_perform(curl);

    AIResponse* response = NULL;
    if (res == CURLE_OK) {
        /* Parse Ollama response format */
        response = calloc(1, sizeof(AIResponse));

        /* Look for "message":{"role":"assistant","content":"..."} */
        const char* content_start = strstr(response_buf.data, "\"content\":");
        if (content_start) {
            content_start = strchr(content_start + 10, '"');
            if (content_start) {
                content_start++;
                const char* content_end = content_start;
                while (*content_end && !(*content_end == '"' && *(content_end - 1) != '\\')) {
                    content_end++;
                }
                size_t len = content_end - content_start;
                response->content = malloc(len + 1);
                strncpy(response->content, content_start, len);
                response->content[len] = '\0';
                response->success = true;
            }
        }

        if (!response->success) {
            response->error = strdup("Failed to parse Ollama response");
        }
    } else {
        response = calloc(1, sizeof(AIResponse));
        response->error = strdup(curl_easy_strerror(res));
    }

    free(json);
    free(response_buf.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return response;
}

#else

static AIResponse* ollama_complete(AIProvider* provider, const AIRequest* request) {
    (void)provider;
    (void)request;

    AIResponse* response = calloc(1, sizeof(AIResponse));
    response->success = false;
    response->error = strdup("HTTP support not compiled (CURL not available)");
    return response;
}

#endif

static AIProviderVTable ollama_vtable = {
    .init = ollama_init,
    .shutdown = openai_shutdown,
    .is_ready = openai_is_ready,
    .complete = ollama_complete,
    .get_status = openai_get_status,
    .get_error = openai_get_error
};

static AIProviderVTable* get_ollama_vtable(void) {
    return &ollama_vtable;
}

/* ========================================================================
 * Gemini Provider
 * ======================================================================== */

static bool gemini_init(AIProvider* provider) {
    if (!provider) return false;

    if (!provider->config.api_key) {
        set_provider_error(provider, "Gemini provider: API key required");
        return false;
    }

    if (!provider->config.base_url) {
        provider->config.base_url = strdup("https://generativelanguage.googleapis.com/v1beta");
    }

    if (!provider->config.model) {
        provider->config.model = strdup("gemini-1.5-flash");
    }

    provider->status = PROVIDER_STATUS_READY;
    log_debug("Gemini provider initialized: %s", provider->config.model);
    return true;
}

/* Build Gemini API request JSON */
static char* build_gemini_request_json(AIProvider* provider, const AIRequest* request) {
    size_t size = 4096;
    for (int i = 0; i < request->message_count; i++) {
        size += strlen(request->messages[i].content) + 100;
    }

    char* json = malloc(size);
    if (!json) return NULL;

    int offset = 0;

    /* Gemini uses a different format: contents array with parts */
    offset += snprintf(json + offset, size - offset, "{\"contents\":[");

    /* Add messages */
    for (int i = 0; i < request->message_count; i++) {
        /* Gemini uses "user" and "model" instead of "assistant" */
        const char* role = request->messages[i].role == AI_ROLE_USER ? "user" : "model";

        /* Skip system messages - Gemini handles them differently */
        if (request->messages[i].role == AI_ROLE_SYSTEM) {
            continue;
        }

        offset += snprintf(json + offset, size - offset,
                           "{\"role\":\"%s\",\"parts\":[{\"text\":\"", role);

        /* Escape content */
        const char* content = request->messages[i].content;
        for (const char* p = content; *p && offset < (int)size - 10; p++) {
            if (*p == '"') offset += snprintf(json + offset, size - offset, "\\\"");
            else if (*p == '\\') offset += snprintf(json + offset, size - offset, "\\\\");
            else if (*p == '\n') offset += snprintf(json + offset, size - offset, "\\n");
            else json[offset++] = *p;
        }

        offset += snprintf(json + offset, size - offset, "\"}]}");
        if (i < request->message_count - 1) {
            offset += snprintf(json + offset, size - offset, ",");
        }
    }

    /* Add generation config */
    int max_tokens = request->max_tokens > 0 ? request->max_tokens : provider->config.max_tokens;
    float temp = request->temperature > 0 ? request->temperature : provider->config.temperature;

    offset += snprintf(json + offset, size - offset,
                       "],\"generationConfig\":{\"maxOutputTokens\":%d,\"temperature\":%.2f}}",
                       max_tokens, temp);

    return json;
}

/* Parse Gemini API response */
static AIResponse* parse_gemini_response(const char* response_body) {
    AIResponse* response = calloc(1, sizeof(AIResponse));
    if (!response) return NULL;

    if (!response_body) {
        response->success = false;
        response->error = strdup("Empty response");
        return response;
    }

    /* Look for candidates[0].content.parts[0].text */
    const char* text_start = strstr(response_body, "\"text\":");
    if (text_start) {
        text_start = strchr(text_start + 7, '"');
        if (text_start) {
            text_start++;
            const char* text_end = text_start;
            while (*text_end && !(*text_end == '"' && *(text_end - 1) != '\\')) {
                text_end++;
            }
            size_t len = text_end - text_start;
            response->content = malloc(len + 1);

            /* Unescape */
            size_t j = 0;
            for (size_t i = 0; i < len; i++) {
                if (text_start[i] == '\\' && i + 1 < len) {
                    i++;
                    switch (text_start[i]) {
                        case 'n': response->content[j++] = '\n'; break;
                        case 't': response->content[j++] = '\t'; break;
                        case '"': response->content[j++] = '"'; break;
                        case '\\': response->content[j++] = '\\'; break;
                        default: response->content[j++] = text_start[i]; break;
                    }
                } else {
                    response->content[j++] = text_start[i];
                }
            }
            response->content[j] = '\0';
            response->success = true;
        }
    }

    /* Check for error */
    if (!response->success) {
        const char* error_start = strstr(response_body, "\"error\":");
        if (error_start) {
            const char* msg_start = strstr(error_start, "\"message\":");
            if (msg_start) {
                msg_start = strchr(msg_start + 10, '"');
                if (msg_start) {
                    msg_start++;
                    const char* msg_end = strchr(msg_start, '"');
                    if (msg_end) {
                        size_t len = msg_end - msg_start;
                        response->error = malloc(len + 1);
                        strncpy(response->error, msg_start, len);
                        response->error[len] = '\0';
                    }
                }
            }
        }
        if (!response->error) {
            response->error = strdup("Failed to parse Gemini response");
        }
    }

    return response;
}

#ifdef CYXMAKE_USE_CURL

static AIResponse* gemini_complete(AIProvider* provider, const AIRequest* request) {
    if (!provider || !request) return NULL;

    CURL* curl = curl_easy_init();
    if (!curl) {
        AIResponse* response = calloc(1, sizeof(AIResponse));
        response->error = strdup("Failed to initialize CURL");
        return response;
    }

    char* json = build_gemini_request_json(provider, request);
    if (!json) {
        curl_easy_cleanup(curl);
        AIResponse* response = calloc(1, sizeof(AIResponse));
        response->error = strdup("Failed to build request");
        return response;
    }

    /* Gemini URL format: {base_url}/models/{model}:generateContent?key={api_key} */
    char url[1024];
    snprintf(url, sizeof(url), "%s/models/%s:generateContent?key=%s",
             provider->config.base_url, provider->config.model, provider->config.api_key);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    struct CurlBuffer response_buf = {0};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cyxmake_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, provider->config.timeout_sec);

    CURLcode res = curl_easy_perform(curl);

    AIResponse* response = NULL;
    if (res == CURLE_OK) {
        response = parse_gemini_response(response_buf.data);
    } else {
        response = calloc(1, sizeof(AIResponse));
        response->error = strdup(curl_easy_strerror(res));
    }

    free(json);
    free(response_buf.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return response;
}

#else

static AIResponse* gemini_complete(AIProvider* provider, const AIRequest* request) {
    (void)provider;
    (void)request;

    AIResponse* response = calloc(1, sizeof(AIResponse));
    response->success = false;
    response->error = strdup("HTTP support not compiled (CURL not available)");
    return response;
}

#endif

static AIProviderVTable gemini_vtable = {
    .init = gemini_init,
    .shutdown = openai_shutdown,
    .is_ready = openai_is_ready,
    .complete = gemini_complete,
    .get_status = openai_get_status,
    .get_error = openai_get_error
};

static AIProviderVTable* get_gemini_vtable(void) {
    return &gemini_vtable;
}

/* ========================================================================
 * Anthropic Provider
 * ======================================================================== */

static bool anthropic_init(AIProvider* provider) {
    if (!provider) return false;

    if (!provider->config.api_key) {
        set_provider_error(provider, "Anthropic provider: API key required");
        return false;
    }

    if (!provider->config.base_url) {
        provider->config.base_url = strdup("https://api.anthropic.com/v1");
    }

    if (!provider->config.model) {
        provider->config.model = strdup("claude-3-haiku-20240307");
    }

    provider->status = PROVIDER_STATUS_READY;
    log_debug("Anthropic provider initialized: %s", provider->config.model);
    return true;
}

/* Build Anthropic API request JSON */
static char* build_anthropic_request_json(AIProvider* provider, const AIRequest* request) {
    size_t size = 4096;
    for (int i = 0; i < request->message_count; i++) {
        size += strlen(request->messages[i].content) + 100;
    }
    if (request->system_prompt) {
        size += strlen(request->system_prompt) + 100;
    }

    char* json = malloc(size);
    if (!json) return NULL;

    int offset = 0;

    /* Anthropic format: model, max_tokens, messages, and optional system */
    int max_tokens = request->max_tokens > 0 ? request->max_tokens : provider->config.max_tokens;

    offset += snprintf(json + offset, size - offset,
                       "{\"model\":\"%s\",\"max_tokens\":%d,",
                       provider->config.model, max_tokens);

    /* Add system prompt if present */
    if (request->system_prompt) {
        offset += snprintf(json + offset, size - offset, "\"system\":\"");
        const char* sys = request->system_prompt;
        for (const char* p = sys; *p && offset < (int)size - 10; p++) {
            if (*p == '"') offset += snprintf(json + offset, size - offset, "\\\"");
            else if (*p == '\\') offset += snprintf(json + offset, size - offset, "\\\\");
            else if (*p == '\n') offset += snprintf(json + offset, size - offset, "\\n");
            else json[offset++] = *p;
        }
        offset += snprintf(json + offset, size - offset, "\",");
    }

    offset += snprintf(json + offset, size - offset, "\"messages\":[");

    /* Add messages */
    bool first = true;
    for (int i = 0; i < request->message_count; i++) {
        /* Anthropic uses "user" and "assistant" */
        const char* role = request->messages[i].role == AI_ROLE_USER ? "user" : "assistant";

        /* Skip system messages - handled separately */
        if (request->messages[i].role == AI_ROLE_SYSTEM) {
            continue;
        }

        if (!first) {
            offset += snprintf(json + offset, size - offset, ",");
        }
        first = false;

        offset += snprintf(json + offset, size - offset,
                           "{\"role\":\"%s\",\"content\":\"", role);

        /* Escape content */
        const char* content = request->messages[i].content;
        for (const char* p = content; *p && offset < (int)size - 10; p++) {
            if (*p == '"') offset += snprintf(json + offset, size - offset, "\\\"");
            else if (*p == '\\') offset += snprintf(json + offset, size - offset, "\\\\");
            else if (*p == '\n') offset += snprintf(json + offset, size - offset, "\\n");
            else json[offset++] = *p;
        }

        offset += snprintf(json + offset, size - offset, "\"}");
    }

    offset += snprintf(json + offset, size - offset, "]}");

    return json;
}

/* Parse Anthropic API response */
static AIResponse* parse_anthropic_response(const char* response_body) {
    AIResponse* response = calloc(1, sizeof(AIResponse));
    if (!response) return NULL;

    if (!response_body) {
        response->success = false;
        response->error = strdup("Empty response");
        return response;
    }

    /* Look for content[0].text */
    const char* text_start = strstr(response_body, "\"text\":");
    if (text_start) {
        text_start = strchr(text_start + 7, '"');
        if (text_start) {
            text_start++;
            const char* text_end = text_start;
            while (*text_end && !(*text_end == '"' && *(text_end - 1) != '\\')) {
                text_end++;
            }
            size_t len = text_end - text_start;
            response->content = malloc(len + 1);

            /* Unescape */
            size_t j = 0;
            for (size_t i = 0; i < len; i++) {
                if (text_start[i] == '\\' && i + 1 < len) {
                    i++;
                    switch (text_start[i]) {
                        case 'n': response->content[j++] = '\n'; break;
                        case 't': response->content[j++] = '\t'; break;
                        case '"': response->content[j++] = '"'; break;
                        case '\\': response->content[j++] = '\\'; break;
                        default: response->content[j++] = text_start[i]; break;
                    }
                } else {
                    response->content[j++] = text_start[i];
                }
            }
            response->content[j] = '\0';
            response->success = true;
        }
    }

    /* Check for error */
    if (!response->success) {
        const char* error_start = strstr(response_body, "\"error\":");
        if (error_start) {
            const char* msg_start = strstr(error_start, "\"message\":");
            if (msg_start) {
                msg_start = strchr(msg_start + 10, '"');
                if (msg_start) {
                    msg_start++;
                    const char* msg_end = strchr(msg_start, '"');
                    if (msg_end) {
                        size_t len = msg_end - msg_start;
                        response->error = malloc(len + 1);
                        strncpy(response->error, msg_start, len);
                        response->error[len] = '\0';
                    }
                }
            }
        }
        if (!response->error) {
            response->error = strdup("Failed to parse Anthropic response");
        }
    }

    /* Parse usage info */
    const char* usage = strstr(response_body, "\"usage\":");
    if (usage) {
        const char* input_tokens = strstr(usage, "\"input_tokens\":");
        const char* output_tokens = strstr(usage, "\"output_tokens\":");

        if (input_tokens) {
            sscanf(input_tokens + 15, "%d", &response->prompt_tokens);
        }
        if (output_tokens) {
            sscanf(output_tokens + 16, "%d", &response->completion_tokens);
        }
        response->total_tokens = response->prompt_tokens + response->completion_tokens;
    }

    return response;
}

#ifdef CYXMAKE_USE_CURL

static AIResponse* anthropic_complete(AIProvider* provider, const AIRequest* request) {
    if (!provider || !request) return NULL;

    CURL* curl = curl_easy_init();
    if (!curl) {
        AIResponse* response = calloc(1, sizeof(AIResponse));
        response->error = strdup("Failed to initialize CURL");
        return response;
    }

    char* json = build_anthropic_request_json(provider, request);
    if (!json) {
        curl_easy_cleanup(curl);
        AIResponse* response = calloc(1, sizeof(AIResponse));
        response->error = strdup("Failed to build request");
        return response;
    }

    /* Anthropic URL: {base_url}/messages */
    char url[512];
    snprintf(url, sizeof(url), "%s/messages", provider->config.base_url);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    /* Anthropic uses x-api-key header */
    if (provider->config.api_key) {
        char auth[512];
        snprintf(auth, sizeof(auth), "x-api-key: %s", provider->config.api_key);
        headers = curl_slist_append(headers, auth);
    }

    /* Anthropic requires anthropic-version header */
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");

    /* Add custom headers */
    for (int i = 0; i < provider->config.header_count; i++) {
        char header[512];
        snprintf(header, sizeof(header), "%s: %s",
                 provider->config.headers[i].name,
                 provider->config.headers[i].value);
        headers = curl_slist_append(headers, header);
    }

    struct CurlBuffer response_buf = {0};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cyxmake_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, provider->config.timeout_sec);

    CURLcode res = curl_easy_perform(curl);

    AIResponse* response = NULL;
    if (res == CURLE_OK) {
        response = parse_anthropic_response(response_buf.data);
    } else {
        response = calloc(1, sizeof(AIResponse));
        response->error = strdup(curl_easy_strerror(res));
    }

    free(json);
    free(response_buf.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return response;
}

#else

static AIResponse* anthropic_complete(AIProvider* provider, const AIRequest* request) {
    (void)provider;
    (void)request;

    AIResponse* response = calloc(1, sizeof(AIResponse));
    response->success = false;
    response->error = strdup("HTTP support not compiled (CURL not available)");
    return response;
}

#endif

static AIProviderVTable anthropic_vtable = {
    .init = anthropic_init,
    .shutdown = openai_shutdown,
    .is_ready = openai_is_ready,
    .complete = anthropic_complete,
    .get_status = openai_get_status,
    .get_error = openai_get_error
};

static AIProviderVTable* get_anthropic_vtable(void) {
    return &anthropic_vtable;
}

/* ========================================================================
 * llama.cpp Provider
 * ======================================================================== */

static bool llamacpp_init(AIProvider* provider) {
    if (!provider) return false;

    if (!provider->config.model_path) {
        set_provider_error(provider, "llama.cpp provider: model_path required");
        return false;
    }

    /* TODO: Initialize llama.cpp context */
    provider->status = PROVIDER_STATUS_READY;
    log_debug("llama.cpp provider initialized: %s", provider->config.model_path);
    return true;
}

static AIResponse* llamacpp_complete(AIProvider* provider, const AIRequest* request) {
    (void)provider;
    (void)request;

    /* TODO: Implement llama.cpp inference */
    AIResponse* response = calloc(1, sizeof(AIResponse));
    response->success = false;
    response->error = strdup("llama.cpp provider not yet implemented");
    return response;
}

static AIProviderVTable llamacpp_vtable = {
    .init = llamacpp_init,
    .shutdown = openai_shutdown,
    .is_ready = openai_is_ready,
    .complete = llamacpp_complete,
    .get_status = openai_get_status,
    .get_error = openai_get_error
};

static AIProviderVTable* get_llamacpp_vtable(void) {
    return &llamacpp_vtable;
}
