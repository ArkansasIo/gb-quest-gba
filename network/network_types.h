#pragma once
/*
 * network_types.h - GBDK MMORPG packet structures and types
 * Memory-optimized for Game Boy Color (64KB total, 8KB RAM typical)
 * Network packet definitions for LabyrinthOfTheDragon
 */

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * NETWORK CONSTANTS AND ENUMS
 * ======================================================================== */

#ifndef GBC_MAX_PLAYERS
#define GBC_MAX_PLAYERS 16   /* Maximum concurrent players in game */
#endif

#ifndef GBC_MAX_MESSAGE_LENGTH
#define GBC_MAX_MESSAGE_LENGTH 256  /* Maximum packet data length */
#endif

/* Packet types - matches existing GBC packet conventions from battle.h */
typedef enum {
    PACKET_TYPE_NONE = 0,      /* Empty/no-op packet */
    PACKET_TYPE_PING = 1,      /* Keep-alive / heartbeat */
    PACKET_TYPE_PONG = 2,      /* Response to PING */
    PACKET_TYPE_JOIN = 3,      /* Player joining server */
    PACKET_TYPE_LEAVE = 4,     /* Player leaving server */
    PACKET_TYPE_STATUS = 5,    /* Connection status request/response */
    PACKET_TYPE_LOCATION = 6,  /* Player position update */
    PACKET_TYPE_ACTION = 7,    /* Player action/interaction */
    PACKET_TYPE_BATTLE_JOIN = 8, /* Join existing battle */
    PACKET_TYPE_BATTLE_LEAVE = 9,/* Leave battle */
    PACKET_TYPE_CHAT = 10,     /* In-game chat message */
    PACKET_TYPE_LISTENERS = 11,/* Register for area events */
    PACKET_TYPE_EVENT = 12,    /* Trigger event notification */
    PACKET_TYPE_GAME_STATE = 13,/* Full game state sync request */
    PACKET_TYPE_PLAYER_LIST = 14,/* Player list update */
    PACKET_TYPE_MAX = 15
} NetPacketType;

/* Client states - mirrors BATTLE_STATE pattern from battle.h */
typedef enum {
    CLIENT_STATE_DISCONNECTED = 0,
    CLIENT_STATE_CONNECTING,
    CLIENT_STATE_CONNECTED,
    CLIENT_STATE_BATTLES,
    CLIENT_STATE_LOBBY
} ClientState;

/* Server capabilities flags (bitmask) */
typedef struct {
    unsigned char supports_multiplayer : 1;
    unsigned char supports_battles : 1;
    unsigned char supports_chat : 1;
    unsigned char supports_area_events : 1;
    unsigned char supports_game_state_sync : 1;
} ServerCapabilities;

/* Network connection status */
typedef struct {
    unsigned char connected        : 1; /* Connection state flag */
    unsigned char reconnected      : 1; /* Last reconnection attempt */
    unsigned char pending_actions  : 1; /* Actions queued for delivery */
} ConnectionStatus;

/* ==========================================================================
 * PACKET STRUCTURES - Memory optimized for GBC constraints
 * All structures are aligned to avoid padding on typical platforms.
 * Uses packed attributes where appropriate for maximum efficiency.
 * ========================================================================== */

#pragma pack(push, 1)

/* PING Packet - Minimal size for keep-alive mechanism */
typedef struct {
    unsigned char packet_type;     /* PACKET_TYPE_PING */
    unsigned char timestamp;        /* Packet creation time (ticks) */
} PingPacket;

#pragma pack(pop)

/* JOIN Packet - Player entry to server */
typedef struct {
    unsigned char packet_type;     /* PACKET_TYPE_JOIN */
    unsigned char player_id;        /* Local player ID (0-15) */
    unsigned char name_length;      /* Length of name string */
    char name[GBC_MAX_MESSAGE_LENGTH];  /* Player name */
} JoinPacket;

#pragma pack(pop)

/* LEAVE Packet - Player exit from server */
typedef struct {
    unsigned char packet_type;     /* PACKET_TYPE_LEAVE */
    unsigned char player_id;        /* Local player ID */
} LeavePacket;

#pragma pack(pop)

/* STATUS Packet - Connection status request/response */
typedef struct {
    unsigned char packet_type;     /* PACKET_TYPE_STATUS */
    unsigned char connection_status;  /* Connection state flag */
    ServerCapabilities capabilities;  /* Server capabilities */
    unsigned char version;          /* Protocol version */
} StatusPacket;

#pragma pack(pop)

/* LOCATION Packet - Player position update (compact, no full coords) */
typedef struct {
    unsigned char packet_type;     /* PACKET_TYPE_LOCATION */
    unsigned char player_id;        /* Local player ID */
    unsigned short x_offset;        /* X delta from anchor point (-32 to +31) */
    unsigned short y_offset;        /* Y delta from anchor point (-32 to +31) */
} LocationPacket;

#pragma pack(pop)

/* ACTION Packet - Player action/interaction */
typedef struct {
    unsigned char packet_type;     /* PACKET_TYPE_ACTION */
    unsigned char player_id;        /* Local player ID */
    unsigned short action_code;     /* Action identifier (from action.h) */
    unsigned short target_x;        /* Target X coordinate (-32 to +31) */
    unsigned short target_y;        /* Target Y coordinate (-32 to +31) */
} ActionPacket;

#pragma pack(pop)

/* BATTLE_JOIN Packet - Join existing battle instance */
typedef struct {
    unsigned char packet_type;     /* PACKET_TYPE_BATTLE_JOIN */
    unsigned char player_id;        /* Local player ID */
    unsigned short battle_x_offset; /* Battle location X offset */
    unsigned short battle_y_offset; /* Battle location Y offset */
} BattleJoinPacket;

#pragma pack(pop)

/* BATTLE_LEAVE Packet - Leave battle instance */
typedef struct {
    unsigned char packet_type;     /* PACKET_TYPE_BATTLE_LEAVE */
    unsigned char player_id;        /* Local player ID */
    unsigned short battle_x_offset; /* Battle location X offset */
    unsigned short battle_y_offset; /* Battle location Y offset */
} BattleLeavePacket;

#pragma pack(pop)

/* CHAT Packet - In-game chat message */
typedef struct {
    unsigned char packet_type;     /* PACKET_TYPE_CHAT */
    unsigned char player_id;        /* Local player ID */
    unsigned short message_length;  /* Length of chat string (12 max) */
    char message[GBC_MAX_MESSAGE_LENGTH];  /* Chat message text */
} ChatPacket;

#pragma pack(pop)

/* LISTENERS Packet - Register for area events */
typedef struct {
    unsigned char packet_type;     /* PACKET_TYPE_LISTENERS */
    unsigned char listener_count;   /* Number of listeners (1-8) */
    unsigned short x_anchor;        /* Anchor X coordinate (-32 to +31) */
    unsigned short y_anchor;        /* Anchor Y coordinate (-32 to +31) */
} ListenersPacket;

#pragma pack(pop)

/* EVENT Packet - Trigger event notification */
typedef struct {
    unsigned char packet_type;     /* PACKET_TYPE_EVENT */
    unsigned char event_code;       /* Event identifier (from area_event.h) */
    unsigned short x_location;      /* Location X coordinate (-32 to +31) */
    unsigned short y_location;      /* Location Y coordinate (-32 to +31) */
} EventPacket;

#pragma pack(pop)

/* GAME_STATE Packet - Full game state sync request/response */
typedef struct {
    unsigned char packet_type;     /* PACKET_TYPE_GAME_STATE */
    unsigned char version;          /* Protocol version */
    ClientState client_state;       /* Current client state */
    ServerCapabilities server_caps; /* Server capabilities */
    unsigned short player_count;    /* Number of connected players */
    unsigned char padding[4];        /* Reserve space for future use */
} GameStatePacket;

#pragma pack(pop)

/* PLAYER_LIST Packet - Player list update broadcast */
typedef struct {
    unsigned char packet_type;     /* PACKET_TYPE_PLAYER_LIST */
    unsigned char player_count;    /* Number of players in list (1-15) */
    unsigned short x_anchor;       /* Anchor X coordinate (-32 to +31) */
    unsigned short y_anchor;       /* Anchor Y coordinate (-32 to +31) */
} PlayerListPacket;

#pragma pack(pop)

/* ==========================================================================
 * NETWORK HANDLERS DECLARATIONS
 * Used by protocol.c for packet processing
 * ======================================================================== */

void handle_ping(NetPacket* packet);
void handle_pong(NetPacket* packet);
void handle_join(NetPacket* packet);
void handle_leave(NetPacket* packet);
void handle_status(NetPacket* packet);
void handle_location(NetPacket* packet);
void handle_action(NetPacket* packet);
void handle_battle_join(NetPacket* packet);
void handle_battle_leave(NetPacket* packet);
void handle_chat(NetPacket* packet);
void handle_listeners(NetPacket* packet);
void handle_event(NetPacket* packet);
void handle_game_state(NetPacket* packet);
void handle_player_list(NetPacket* packet);

#ifdef __cplusplus
}
#endif

#pragma pack(pop)
