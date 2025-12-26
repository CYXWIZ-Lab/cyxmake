/**
 * @file network_client.c
 * @brief WebSocket client implementation for distributed builds
 *
 * Implements the worker-side network client using libwebsockets.
 * Connects to coordinator, handles auto-reconnection, and manages message I/O.
 */

#include "cyxmake/distributed/network_transport.h"
#include "cyxmake/distributed/protocol.h"
#include "cyxmake/logger.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef CYXMAKE_ENABLE_DISTRIBUTED

#include <libwebsockets.h>
#include "cyxmake/threading.h"

/* ============================================================
 * Constants
 * ============================================================ */

#define CLIENT_RX_BUFFER_SIZE (64 * 1024)  /* 64KB receive buffer */
#define MAX_PENDING_MESSAGES 64

/* ============================================================
 * Message Queue Entry
 * ============================================================ */

typedef struct MessageQueueEntry {
    uint8_t* data;
    size_t len;
    struct MessageQueueEntry* next;
} MessageQueueEntry;

/* ============================================================
 * Network Client Structure
 * ============================================================ */

struct NetworkClient {
    NetworkConfig config;
    struct lws_context* context;
    struct lws* wsi;                       /* WebSocket instance */

    /* Connection info */
    char* url;
    char* host;
    char* path;
    int port;
    bool use_ssl;

    /* State */
    TransportState state;
    volatile bool running;
    volatile bool should_reconnect;

    /* Auto-reconnect settings */
    bool auto_reconnect;
    int reconnect_delay_ms;
    int max_reconnect_attempts;
    int reconnect_attempts;

    /* Message queue (outgoing) */
    MessageQueueEntry* msg_queue_head;
    MessageQueueEntry* msg_queue_tail;
    int msg_queue_count;
    MutexHandle queue_mutex;

    /* Receive buffer for fragmented messages */
    uint8_t* rx_buffer;
    size_t rx_buffer_size;
    size_t rx_buffer_used;

    /* Callbacks */
    NetworkClientCallbacks callbacks;

    /* Service thread */
    ThreadHandle service_thread;

    /* Virtual connection for callback compatibility */
    NetworkConnection connection;
};

/* ============================================================
 * URL Parsing
 * ============================================================ */

static bool parse_websocket_url(const char* url, char** host, int* port,
                                 char** path, bool* use_ssl) {
    if (!url || !host || !port || !path || !use_ssl) {
        return false;
    }

    *host = NULL;
    *path = NULL;
    *port = 80;
    *use_ssl = false;

    /* Check protocol */
    const char* start = url;
    if (strncmp(url, "wss://", 6) == 0) {
        *use_ssl = true;
        *port = 443;
        start = url + 6;
    } else if (strncmp(url, "ws://", 5) == 0) {
        *use_ssl = false;
        *port = 80;
        start = url + 5;
    } else {
        log_error("Invalid WebSocket URL: %s (must start with ws:// or wss://)", url);
        return false;
    }

    /* Find host end (port separator or path) */
    const char* host_end = start;
    while (*host_end && *host_end != ':' && *host_end != '/') {
        host_end++;
    }

    size_t host_len = host_end - start;
    if (host_len == 0) {
        log_error("Invalid WebSocket URL: empty host");
        return false;
    }

    *host = malloc(host_len + 1);
    if (!*host) return false;
    memcpy(*host, start, host_len);
    (*host)[host_len] = '\0';

    /* Parse port if present */
    if (*host_end == ':') {
        host_end++;
        char* port_end;
        long port_num = strtol(host_end, &port_end, 10);
        if (port_num <= 0 || port_num > 65535) {
            log_error("Invalid port in URL: %s", url);
            free(*host);
            *host = NULL;
            return false;
        }
        *port = (int)port_num;
        host_end = port_end;
    }

    /* Parse path */
    if (*host_end == '/') {
        size_t path_len = strlen(host_end);
        *path = malloc(path_len + 1);
        if (!*path) {
            free(*host);
            *host = NULL;
            return false;
        }
        strcpy(*path, host_end);
    } else {
        *path = strdup("/");
        if (!*path) {
            free(*host);
            *host = NULL;
            return false;
        }
    }

    return true;
}

/* ============================================================
 * Message Queue Operations
 * ============================================================ */

static bool queue_message(NetworkClient* client, const uint8_t* data, size_t len) {
    if (client->msg_queue_count >= MAX_PENDING_MESSAGES) {
        log_warning("Client message queue full, dropping message");
        return false;
    }

    MessageQueueEntry* entry = malloc(sizeof(MessageQueueEntry));
    if (!entry) return false;

    /* Allocate with LWS_PRE padding for libwebsockets */
    entry->data = malloc(LWS_PRE + len);
    if (!entry->data) {
        free(entry);
        return false;
    }

    memcpy(entry->data + LWS_PRE, data, len);
    entry->len = len;
    entry->next = NULL;

    mutex_lock(client->queue_mutex);

    if (client->msg_queue_tail) {
        client->msg_queue_tail->next = entry;
    } else {
        client->msg_queue_head = entry;
    }
    client->msg_queue_tail = entry;
    client->msg_queue_count++;

    mutex_unlock(client->queue_mutex);

    /* Request write callback */
    if (client->wsi) {
        lws_callback_on_writable(client->wsi);
    }

    return true;
}

static MessageQueueEntry* dequeue_message(NetworkClient* client) {
    mutex_lock(client->queue_mutex);

    MessageQueueEntry* entry = client->msg_queue_head;
    if (entry) {
        client->msg_queue_head = entry->next;
        if (!client->msg_queue_head) {
            client->msg_queue_tail = NULL;
        }
        client->msg_queue_count--;
    }

    mutex_unlock(client->queue_mutex);
    return entry;
}

static void free_message_entry(MessageQueueEntry* entry) {
    if (entry) {
        free(entry->data);
        free(entry);
    }
}

static void clear_message_queue(NetworkClient* client) {
    MessageQueueEntry* entry;
    while ((entry = dequeue_message(client)) != NULL) {
        free_message_entry(entry);
    }
}

/* ============================================================
 * libwebsockets Client Callback
 * ============================================================ */

static int callback_cyxmake_client(struct lws* wsi, enum lws_callback_reasons reason,
                                    void* user, void* in, size_t len) {
    NetworkClient* client = (NetworkClient*)lws_context_user(lws_get_context(wsi));

    if (!client) {
        return 0;
    }

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED: {
            log_info("Connected to coordinator");
            client->state = TRANSPORT_CONNECTED;
            client->wsi = wsi;
            client->reconnect_attempts = 0;

            if (client->callbacks.on_connect) {
                client->callbacks.on_connect(&client->connection,
                                              client->callbacks.user_data);
            }

            /* Request write callback if messages pending */
            if (client->msg_queue_head) {
                lws_callback_on_writable(wsi);
            }
            break;
        }

        case LWS_CALLBACK_CLIENT_RECEIVE: {
            /* Handle incoming message */
            const uint8_t* data = (const uint8_t*)in;

            /* Check for first fragment */
            if (lws_is_first_fragment(wsi)) {
                client->rx_buffer_used = 0;
            }

            /* Append to receive buffer */
            if (client->rx_buffer_used + len > client->rx_buffer_size) {
                /* Grow buffer */
                size_t new_size = client->rx_buffer_size * 2;
                while (new_size < client->rx_buffer_used + len) {
                    new_size *= 2;
                }
                uint8_t* new_buffer = realloc(client->rx_buffer, new_size);
                if (!new_buffer) {
                    log_error("Failed to grow receive buffer");
                    return -1;
                }
                client->rx_buffer = new_buffer;
                client->rx_buffer_size = new_size;
            }

            memcpy(client->rx_buffer + client->rx_buffer_used, data, len);
            client->rx_buffer_used += len;

            /* Check for final fragment */
            if (lws_is_final_fragment(wsi)) {
                /* Parse complete message */
                ProtocolMessage* msg = protocol_message_deserialize(
                    (const char*)client->rx_buffer, client->rx_buffer_used);

                if (msg) {
                    if (client->callbacks.on_message) {
                        client->callbacks.on_message(&client->connection, msg,
                                                      client->callbacks.user_data);
                    }
                    protocol_message_free(msg);
                } else {
                    log_warning("Failed to parse message from coordinator");
                }

                client->rx_buffer_used = 0;
            }
            break;
        }

        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            /* Send pending messages */
            MessageQueueEntry* entry = dequeue_message(client);
            if (entry) {
                int written = lws_write(wsi, entry->data + LWS_PRE,
                                        entry->len, LWS_WRITE_TEXT);
                if (written < (int)entry->len) {
                    log_error("Failed to send message: wrote %d of %zu bytes",
                              written, entry->len);
                }
                free_message_entry(entry);

                /* Request another write callback if more messages pending */
                if (client->msg_queue_head) {
                    lws_callback_on_writable(wsi);
                }
            }
            break;
        }

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR: {
            const char* error_msg = in ? (const char*)in : "Connection failed";
            log_error("Connection error: %s", error_msg);
            client->state = TRANSPORT_ERROR;
            client->wsi = NULL;

            if (client->callbacks.on_error) {
                client->callbacks.on_error(&client->connection, error_msg,
                                            client->callbacks.user_data);
            }

            /* Schedule reconnect if enabled */
            if (client->auto_reconnect && client->running) {
                client->should_reconnect = true;
            }
            break;
        }

        case LWS_CALLBACK_CLIENT_CLOSED: {
            log_info("Disconnected from coordinator");
            client->state = TRANSPORT_DISCONNECTED;
            client->wsi = NULL;

            if (client->callbacks.on_disconnect) {
                client->callbacks.on_disconnect(&client->connection,
                                                 "Connection closed",
                                                 client->callbacks.user_data);
            }

            /* Schedule reconnect if enabled */
            if (client->auto_reconnect && client->running) {
                client->should_reconnect = true;
            }
            break;
        }

        case LWS_CALLBACK_WSI_DESTROY: {
            if (client->wsi == wsi) {
                client->wsi = NULL;
            }
            break;
        }

        default:
            break;
    }

    return 0;
}

/* ============================================================
 * Client Service Thread
 * ============================================================ */

static const struct lws_protocols client_protocols[] = {
    {
        .name = "cyxmake-distributed",
        .callback = callback_cyxmake_client,
        .per_session_data_size = 0,
        .rx_buffer_size = CLIENT_RX_BUFFER_SIZE,
    },
    { NULL, NULL, 0, 0 }
};

static THREAD_RETURN_TYPE THREAD_CALL_CONVENTION client_service_thread(void* arg) {
    NetworkClient* client = (NetworkClient*)arg;

    log_debug("Client service thread started");

    while (client->running) {
        /* Handle reconnection */
        if (client->should_reconnect && !client->wsi) {
            if (client->reconnect_attempts < client->max_reconnect_attempts) {
                log_info("Attempting to reconnect (%d/%d)...",
                         client->reconnect_attempts + 1, client->max_reconnect_attempts);

                /* Wait before reconnecting */
#ifdef _WIN32
                Sleep(client->reconnect_delay_ms);
#else
                usleep(client->reconnect_delay_ms * 1000);
#endif

                /* Attempt connection */
                struct lws_client_connect_info ccinfo = {0};
                ccinfo.context = client->context;
                ccinfo.address = client->host;
                ccinfo.port = client->port;
                ccinfo.path = client->path;
                ccinfo.host = client->host;
                ccinfo.origin = client->host;
                ccinfo.protocol = client_protocols[0].name;
                ccinfo.ssl_connection = client->use_ssl ?
                    (LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED) : 0;

                client->state = TRANSPORT_CONNECTING;
                struct lws* wsi = lws_client_connect_via_info(&ccinfo);

                if (!wsi) {
                    log_warning("Reconnection attempt failed");
                    client->reconnect_attempts++;
                } else {
                    client->should_reconnect = false;
                }
            } else {
                log_error("Max reconnection attempts (%d) reached, giving up",
                          client->max_reconnect_attempts);
                client->should_reconnect = false;
                client->running = false;
            }
        }

        /* Service the event loop */
        lws_service(client->context, 50);
    }

    log_debug("Client service thread exiting");
    return (THREAD_RETURN_TYPE)0;
}

/* ============================================================
 * Public API Implementation
 * ============================================================ */

NetworkClient* network_client_create(const NetworkConfig* config) {
    NetworkClient* client = calloc(1, sizeof(NetworkClient));
    if (!client) {
        log_error("Failed to allocate network client");
        return NULL;
    }

    /* Copy configuration */
    if (config) {
        client->config = *config;
        if (config->bind_address) {
            client->config.bind_address = strdup(config->bind_address);
        }
        if (config->cert_path) {
            client->config.cert_path = strdup(config->cert_path);
        }
        if (config->key_path) {
            client->config.key_path = strdup(config->key_path);
        }
        if (config->ca_path) {
            client->config.ca_path = strdup(config->ca_path);
        }
    }

    /* Initialize state */
    client->state = TRANSPORT_DISCONNECTED;
    client->auto_reconnect = true;
    client->reconnect_delay_ms = 5000;
    client->max_reconnect_attempts = 10;

    /* Allocate receive buffer */
    client->rx_buffer_size = CLIENT_RX_BUFFER_SIZE;
    client->rx_buffer = malloc(client->rx_buffer_size);
    if (!client->rx_buffer) {
        log_error("Failed to allocate receive buffer");
        free(client);
        return NULL;
    }

    /* Create queue mutex */
    client->queue_mutex = mutex_create();
    if (!client->queue_mutex) {
        log_error("Failed to create queue mutex");
        free(client->rx_buffer);
        free(client);
        return NULL;
    }

    /* Initialize virtual connection */
    client->connection.id = strdup("client-connection");
    client->connection.state = TRANSPORT_DISCONNECTED;

    log_debug("Network client created");
    return client;
}

void network_client_set_callbacks(NetworkClient* client,
                                   const NetworkClientCallbacks* callbacks) {
    if (!client || !callbacks) return;
    client->callbacks = *callbacks;
}

bool network_client_connect(NetworkClient* client, const char* url) {
    if (!client || !url) {
        return false;
    }

    if (client->running) {
        log_warning("Client already connected or connecting");
        return false;
    }

    /* Parse URL */
    if (!parse_websocket_url(url, &client->host, &client->port,
                              &client->path, &client->use_ssl)) {
        return false;
    }

    client->url = strdup(url);

    log_info("Connecting to %s://%s:%d%s",
             client->use_ssl ? "wss" : "ws",
             client->host, client->port, client->path);

    /* Create libwebsockets context */
    struct lws_context_creation_info info = {0};
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = client_protocols;
    info.user = client;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    /* TLS configuration */
    if (client->use_ssl && client->config.ca_path) {
        info.client_ssl_ca_filepath = client->config.ca_path;
    }
    if (client->config.cert_path && client->config.key_path) {
        info.client_ssl_cert_filepath = client->config.cert_path;
        info.client_ssl_private_key_filepath = client->config.key_path;
    }

    client->context = lws_create_context(&info);
    if (!client->context) {
        log_error("Failed to create libwebsockets context");
        return false;
    }

    /* Initiate connection */
    struct lws_client_connect_info ccinfo = {0};
    ccinfo.context = client->context;
    ccinfo.address = client->host;
    ccinfo.port = client->port;
    ccinfo.path = client->path;
    ccinfo.host = client->host;
    ccinfo.origin = client->host;
    ccinfo.protocol = client_protocols[0].name;
    ccinfo.ssl_connection = client->use_ssl ?
        (LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED) : 0;

    client->state = TRANSPORT_CONNECTING;
    struct lws* wsi = lws_client_connect_via_info(&ccinfo);

    if (!wsi) {
        log_error("Failed to initiate connection");
        lws_context_destroy(client->context);
        client->context = NULL;
        client->state = TRANSPORT_ERROR;
        return false;
    }

    /* Start service thread */
    client->running = true;
    client->service_thread = thread_create(client_service_thread, client);

    if (!client->service_thread) {
        log_error("Failed to create service thread");
        client->running = false;
        lws_context_destroy(client->context);
        client->context = NULL;
        client->state = TRANSPORT_ERROR;
        return false;
    }

    return true;
}

void network_client_disconnect(NetworkClient* client) {
    if (!client) return;

    client->running = false;
    client->auto_reconnect = false;
    client->should_reconnect = false;

    if (client->wsi) {
        client->state = TRANSPORT_CLOSING;
        lws_set_timeout(client->wsi, PENDING_TIMEOUT_CLOSE_SEND, 1);
    }

    /* Wait for service thread */
    if (client->service_thread) {
        thread_join(client->service_thread);
        client->service_thread = NULL;
    }

    if (client->context) {
        lws_context_destroy(client->context);
        client->context = NULL;
    }

    client->wsi = NULL;
    client->state = TRANSPORT_DISCONNECTED;

    log_info("Client disconnected");
}

void network_client_free(NetworkClient* client) {
    if (!client) return;

    network_client_disconnect(client);

    /* Clear message queue */
    clear_message_queue(client);

    /* Free resources */
    if (client->queue_mutex) {
        mutex_destroy(client->queue_mutex);
    }

    free(client->rx_buffer);
    free(client->url);
    free(client->host);
    free(client->path);
    free(client->config.bind_address);
    free(client->config.cert_path);
    free(client->config.key_path);
    free(client->config.ca_path);
    free((void*)client->connection.id);

    free(client);
    log_debug("Network client freed");
}

bool network_client_send(NetworkClient* client, ProtocolMessage* msg) {
    if (!client || !msg) {
        return false;
    }

    if (client->state != TRANSPORT_CONNECTED) {
        log_warning("Cannot send message: not connected");
        return false;
    }

    /* Serialize message */
    size_t json_len;
    char* json = protocol_message_serialize(msg, &json_len);
    if (!json) {
        log_error("Failed to serialize message");
        return false;
    }

    /* Queue for sending */
    bool result = queue_message(client, (const uint8_t*)json, json_len);
    free(json);

    return result;
}

TransportState network_client_get_state(NetworkClient* client) {
    return client ? client->state : TRANSPORT_DISCONNECTED;
}

bool network_client_is_connected(NetworkClient* client) {
    return client && client->state == TRANSPORT_CONNECTED;
}

void network_client_set_auto_reconnect(NetworkClient* client,
                                        bool enabled,
                                        int delay_ms,
                                        int max_attempts) {
    if (!client) return;

    client->auto_reconnect = enabled;
    if (delay_ms > 0) {
        client->reconnect_delay_ms = delay_ms;
    }
    if (max_attempts > 0) {
        client->max_reconnect_attempts = max_attempts;
    }
}

/* Shared utility functions (connection accessors, transport_state_name, etc.)
 * are in network_common.c to avoid duplicate symbol errors */

#else /* !CYXMAKE_ENABLE_DISTRIBUTED */

/* ============================================================
 * Stub Implementations (when distributed is disabled)
 * ============================================================ */

NetworkClient* network_client_create(const NetworkConfig* config) {
    (void)config;
    log_warning("Distributed builds not enabled - network client unavailable");
    return NULL;
}

void network_client_set_callbacks(NetworkClient* client,
                                   const NetworkClientCallbacks* callbacks) {
    (void)client;
    (void)callbacks;
}

bool network_client_connect(NetworkClient* client, const char* url) {
    (void)client;
    (void)url;
    return false;
}

void network_client_disconnect(NetworkClient* client) {
    (void)client;
}

void network_client_free(NetworkClient* client) {
    (void)client;
}

bool network_client_send(NetworkClient* client, ProtocolMessage* msg) {
    (void)client;
    (void)msg;
    return false;
}

TransportState network_client_get_state(NetworkClient* client) {
    (void)client;
    return TRANSPORT_DISCONNECTED;
}

bool network_client_is_connected(NetworkClient* client) {
    (void)client;
    return false;
}

void network_client_set_auto_reconnect(NetworkClient* client,
                                        bool enabled,
                                        int delay_ms,
                                        int max_attempts) {
    (void)client;
    (void)enabled;
    (void)delay_ms;
    (void)max_attempts;
}

/* Shared utility functions are in network_common.c */

#endif /* CYXMAKE_ENABLE_DISTRIBUTED */
