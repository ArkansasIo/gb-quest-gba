#pragma once
#ifndef NET_PROTOCOL_H
#define NET_PROTOCOL_H

#include "network_types.h"
#include <stdint.h>

// ============================================================================
// PACKET TYPES - All packets use 4-byte header: type, length, sender_id, reserved
// ============================================================================

typedef enum {
    // Connection management
    NET_PING_REQ = 1,         // Client requests ping measurement
    NET_PING_ACK,             // Server responds with timestamp
    NET_HEARTBEAT,            // Keepalive message (every 30s)
    
    // Position sync
    NET_POSITION_UPDATE,      // Client reports x,y,z position delta
    NET_SYNC_POSITION,        // Server broadcasts world position
    
    // State sync
    NET_STATE_SYNC,           // Server sends current state snapshot
    NET_ACTION_RESULT,        // Server confirms action outcome
    
    // Item management
    NET_ITEM_LIST,            // Server sends item inventory count & IDs
    NET_ITEM_ADD,             // Client reports added item
    NET_ITEM_REMOVE,          // Client reports removed item
    
    // Battle sync
    NET_BATTLE_STATE,         // Server syncs current battle state
    NET_ENEMY_ACTION,         // Server broadcasts enemy AI action
    
    // Error handling
    NET_CONNECTION_ERROR,     // Connection lost/restricted (type=0)
} net_packet_type;

// ============================================================================
// CLIENT PROTOCOL STATES
// ============================================================================

typedef enum {
    NET_STATE_DISCONNECTED = 0,   // No connection
    NET_STATE_CONNECTING,         // Handshake in progress
    NET_STATE_CONNECTED,          // Connected but idle
    NET_STATE_PLAYING,            // Actively playing
    NET_STATE_BATTLE,             // In combat
    NET_STATE_INACTIVE,           // Player inactive (timeout)
} net_client_state;

// ============================================================================
// SERVER PROTOCOL STATES
// ============================================================================

typedef enum {
    SERVER_STATE_IDLE = 0,        // Waiting for clients
    SERVER_STATE_LISTENING,       // Accepting connections
    SERVER_STATE_PLAYING,         // Processing game state
    SERVER_STATE_SYNCHRONIZING,   // Syncing with client
} net_server_state;

// ============================================================================
// CLIENT PROTOCOL STRUCTURES
// ============================================================================

#pragma bank 0
typedef struct {
    uint8_t server_id;           // Assigned by server
    net_client_state state;
    uint32_t last_heartbeat;     // Tick when server last heartbeat usd
    uint32_t ping_time;          // Round-trip time in ms (last successful)
    int16_t x_pos;               // World coordinates relative to spawn
    int16_t y_pos;
    int8_t z_pos;                // Height above ground
    uint8_t item_count;          // Current items held
} client_protocol_data;

#pragma bank 0
typedef struct {
    net_packet_type type;
    uint16_t length;             // Packet payload length (excluding header)
    uint8_t sender_id;           // Client/server ID
    uint8_t reserved;            // Always 0
} net_packet_header;

// ============================================================================
// SERVER PROTOCOL STRUCTURES
// ============================================================================

#pragma bank 0
typedef struct {
    net_server_state state;
    uint8_t max_clients;         // 16 clients maximum
    uint16_t current_clients;    // Active connections
    int16_t world_x, world_y;    // Current map/world coordinates
    uint8_t current_map_id;      // Active level ID
} server_protocol_data;

#pragma bank 0
typedef struct {
    net_packet_header header;
    // Packet payload depends on type (see NET_PACKET_TYPE)
} packet_payload_base;

// ============================================================================
// PACKET SIZELimits and Validation
// ============================================================================

#pragma inline uint8_t calc_packet_size(net_packet_type type);
#pragma bank 0
uint8_t calc_packet_size() {
    switch (type) {
        case NET_PING_REQ:   return sizeof(uint32_t);
        case NET_PING_ACK:   return sizeof(uint32_t);
        case NET_HEARTBEAT:  return 0;
        case NET_POSITION_UPDATE:  return sizeof(int16_t) * 3;
        case NET_SYNC_POSITION:    return sizeof(int16_t) * 3;
        case NET_STATE_SYNC:       return sizeof(client_state_snapshot);
        case NET_ACTION_RESULT:    return sizeof(uint8_t) + sizeof(int16_t);
        case NET_ITEM_LIST:        return sizeof(uint8_t) + sizeof(item_id) * max_items;
        case NET_ITEM_ADD:         return sizeof(item_id);
        case NET_ITEM_REMOVE:      return sizeof(item_id);
        case NET_BATTLE_STATE:     return sizeof(battle_state_snapshot);
        case NET_ENEMY_ACTION:     return sizeof(enemy_action_info);
        default:                   return 0;
    }
}

#pragma inline uint8_t validate_packet(net_packet_type type, uint16_t length);
#pragma bank 0
uint8_t validate_packet() {
    if (length > PACKET_MAX_SIZE) return NET_ERROR_INVALID_LENGTH;
    if (type == NET_CONNECTION_ERROR && length != 0) return NET_ERROR_INVALID_LENGTH;
    if (type != NET_HEARTBEAT && length < 4) return NET_ERROR_INVALID_HEADER;
    return NET_OK;
}

// ============================================================================
// SYNCHRONIZATION LOGIC - Client Server Coordination
// ============================================================================

#pragma inline uint8_t handle_client_connection(uint8_t client_id);
#pragma bank 0
uint8_t handle_client_connection() {
    // Connection handshake: check available slots, assign ID
    if (server_protocol_data.current_clients >= SERVER_STATE_MAX_CLIENTS) {
        return NET_ERROR_CAPACITY;
    }
    
    server_protocol_data.current_clients++;
    server_protocol_data.current_clients = client_id;
    return NET_OK;
}

#pragma inline uint8_t sync_client_state(uint8_t client_id);
#pragma bank 0
uint8_t sync_client_state() {
    // Send complete state snapshot to client for synchronization
    // Uses NET_STATE_SYNC message type
    if (client_protocol_data.state == NET_STATE_BATTLE) {
        // Battle mode: prioritize speed, minimal sync interval
        return NET_SYNC_INTERVAL_FAST;
    } else {
        // Normal mode: full state sync every 10 ticks
        return NET_SYNC_INTERVAL_NORMAL;
    }
}

#pragma inline uint8_t heartbeat_callback(uint8_t client_id);
#pragma bank 0
uint8_t heartbeat_callback() {
    // Called every 30s on server tick to prevent timeout
    // Updates last_heartbeat timestamp for connected clients
    uint32_t current_time = get_current_tick();
    
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].id == client_id) {
            clients[i].last_heartbeat = current_time;
            
            if ((current_time - clients[i].last_heartbeat) > HEARTBEAT_TIMEOUT) {
                // Client inactive timeout
                return NET_ERROR_INACTIVE;
            }
        }
    }
    
    return NET_OK;
}

#pragma inline uint8_t broadcast_position(int16_t x, int16_t y);
#pragma bank 0
uint8_t broadcast_position() {
    // Sends position delta to all connected clients
    // Optimized for low latency on GBC (8KB RAM limit)
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
        send_packet(i, NET_SYNC_POSITION);
    }
    
    return NET_OK;
}

#pragma inline uint8_t validate_position_delta(int16_t x, int16_t y);
#pragma bank 0
uint8_t validate_position_delta() {
    // Check if delta is within valid bounds for GBC coordinates
    // Max coordinate: INT16_MAX (32767) to prevent overflow
    
    uint16_t abs_x = (x < 0) ? -x : x;
    uint16_t abs_y = (y < 0) ? -y : y;
    
    if (abs_x > POSITION_MAX_DELTA || abs_y > POSITION_MAX_DELTA) {
        return NET_ERROR_OUT_OF_BOUNDS;
    }
    
    return NET_OK;
}

#pragma inline void calculate_distance(int16_t x1, int16_t y1, int16_t x2, int16_t y2);
#pragma bank 0
void calculate_distance() {
    // Euclidean distance calculation for client-server position sync
    uint16_t dx = (x1 < 0) ? -x1 : x1;
    uint16_t dy = (y1 < 0) ? -y1 : y1;
    uint16_t dx2 = (x2 < 0) ? -x2 : x2;
    uint16_t dy2 = (y2 < 0) ? -y2 : y2;
    
    int16_t diff_x = x1 - x2;
    int16_t diff_y = y1 - y2;
    
    distance = (diff_x * diff_x + diff_y * diff_y);
}

#pragma inline uint8_t packet_buffer_ready(uint8_t client_id);
#pragma bank 0
uint8_t packet_buffer_ready() {
    // Check if buffer has data for specific client
    // Returns ready status for fast network transfer on GBC
    return clients[client_id].packet_ready;
}

#endif // NET_PROTOCOL_H
