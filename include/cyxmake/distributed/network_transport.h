/**
 * @file network_transport.h
 * @brief Network transport layer for distributed builds
 *
 * Provides WebSocket-based communication between coordinator and workers.
 * Uses libwebsockets for cross-platform WebSocket support.
 */

#ifndef CYXMAKE_DISTRIBUTED_NETWORK_TRANSPORT_H
#define CYXMAKE_DISTRIBUTED_NETWORK_TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cyxmake/distributed/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Forward Declarations
 * ============================================================ */

typedef struct NetworkServer NetworkServer;
typedef struct NetworkClient NetworkClient;

/* ============================================================
 * Transport State
 * ============================================================ */

typedef enum {
    TRANSPORT_DISCONNECTED,
    TRANSPORT_CONNECTING,
    TRANSPORT_CONNECTED,
    TRANSPORT_CLOSING,
    TRANSPORT_ERROR
} TransportState;

/* ============================================================
 * Network Connection
 * ============================================================ */

/**
 * Represents a network connection (server-side or client-side)
 */
typedef struct NetworkConnection {
    const char* id;               /* Unique connection identifier */
    char* remote_addr;            /* Remote address (IP:port) */
    TransportState state;         /* Current connection state */
    void* user_data;              /* User-defined data */
    double latency_ms;            /* Round-trip latency in milliseconds */
    void* internal;               /* Internal implementation data */
} NetworkConnection;

/* ============================================================
 * Network Configuration
 * ============================================================ */

typedef struct NetworkConfig {
    /* Server settings */
    char* bind_address;           /* Server bind address (default: "0.0.0.0") */
    uint16_t port;                /* Server port (default: 9876) */

    /* TLS settings */
    bool use_tls;                 /* Enable TLS */
    char* cert_path;              /* TLS certificate path */
    char* key_path;               /* TLS private key path */
    char* ca_path;                /* CA certificate for client auth */
    bool verify_peer;             /* Verify client certificates */

    /* Connection settings */
    int max_connections;          /* Maximum concurrent connections */
    int ping_interval_sec;        /* WebSocket ping interval (default: 30) */
    int connection_timeout_sec;   /* Connection timeout (default: 10) */
    int message_timeout_sec;      /* Message response timeout (default: 60) */

    /* Buffer settings */
    size_t max_message_size;      /* Maximum message size (default: 64MB) */
    size_t rx_buffer_size;        /* Receive buffer size */
    size_t tx_buffer_size;        /* Transmit buffer size */
} NetworkConfig;

/* ============================================================
 * Callback Types
 * ============================================================ */

/**
 * Called when a message is received
 */
typedef void (*OnMessageCallback)(
    NetworkConnection* connection,
    ProtocolMessage* msg,
    void* user_data
);

/**
 * Called when a new connection is established
 */
typedef void (*OnConnectCallback)(
    NetworkConnection* connection,
    void* user_data
);

/**
 * Called when a connection is closed
 */
typedef void (*OnDisconnectCallback)(
    NetworkConnection* connection,
    const char* reason,
    void* user_data
);

/**
 * Called when an error occurs
 */
typedef void (*OnErrorCallback)(
    NetworkConnection* connection,
    const char* error,
    void* user_data
);

/* ============================================================
 * Network Server API (Coordinator)
 * ============================================================ */

/**
 * Server callback structure
 */
typedef struct {
    OnMessageCallback on_message;
    OnConnectCallback on_connect;
    OnDisconnectCallback on_disconnect;
    OnErrorCallback on_error;
    void* user_data;
} NetworkServerCallbacks;

/**
 * Create a network server
 */
NetworkServer* network_server_create(const NetworkConfig* config);

/**
 * Set server callbacks
 */
void network_server_set_callbacks(NetworkServer* server,
                                   const NetworkServerCallbacks* callbacks);

/**
 * Start the server (non-blocking, spawns service thread)
 */
bool network_server_start(NetworkServer* server);

/**
 * Stop the server
 */
void network_server_stop(NetworkServer* server);

/**
 * Free server resources
 */
void network_server_free(NetworkServer* server);

/**
 * Send message to a specific connection
 */
bool network_server_send(NetworkServer* server,
                          NetworkConnection* connection,
                          ProtocolMessage* msg);

/**
 * Broadcast message to all connections
 */
void network_server_broadcast(NetworkServer* server, ProtocolMessage* msg);

/**
 * Get number of active connections
 */
int network_server_get_connection_count(NetworkServer* server);

/**
 * Close a specific connection
 */
void network_server_close_connection(NetworkServer* server,
                                      NetworkConnection* connection,
                                      const char* reason);

/**
 * Check if server is running
 */
bool network_server_is_running(NetworkServer* server);

/* ============================================================
 * Network Client API (Worker)
 * ============================================================ */

/**
 * Client callback structure
 */
typedef struct {
    OnMessageCallback on_message;
    OnConnectCallback on_connect;
    OnDisconnectCallback on_disconnect;
    OnErrorCallback on_error;
    void* user_data;
} NetworkClientCallbacks;

/**
 * Create a network client
 */
NetworkClient* network_client_create(const NetworkConfig* config);

/**
 * Set client callbacks
 */
void network_client_set_callbacks(NetworkClient* client,
                                   const NetworkClientCallbacks* callbacks);

/**
 * Connect to coordinator
 * @param url WebSocket URL (e.g., "wss://coordinator:9876")
 */
bool network_client_connect(NetworkClient* client, const char* url);

/**
 * Disconnect from coordinator
 */
void network_client_disconnect(NetworkClient* client);

/**
 * Free client resources
 */
void network_client_free(NetworkClient* client);

/**
 * Send message to coordinator
 */
bool network_client_send(NetworkClient* client, ProtocolMessage* msg);

/**
 * Get current connection state
 */
TransportState network_client_get_state(NetworkClient* client);

/**
 * Check if connected
 */
bool network_client_is_connected(NetworkClient* client);

/**
 * Set auto-reconnect behavior
 */
void network_client_set_auto_reconnect(NetworkClient* client,
                                        bool enabled,
                                        int delay_ms,
                                        int max_attempts);

/* ============================================================
 * Network Connection API
 * ============================================================ */

/**
 * Get connection ID
 */
const char* network_connection_get_id(NetworkConnection* connection);

/**
 * Get remote address
 */
const char* network_connection_get_remote_addr(NetworkConnection* connection);

/**
 * Get connection state
 */
TransportState network_connection_get_state(NetworkConnection* connection);

/**
 * Set user data for connection
 */
void network_connection_set_user_data(NetworkConnection* connection,
                                       void* user_data);

/**
 * Get user data for connection
 */
void* network_connection_get_user_data(NetworkConnection* connection);

/**
 * Get connection latency (round-trip time in ms)
 */
double network_connection_get_latency_ms(NetworkConnection* connection);

/* ============================================================
 * Configuration Helpers
 * ============================================================ */

/**
 * Create default configuration
 */
NetworkConfig* network_config_create_default(void);

/**
 * Free configuration
 */
void network_config_free(NetworkConfig* config);

/**
 * Load configuration from TOML
 */
NetworkConfig* network_config_load(const char* path);

/**
 * Validate configuration
 */
bool network_config_validate(const NetworkConfig* config, char** error_out);

/* ============================================================
 * Utility Functions
 * ============================================================ */

/**
 * Get transport state name
 */
const char* transport_state_name(TransportState state);

/**
 * Check if libwebsockets is available
 */
bool network_is_available(void);

/**
 * Get libwebsockets version string
 */
const char* network_get_library_version(void);

#ifdef __cplusplus
}
#endif

#endif /* CYXMAKE_DISTRIBUTED_NETWORK_TRANSPORT_H */
