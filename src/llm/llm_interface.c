/**
 * @file llm_interface.c
 * @brief LLM inference implementation using llama.cpp
 */

#include "cyxmake/llm_interface.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #define PATH_SEPARATOR "\\"
    #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
    #define strdup _strdup
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #define PATH_SEPARATOR "/"
#endif

/* llama.cpp headers */
#include "llama.h"

/* Maximum error message length */
#define MAX_ERROR_LEN 512

/**
 * LLM context structure
 */
struct LLMContext {
    struct llama_model* model;        /* llama.cpp model */
    struct llama_context* ctx;        /* llama.cpp context */
    struct llama_sampler* sampler;    /* Sampler for token generation */
    LLMConfig config;                 /* Configuration */
    char last_error[MAX_ERROR_LEN];   /* Last error message */
    bool is_ready;                    /* Ready for inference */
};

/* ========================================================================
 * Internal Helpers
 * ======================================================================== */

/**
 * Set error message in context
 */
static void set_error(LLMContext* llm_ctx, const char* format, ...) {
    if (!llm_ctx) return;

    va_list args;
    va_start(args, format);
    vsnprintf(llm_ctx->last_error, MAX_ERROR_LEN, format, args);
    va_end(args);

    log_error("%s", llm_ctx->last_error);
}

/**
 * Get user home directory
 */
static char* get_home_dir(void) {
#ifdef _WIN32
    char* home = getenv("USERPROFILE");
    if (!home) home = getenv("HOMEPATH");
#else
    char* home = getenv("HOME");
#endif

    if (!home) {
        log_error("Cannot determine home directory");
        return NULL;
    }

    return strdup(home);
}

/**
 * Expand ~ in path
 */
static char* expand_path(const char* path) {
    if (!path) return NULL;

    if (path[0] == '~') {
        char* home = get_home_dir();
        if (!home) return NULL;

        size_t len = strlen(home) + strlen(path);
        char* expanded = malloc(len);
        snprintf(expanded, len, "%s%s", home, path + 1);
        free(home);
        return expanded;
    }

    return strdup(path);
}

/* ========================================================================
 * Configuration
 * ======================================================================== */

LLMConfig* llm_config_default(void) {
    LLMConfig* config = calloc(1, sizeof(LLMConfig));
    if (!config) return NULL;

    config->model_path = NULL;  /* Must be set by caller */
    config->n_ctx = 8192;       /* 8K context */
    config->n_threads = 0;      /* Auto-detect */
    config->n_gpu_layers = 0;   /* CPU only by default */
    config->use_mmap = true;    /* Memory mapping */
    config->use_mlock = false;  /* Don't lock in RAM */
    config->verbose = false;    /* Quiet by default */

    return config;
}

void llm_config_free(LLMConfig* config) {
    if (!config) return;
    /* NOTE: model_path is not owned by config - caller manages it */
    /* llm_init() makes its own copy with strdup() */
    free(config);
}

/* ========================================================================
 * Lifecycle
 * ======================================================================== */

LLMContext* llm_init(const LLMConfig* config) {
    if (!config || !config->model_path) {
        log_error("LLM config or model path is NULL");
        return NULL;
    }

    /* Allocate context */
    LLMContext* llm_ctx = calloc(1, sizeof(LLMContext));
    if (!llm_ctx) {
        log_error("Failed to allocate LLM context");
        return NULL;
    }

    /* Copy config */
    memcpy(&llm_ctx->config, config, sizeof(LLMConfig));
    llm_ctx->config.model_path = strdup(config->model_path);

    /* Expand path if needed */
    char* model_path = expand_path(config->model_path);
    if (!model_path) {
        set_error(llm_ctx, "Failed to expand model path");
        free(llm_ctx);
        return NULL;
    }

    log_info("Loading LLM model: %s", model_path);

    /* Initialize llama.cpp backend */
    llama_backend_init();

    /* Set up model parameters */
    struct llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = config->n_gpu_layers;

    /* Load model */
    llm_ctx->model = llama_load_model_from_file(model_path, model_params);
    free(model_path);

    if (!llm_ctx->model) {
        set_error(llm_ctx, "Failed to load model from: %s", config->model_path);
        llm_shutdown(llm_ctx);
        return NULL;
    }

    /* Set up context parameters */
    struct llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = config->n_ctx;
    ctx_params.n_threads = config->n_threads > 0 ? config->n_threads : 4;
    ctx_params.n_threads_batch = ctx_params.n_threads;

    /* Create context */
    llm_ctx->ctx = llama_new_context_with_model(llm_ctx->model, ctx_params);
    if (!llm_ctx->ctx) {
        set_error(llm_ctx, "Failed to create llama context");
        llm_shutdown(llm_ctx);
        return NULL;
    }

    /* Create sampler chain */
    struct llama_sampler_chain_params sampler_params = llama_sampler_chain_default_params();
    llm_ctx->sampler = llama_sampler_chain_init(sampler_params);

    /* Add sampling strategies */
    llama_sampler_chain_add(llm_ctx->sampler, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(llm_ctx->sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(llm_ctx->sampler, llama_sampler_init_temp(0.7f));
    llama_sampler_chain_add(llm_ctx->sampler, llama_sampler_init_dist(1234));  /* seed */

    llm_ctx->is_ready = true;
    log_success("LLM model loaded successfully");

    /* Log model info */
    const struct llama_vocab * vocab = llama_model_get_vocab(llm_ctx->model);
    int n_ctx_train = llama_model_n_ctx_train(llm_ctx->model);
    int n_vocab = llama_vocab_n_tokens(vocab);
    log_info("Vocabulary size: %d", n_vocab);
    log_info("Context length: %d", n_ctx_train);
    log_info("Using %d threads", ctx_params.n_threads);

    return llm_ctx;
}

void llm_shutdown(LLMContext* ctx) {
    if (!ctx) return;

    if (ctx->sampler) {
        llama_sampler_free(ctx->sampler);
    }

    if (ctx->ctx) {
        llama_free(ctx->ctx);
    }

    if (ctx->model) {
        llama_free_model(ctx->model);
    }

    llama_backend_free();

    free((void*)ctx->config.model_path);
    free(ctx);
}

bool llm_is_ready(const LLMContext* ctx) {
    return ctx && ctx->is_ready && ctx->model && ctx->ctx;
}

LLMModelInfo* llm_get_model_info(const LLMContext* ctx) {
    if (!llm_is_ready(ctx)) return NULL;

    LLMModelInfo* info = calloc(1, sizeof(LLMModelInfo));
    if (!info) return NULL;

    /* Get model metadata from llama.cpp */
    info->model_name = strdup("qwen2.5-coder");

    char desc[256];
    llama_model_desc(ctx->model, desc, sizeof(desc));
    info->model_type = strdup(desc);

    const struct llama_vocab * vocab = llama_model_get_vocab(ctx->model);
    info->vocab_size = llama_vocab_n_tokens(vocab);
    info->context_length = llama_model_n_ctx_train(ctx->model);
    info->is_loaded = true;

    /* Try to get file size */
    struct stat st;
    if (stat(ctx->config.model_path, &st) == 0) {
        info->model_size_bytes = (size_t)st.st_size;
    } else {
        info->model_size_bytes = 0;
    }

    return info;
}

void llm_model_info_free(LLMModelInfo* info) {
    if (!info) return;
    free(info->model_name);
    free(info->model_type);
    free(info);
}

/* ========================================================================
 * Request/Response
 * ======================================================================== */

LLMRequest* llm_request_create(const char* prompt) {
    if (!prompt) return NULL;

    LLMRequest* req = calloc(1, sizeof(LLMRequest));
    if (!req) return NULL;

    req->prompt = strdup(prompt);
    req->max_tokens = 512;
    req->temperature = 0.7f;
    req->top_k = 40;
    req->top_p = 0.9f;
    req->repeat_penalty = 1.1f;
    req->stop_sequence = NULL;

    return req;
}

void llm_request_free(LLMRequest* request) {
    if (!request) return;
    free((void*)request->prompt);
    free((void*)request->stop_sequence);
    free(request);
}

void llm_response_free(LLMResponse* response) {
    if (!response) return;
    free(response->text);
    free(response->error_message);
    free(response);
}

/* ========================================================================
 * Inference (Simplified placeholder)
 * ======================================================================== */

LLMResponse* llm_query(LLMContext* llm_ctx, const LLMRequest* request) {
    if (!llm_is_ready(llm_ctx)) {
        log_error("LLM context is not ready");
        return NULL;
    }

    if (!request || !request->prompt) {
        set_error(llm_ctx, "Invalid request or empty prompt");
        return NULL;
    }

    log_info("Running LLM inference...");
    log_debug("Prompt: %s", request->prompt);

    /* Allocate response */
    LLMResponse* response = calloc(1, sizeof(LLMResponse));
    if (!response) return NULL;

    /* Start timing */
    clock_t start = clock();

    const struct llama_vocab * vocab = llama_model_get_vocab(llm_ctx->model);

    /* Step 1: Tokenize the prompt */
    const int n_ctx = llama_n_ctx(llm_ctx->ctx);
    llama_token* tokens = malloc(n_ctx * sizeof(llama_token));
    if (!tokens) {
        response->success = false;
        response->error_message = strdup("Memory allocation failed");
        return response;
    }

    int n_tokens = llama_tokenize(
        vocab,
        request->prompt,
        strlen(request->prompt),
        tokens,
        n_ctx,
        llama_vocab_get_add_bos(vocab),  /* add_bos */
        true  /* special */
    );

    if (n_tokens < 0) {
        free(tokens);
        response->success = false;
        response->error_message = strdup("Tokenization failed");
        return response;
    }

    response->tokens_prompt = n_tokens;
    log_debug("Tokenized prompt: %d tokens", n_tokens);

    /* Step 2: Create batch and process prompt */
    struct llama_batch batch = llama_batch_get_one(tokens, n_tokens);

    /* Note: KV cache clearing may not be needed or API may have changed */
    /* llama_kv_cache_clear(llm_ctx->ctx); */

    /* Process prompt */
    if (llama_decode(llm_ctx->ctx, batch) != 0) {
        free(tokens);
        response->success = false;
        response->error_message = strdup("Failed to process prompt");
        return response;
    }

    /* Step 3: Generate tokens */
    const int max_tokens = request->max_tokens;
    char* output = malloc(max_tokens * 32);  /* Estimate 32 bytes per token */
    if (!output) {
        free(tokens);
        response->success = false;
        response->error_message = strdup("Memory allocation failed");
        return response;
    }

    output[0] = '\0';
    size_t output_pos = 0;
    int n_generated = 0;
    int n_cur = n_tokens;

    for (int i = 0; i < max_tokens; i++) {
        /* Get logits for the last token */
        float* logits = llama_get_logits_ith(llm_ctx->ctx, batch.n_tokens - 1);

        /* Sample next token */
        llama_token new_token = llama_sampler_sample(llm_ctx->sampler, llm_ctx->ctx, batch.n_tokens - 1);

        /* Accept token into sampler */
        llama_sampler_accept(llm_ctx->sampler, new_token);

        /* Check for end of generation */
        if (llama_vocab_is_eog(vocab, new_token)) {
            break;
        }

        /* Convert token to text */
        char piece[128];
        int n_chars = llama_token_to_piece(vocab, new_token, piece, sizeof(piece), 0, true);

        if (n_chars > 0 && output_pos + n_chars < max_tokens * 32 - 1) {
            memcpy(output + output_pos, piece, n_chars);
            output_pos += n_chars;
            output[output_pos] = '\0';
        }

        /* Prepare next batch */
        batch = llama_batch_get_one(&new_token, 1);

        /* Decode next token */
        if (llama_decode(llm_ctx->ctx, batch) != 0) {
            log_warning("Failed to decode token %d", i);
            break;
        }

        n_generated++;
        n_cur++;

        /* Check stop sequence */
        if (request->stop_sequence && strstr(output, request->stop_sequence)) {
            break;
        }
    }

    free(tokens);

    /* Calculate duration */
    clock_t end = clock();
    response->duration_sec = ((double)(end - start)) / CLOCKS_PER_SEC;

    /* Set response */
    response->text = output;
    response->tokens_generated = n_generated;
    response->success = true;
    response->error_message = NULL;

    log_success("Generated %d tokens in %.2f seconds (%.1f tok/s)",
                n_generated, response->duration_sec,
                n_generated / response->duration_sec);

    return response;
}

/* ========================================================================
 * Convenience Functions
 * ======================================================================== */

char* llm_query_simple(LLMContext* ctx, const char* prompt, int max_tokens) {
    LLMRequest* req = llm_request_create(prompt);
    if (!req) return NULL;

    req->max_tokens = max_tokens;

    LLMResponse* resp = llm_query(ctx, req);
    llm_request_free(req);

    if (!resp || !resp->success) {
        llm_response_free(resp);
        return NULL;
    }

    char* text = resp->text;
    resp->text = NULL;  /* Don't free text */
    llm_response_free(resp);

    return text;
}

bool llm_validate_model_file(const char* model_path) {
    if (!model_path) return false;

    /* Expand path */
    char* expanded = expand_path(model_path);
    if (!expanded) return false;

    /* Check if file exists and get size */
    struct stat st;
    if (stat(expanded, &st) != 0) {
        log_error("Model file not found: %s", expanded);
        free(expanded);
        return false;
    }

    /* Check if regular file */
    if (!S_ISREG(st.st_mode)) {
        log_error("Model path is not a regular file: %s", expanded);
        free(expanded);
        return false;
    }

    /* Check size (should be > 100 MB) */
    if (st.st_size < 100 * 1024 * 1024) {
        log_error("Model file too small (< 100 MB): %s", expanded);
        free(expanded);
        return false;
    }

    /* Check GGUF magic header */
    FILE* fp = fopen(expanded, "rb");
    free(expanded);

    if (!fp) {
        log_error("Cannot open model file for reading");
        return false;
    }

    char magic[4];
    size_t n_read = fread(magic, 1, 4, fp);
    fclose(fp);

    if (n_read != 4) {
        log_error("Cannot read model file header");
        return false;
    }

    /* GGUF magic: "GGUF" */
    if (memcmp(magic, "GGUF", 4) != 0) {
        log_error("Invalid GGUF magic header");
        return false;
    }

    return true;
}

char* llm_get_default_model_path(void) {
    char* home = get_home_dir();
    if (!home) return NULL;

    size_t len = strlen(home) + 100;
    char* path = malloc(len);
    snprintf(path, len, "%s%s.cyxmake%smodels%sqwen2.5-coder-3b-q4_k_m.gguf",
             home, PATH_SEPARATOR, PATH_SEPARATOR, PATH_SEPARATOR);

    free(home);
    return path;
}

int llm_estimate_tokens(const char* text) {
    if (!text) return 0;

    /* Rough estimation: 1 token ≈ 4 characters */
    return (int)(strlen(text) / 4);
}

/* ========================================================================
 * Error Handling
 * ======================================================================== */

const char* llm_get_last_error(const LLMContext* ctx) {
    if (!ctx || ctx->last_error[0] == '\0') {
        return NULL;
    }
    return ctx->last_error;
}

void llm_clear_error(LLMContext* ctx) {
    if (ctx) {
        ctx->last_error[0] = '\0';
    }
}
