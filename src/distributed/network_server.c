/**
 * @file network_server.c
 * @brief WebSocket server implementation for distributed builds
 *
 * Uses libwebsockets for cross-platform WebSocket support.
 */

#include "cyxmake/distributed/network_transport.h"
#include "cyxmake/distributed/protocol.h"
#include "cyxmake/threading.h"
#include "cyxmake/compat.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef CYXMAKE_ENABLE_DISTRIBUTED

#include <libwebsockets.h>

/* ============================================================
 * Internal Structures
 * ============================================================ */

#define MAX_CONNECTIONS 256
#define RX_BUFFER_SIZE (64 * 1024)
#define MAX_MESSAGE_SIZE (64 * 1024 * 1024)  /* 64 MB */

/* Per-connection data */
typedef struct ConnectionData {
    char id[64];
    char remote_addr[128];
    TransportState state;
    void* user_data;

    /* Receive buffer for fragmented messages */
    char* rx_buffer;
    size_t rx_len;
    size_t rx_capacity;

    /* Send queue */
    char** tx_queue;
    size_t* tx_sizes;
    size_t tx_count;
    size_t tx_capacity;
    MutexHandle tx_mutex;

    /* Latency tracking */
    uint64_t last_ping_time;
    double latency_ms;

    /* Back-reference to server */
    struct NetworkServer* server;
    struct lws* wsi;
} ConnectionData;

/* Server structure */
struct NetworkServer {
    NetworkConfig config;
    struct lws_context* context;
    struct lws_protocols* protocols;

    /* Connection management */
    ConnectionData* connections[MAX_CONNECTIONS];
    int connection_count;
    MutexHandle connections_mutex;

    /* Callbacks */
    NetworkServerCallbacks callbacks;

    /* Service thread */
    ThreadHandle service_thread;
    volatile bool running;
    volatile bool shutdown_requested;

    /* Server ID */
    char* server_id;
};

/* ============================================================
 * Forward Declarations
 * ============================================================ */

static int callback_cyxmake(struct lws* wsi, enum lws_callback_reasons reason,
                            void* user, void* in, size_t len);
static ConnectionData* find_connection_by_wsi(NetworkServer* server, struct lws* wsi);
static ConnectionData* create_connection(NetworkServer* server, struct lws* wsi);
static void destroy_connection(NetworkServer* server, ConnectionData* conn);
static void* server_thread_func(void* arg);
static bool queue_message(ConnectionData* conn, const char* data, size_t len);

/* ============================================================
 * Protocol Definition
 * ============================================================ */

static struct lws_protocols protocols[] = {
    {
        .name = "cyxmake-distributed",
        .callback = callback_cyxmake,
        .per_session_data_size = sizeof(ConnectionData*),
        .rx_buffer_size = RX_BUFFER_SIZE,
        .id = 0,
        .user = NULL,
        .tx_packet_size = 0
    },
    { NULL, NULL, 0, 0, 0, NULL, 0 }  /* Terminator */
};

/* ============================================================
 * WebSocket Callback
 * ============================================================ */

static int callback_cyxmake(struct lws* wsi, enum lws_callback_reasons reason,
                            void* user, void* in, size_t len) {
    ConnectionData** pconn = (ConnectionData**)user;
    ConnectionData* conn = pconn ? *pconn : NULL;
    NetworkServer* server = NULL;

    /* Get server from protocol user data */
    const struct lws_protocols* protocol = lws_get_protocol(wsi);
    if (protocol) {
        server = (NetworkServer*)protocol->user;
    }

    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            if (!server) break;

            /* Create connection data */
            conn = create_connection(server, wsi);
            if (!conn) {
                return -1;
            }
            *pconn = conn;

            /* Get remote address */
            char name[128] = {0};
            char ip[64] = {0};
            lws_get_peer_addresses(wsi, lws_get_socket_fd(wsi),
                                   name, sizeof(name), ip, sizeof(ip));
            snprintf(conn->remote_addr, sizeof(conn->remote_addr), "%s (%s)", ip, name);

            conn->state = TRANSPORT_CONNECTED;

            /* Notify callback */
            if (server->callbacks.on_connect) {
                server->callbacks.on_connect((NetworkConnection*)conn,
                                             server->callbacks.user_data);
            }
            break;
        }

        case LWS_CALLBACK_RECEIVE: {
            if (!conn || !server) break;

            /* Handle fragmented messages */
            size_t remaining = lws_remaining_packet_payload(wsi);
            bool is_final = lws_is_final_fragment(wsi);

            /* Grow buffer if needed */
            size_t needed = conn->rx_len + len;
            if (needed > conn->rx_capacity) {
                size_t new_cap = conn->rx_capacity * 2;
                if (new_cap < needed) new_cap = needed;
                if (new_cap > MAX_MESSAGE_SIZE) {
                    /* Message too large */
                    if (server->callbacks.on_error) {
                        server->callbacks.on_error((NetworkConnection*)conn,
                                                   "Message exceeds maximum size",
                                                   server->callbacks.user_data);
                    }
                    conn->rx_len = 0;
                    break;
                }
                char* new_buf = (char*)realloc(conn->rx_buffer, new_cap);
                if (!new_buf) break;
                conn->rx_buffer = new_buf;
                conn->rx_capacity = new_cap;
            }

            /* Append data */
            memcpy(conn->rx_buffer + conn->rx_len, in, len);
            conn->rx_len += len;

            /* Process complete message */
            if (is_final && remaining == 0) {
                conn->rx_buffer[conn->rx_len] = '\0';

                /* Parse and deliver message */
                ProtocolMessage* msg = protocol_message_deserialize(conn->rx_buffer);
                if (msg && server->callbacks.on_message) {
                    server->callbacks.on_message((NetworkConnection*)conn, msg,
                                                 server->callbacks.user_data);
                }
                protocol_message_free(msg);

                conn->rx_len = 0;
            }
            break;
        }

        case LWS_CALLBACK_SERVER_WRITEABLE: {
            if (!conn) break;

            mutex_lock(&conn->tx_mutex);

            if (conn->tx_count > 0) {
                /* Send first message in queue */
                char* data = conn->tx_queue[0];
                size_t size = conn->tx_sizes[0];

                /* Allocate buffer with LWS_PRE padding */
                unsigned char* buf = (unsigned char*)malloc(LWS_PRE + size);
                if (buf) {
                    memcpy(buf + LWS_PRE, data, size);
                    int written = lws_write(wsi, buf + LWS_PRE, size, LWS_WRITE_TEXT);
                    free(buf);

                    if (written < (int)size) {
                        /* Write failed */
                        mutex_unlock(&conn->tx_mutex);
                        return -1;
                    }
                }

                /* Remove from queue */
                free(data);
                memmove(conn->tx_queue, conn->tx_queue + 1,
                        (conn->tx_count - 1) * sizeof(char*));
                memmove(conn->tx_sizes, conn->tx_sizes + 1,
                        (conn->tx_count - 1) * sizeof(size_t));
                conn->tx_count--;

                /* Schedule more writes if needed */
                if (conn->tx_count > 0) {
                    lws_callback_on_writable(wsi);
                }
            }

            mutex_unlock(&conn->tx_mutex);
            break;
        }

        case LWS_CALLBACK_CLOSED: {
            if (!conn || !server) break;

            conn->state = TRANSPORT_DISCONNECTED;

            /* Notify callback */
            if (server->callbacks.on_disconnect) {
                server->callbacks.on_disconnect((NetworkConnection*)conn,
                                                "Connection closed",
                                                server->callbacks.user_data);
            }

            destroy_connection(server, conn);
            *pconn = NULL;
            break;
        }

        case LWS_CALLBACK_WSI_DESTROY: {
            /* Connection destroyed, cleanup if not already done */
            break;
        }

        default:
            break;
    }

    return 0;
}

/* ============================================================
 * Connection Management
 * ============================================================ */

static ConnectionData* create_connection(NetworkServer* server, struct lws* wsi) {
    mutex_lock(&server->connections_mutex);

    if (server->connection_count >= MAX_CONNECTIONS) {
        mutex_unlock(&server->connections_mutex);
        return NULL;
    }

    ConnectionData* conn = (ConnectionData*)calloc(1, sizeof(ConnectionData));
    if (!conn) {
        mutex_unlock(&server->connections_mutex);
        return NULL;
    }

    /* Generate connection ID */
    char* uuid = protocol_generate_uuid();
    if (uuid) {
        strncpy(conn->id, uuid, sizeof(conn->id) - 1);
        free(uuid);
    }

    conn->server = server;
    conn->wsi = wsi;
    conn->state = TRANSPORT_CONNECTING;

    /* Initialize receive buffer */
    conn->rx_capacity = RX_BUFFER_SIZE;
    conn->rx_buffer = (char*)malloc(conn->rx_capacity);
    conn->rx_len = 0;

    /* Initialize send queue */
    conn->tx_capacity = 16;
    conn->tx_queue = (char**)calloc(conn->tx_capacity, sizeof(char*));
    conn->tx_sizes = (size_t*)calloc(conn->tx_capacity, sizeof(size_t));
    conn->tx_count = 0;
    mutex_init(&conn->tx_mutex);

    /* Add to server's connection list */
    server->connections[server->connection_count++] = conn;

    mutex_unlock(&server->connections_mutex);
    return conn;
}

static void destroy_connection(NetworkServer* server, ConnectionData* conn) {
    if (!server || !conn) return;

    mutex_lock(&server->connections_mutex);

    /* Find and remove from list */
    for (int i = 0; i < server->connection_count; i++) {
        if (server->connections[i] == conn) {
            memmove(&server->connections[i], &server->connections[i + 1],
                    (server->connection_count - i - 1) * sizeof(ConnectionData*));
            server->connection_count--;
            break;
        }
    }

    mutex_unlock(&server->connections_mutex);

    /* Free connection resources */
    free(conn->rx_buffer);

    mutex_lock(&conn->tx_mutex);
    for (size_t i = 0; i < conn->tx_count; i++) {
        free(conn->tx_queue[i]);
    }
    free(conn->tx_queue);
    free(conn->tx_sizes);
    mutex_unlock(&conn->tx_mutex);
    mutex_destroy(&conn->tx_mutex);

    free(conn);
}

static ConnectionData* find_connection_by_wsi(NetworkServer* server, struct lws* wsi) {
    mutex_lock(&server->connections_mutex);

    for (int i = 0; i < server->connection_count; i++) {
        if (server->connections[i]->wsi == wsi) {
            ConnectionData* conn = server->connections[i];
            mutex_unlock(&server->connections_mutex);
            return conn;
        }
    }

    mutex_unlock(&server->connections_mutex);
    return NULL;
}

static bool queue_message(ConnectionData* conn, const char* data, size_t len) {
    mutex_lock(&conn->tx_mutex);

    /* Grow queue if needed */
    if (conn->tx_count >= conn->tx_capacity) {
        size_t new_cap = conn->tx_capacity * 2;
        char** new_queue = (char**)realloc(conn->tx_queue, new_cap * sizeof(char*));
        size_t* new_sizes = (size_t*)realloc(conn->tx_sizes, new_cap * sizeof(size_t));
        if (!new_queue || !new_sizes) {
            mutex_unlock(&conn->tx_mutex);
            return false;
        }
        conn->tx_queue = new_queue;
        conn->tx_sizes = new_sizes;
        conn->tx_capacity = new_cap;
    }

    /* Copy message to queue */
    char* copy = (char*)malloc(len + 1);
    if (!copy) {
        mutex_unlock(&conn->tx_mutex);
        return false;
    }
    memcpy(copy, data, len);
    copy[len] = '\0';

    conn->tx_queue[conn->tx_count] = copy;
    conn->tx_sizes[conn->tx_count] = len;
    conn->tx_count++;

    mutex_unlock(&conn->tx_mutex);

    /* Request write callback */
    lws_callback_on_writable(conn->wsi);

    return true;
}

/* ============================================================
 * Server Thread
 * ============================================================ */

static void* server_thread_func(void* arg) {
    NetworkServer* server = (NetworkServer*)arg;

    while (server->running && !server->shutdown_requested) {
        /* Service libwebsockets */
        int result = lws_service(server->context, 50);  /* 50ms timeout */
        if (result < 0) {
            break;
        }
    }

    return NULL;
}

/* ============================================================
 * Public Server API
 * ============================================================ */

NetworkServer* network_server_create(const NetworkConfig* config) {
    if (!config) return NULL;

    NetworkServer* server = (NetworkServer*)calloc(1, sizeof(NetworkServer));
    if (!server) return NULL;

    /* Copy configuration */
    server->config = *config;
    if (config->bind_address) {
        server->config.bind_address = strdup(config->bind_address);
    }
    if (config->cert_path) {
        server->config.cert_path = strdup(config->cert_path);
    }
    if (config->key_path) {
        server->config.key_path = strdup(config->key_path);
    }
    if (config->ca_path) {
        server->config.ca_path = strdup(config->ca_path);
    }

    /* Generate server ID */
    server->server_id = protocol_generate_uuid();

    /* Initialize mutex */
    mutex_init(&server->connections_mutex);

    return server;
}

void network_server_set_callbacks(NetworkServer* server,
                                   const NetworkServerCallbacks* callbacks) {
    if (!server || !callbacks) return;
    server->callbacks = *callbacks;
}

bool network_server_start(NetworkServer* server) {
    if (!server || server->running) return false;

    /* Set up libwebsockets context creation info */
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    info.port = server->config.port > 0 ? server->config.port : 9876;
    info.iface = server->config.bind_address;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_VALIDATE_UTF8;

    /* TLS configuration */
    if (server->config.use_tls && server->config.cert_path && server->config.key_path) {
        info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
        info.ssl_cert_filepath = server->config.cert_path;
        info.ssl_private_key_filepath = server->config.key_path;
        if (server->config.ca_path) {
            info.ssl_ca_filepath = server->config.ca_path;
        }
        if (server->config.verify_peer) {
            info.options |= LWS_SERVER_OPTION_REQUIRE_VALID_OPENSSL_CLIENT_CERT;
        }
    }

    /* Set user data to point to server */
    protocols[0].user = server;

    /* Create context */
    server->context = lws_create_context(&info);
    if (!server->context) {
        return false;
    }

    server->running = true;
    server->shutdown_requested = false;

    /* Start service thread */
    if (!thread_create(&server->service_thread, server_thread_func, server)) {
        lws_context_destroy(server->context);
        server->context = NULL;
        server->running = false;
        return false;
    }

    return true;
}

void network_server_stop(NetworkServer* server) {
    if (!server || !server->running) return;

    server->shutdown_requested = true;

    /* Wait for service thread */
    if (server->service_thread) {
        thread_join(server->service_thread);
        server->service_thread = NULL;
    }

    /* Close all connections */
    mutex_lock(&server->connections_mutex);
    while (server->connection_count > 0) {
        ConnectionData* conn = server->connections[0];
        if (conn->wsi) {
            lws_close_reason(conn->wsi, LWS_CLOSE_STATUS_GOINGAWAY,
                            (unsigned char*)"Server shutdown", 15);
        }
        mutex_unlock(&server->connections_mutex);
        destroy_connection(server, conn);
        mutex_lock(&server->connections_mutex);
    }
    mutex_unlock(&server->connections_mutex);

    /* Destroy context */
    if (server->context) {
        lws_context_destroy(server->context);
        server->context = NULL;
    }

    server->running = false;
}

void network_server_free(NetworkServer* server) {
    if (!server) return;

    if (server->running) {
        network_server_stop(server);
    }

    mutex_destroy(&server->connections_mutex);

    free(server->config.bind_address);
    free(server->config.cert_path);
    free(server->config.key_path);
    free(server->config.ca_path);
    free(server->server_id);
    free(server);
}

bool network_server_send(NetworkServer* server,
                          NetworkConnection* connection,
                          ProtocolMessage* msg) {
    if (!server || !connection || !msg) return false;

    ConnectionData* conn = (ConnectionData*)connection;

    /* Set sender ID if not set */
    if (!msg->sender_id && server->server_id) {
        msg->sender_id = strdup(server->server_id);
    }

    /* Serialize message */
    char* json = protocol_message_serialize(msg);
    if (!json) return false;

    bool result = queue_message(conn, json, strlen(json));
    free(json);

    return result;
}

void network_server_broadcast(NetworkServer* server, ProtocolMessage* msg) {
    if (!server || !msg) return;

    /* Set sender ID if not set */
    if (!msg->sender_id && server->server_id) {
        msg->sender_id = strdup(server->server_id);
    }

    /* Serialize once */
    char* json = protocol_message_serialize(msg);
    if (!json) return;
    size_t len = strlen(json);

    mutex_lock(&server->connections_mutex);

    for (int i = 0; i < server->connection_count; i++) {
        ConnectionData* conn = server->connections[i];
        if (conn->state == TRANSPORT_CONNECTED) {
            queue_message(conn, json, len);
        }
    }

    mutex_unlock(&server->connections_mutex);

    free(json);
}

int network_server_get_connection_count(NetworkServer* server) {
    if (!server) return 0;

    mutex_lock(&server->connections_mutex);
    int count = server->connection_count;
    mutex_unlock(&server->connections_mutex);

    return count;
}

void network_server_close_connection(NetworkServer* server,
                                      NetworkConnection* connection,
                                      const char* reason) {
    if (!server || !connection) return;

    ConnectionData* conn = (ConnectionData*)connection;

    if (conn->wsi) {
        lws_close_reason(conn->wsi, LWS_CLOSE_STATUS_NORMAL,
                        (unsigned char*)(reason ? reason : "Closed"),
                        reason ? strlen(reason) : 6);
        lws_callback_on_writable(conn->wsi);
    }
}

bool network_server_is_running(NetworkServer* server) {
    return server && server->running && !server->shutdown_requested;
}

/* Connection API functions are in network_common.c */

/* ============================================================
 * Configuration Helpers
 * ============================================================ */

NetworkConfig* network_config_create_default(void) {
    NetworkConfig* config = (NetworkConfig*)calloc(1, sizeof(NetworkConfig));
    if (!config) return NULL;

    config->port = 9876;
    config->use_tls = false;
    config->max_connections = MAX_CONNECTIONS;
    config->ping_interval_sec = 30;
    config->connection_timeout_sec = 10;
    config->message_timeout_sec = 60;
    config->max_message_size = MAX_MESSAGE_SIZE;
    config->rx_buffer_size = RX_BUFFER_SIZE;
    config->tx_buffer_size = RX_BUFFER_SIZE;

    return config;
}

void network_config_free(NetworkConfig* config) {
    if (!config) return;

    free(config->bind_address);
    free(config->cert_path);
    free(config->key_path);
    free(config->ca_path);
    free(config);
}

/* Utility functions (transport_state_name, network_is_available, etc.)
 * are in network_common.c */

#else /* !CYXMAKE_ENABLE_DISTRIBUTED */

/* Stub implementations when distributed builds are disabled */

NetworkServer* network_server_create(const NetworkConfig* config) {
    (void)config;
    return NULL;
}

void network_server_set_callbacks(NetworkServer* server,
                                   const NetworkServerCallbacks* callbacks) {
    (void)server;
    (void)callbacks;
}

bool network_server_start(NetworkServer* server) {
    (void)server;
    return false;
}

void network_server_stop(NetworkServer* server) {
    (void)server;
}

void network_server_free(NetworkServer* server) {
    (void)server;
}

bool network_server_send(NetworkServer* server,
                          NetworkConnection* connection,
                          ProtocolMessage* msg) {
    (void)server;
    (void)connection;
    (void)msg;
    return false;
}

void network_server_broadcast(NetworkServer* server, ProtocolMessage* msg) {
    (void)server;
    (void)msg;
}

int network_server_get_connection_count(NetworkServer* server) {
    (void)server;
    return 0;
}

void network_server_close_connection(NetworkServer* server,
                                      NetworkConnection* connection,
                                      const char* reason) {
    (void)server;
    (void)connection;
    (void)reason;
}

bool network_server_is_running(NetworkServer* server) {
    (void)server;
    return false;
}

NetworkConfig* network_config_create_default(void) {
    return NULL;
}

void network_config_free(NetworkConfig* config) {
    (void)config;
}

/* Shared utility functions are in network_common.c */

#endif /* CYXMAKE_ENABLE_DISTRIBUTED */
