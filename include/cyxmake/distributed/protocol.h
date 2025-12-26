/**
 * @file protocol.h
 * @brief Distributed build wire protocol definitions
 *
 * Defines message types, structures, and serialization for
 * communication between coordinator and workers.
 */

#ifndef CYXMAKE_DISTRIBUTED_PROTOCOL_H
#define CYXMAKE_DISTRIBUTED_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Protocol Message Types
 * ============================================================ */

typedef enum {
    /* Connection management */
    PROTO_MSG_HELLO = 1,           /* Worker -> Coordinator: Initial handshake */
    PROTO_MSG_WELCOME,             /* Coordinator -> Worker: Handshake accepted */
    PROTO_MSG_GOODBYE,             /* Either: Graceful disconnect */

    /* Authentication */
    PROTO_MSG_AUTH_CHALLENGE = 10, /* Coordinator -> Worker: Auth challenge */
    PROTO_MSG_AUTH_RESPONSE,       /* Worker -> Coordinator: Auth response */
    PROTO_MSG_AUTH_SUCCESS,        /* Coordinator -> Worker: Auth successful */
    PROTO_MSG_AUTH_FAILED,         /* Coordinator -> Worker: Auth failed */

    /* Health monitoring */
    PROTO_MSG_HEARTBEAT = 20,      /* Bidirectional: Alive signal */
    PROTO_MSG_HEARTBEAT_ACK,       /* Response to heartbeat */
    PROTO_MSG_STATUS_UPDATE,       /* Worker -> Coordinator: Load update */

    /* Work distribution */
    PROTO_MSG_JOB_REQUEST = 30,    /* Coordinator -> Worker: Execute job */
    PROTO_MSG_JOB_ACCEPT,          /* Worker -> Coordinator: Job accepted */
    PROTO_MSG_JOB_REJECT,          /* Worker -> Coordinator: Job rejected */
    PROTO_MSG_JOB_PROGRESS,        /* Worker -> Coordinator: Progress update */
    PROTO_MSG_JOB_COMPLETE,        /* Worker -> Coordinator: Job finished */
    PROTO_MSG_JOB_FAILED,          /* Worker -> Coordinator: Job failed */
    PROTO_MSG_JOB_CANCEL,          /* Coordinator -> Worker: Cancel job */
    PROTO_MSG_JOB_CANCELLED,       /* Worker -> Coordinator: Job cancelled */

    /* Artifact transfer */
    PROTO_MSG_ARTIFACT_REQUEST = 40, /* Request artifact from cache */
    PROTO_MSG_ARTIFACT_RESPONSE,     /* Artifact data or not found */
    PROTO_MSG_ARTIFACT_PUSH,         /* Push artifact to cache */
    PROTO_MSG_ARTIFACT_ACK,          /* Artifact received */

    /* File transfer */
    PROTO_MSG_FILE_TRANSFER_START = 50, /* Begin file transfer */
    PROTO_MSG_FILE_CHUNK,               /* File data chunk */
    PROTO_MSG_FILE_TRANSFER_END,        /* End file transfer */
    PROTO_MSG_FILE_TRANSFER_ACK,        /* Transfer complete */

    /* Control */
    PROTO_MSG_SHUTDOWN = 60,       /* Graceful shutdown request */
    PROTO_MSG_ERROR                /* Error message */
} ProtocolMessageType;

/* ============================================================
 * Protocol Message
 * ============================================================ */

typedef struct ProtocolMessage {
    ProtocolMessageType type;
    char* id;                     /* Message ID (UUID for correlation) */
    char* correlation_id;         /* Links response to request */
    uint64_t timestamp;           /* Unix timestamp ms */
    char* sender_id;              /* Worker or Coordinator ID */

    /* Payload as JSON string */
    char* payload_json;
    size_t payload_size;

    /* Binary data (for file transfer) */
    uint8_t* binary_data;
    size_t binary_size;
} ProtocolMessage;

/* ============================================================
 * Worker Capability Flags
 * ============================================================ */

typedef enum {
    WORKER_CAP_NONE           = 0,

    /* Compilation capabilities */
    WORKER_CAP_COMPILE_C      = (1 << 0),
    WORKER_CAP_COMPILE_CPP    = (1 << 1),
    WORKER_CAP_COMPILE_RUST   = (1 << 2),
    WORKER_CAP_COMPILE_GO     = (1 << 3),
    WORKER_CAP_LINK           = (1 << 4),

    /* Build systems */
    WORKER_CAP_CMAKE          = (1 << 5),
    WORKER_CAP_MAKE           = (1 << 6),
    WORKER_CAP_NINJA          = (1 << 7),
    WORKER_CAP_MSBUILD        = (1 << 8),
    WORKER_CAP_MSVC           = (1 << 9),

    /* Cross-compilation */
    WORKER_CAP_CROSS_ARM      = (1 << 10),
    WORKER_CAP_CROSS_ARM64    = (1 << 11),
    WORKER_CAP_CROSS_X86      = (1 << 12),
    WORKER_CAP_CROSS_X64      = (1 << 13),
    WORKER_CAP_CROSS_WASM     = (1 << 14),

    /* GPU capabilities */
    WORKER_CAP_GPU_CUDA       = (1 << 15),
    WORKER_CAP_GPU_OPENCL     = (1 << 16),
    WORKER_CAP_GPU_VULKAN     = (1 << 17),
    WORKER_CAP_GPU_METAL      = (1 << 18),

    /* Special capabilities */
    WORKER_CAP_SANDBOX        = (1 << 19),
    WORKER_CAP_DOCKER         = (1 << 20),
    WORKER_CAP_HIGH_MEMORY    = (1 << 21),
    WORKER_CAP_SSD_STORAGE    = (1 << 22)
} WorkerCapability;

/* ============================================================
 * Worker State
 * ============================================================ */

typedef enum {
    WORKER_STATE_OFFLINE,
    WORKER_STATE_CONNECTING,
    WORKER_STATE_AUTHENTICATING,
    WORKER_STATE_ONLINE,
    WORKER_STATE_BUSY,
    WORKER_STATE_DRAINING,
    WORKER_STATE_ERROR
} WorkerState;

/* ============================================================
 * System Information (sent in HELLO)
 * ============================================================ */

typedef struct WorkerSystemInfo {
    char* arch;              /* e.g., "x86_64", "arm64" */
    char* os;                /* e.g., "linux", "windows", "darwin" */
    char* os_version;        /* e.g., "Ubuntu 22.04", "Windows 11" */
    int cpu_cores;
    int cpu_threads;
    uint64_t memory_mb;
    uint64_t disk_free_mb;
} WorkerSystemInfo;

/* ============================================================
 * Tool Information (sent in HELLO)
 * ============================================================ */

typedef struct WorkerToolInfo {
    char* name;
    char* version;
    char* path;
} WorkerToolInfo;

/* ============================================================
 * Job Types
 * ============================================================ */

typedef enum {
    JOB_TYPE_COMPILE,        /* Single file compilation */
    JOB_TYPE_LINK,           /* Link object files */
    JOB_TYPE_CMAKE_CONFIG,   /* CMake configuration */
    JOB_TYPE_CMAKE_BUILD,    /* CMake build */
    JOB_TYPE_FULL_BUILD,     /* Full project build */
    JOB_TYPE_CUSTOM          /* Custom command */
} DistributedJobType;

/* ============================================================
 * Distributed Job (sent in JOB_REQUEST)
 * ============================================================ */

typedef struct DistributedJob {
    char* job_id;
    DistributedJobType type;
    int priority;

    /* For compilation jobs */
    char* source_file;            /* Source file path/content */
    char* output_file;            /* Expected output */
    char* compiler;               /* Compiler to use */
    char** compiler_args;         /* Compiler arguments */
    size_t arg_count;
    char** include_paths;         /* Include directories */
    size_t include_count;

    /* For full build jobs */
    char* project_archive_hash;   /* Hash of project archive */
    char* build_command;          /* Build command to run */
    char* working_dir;            /* Working directory */

    /* Environment */
    char** env_vars;              /* KEY=VALUE pairs */
    size_t env_count;

    /* Timeout */
    int timeout_sec;

    /* Required capabilities */
    unsigned int required_caps;
} DistributedJob;

/* ============================================================
 * Job Result (sent in JOB_COMPLETE/JOB_FAILED)
 * ============================================================ */

typedef struct DistributedJobResult {
    char* job_id;
    bool success;
    int exit_code;

    char* stdout_output;
    char* stderr_output;

    /* Output artifacts */
    char** artifact_paths;        /* Paths to generated artifacts */
    char** artifact_hashes;       /* SHA256 hashes */
    size_t artifact_count;

    double duration_sec;
    double cpu_time_sec;
} DistributedJobResult;

/* ============================================================
 * Protocol Message API
 * ============================================================ */

/**
 * Create a new protocol message
 */
ProtocolMessage* protocol_message_create(ProtocolMessageType type);

/**
 * Create a response message (copies correlation_id from request)
 */
ProtocolMessage* protocol_message_create_response(
    const ProtocolMessage* request,
    ProtocolMessageType response_type
);

/**
 * Free a protocol message
 */
void protocol_message_free(ProtocolMessage* msg);

/**
 * Serialize message to JSON string
 */
char* protocol_message_serialize(const ProtocolMessage* msg);

/**
 * Deserialize JSON string to message
 */
ProtocolMessage* protocol_message_deserialize(const char* json);

/**
 * Set message payload from JSON object
 */
bool protocol_message_set_payload(ProtocolMessage* msg, const char* json);

/**
 * Set binary data for file transfer
 */
bool protocol_message_set_binary(ProtocolMessage* msg,
                                  const uint8_t* data,
                                  size_t size);

/* ============================================================
 * Job Serialization API
 * ============================================================ */

/**
 * Serialize job to JSON
 */
char* distributed_job_to_json(const DistributedJob* job);

/**
 * Deserialize JSON to job
 */
DistributedJob* distributed_job_from_json(const char* json);

/**
 * Free a distributed job
 */
void distributed_job_free(DistributedJob* job);

/**
 * Serialize job result to JSON
 */
char* distributed_job_result_to_json(const DistributedJobResult* result);

/**
 * Deserialize JSON to job result
 */
DistributedJobResult* distributed_job_result_from_json(const char* json);

/**
 * Free a job result
 */
void distributed_job_result_free(DistributedJobResult* result);

/* ============================================================
 * System Info Serialization
 * ============================================================ */

/**
 * Serialize system info to JSON
 */
char* worker_system_info_to_json(const WorkerSystemInfo* info);

/**
 * Deserialize JSON to system info
 */
WorkerSystemInfo* worker_system_info_from_json(const char* json);

/**
 * Free system info
 */
void worker_system_info_free(WorkerSystemInfo* info);

/* ============================================================
 * Utility Functions
 * ============================================================ */

/**
 * Generate a new UUID string
 */
char* protocol_generate_uuid(void);

/**
 * Get current timestamp in milliseconds
 */
uint64_t protocol_get_timestamp_ms(void);

/**
 * Get message type name as string
 */
const char* protocol_message_type_name(ProtocolMessageType type);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_DISTRIBUTED_PROTOCOL_H */
