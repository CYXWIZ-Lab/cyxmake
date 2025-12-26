/**
 * @file protocol_codec.c
 * @brief Protocol message serialization/deserialization
 */

#include "cyxmake/distributed/protocol.h"
#include "cyxmake/compat.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <rpc.h>
#pragma comment(lib, "rpcrt4.lib")
#else
#include <uuid/uuid.h>
#endif

/* ============================================================
 * Message Type Names
 * ============================================================ */

static const char* message_type_names[] = {
    [PROTO_MSG_HELLO] = "HELLO",
    [PROTO_MSG_WELCOME] = "WELCOME",
    [PROTO_MSG_GOODBYE] = "GOODBYE",
    [PROTO_MSG_AUTH_CHALLENGE] = "AUTH_CHALLENGE",
    [PROTO_MSG_AUTH_RESPONSE] = "AUTH_RESPONSE",
    [PROTO_MSG_AUTH_SUCCESS] = "AUTH_SUCCESS",
    [PROTO_MSG_AUTH_FAILED] = "AUTH_FAILED",
    [PROTO_MSG_HEARTBEAT] = "HEARTBEAT",
    [PROTO_MSG_HEARTBEAT_ACK] = "HEARTBEAT_ACK",
    [PROTO_MSG_STATUS_UPDATE] = "STATUS_UPDATE",
    [PROTO_MSG_JOB_REQUEST] = "JOB_REQUEST",
    [PROTO_MSG_JOB_ACCEPT] = "JOB_ACCEPT",
    [PROTO_MSG_JOB_REJECT] = "JOB_REJECT",
    [PROTO_MSG_JOB_PROGRESS] = "JOB_PROGRESS",
    [PROTO_MSG_JOB_COMPLETE] = "JOB_COMPLETE",
    [PROTO_MSG_JOB_FAILED] = "JOB_FAILED",
    [PROTO_MSG_JOB_CANCEL] = "JOB_CANCEL",
    [PROTO_MSG_JOB_CANCELLED] = "JOB_CANCELLED",
    [PROTO_MSG_ARTIFACT_REQUEST] = "ARTIFACT_REQUEST",
    [PROTO_MSG_ARTIFACT_RESPONSE] = "ARTIFACT_RESPONSE",
    [PROTO_MSG_ARTIFACT_PUSH] = "ARTIFACT_PUSH",
    [PROTO_MSG_ARTIFACT_ACK] = "ARTIFACT_ACK",
    [PROTO_MSG_FILE_TRANSFER_START] = "FILE_TRANSFER_START",
    [PROTO_MSG_FILE_CHUNK] = "FILE_CHUNK",
    [PROTO_MSG_FILE_TRANSFER_END] = "FILE_TRANSFER_END",
    [PROTO_MSG_FILE_TRANSFER_ACK] = "FILE_TRANSFER_ACK",
    [PROTO_MSG_SHUTDOWN] = "SHUTDOWN",
    [PROTO_MSG_ERROR] = "ERROR"
};

static ProtocolMessageType message_type_from_name(const char* name) {
    if (!name) return PROTO_MSG_ERROR;

    for (int i = 0; i <= PROTO_MSG_ERROR; i++) {
        if (message_type_names[i] && strcmp(message_type_names[i], name) == 0) {
            return (ProtocolMessageType)i;
        }
    }
    return PROTO_MSG_ERROR;
}

/* ============================================================
 * UUID Generation
 * ============================================================ */

char* protocol_generate_uuid(void) {
    char* uuid_str = (char*)malloc(37);
    if (!uuid_str) return NULL;

#ifdef _WIN32
    UUID uuid;
    UuidCreate(&uuid);
    unsigned char* str;
    UuidToStringA(&uuid, &str);
    strncpy(uuid_str, (char*)str, 36);
    uuid_str[36] = '\0';
    RpcStringFreeA(&str);
#else
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, uuid_str);
#endif

    return uuid_str;
}

/* ============================================================
 * Timestamp
 * ============================================================ */

uint64_t protocol_get_timestamp_ms(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    /* Convert from 100-nanosecond intervals since 1601 to ms since 1970 */
    return (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

/* ============================================================
 * Protocol Message API
 * ============================================================ */

ProtocolMessage* protocol_message_create(ProtocolMessageType type) {
    ProtocolMessage* msg = (ProtocolMessage*)calloc(1, sizeof(ProtocolMessage));
    if (!msg) return NULL;

    msg->type = type;
    msg->id = protocol_generate_uuid();
    msg->timestamp = protocol_get_timestamp_ms();

    return msg;
}

ProtocolMessage* protocol_message_create_response(
    const ProtocolMessage* request,
    ProtocolMessageType response_type
) {
    ProtocolMessage* msg = protocol_message_create(response_type);
    if (!msg) return NULL;

    if (request && request->id) {
        msg->correlation_id = strdup(request->id);
    }

    return msg;
}

void protocol_message_free(ProtocolMessage* msg) {
    if (!msg) return;

    free(msg->id);
    free(msg->correlation_id);
    free(msg->sender_id);
    free(msg->payload_json);
    free(msg->binary_data);
    free(msg);
}

bool protocol_message_set_payload(ProtocolMessage* msg, const char* json) {
    if (!msg) return false;

    free(msg->payload_json);
    if (json) {
        msg->payload_json = strdup(json);
        msg->payload_size = strlen(json);
        return msg->payload_json != NULL;
    } else {
        msg->payload_json = NULL;
        msg->payload_size = 0;
        return true;
    }
}

bool protocol_message_set_binary(ProtocolMessage* msg,
                                  const uint8_t* data,
                                  size_t size) {
    if (!msg) return false;

    free(msg->binary_data);
    if (data && size > 0) {
        msg->binary_data = (uint8_t*)malloc(size);
        if (!msg->binary_data) return false;
        memcpy(msg->binary_data, data, size);
        msg->binary_size = size;
    } else {
        msg->binary_data = NULL;
        msg->binary_size = 0;
    }
    return true;
}

const char* protocol_message_type_name(ProtocolMessageType type) {
    if (type < 0 || type > PROTO_MSG_ERROR) return "UNKNOWN";
    return message_type_names[type] ? message_type_names[type] : "UNKNOWN";
}

/* ============================================================
 * Message Serialization
 * ============================================================ */

char* protocol_message_serialize(const ProtocolMessage* msg) {
    if (!msg) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "type", protocol_message_type_name(msg->type));
    if (msg->id) cJSON_AddStringToObject(root, "id", msg->id);
    if (msg->correlation_id) cJSON_AddStringToObject(root, "correlation_id", msg->correlation_id);
    cJSON_AddNumberToObject(root, "timestamp", (double)msg->timestamp);
    if (msg->sender_id) cJSON_AddStringToObject(root, "sender", msg->sender_id);

    /* Add payload as nested object if it's valid JSON */
    if (msg->payload_json && msg->payload_size > 0) {
        cJSON* payload = cJSON_Parse(msg->payload_json);
        if (payload) {
            cJSON_AddItemToObject(root, "payload", payload);
        } else {
            /* If not valid JSON, add as string */
            cJSON_AddStringToObject(root, "payload", msg->payload_json);
        }
    }

    /* Binary data is base64 encoded */
    if (msg->binary_data && msg->binary_size > 0) {
        /* Simple base64 encoding - in production use a proper base64 library */
        cJSON_AddNumberToObject(root, "binary_size", (double)msg->binary_size);
        /* For now, just indicate binary data exists */
        cJSON_AddBoolToObject(root, "has_binary", true);
    }

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

ProtocolMessage* protocol_message_deserialize(const char* json) {
    if (!json) return NULL;

    cJSON* root = cJSON_Parse(json);
    if (!root) return NULL;

    ProtocolMessage* msg = (ProtocolMessage*)calloc(1, sizeof(ProtocolMessage));
    if (!msg) {
        cJSON_Delete(root);
        return NULL;
    }

    /* Parse type */
    cJSON* type_item = cJSON_GetObjectItem(root, "type");
    if (cJSON_IsString(type_item)) {
        msg->type = message_type_from_name(type_item->valuestring);
    }

    /* Parse id */
    cJSON* id_item = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsString(id_item)) {
        msg->id = strdup(id_item->valuestring);
    }

    /* Parse correlation_id */
    cJSON* corr_item = cJSON_GetObjectItem(root, "correlation_id");
    if (cJSON_IsString(corr_item)) {
        msg->correlation_id = strdup(corr_item->valuestring);
    }

    /* Parse timestamp */
    cJSON* ts_item = cJSON_GetObjectItem(root, "timestamp");
    if (cJSON_IsNumber(ts_item)) {
        msg->timestamp = (uint64_t)ts_item->valuedouble;
    }

    /* Parse sender */
    cJSON* sender_item = cJSON_GetObjectItem(root, "sender");
    if (cJSON_IsString(sender_item)) {
        msg->sender_id = strdup(sender_item->valuestring);
    }

    /* Parse payload */
    cJSON* payload_item = cJSON_GetObjectItem(root, "payload");
    if (payload_item) {
        if (cJSON_IsObject(payload_item) || cJSON_IsArray(payload_item)) {
            char* payload_str = cJSON_PrintUnformatted(payload_item);
            if (payload_str) {
                msg->payload_json = payload_str;
                msg->payload_size = strlen(payload_str);
            }
        } else if (cJSON_IsString(payload_item)) {
            msg->payload_json = strdup(payload_item->valuestring);
            msg->payload_size = strlen(payload_item->valuestring);
        }
    }

    cJSON_Delete(root);
    return msg;
}

/* ============================================================
 * Job Serialization
 * ============================================================ */

static const char* job_type_names[] = {
    [JOB_TYPE_COMPILE] = "compile",
    [JOB_TYPE_LINK] = "link",
    [JOB_TYPE_CMAKE_CONFIG] = "cmake_config",
    [JOB_TYPE_CMAKE_BUILD] = "cmake_build",
    [JOB_TYPE_FULL_BUILD] = "full_build",
    [JOB_TYPE_CUSTOM] = "custom"
};

char* distributed_job_to_json(const DistributedJob* job) {
    if (!job) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    if (job->job_id) cJSON_AddStringToObject(root, "job_id", job->job_id);
    cJSON_AddStringToObject(root, "type", job_type_names[job->type]);
    cJSON_AddNumberToObject(root, "priority", job->priority);

    if (job->source_file) cJSON_AddStringToObject(root, "source_file", job->source_file);
    if (job->output_file) cJSON_AddStringToObject(root, "output_file", job->output_file);
    if (job->compiler) cJSON_AddStringToObject(root, "compiler", job->compiler);

    /* Compiler args */
    if (job->compiler_args && job->arg_count > 0) {
        cJSON* args = cJSON_CreateArray();
        for (size_t i = 0; i < job->arg_count; i++) {
            cJSON_AddItemToArray(args, cJSON_CreateString(job->compiler_args[i]));
        }
        cJSON_AddItemToObject(root, "compiler_args", args);
    }

    /* Include paths */
    if (job->include_paths && job->include_count > 0) {
        cJSON* includes = cJSON_CreateArray();
        for (size_t i = 0; i < job->include_count; i++) {
            cJSON_AddItemToArray(includes, cJSON_CreateString(job->include_paths[i]));
        }
        cJSON_AddItemToObject(root, "include_paths", includes);
    }

    if (job->project_archive_hash) {
        cJSON_AddStringToObject(root, "project_archive_hash", job->project_archive_hash);
    }
    if (job->build_command) cJSON_AddStringToObject(root, "build_command", job->build_command);
    if (job->working_dir) cJSON_AddStringToObject(root, "working_dir", job->working_dir);

    /* Environment vars */
    if (job->env_vars && job->env_count > 0) {
        cJSON* env = cJSON_CreateArray();
        for (size_t i = 0; i < job->env_count; i++) {
            cJSON_AddItemToArray(env, cJSON_CreateString(job->env_vars[i]));
        }
        cJSON_AddItemToObject(root, "env_vars", env);
    }

    cJSON_AddNumberToObject(root, "timeout_sec", job->timeout_sec);
    cJSON_AddNumberToObject(root, "required_caps", job->required_caps);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

DistributedJob* distributed_job_from_json(const char* json) {
    if (!json) return NULL;

    cJSON* root = cJSON_Parse(json);
    if (!root) return NULL;

    DistributedJob* job = (DistributedJob*)calloc(1, sizeof(DistributedJob));
    if (!job) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON* item;

    item = cJSON_GetObjectItem(root, "job_id");
    if (cJSON_IsString(item)) job->job_id = strdup(item->valuestring);

    item = cJSON_GetObjectItem(root, "type");
    if (cJSON_IsString(item)) {
        for (int i = 0; i <= JOB_TYPE_CUSTOM; i++) {
            if (strcmp(job_type_names[i], item->valuestring) == 0) {
                job->type = (DistributedJobType)i;
                break;
            }
        }
    }

    item = cJSON_GetObjectItem(root, "priority");
    if (cJSON_IsNumber(item)) job->priority = (int)item->valuedouble;

    item = cJSON_GetObjectItem(root, "source_file");
    if (cJSON_IsString(item)) job->source_file = strdup(item->valuestring);

    item = cJSON_GetObjectItem(root, "output_file");
    if (cJSON_IsString(item)) job->output_file = strdup(item->valuestring);

    item = cJSON_GetObjectItem(root, "compiler");
    if (cJSON_IsString(item)) job->compiler = strdup(item->valuestring);

    /* Parse compiler args */
    item = cJSON_GetObjectItem(root, "compiler_args");
    if (cJSON_IsArray(item)) {
        job->arg_count = cJSON_GetArraySize(item);
        job->compiler_args = (char**)calloc(job->arg_count, sizeof(char*));
        for (size_t i = 0; i < job->arg_count; i++) {
            cJSON* arg = cJSON_GetArrayItem(item, (int)i);
            if (cJSON_IsString(arg)) {
                job->compiler_args[i] = strdup(arg->valuestring);
            }
        }
    }

    /* Parse include paths */
    item = cJSON_GetObjectItem(root, "include_paths");
    if (cJSON_IsArray(item)) {
        job->include_count = cJSON_GetArraySize(item);
        job->include_paths = (char**)calloc(job->include_count, sizeof(char*));
        for (size_t i = 0; i < job->include_count; i++) {
            cJSON* path = cJSON_GetArrayItem(item, (int)i);
            if (cJSON_IsString(path)) {
                job->include_paths[i] = strdup(path->valuestring);
            }
        }
    }

    item = cJSON_GetObjectItem(root, "project_archive_hash");
    if (cJSON_IsString(item)) job->project_archive_hash = strdup(item->valuestring);

    item = cJSON_GetObjectItem(root, "build_command");
    if (cJSON_IsString(item)) job->build_command = strdup(item->valuestring);

    item = cJSON_GetObjectItem(root, "working_dir");
    if (cJSON_IsString(item)) job->working_dir = strdup(item->valuestring);

    /* Parse env vars */
    item = cJSON_GetObjectItem(root, "env_vars");
    if (cJSON_IsArray(item)) {
        job->env_count = cJSON_GetArraySize(item);
        job->env_vars = (char**)calloc(job->env_count, sizeof(char*));
        for (size_t i = 0; i < job->env_count; i++) {
            cJSON* env = cJSON_GetArrayItem(item, (int)i);
            if (cJSON_IsString(env)) {
                job->env_vars[i] = strdup(env->valuestring);
            }
        }
    }

    item = cJSON_GetObjectItem(root, "timeout_sec");
    if (cJSON_IsNumber(item)) job->timeout_sec = (int)item->valuedouble;

    item = cJSON_GetObjectItem(root, "required_caps");
    if (cJSON_IsNumber(item)) job->required_caps = (unsigned int)item->valuedouble;

    cJSON_Delete(root);
    return job;
}

void distributed_job_free(DistributedJob* job) {
    if (!job) return;

    free(job->job_id);
    free(job->source_file);
    free(job->output_file);
    free(job->compiler);

    for (size_t i = 0; i < job->arg_count; i++) {
        free(job->compiler_args[i]);
    }
    free(job->compiler_args);

    for (size_t i = 0; i < job->include_count; i++) {
        free(job->include_paths[i]);
    }
    free(job->include_paths);

    free(job->project_archive_hash);
    free(job->build_command);
    free(job->working_dir);

    for (size_t i = 0; i < job->env_count; i++) {
        free(job->env_vars[i]);
    }
    free(job->env_vars);

    free(job);
}

/* ============================================================
 * Job Result Serialization
 * ============================================================ */

char* distributed_job_result_to_json(const DistributedJobResult* result) {
    if (!result) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    if (result->job_id) cJSON_AddStringToObject(root, "job_id", result->job_id);
    cJSON_AddBoolToObject(root, "success", result->success);
    cJSON_AddNumberToObject(root, "exit_code", result->exit_code);

    if (result->stdout_output) cJSON_AddStringToObject(root, "stdout", result->stdout_output);
    if (result->stderr_output) cJSON_AddStringToObject(root, "stderr", result->stderr_output);

    /* Artifacts */
    if (result->artifact_paths && result->artifact_count > 0) {
        cJSON* artifacts = cJSON_CreateArray();
        for (size_t i = 0; i < result->artifact_count; i++) {
            cJSON* artifact = cJSON_CreateObject();
            if (result->artifact_paths[i]) {
                cJSON_AddStringToObject(artifact, "path", result->artifact_paths[i]);
            }
            if (result->artifact_hashes && result->artifact_hashes[i]) {
                cJSON_AddStringToObject(artifact, "hash", result->artifact_hashes[i]);
            }
            cJSON_AddItemToArray(artifacts, artifact);
        }
        cJSON_AddItemToObject(root, "artifacts", artifacts);
    }

    cJSON_AddNumberToObject(root, "duration_sec", result->duration_sec);
    cJSON_AddNumberToObject(root, "cpu_time_sec", result->cpu_time_sec);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

DistributedJobResult* distributed_job_result_from_json(const char* json) {
    if (!json) return NULL;

    cJSON* root = cJSON_Parse(json);
    if (!root) return NULL;

    DistributedJobResult* result = (DistributedJobResult*)calloc(1, sizeof(DistributedJobResult));
    if (!result) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON* item;

    item = cJSON_GetObjectItem(root, "job_id");
    if (cJSON_IsString(item)) result->job_id = strdup(item->valuestring);

    item = cJSON_GetObjectItem(root, "success");
    if (cJSON_IsBool(item)) result->success = cJSON_IsTrue(item);

    item = cJSON_GetObjectItem(root, "exit_code");
    if (cJSON_IsNumber(item)) result->exit_code = (int)item->valuedouble;

    item = cJSON_GetObjectItem(root, "stdout");
    if (cJSON_IsString(item)) result->stdout_output = strdup(item->valuestring);

    item = cJSON_GetObjectItem(root, "stderr");
    if (cJSON_IsString(item)) result->stderr_output = strdup(item->valuestring);

    /* Parse artifacts */
    item = cJSON_GetObjectItem(root, "artifacts");
    if (cJSON_IsArray(item)) {
        result->artifact_count = cJSON_GetArraySize(item);
        result->artifact_paths = (char**)calloc(result->artifact_count, sizeof(char*));
        result->artifact_hashes = (char**)calloc(result->artifact_count, sizeof(char*));

        for (size_t i = 0; i < result->artifact_count; i++) {
            cJSON* artifact = cJSON_GetArrayItem(item, (int)i);
            cJSON* path = cJSON_GetObjectItem(artifact, "path");
            cJSON* hash = cJSON_GetObjectItem(artifact, "hash");

            if (cJSON_IsString(path)) result->artifact_paths[i] = strdup(path->valuestring);
            if (cJSON_IsString(hash)) result->artifact_hashes[i] = strdup(hash->valuestring);
        }
    }

    item = cJSON_GetObjectItem(root, "duration_sec");
    if (cJSON_IsNumber(item)) result->duration_sec = item->valuedouble;

    item = cJSON_GetObjectItem(root, "cpu_time_sec");
    if (cJSON_IsNumber(item)) result->cpu_time_sec = item->valuedouble;

    cJSON_Delete(root);
    return result;
}

void distributed_job_result_free(DistributedJobResult* result) {
    if (!result) return;

    free(result->job_id);
    free(result->stdout_output);
    free(result->stderr_output);

    for (size_t i = 0; i < result->artifact_count; i++) {
        free(result->artifact_paths[i]);
        free(result->artifact_hashes[i]);
    }
    free(result->artifact_paths);
    free(result->artifact_hashes);

    free(result);
}

/* ============================================================
 * System Info Serialization
 * ============================================================ */

char* worker_system_info_to_json(const WorkerSystemInfo* info) {
    if (!info) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    if (info->arch) cJSON_AddStringToObject(root, "arch", info->arch);
    if (info->os) cJSON_AddStringToObject(root, "os", info->os);
    if (info->os_version) cJSON_AddStringToObject(root, "os_version", info->os_version);
    cJSON_AddNumberToObject(root, "cpu_cores", info->cpu_cores);
    cJSON_AddNumberToObject(root, "cpu_threads", info->cpu_threads);
    cJSON_AddNumberToObject(root, "memory_mb", (double)info->memory_mb);
    cJSON_AddNumberToObject(root, "disk_free_mb", (double)info->disk_free_mb);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

WorkerSystemInfo* worker_system_info_from_json(const char* json) {
    if (!json) return NULL;

    cJSON* root = cJSON_Parse(json);
    if (!root) return NULL;

    WorkerSystemInfo* info = (WorkerSystemInfo*)calloc(1, sizeof(WorkerSystemInfo));
    if (!info) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON* item;

    item = cJSON_GetObjectItem(root, "arch");
    if (cJSON_IsString(item)) info->arch = strdup(item->valuestring);

    item = cJSON_GetObjectItem(root, "os");
    if (cJSON_IsString(item)) info->os = strdup(item->valuestring);

    item = cJSON_GetObjectItem(root, "os_version");
    if (cJSON_IsString(item)) info->os_version = strdup(item->valuestring);

    item = cJSON_GetObjectItem(root, "cpu_cores");
    if (cJSON_IsNumber(item)) info->cpu_cores = (int)item->valuedouble;

    item = cJSON_GetObjectItem(root, "cpu_threads");
    if (cJSON_IsNumber(item)) info->cpu_threads = (int)item->valuedouble;

    item = cJSON_GetObjectItem(root, "memory_mb");
    if (cJSON_IsNumber(item)) info->memory_mb = (uint64_t)item->valuedouble;

    item = cJSON_GetObjectItem(root, "disk_free_mb");
    if (cJSON_IsNumber(item)) info->disk_free_mb = (uint64_t)item->valuedouble;

    cJSON_Delete(root);
    return info;
}

void worker_system_info_free(WorkerSystemInfo* info) {
    if (!info) return;

    free(info->arch);
    free(info->os);
    free(info->os_version);
    free(info);
}
