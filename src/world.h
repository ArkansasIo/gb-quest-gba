#ifndef _WORLD_H
#define _WORLD_H

#include <gb/gb.h>
#include <stdint.h>
#include <stdbool.h>

#include "map.h"

/**
 * The open world is a 9x8 grid of zones. Zone maps (and the 100 tower floors)
 * are generated at runtime into RAM from a seed derived from the zone id, so
 * every zone has a stable layout without costing ROM for map data.
 */
#define WORLD_ZONE_COUNT 72
#define WORLD_ZONE_COLS 9
#define WORLD_ZONE_ROWS 8
#define WORLD_BIOME_COUNT 9
#define WORLD_LEVEL_TIER_COUNT 9
#define WORLD_LEVELS_PER_TIER 80
#define WORLD_TOWER_MAX_FLOOR 100
#define WORLD_RAID_COUNT 9
#define WORLD_RAID_FLOORS 3
#define WORLD_TRAIL_COUNT 16

/**
 * Size (in map tiles) of a generated zone map.
 */
#define WORLD_MAP_W 16
#define WORLD_MAP_H 14

/**
 * Zone where a new game begins (a town in the north west region).
 */
#define WORLD_START_ZONE 10

/**
 * Zone that holds the entrance to the tower.
 */
#define WORLD_TOWER_ZONE 31

/**
 * Zone that holds the entrance to the original labyrinth (floor 1).
 */
#define WORLD_DUNGEON_ZONE 1

/**
 * Bank in which the world system resides.
 */
#define WORLD_BANK 23

typedef enum WorldBiome {
  WORLD_BIOME_FOREST = 0,
  WORLD_BIOME_PLAINS,
  WORLD_BIOME_DESERT,
  WORLD_BIOME_SWAMP,
  WORLD_BIOME_FOOTHILLS,
  WORLD_BIOME_COASTAL,
  WORLD_BIOME_SNOWFIELD,
  WORLD_BIOME_VOLCANIC,
  WORLD_BIOME_UNDERGROUND,
} WorldBiome;

/**
 * Where the player currently is.
 */
typedef enum WorldLocation {
  WORLD_LOC_ZONE,
  WORLD_LOC_TOWER,
  WORLD_LOC_RAID,
  WORLD_LOC_DUNGEON,
} WorldLocation;

typedef struct TowerState {
  bool active;
  uint8_t floor;
  uint8_t entrance_zone_id;
} TowerState;

typedef struct RaidState {
  bool active;
  uint8_t raid_id;
  uint8_t floor;
  uint8_t entrance_zone_id;
} RaidState;

extern TowerState tower_state;
extern RaidState raid_state;

/**
 * Last town the player visited (respawn point).
 */
extern uint8_t world_last_town;

/**
 * Highest tower floor the player has reached.
 */
extern uint8_t world_best_tower_floor;

/**
 * Bit flags of zones the player has visited (72 bits).
 */
extern uint8_t world_visited[9];

/**
 * Enters the given zone directly (new game, respawn). Loads the floor but does
 * not initialize the map controller.
 */
void world_enter_zone(uint8_t zone_id) BANKED;

/**
 * Called by the map system before any floor is loaded. Generates world zone
 * and tower floor data when needed.
 * @return The floor bank to load.
 */
FloorBank *world_prepare_floor(FloorBank *f) BANKED;

/**
 * @return Id of the current zone, or 0xFF when in the tower or labyrinth.
 */
uint8_t world_current_zone(void) BANKED;

/**
 * @return Where the player currently is.
 */
WorldLocation world_location(void) BANKED;

/**
 * @return `true` if the player is in a town zone.
 */
bool world_in_town(void) BANKED;

/**
 * @return The monster level for the current area.
 */
uint16_t world_area_level(void) BANKED;

/** Converts character or area level (1-720) into world tier (1-9). */
uint8_t world_tier_for_level(uint16_t level) BANKED;

/** Returns the tier of the current zone, raid, tower floor, or labyrinth. */
uint8_t world_current_tier(void) BANKED;

/**
 * @return The biome of the current area.
 */
uint8_t world_biome(void) BANKED;

/**
 * Writes a short name for the current area into `buf` (max 18 chars).
 */
void world_area_name(char *buf) BANKED;

/**
 * Loads the respawn floor after the player is defeated.
 */
void world_respawn(void) BANKED;

#endif
