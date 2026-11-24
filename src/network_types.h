/*
 * LabyrinthOfTheDragon - MMORPG Networking Module
 * Packet structure definitions for Game Boy Color platform
 * GBDK-2020 compatible with ROM_BANKS=32 memory constraints
 */

#pragma once

#ifndef NETWORK_TYPES_H
#define NETWORK_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* Packet type enumeration - follows GBC networking conventions */
typedef enum {
    NET_PKT_SYNC = 0,      /* Server sync state */
    NET_PKT_STATE,         /* Client state report */
    NET_PKT_POS,           /* Position packet (x,y,z) */
    NET_PKT_ITEM,          /* Item inventory update */
    NET_PKT_MAP,           /* Map data download */
    NET_PKT_ACTION,        /* Action queue from server */
    NET_PKT_CHAT,          /* Chat message */
    NET_PKT_INVENTORY,     /* Full inventory dump */
    NET_PKT_STATUS,        /* Server status/pong */
    NET_PKT_JOIN,          /* Client join request */
    NET_PKT_LEAVE,         /* Client disconnect notification */
    NET_PKT_MAX            /* Sentinel - don't use this directly */
} net_packet_type_t;

/* Client/server state enumeration */
typedef enum {
    NET_STATE_NONE = 0,
    NET_STATE_CONNECTING,
    NET_STATE_CONNECTED,
    NET_STATE_DISCOVERED,
    NET_STATE_PLAYING,
    NET_STATE_PAUSED,
    NET_STATE_DISCONNECTED,
    NET_STATE_MAX
} net_state_t;

/* Packet header structure - fixed 4 bytes for fast parsing */
typedef struct {
    uint8_t type;          /* packet type (0-15) */
    uint8_t length;        /* payload length in bytes */
    uint8_t sender_id;     /* unique client/server ID */
    uint8_t reserved;      /* padding/flags */
} net_packet_header_t;

/* Inline helper macros for fast packet assembly/disassembly */
#pragma inline
static inline void net_packet_set_type(net_packet_header_t *pkt, uint8_t type) {
    pkt->type = type;
}

#pragma inline
static inline uint8_t net_packet_get_length(const net_packet_header_t *pkt) {
    return pkt->length;
}

#pragma inline
static inline void net_packet_set_sender(net_packet_header_t *pkt, uint8_t id) {
    pkt->sender_id = id;
}

#pragma inline
static inline uint8_t net_packet_get_sender(const net_packet_header_t *pkt) {
    return pkt->sender_id;
}

/* Client-side data structures */
typedef struct {
    /* Network state machine - mirrors BATTLE_STATE pattern from core.h */
    net_state_t state;
    
    /* Connection metadata (packed for GBC RAM efficiency) */
    uint8_t server_id[4];  /* Unique server identifier */
    uint8_t last_heartbeat;
    uint8_t ping_time;     /* ms since last pong */
    
    /* Position (x,y,z relative to local coords) - Game Boy Color fits in RAM_BANKS=4 */
    int16_t x, y, z;      /* Position with signed bytes for GBC memory efficiency */
    
    /* Inventory count track for quick updates */
    uint8_t item_count;    /* Number of items currently tracked */
    uint8_t max_items;     /* Capacity limit */
} client_network_t;

/* Server-side data structures - larger, may need ROM-bank swapping */
typedef struct {
    /* Server state management */
    net_state_t state;
    bool is_host;          /* True for game host */
    
    /* Session management (packed) */
    uint8_t max_clients;
    uint8_t current_clients;
    uint8_t session_id[4];
    
    /* Client registry array index (not full struct - use linked list for RAM efficiency) */
    uint8_t active_clients[MAX_CLIENTS];
    
    /* Shared state broadcastable to all clients */
    int16_t world_x, world_y, world_z;  /* World position */
    uint8_t current_map_id;              /* Active map identifier */
} server_network_t;

/* Global client count - fits in RAM_BANKS=4 */
#pragma inline static const uint8_t MAX_CLIENTS = 16;

/* Server network structure can be ROM-banked for larger allocation */
typedef struct {
    server_network_t data;
    /* Additional server state if needed (expandable) */
} server_network_struct_t;

#endif /* NETWORK_TYPES_H */
