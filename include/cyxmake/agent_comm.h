/**
 * @file agent_comm.h
 * @brief Agent Communication - Hybrid MessageBus + SharedState
 *
 * Provides inter-agent communication via:
 * - MessageBus: Async pub/sub for commands and events
 * - SharedState: Thread-safe key-value store for context sharing
 */

#ifndef CYXMAKE_AGENT_COMM_H
#define CYXMAKE_AGENT_COMM_H

#include "cyxmake/threading.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct AgentInstance AgentInstance;

/* ============================================================================
 * Message Types
 * ============================================================================ */

/**
 * Types of messages that can be sent between agents
 */
typedef enum {
    MSG_TYPE_TASK_REQUEST,       /* Request agent to perform task */
    MSG_TYPE_TASK_RESPONSE,      /* Task completion result */
    MSG_TYPE_STATUS_UPDATE,      /* Progress update */
    MSG_TYPE_ERROR_REPORT,       /* Error notification */
    MSG_TYPE_RESOURCE_REQUEST,   /* Request to use shared resource */
    MSG_TYPE_RESOURCE_RELEASE,   /* Release shared resource */
    MSG_TYPE_RESOURCE_CONFLICT,  /* Resource conflict notification */
    MSG_TYPE_TERMINATE,          /* Request agent termination */
    MSG_TYPE_SYNC,               /* Synchronization point */
    MSG_TYPE_CONTEXT_SHARE,      /* Share context data */
    MSG_TYPE_BROADCAST,          /* Broadcast to all agents */
    MSG_TYPE_HEARTBEAT,          /* Agent alive signal */
    MSG_TYPE_CUSTOM              /* User-defined message type */
} AgentMessageType;

/**
 * Message delivery status
 */
typedef enum {
    MSG_STATUS_PENDING,          /* Waiting to be delivered */
    MSG_STATUS_DELIVERED,        /* Delivered to recipient */
    MSG_STATUS_ACKNOWLEDGED,     /* Recipient acknowledged */
    MSG_STATUS_FAILED,           /* Delivery failed */
    MSG_STATUS_TIMEOUT           /* Delivery timed out */
} MessageStatus;

/* ============================================================================
 * Agent Message
 * ============================================================================ */

/**
 * A message exchanged between agents
 */
typedef struct AgentMessage {
    /* Identity */
    char* id;                    /* Unique message ID */
    AgentMessageType type;
    int priority;                /* Higher = more urgent */

    /* Routing */
    char* sender_id;             /* Sending agent ID */
    char* sender_name;           /* Sending agent name (for display) */
    char* receiver_id;           /* Receiving agent ID (NULL = broadcast) */

    /* Payload */
    char* payload_json;          /* JSON-encoded message data */
    size_t payload_size;         /* Size of payload */

    /* Request/Response correlation */
    char* correlation_id;        /* Links response to request */
    bool expects_response;       /* True if sender expects reply */

    /* Delivery tracking */
    MessageStatus status;
    time_t created_at;
    time_t delivered_at;

    /* Internal linkage */
    struct AgentMessage* next;
} AgentMessage;

/* ============================================================================
 * Message Bus
 * ============================================================================ */

/**
 * Handler function for received messages
 */
typedef void (*MessageHandler)(AgentMessage* msg, void* context);

/**
 * Subscription to message types
 */
typedef struct {
    char* agent_id;              /* Subscribing agent */
    AgentMessageType type;       /* Message type filter (or -1 for all) */
    MessageHandler handler;
    void* context;
} MessageSubscription;

/**
 * Message bus for async inter-agent communication
 */
typedef struct MessageBus {
    /* Message queues - one per recipient */
    AgentMessage** queues;       /* Array of message queue heads */
    char** queue_ids;            /* Agent IDs for each queue */
    size_t queue_count;
    size_t queue_capacity;

    /* Subscriptions */
    MessageSubscription* subscriptions;
    size_t subscription_count;
    size_t subscription_capacity;

    /* Thread safety */
    MutexHandle mutex;
    ConditionHandle message_available;

    /* Configuration */
    int default_timeout_ms;
    int max_queue_size;
    bool shutdown;
} MessageBus;

/* ============================================================================
 * Message Bus API
 * ============================================================================ */

/**
 * Create a message bus
 */
MessageBus* message_bus_create(void);

/**
 * Destroy the message bus
 */
void message_bus_free(MessageBus* bus);

/**
 * Create a message
 *
 * @param type Message type
 * @param sender_id Sending agent ID
 * @param receiver_id Receiving agent ID (NULL for broadcast)
 * @param payload_json JSON payload (copied)
 * @return New message or NULL on failure
 */
AgentMessage* message_create(AgentMessageType type, const char* sender_id,
                             const char* receiver_id, const char* payload_json);

/**
 * Free a message
 */
void message_free(AgentMessage* msg);

/**
 * Send a message to a specific agent
 *
 * @param bus The message bus
 * @param msg Message to send (bus takes ownership)
 * @return true on success
 */
bool message_bus_send(MessageBus* bus, AgentMessage* msg);

/**
 * Broadcast a message to all agents
 *
 * @param bus The message bus
 * @param msg Message to broadcast (bus takes ownership)
 * @return true on success
 */
bool message_bus_broadcast(MessageBus* bus, AgentMessage* msg);

/**
 * Receive a message for a specific agent (blocking)
 *
 * @param bus The message bus
 * @param agent_id Agent ID to receive for
 * @return Next message or NULL if shutdown
 */
AgentMessage* message_bus_receive(MessageBus* bus, const char* agent_id);

/**
 * Receive with timeout
 *
 * @param bus The message bus
 * @param agent_id Agent ID to receive for
 * @param timeout_ms Maximum wait time
 * @return Message or NULL on timeout/shutdown
 */
AgentMessage* message_bus_receive_timeout(MessageBus* bus, const char* agent_id,
                                          int timeout_ms);

/**
 * Try to receive without blocking
 *
 * @param bus The message bus
 * @param agent_id Agent ID to receive for
 * @return Message or NULL if none available
 */
AgentMessage* message_bus_try_receive(MessageBus* bus, const char* agent_id);

/**
 * Send a request and wait for response
 *
 * @param bus The message bus
 * @param request Request message (bus takes ownership)
 * @param timeout_ms Maximum wait time for response
 * @return Response message or NULL on timeout (caller must free)
 */
AgentMessage* message_bus_request(MessageBus* bus, AgentMessage* request,
                                  int timeout_ms);

/**
 * Subscribe to messages
 *
 * @param bus The message bus
 * @param agent_id Subscribing agent ID
 * @param type Message type to subscribe to (-1 for all)
 * @param handler Handler function
 * @param context User context passed to handler
 * @return true on success
 */
bool message_bus_subscribe(MessageBus* bus, const char* agent_id,
                           AgentMessageType type, MessageHandler handler,
                           void* context);

/**
 * Unsubscribe from messages
 *
 * @param bus The message bus
 * @param agent_id Agent ID to unsubscribe
 */
void message_bus_unsubscribe(MessageBus* bus, const char* agent_id);

/**
 * Get pending message count for an agent
 *
 * @param bus The message bus
 * @param agent_id Agent ID to check
 * @return Number of pending messages
 */
int message_bus_pending_count(MessageBus* bus, const char* agent_id);

/**
 * Acknowledge a message
 *
 * @param bus The message bus
 * @param msg Message to acknowledge
 */
void message_bus_acknowledge(MessageBus* bus, AgentMessage* msg);

/**
 * Shutdown the message bus
 */
void message_bus_shutdown(MessageBus* bus);

/* ============================================================================
 * Shared State
 * ============================================================================ */

/**
 * A single entry in the shared state
 */
typedef struct StateEntry {
    char* key;
    char* value;
    char* locked_by;             /* Agent ID holding lock (NULL if unlocked) */
    time_t created_at;
    time_t modified_at;
    time_t locked_at;
    struct StateEntry* next;
} StateEntry;

/**
 * Thread-safe shared state store
 */
typedef struct SharedState {
    StateEntry** buckets;        /* Hash table buckets */
    size_t bucket_count;
    size_t entry_count;

    MutexHandle mutex;

    /* Persistence */
    char* persistence_path;      /* Path to save state */
    bool dirty;                  /* Has unsaved changes */
} SharedState;

/* ============================================================================
 * Shared State API
 * ============================================================================ */

/**
 * Create a shared state store
 */
SharedState* shared_state_create(void);

/**
 * Destroy the shared state store
 */
void shared_state_free(SharedState* state);

/**
 * Set a value in shared state
 *
 * @param state The shared state
 * @param key Key to set
 * @param value Value to set (copied)
 * @return true on success
 */
bool shared_state_set(SharedState* state, const char* key, const char* value);

/**
 * Get a value from shared state
 *
 * @param state The shared state
 * @param key Key to get
 * @return Value (caller must free) or NULL if not found
 */
char* shared_state_get(SharedState* state, const char* key);

/**
 * Check if a key exists
 *
 * @param state The shared state
 * @param key Key to check
 * @return true if key exists
 */
bool shared_state_exists(SharedState* state, const char* key);

/**
 * Delete a key from shared state
 *
 * @param state The shared state
 * @param key Key to delete
 * @return true if key was deleted
 */
bool shared_state_delete(SharedState* state, const char* key);

/**
 * Lock a key for exclusive access
 *
 * @param state The shared state
 * @param key Key to lock
 * @param agent_id Agent requesting lock
 * @return true if lock acquired
 */
bool shared_state_lock(SharedState* state, const char* key, const char* agent_id);

/**
 * Try to lock without blocking
 *
 * @param state The shared state
 * @param key Key to lock
 * @param agent_id Agent requesting lock
 * @return true if lock acquired
 */
bool shared_state_trylock(SharedState* state, const char* key, const char* agent_id);

/**
 * Unlock a key
 *
 * @param state The shared state
 * @param key Key to unlock
 * @param agent_id Agent releasing lock (must match locker)
 * @return true if unlocked
 */
bool shared_state_unlock(SharedState* state, const char* key, const char* agent_id);

/**
 * Check if a key is locked
 *
 * @param state The shared state
 * @param key Key to check
 * @return Agent ID holding lock, or NULL if unlocked (do not free)
 */
const char* shared_state_locked_by(SharedState* state, const char* key);

/**
 * Get all keys in shared state
 *
 * @param state The shared state
 * @param count Output: number of keys
 * @return Array of keys (caller must free array and each string)
 */
char** shared_state_keys(SharedState* state, int* count);

/**
 * Get keys matching a prefix
 *
 * @param state The shared state
 * @param prefix Key prefix to match
 * @param count Output: number of matching keys
 * @return Array of keys (caller must free array and each string)
 */
char** shared_state_keys_prefix(SharedState* state, const char* prefix, int* count);

/**
 * Clear all entries
 *
 * @param state The shared state
 */
void shared_state_clear(SharedState* state);

/**
 * Set persistence path for saving state
 *
 * @param state The shared state
 * @param path File path for persistence
 */
void shared_state_set_persistence(SharedState* state, const char* path);

/**
 * Save state to persistence file
 *
 * @param state The shared state
 * @return true on success
 */
bool shared_state_save(SharedState* state);

/**
 * Load state from persistence file
 *
 * @param state The shared state
 * @return true on success
 */
bool shared_state_load(SharedState* state);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Get string representation of message type
 */
const char* message_type_to_string(AgentMessageType type);

/**
 * Get string representation of message status
 */
const char* message_status_to_string(MessageStatus status);

/**
 * Generate a unique message ID
 */
char* message_generate_id(void);

/**
 * Create a response message to a request
 *
 * @param request Original request message
 * @param payload_json Response payload
 * @return Response message or NULL
 */
AgentMessage* message_create_response(AgentMessage* request,
                                      const char* payload_json);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_AGENT_COMM_H */
