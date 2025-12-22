/**
 * @file message_bus.c
 * @brief Message bus implementation for inter-agent communication
 */

#include "cyxmake/agent_comm.h"
#include "cyxmake/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef CYXMAKE_WINDOWS
    #include <windows.h>
#else
    #include <uuid/uuid.h>
#endif

/* ============================================================================
 * String Conversions
 * ============================================================================ */

const char* message_type_to_string(AgentMessageType type) {
    switch (type) {
        case MSG_TYPE_TASK_REQUEST:     return "task_request";
        case MSG_TYPE_TASK_RESPONSE:    return "task_response";
        case MSG_TYPE_STATUS_UPDATE:    return "status_update";
        case MSG_TYPE_ERROR_REPORT:     return "error_report";
        case MSG_TYPE_RESOURCE_REQUEST: return "resource_request";
        case MSG_TYPE_RESOURCE_RELEASE: return "resource_release";
        case MSG_TYPE_RESOURCE_CONFLICT:return "resource_conflict";
        case MSG_TYPE_TERMINATE:        return "terminate";
        case MSG_TYPE_SYNC:             return "sync";
        case MSG_TYPE_CONTEXT_SHARE:    return "context_share";
        case MSG_TYPE_BROADCAST:        return "broadcast";
        case MSG_TYPE_HEARTBEAT:        return "heartbeat";
        case MSG_TYPE_CUSTOM:           return "custom";
        default:                        return "unknown";
    }
}

const char* message_status_to_string(MessageStatus status) {
    switch (status) {
        case MSG_STATUS_PENDING:      return "pending";
        case MSG_STATUS_DELIVERED:    return "delivered";
        case MSG_STATUS_ACKNOWLEDGED: return "acknowledged";
        case MSG_STATUS_FAILED:       return "failed";
        case MSG_STATUS_TIMEOUT:      return "timeout";
        default:                      return "unknown";
    }
}

char* message_generate_id(void) {
    char* id = (char*)malloc(48);
    if (!id) return NULL;

#ifdef CYXMAKE_WINDOWS
    snprintf(id, 48, "msg-%08lx-%04x-%04x",
             (unsigned long)time(NULL),
             (unsigned int)(rand() & 0xFFFF),
             (unsigned int)(rand() & 0xFFFF));
#else
    uuid_t uuid;
    uuid_generate(uuid);
    strcpy(id, "msg-");
    uuid_unparse_lower(uuid, id + 4);
#endif

    return id;
}

/* ============================================================================
 * Message Lifecycle
 * ============================================================================ */

AgentMessage* message_create(AgentMessageType type, const char* sender_id,
                             const char* receiver_id, const char* payload_json) {
    AgentMessage* msg = (AgentMessage*)calloc(1, sizeof(AgentMessage));
    if (!msg) {
        log_error("Failed to allocate message");
        return NULL;
    }

    msg->id = message_generate_id();
    if (!msg->id) {
        free(msg);
        return NULL;
    }

    msg->type = type;
    msg->sender_id = sender_id ? strdup(sender_id) : NULL;
    msg->receiver_id = receiver_id ? strdup(receiver_id) : NULL;
    msg->payload_json = payload_json ? strdup(payload_json) : NULL;
    msg->payload_size = payload_json ? strlen(payload_json) : 0;
    msg->priority = 0;
    msg->status = MSG_STATUS_PENDING;
    msg->created_at = time(NULL);
    msg->expects_response = false;
    msg->next = NULL;

    return msg;
}

void message_free(AgentMessage* msg) {
    if (!msg) return;

    free(msg->id);
    free(msg->sender_id);
    free(msg->sender_name);
    free(msg->receiver_id);
    free(msg->payload_json);
    free(msg->correlation_id);
    free(msg);
}

AgentMessage* message_create_response(AgentMessage* request,
                                      const char* payload_json) {
    if (!request) return NULL;

    AgentMessage* response = message_create(
        MSG_TYPE_TASK_RESPONSE,
        request->receiver_id,   /* Response sender = request receiver */
        request->sender_id,     /* Response receiver = request sender */
        payload_json
    );

    if (response) {
        response->correlation_id = request->id ? strdup(request->id) : NULL;
    }

    return response;
}

/* ============================================================================
 * Message Bus Lifecycle
 * ============================================================================ */

MessageBus* message_bus_create(void) {
    MessageBus* bus = (MessageBus*)calloc(1, sizeof(MessageBus));
    if (!bus) {
        log_error("Failed to allocate message bus");
        return NULL;
    }

    bus->queue_capacity = 16;
    bus->queues = (AgentMessage**)calloc(bus->queue_capacity,
                                         sizeof(AgentMessage*));
    bus->queue_ids = (char**)calloc(bus->queue_capacity, sizeof(char*));

    if (!bus->queues || !bus->queue_ids) {
        log_error("Failed to allocate message queues");
        free(bus->queues);
        free(bus->queue_ids);
        free(bus);
        return NULL;
    }

    bus->subscription_capacity = 16;
    bus->subscriptions = (MessageSubscription*)calloc(
        bus->subscription_capacity, sizeof(MessageSubscription));

    if (!bus->subscriptions) {
        log_error("Failed to allocate subscriptions");
        free(bus->queues);
        free(bus->queue_ids);
        free(bus);
        return NULL;
    }

    if (!mutex_init(&bus->mutex)) {
        log_error("Failed to initialize bus mutex");
        free(bus->subscriptions);
        free(bus->queues);
        free(bus->queue_ids);
        free(bus);
        return NULL;
    }

    if (!condition_init(&bus->message_available)) {
        log_error("Failed to initialize bus condition");
        mutex_destroy(&bus->mutex);
        free(bus->subscriptions);
        free(bus->queues);
        free(bus->queue_ids);
        free(bus);
        return NULL;
    }

    bus->default_timeout_ms = 30000; /* 30 seconds */
    bus->max_queue_size = 1000;
    bus->shutdown = false;

    log_debug("Message bus created");
    return bus;
}

void message_bus_free(MessageBus* bus) {
    if (!bus) return;

    message_bus_shutdown(bus);

    mutex_lock(&bus->mutex);

    /* Free all queued messages */
    for (size_t i = 0; i < bus->queue_count; i++) {
        AgentMessage* msg = bus->queues[i];
        while (msg) {
            AgentMessage* next = msg->next;
            message_free(msg);
            msg = next;
        }
        free(bus->queue_ids[i]);
    }

    /* Free subscriptions */
    for (size_t i = 0; i < bus->subscription_count; i++) {
        free(bus->subscriptions[i].agent_id);
    }

    mutex_unlock(&bus->mutex);

    condition_destroy(&bus->message_available);
    mutex_destroy(&bus->mutex);

    free(bus->queues);
    free(bus->queue_ids);
    free(bus->subscriptions);
    free(bus);

    log_debug("Message bus destroyed");
}

/* ============================================================================
 * Queue Management
 * ============================================================================ */

static int find_or_create_queue(MessageBus* bus, const char* agent_id) {
    /* Find existing queue */
    for (size_t i = 0; i < bus->queue_count; i++) {
        if (strcmp(bus->queue_ids[i], agent_id) == 0) {
            return (int)i;
        }
    }

    /* Create new queue */
    if (bus->queue_count >= bus->queue_capacity) {
        size_t new_cap = bus->queue_capacity * 2;
        AgentMessage** new_queues = (AgentMessage**)realloc(
            bus->queues, new_cap * sizeof(AgentMessage*));
        char** new_ids = (char**)realloc(
            bus->queue_ids, new_cap * sizeof(char*));

        if (!new_queues || !new_ids) {
            return -1;
        }

        bus->queues = new_queues;
        bus->queue_ids = new_ids;
        bus->queue_capacity = new_cap;
    }

    int idx = (int)bus->queue_count;
    bus->queue_ids[idx] = strdup(agent_id);
    bus->queues[idx] = NULL;
    bus->queue_count++;

    return idx;
}

static void enqueue_message(MessageBus* bus, int queue_idx, AgentMessage* msg) {
    msg->next = NULL;

    if (!bus->queues[queue_idx]) {
        bus->queues[queue_idx] = msg;
    } else {
        /* Add to end of queue */
        AgentMessage* tail = bus->queues[queue_idx];
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = msg;
    }
}

static AgentMessage* dequeue_message(MessageBus* bus, int queue_idx) {
    AgentMessage* msg = bus->queues[queue_idx];
    if (msg) {
        bus->queues[queue_idx] = msg->next;
        msg->next = NULL;
    }
    return msg;
}

/* ============================================================================
 * Send/Receive Operations
 * ============================================================================ */

bool message_bus_send(MessageBus* bus, AgentMessage* msg) {
    if (!bus || !msg || !msg->receiver_id) {
        log_error("Invalid message send parameters");
        message_free(msg);
        return false;
    }

    mutex_lock(&bus->mutex);

    if (bus->shutdown) {
        mutex_unlock(&bus->mutex);
        message_free(msg);
        return false;
    }

    int queue_idx = find_or_create_queue(bus, msg->receiver_id);
    if (queue_idx < 0) {
        mutex_unlock(&bus->mutex);
        message_free(msg);
        return false;
    }

    enqueue_message(bus, queue_idx, msg);
    msg->status = MSG_STATUS_DELIVERED;
    msg->delivered_at = time(NULL);

    /* Notify subscribed handlers */
    for (size_t i = 0; i < bus->subscription_count; i++) {
        MessageSubscription* sub = &bus->subscriptions[i];
        if (strcmp(sub->agent_id, msg->receiver_id) == 0 &&
            (sub->type == (AgentMessageType)-1 || sub->type == msg->type)) {
            if (sub->handler) {
                /* Copy message for handler (handler shouldn't free it) */
                sub->handler(msg, sub->context);
            }
        }
    }

    condition_broadcast(&bus->message_available);
    mutex_unlock(&bus->mutex);

    log_debug("Message '%s' sent to '%s'", msg->id, msg->receiver_id);
    return true;
}

bool message_bus_broadcast(MessageBus* bus, AgentMessage* msg) {
    if (!bus || !msg) {
        message_free(msg);
        return false;
    }

    mutex_lock(&bus->mutex);

    if (bus->shutdown) {
        mutex_unlock(&bus->mutex);
        message_free(msg);
        return false;
    }

    /* Send to all queues except sender */
    for (size_t i = 0; i < bus->queue_count; i++) {
        if (msg->sender_id && strcmp(bus->queue_ids[i], msg->sender_id) == 0) {
            continue; /* Don't send to self */
        }

        /* Clone message for each recipient */
        AgentMessage* clone = message_create(msg->type, msg->sender_id,
                                             bus->queue_ids[i], msg->payload_json);
        if (clone) {
            clone->priority = msg->priority;
            enqueue_message(bus, (int)i, clone);
        }
    }

    message_free(msg);
    condition_broadcast(&bus->message_available);
    mutex_unlock(&bus->mutex);

    return true;
}

AgentMessage* message_bus_receive(MessageBus* bus, const char* agent_id) {
    if (!bus || !agent_id) return NULL;

    mutex_lock(&bus->mutex);

    int queue_idx = find_or_create_queue(bus, agent_id);
    if (queue_idx < 0) {
        mutex_unlock(&bus->mutex);
        return NULL;
    }

    while (!bus->queues[queue_idx] && !bus->shutdown) {
        condition_wait(&bus->message_available, &bus->mutex);
    }

    if (bus->shutdown && !bus->queues[queue_idx]) {
        mutex_unlock(&bus->mutex);
        return NULL;
    }

    AgentMessage* msg = dequeue_message(bus, queue_idx);
    mutex_unlock(&bus->mutex);

    return msg;
}

AgentMessage* message_bus_receive_timeout(MessageBus* bus, const char* agent_id,
                                          int timeout_ms) {
    if (!bus || !agent_id) return NULL;

    mutex_lock(&bus->mutex);

    int queue_idx = find_or_create_queue(bus, agent_id);
    if (queue_idx < 0) {
        mutex_unlock(&bus->mutex);
        return NULL;
    }

    if (!bus->queues[queue_idx] && !bus->shutdown) {
        condition_timedwait(&bus->message_available, &bus->mutex, timeout_ms);
    }

    AgentMessage* msg = NULL;
    if (bus->queues[queue_idx]) {
        msg = dequeue_message(bus, queue_idx);
    }

    mutex_unlock(&bus->mutex);
    return msg;
}

AgentMessage* message_bus_try_receive(MessageBus* bus, const char* agent_id) {
    if (!bus || !agent_id) return NULL;

    mutex_lock(&bus->mutex);

    int queue_idx = -1;
    for (size_t i = 0; i < bus->queue_count; i++) {
        if (strcmp(bus->queue_ids[i], agent_id) == 0) {
            queue_idx = (int)i;
            break;
        }
    }

    AgentMessage* msg = NULL;
    if (queue_idx >= 0 && bus->queues[queue_idx]) {
        msg = dequeue_message(bus, queue_idx);
    }

    mutex_unlock(&bus->mutex);
    return msg;
}

AgentMessage* message_bus_request(MessageBus* bus, AgentMessage* request,
                                  int timeout_ms) {
    if (!bus || !request || !request->receiver_id || !request->sender_id) {
        message_free(request);
        return NULL;
    }

    request->expects_response = true;
    char* correlation_id = request->id ? strdup(request->id) : NULL;

    if (!message_bus_send(bus, request)) {
        free(correlation_id);
        return NULL;
    }

    /* Wait for response with matching correlation ID */
    int elapsed = 0;
    int interval = 50;

    while (elapsed < timeout_ms) {
        AgentMessage* msg = message_bus_receive_timeout(bus, request->sender_id,
                                                        interval);
        if (msg) {
            if (msg->correlation_id && correlation_id &&
                strcmp(msg->correlation_id, correlation_id) == 0) {
                free(correlation_id);
                return msg;
            }
            /* Not our response, put it back (simplified - should use priority) */
            message_bus_send(bus, msg);
        }
        elapsed += interval;
    }

    free(correlation_id);
    return NULL; /* Timeout */
}

/* ============================================================================
 * Subscriptions
 * ============================================================================ */

bool message_bus_subscribe(MessageBus* bus, const char* agent_id,
                           AgentMessageType type, MessageHandler handler,
                           void* context) {
    if (!bus || !agent_id || !handler) return false;

    mutex_lock(&bus->mutex);

    /* Check for existing subscription */
    for (size_t i = 0; i < bus->subscription_count; i++) {
        if (strcmp(bus->subscriptions[i].agent_id, agent_id) == 0 &&
            bus->subscriptions[i].type == type) {
            /* Update existing */
            bus->subscriptions[i].handler = handler;
            bus->subscriptions[i].context = context;
            mutex_unlock(&bus->mutex);
            return true;
        }
    }

    /* Add new subscription */
    if (bus->subscription_count >= bus->subscription_capacity) {
        size_t new_cap = bus->subscription_capacity * 2;
        MessageSubscription* new_subs = (MessageSubscription*)realloc(
            bus->subscriptions, new_cap * sizeof(MessageSubscription));
        if (!new_subs) {
            mutex_unlock(&bus->mutex);
            return false;
        }
        bus->subscriptions = new_subs;
        bus->subscription_capacity = new_cap;
    }

    MessageSubscription* sub = &bus->subscriptions[bus->subscription_count++];
    sub->agent_id = strdup(agent_id);
    sub->type = type;
    sub->handler = handler;
    sub->context = context;

    /* Ensure queue exists for this agent */
    find_or_create_queue(bus, agent_id);

    mutex_unlock(&bus->mutex);
    return true;
}

void message_bus_unsubscribe(MessageBus* bus, const char* agent_id) {
    if (!bus || !agent_id) return;

    mutex_lock(&bus->mutex);

    size_t i = 0;
    while (i < bus->subscription_count) {
        if (strcmp(bus->subscriptions[i].agent_id, agent_id) == 0) {
            free(bus->subscriptions[i].agent_id);
            /* Move last subscription to this slot */
            bus->subscriptions[i] = bus->subscriptions[--bus->subscription_count];
        } else {
            i++;
        }
    }

    mutex_unlock(&bus->mutex);
}

int message_bus_pending_count(MessageBus* bus, const char* agent_id) {
    if (!bus || !agent_id) return 0;

    mutex_lock(&bus->mutex);

    int count = 0;
    for (size_t i = 0; i < bus->queue_count; i++) {
        if (strcmp(bus->queue_ids[i], agent_id) == 0) {
            AgentMessage* msg = bus->queues[i];
            while (msg) {
                count++;
                msg = msg->next;
            }
            break;
        }
    }

    mutex_unlock(&bus->mutex);
    return count;
}

void message_bus_acknowledge(MessageBus* bus, AgentMessage* msg) {
    if (!bus || !msg) return;
    msg->status = MSG_STATUS_ACKNOWLEDGED;
}

void message_bus_shutdown(MessageBus* bus) {
    if (!bus) return;

    mutex_lock(&bus->mutex);
    bus->shutdown = true;
    condition_broadcast(&bus->message_available);
    mutex_unlock(&bus->mutex);
}
