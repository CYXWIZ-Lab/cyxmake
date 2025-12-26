/**
 * @file network_common.c
 * @brief Shared network utility functions
 *
 * Contains utility functions used by both network_client.c and network_server.c
 * to avoid duplicate symbol definitions.
 */

#include "cyxmake/distributed/network_transport.h"
#include "cyxmake/logger.h"

#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Connection Accessors
 * ============================================================ */

const char* network_connection_get_id(NetworkConnection* connection) {
    return connection ? connection->id : NULL;
}

const char* network_connection_get_remote_addr(NetworkConnection* connection) {
    return connection ? connection->remote_addr : NULL;
}

TransportState network_connection_get_state(NetworkConnection* connection) {
    return connection ? connection->state : TRANSPORT_DISCONNECTED;
}

void network_connection_set_user_data(NetworkConnection* connection, void* user_data) {
    if (connection) {
        connection->user_data = user_data;
    }
}

void* network_connection_get_user_data(NetworkConnection* connection) {
    return connection ? connection->user_data : NULL;
}

double network_connection_get_latency_ms(NetworkConnection* connection) {
    return connection ? connection->latency_ms : 0.0;
}

/* ============================================================
 * Utility Functions
 * ============================================================ */

const char* transport_state_name(TransportState state) {
    switch (state) {
        case TRANSPORT_DISCONNECTED: return "DISCONNECTED";
        case TRANSPORT_CONNECTING: return "CONNECTING";
        case TRANSPORT_CONNECTED: return "CONNECTED";
        case TRANSPORT_CLOSING: return "CLOSING";
        case TRANSPORT_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

bool network_is_available(void) {
#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    return true;
#else
    return false;
#endif
}

const char* network_get_library_version(void) {
#ifdef CYXMAKE_ENABLE_DISTRIBUTED
    /* When libwebsockets is available */
    extern const char* lws_get_library_version(void);
    return lws_get_library_version();
#else
    return "stub-1.0.0";
#endif
}
