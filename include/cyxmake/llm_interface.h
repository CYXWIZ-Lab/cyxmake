/**
 * @file llm_interface.h
 * @brief LLM inference interface for CyxMake
 *
 * This module provides a synchronous API for loading and querying
 * LLM models (specifically Qwen2.5-Coder-3B) for build error analysis
 * and project understanding.
 *
 * Architecture:
 * - Synchronous API (blocking calls)
 * - Single model instance per context
 * - Memory-mapped model loading for performance
 * - Thread-safe operations (future enhancement)
 *
 * Model Requirements:
 * - Format: GGUF (llama.cpp compatible)
 * - Recommended: Qwen2.5-Coder-3B-Instruct-Q4_K_M.gguf
 * - Size: ~1.8 GB
 * - Context window: 8K tokens
 */

#ifndef CYXMAKE_LLM_INTERFACE_H
#define CYXMAKE_LLM_INTERFACE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct LLMContext LLMContext;

/**
 * GPU backend type
 */
typedef enum {
    LLM_GPU_NONE = 0,    /**< CPU only */
    LLM_GPU_CUDA,        /**< NVIDIA CUDA */
    LLM_GPU_VULKAN,      /**< Vulkan (cross-platform) */
    LLM_GPU_METAL,       /**< Apple Metal */
    LLM_GPU_OPENCL       /**< OpenCL */
} LLMGpuBackend;

/**
 * LLM configuration options
 */
typedef struct {
    const char* model_path;          /**< Path to GGUF model file */
    int n_ctx;                       /**< Context size (default: 8192) */
    int n_threads;                   /**< Number of threads (0 = auto-detect) */
    int n_gpu_layers;                /**< Number of layers to offload to GPU (-1 = auto, 0 = CPU only) */
    bool use_mmap;                   /**< Use memory-mapped file (default: true) */
    bool use_mlock;                  /**< Lock model in RAM (default: false) */
    bool verbose;                    /**< Enable verbose logging (default: false) */
    bool gpu_auto;                   /**< Auto-detect and use GPU if available (default: true) */
    LLMGpuBackend gpu_backend;       /**< Preferred GPU backend (default: auto-detect) */
} LLMConfig;

/**
 * LLM query request
 */
typedef struct {
    const char* prompt;              /**< Input prompt text */
    int max_tokens;                  /**< Maximum tokens to generate (default: 512) */
    float temperature;               /**< Sampling temperature (default: 0.7) */
    int top_k;                       /**< Top-K sampling (default: 40) */
    float top_p;                     /**< Top-P (nucleus) sampling (default: 0.9) */
    float repeat_penalty;            /**< Repetition penalty (default: 1.1) */
    const char* stop_sequence;       /**< Stop generation at this sequence (optional) */
} LLMRequest;

/**
 * LLM query response
 */
typedef struct {
    char* text;                      /**< Generated text (caller must free) */
    int tokens_generated;            /**< Number of tokens generated */
    int tokens_prompt;               /**< Number of tokens in prompt */
    double duration_sec;             /**< Inference duration in seconds */
    bool success;                    /**< True if generation succeeded */
    char* error_message;             /**< Error message if success=false (caller must free) */
} LLMResponse;

/**
 * LLM model information
 */
typedef struct {
    char* model_name;                /**< Model name (e.g., "qwen2.5-coder-3b") */
    char* model_type;                /**< Architecture (e.g., "qwen2") */
    size_t model_size_bytes;         /**< Model file size */
    int vocab_size;                  /**< Vocabulary size */
    int context_length;              /**< Maximum context length */
    int n_gpu_layers;                /**< Number of layers on GPU (0 = CPU only) */
    LLMGpuBackend gpu_backend;       /**< Active GPU backend */
    bool is_loaded;                  /**< True if model is loaded */
} LLMModelInfo;

/* ========================================================================
 * Lifecycle Management
 * ======================================================================== */

/**
 * Create default LLM configuration
 *
 * @return Newly allocated configuration with sensible defaults
 *         Caller must free with llm_config_free()
 */
LLMConfig* llm_config_default(void);

/**
 * Free LLM configuration
 *
 * @param config Configuration to free
 */
void llm_config_free(LLMConfig* config);

/**
 * Initialize LLM context and load model
 *
 * This operation can take 2-3 seconds for a 1.8 GB model.
 * The model is memory-mapped if use_mmap=true for faster subsequent loads.
 *
 * @param config Configuration (uses default if NULL)
 * @return Initialized context, or NULL on error
 *
 * Example:
 *   LLMConfig* cfg = llm_config_default();
 *   cfg->model_path = "~/.cyxmake/models/qwen2.5-coder-3b-q4_k_m.gguf";
 *   LLMContext* ctx = llm_init(cfg);
 *   if (!ctx) {
 *       fprintf(stderr, "Failed to load model\n");
 *   }
 */
LLMContext* llm_init(const LLMConfig* config);

/**
 * Shutdown LLM context and unload model
 *
 * Frees all resources associated with the context.
 *
 * @param ctx Context to shutdown (NULL-safe)
 */
void llm_shutdown(LLMContext* ctx);

/**
 * Check if LLM is ready for inference
 *
 * @param ctx LLM context
 * @return True if model is loaded and ready
 */
bool llm_is_ready(const LLMContext* ctx);

/**
 * Get model information
 *
 * @param ctx LLM context
 * @return Model info, or NULL if not loaded
 *         Caller must free with llm_model_info_free()
 */
LLMModelInfo* llm_get_model_info(const LLMContext* ctx);

/**
 * Free model information
 *
 * @param info Model info to free
 */
void llm_model_info_free(LLMModelInfo* info);

/* ========================================================================
 * GPU Detection
 * ======================================================================== */

/**
 * Detect available GPU backend
 *
 * Checks for available GPU backends in order of preference:
 * 1. CUDA (NVIDIA)
 * 2. Metal (Apple)
 * 3. Vulkan (cross-platform)
 * 4. OpenCL
 *
 * @return Detected GPU backend, or LLM_GPU_NONE if no GPU available
 */
LLMGpuBackend llm_detect_gpu(void);

/**
 * Get GPU backend name as string
 *
 * @param backend GPU backend type
 * @return Static string name (e.g., "CUDA", "Metal", "CPU")
 */
const char* llm_gpu_backend_name(LLMGpuBackend backend);

/**
 * Check if GPU backend is available at runtime
 *
 * @param backend GPU backend to check
 * @return True if backend is available and functional
 */
bool llm_gpu_backend_available(LLMGpuBackend backend);

/* ========================================================================
 * Query Interface
 * ======================================================================== */

/**
 * Create default LLM request
 *
 * @param prompt Input prompt text (required)
 * @return Newly allocated request with defaults
 *         Caller must free with llm_request_free()
 */
LLMRequest* llm_request_create(const char* prompt);

/**
 * Free LLM request
 *
 * @param request Request to free
 */
void llm_request_free(LLMRequest* request);

/**
 * Query the LLM (synchronous)
 *
 * This is a blocking call that takes 1-2 seconds on average.
 * The caller should display a "thinking..." message before calling.
 *
 * @param ctx LLM context
 * @param request Query parameters
 * @return Response with generated text, or NULL on error
 *         Caller must free with llm_response_free()
 *
 * Example:
 *   LLMRequest* req = llm_request_create("Fix this error: undefined reference to foo");
 *   req->max_tokens = 256;
 *   req->temperature = 0.3;  // Lower temp for code = less creative
 *
 *   printf("Analyzing error...\n");
 *   LLMResponse* resp = llm_query(ctx, req);
 *
 *   if (resp && resp->success) {
 *       printf("Suggestion:\n%s\n", resp->text);
 *   } else {
 *       printf("Error: %s\n", resp ? resp->error_message : "Unknown");
 *   }
 *
 *   llm_response_free(resp);
 *   llm_request_free(req);
 */
LLMResponse* llm_query(LLMContext* ctx, const LLMRequest* request);

/**
 * Free LLM response
 *
 * @param response Response to free
 */
void llm_response_free(LLMResponse* response);

/* ========================================================================
 * Convenience Functions
 * ======================================================================== */

/**
 * Simple query with just prompt and max tokens
 *
 * Convenience wrapper around llm_query() for simple use cases.
 *
 * @param ctx LLM context
 * @param prompt Input prompt
 * @param max_tokens Maximum tokens to generate
 * @return Response text (caller must free), or NULL on error
 */
char* llm_query_simple(LLMContext* ctx, const char* prompt, int max_tokens);

/**
 * Check if a model file exists and is valid
 *
 * Verifies:
 * - File exists
 * - File is readable
 * - File has GGUF magic header
 * - File size is reasonable (> 100 MB)
 *
 * @param model_path Path to model file
 * @return True if model appears valid
 */
bool llm_validate_model_file(const char* model_path);

/**
 * Get default model path
 *
 * Returns: ~/.cyxmake/models/qwen2.5-coder-3b-q4_k_m.gguf
 *
 * @return Newly allocated path string (caller must free)
 */
char* llm_get_default_model_path(void);

/**
 * Estimate tokens in text
 *
 * Rough estimation: 1 token ≈ 4 characters for English
 *
 * @param text Input text
 * @return Estimated token count
 */
int llm_estimate_tokens(const char* text);

/* ========================================================================
 * Error Handling
 * ======================================================================== */

/**
 * Get last error message from context
 *
 * @param ctx LLM context
 * @return Error string (do not free), or NULL if no error
 */
const char* llm_get_last_error(const LLMContext* ctx);

/**
 * Clear last error
 *
 * @param ctx LLM context
 */
void llm_clear_error(LLMContext* ctx);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_LLM_INTERFACE_H */
