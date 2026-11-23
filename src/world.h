#ifndef _WORLD_H
#define _WORLD_H

#include <stdint.h>
#include "map.h"

/**
 * Number of zones in the world: a 9x8 grid = 72 zones.
 */
#define WORLD_ZONE_COUNT 72

/**
 * Grid dimensions of the world.
 */
#define WORLD_ZONE_COLS 9
#define WORLD_ZONE_ROWS 8

/**
 * Map size (in map tiles) of each world zone.
 */
#define WORLD_ZONE_MAP_W 8
#define WORLD_ZONE_MAP_H 8

#define WORLD_ZONE_MAP_BYTES (WORLD_ZONE_MAP_W * WORLD_ZONE_MAP_H)

/**
 * Number of biomes.
 */
#define WORLD_BIOME_COUNT 9

/**
 * Number of tower floors.
 */
#define WORLD_TOWER_MAX_FLOOR 100

/**
 * Number of world routes and trails.
 */
#define WORLD_ROUTE_COUNT 18
#define WORLD_TRAIL_COUNT 16

/**
 * Bank in which the world zone data resides (see world_zones.c).
 */
#define WORLD_DATA_BANK 18

/**
 * Enumeration of world biomes.
 */
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
 * Flags denoting the type of a world zone.
 */
typedef enum WorldZoneFlag {
  /**
   * Zone is a town, city, or village.
   */
  ZONE_FLAG_TOWN = 1,
  /**
   * Zone is a major connection hub.
   */
  ZONE_FLAG_HUB = 2,
  /**
   * Zone is underground (e.g. a dungeon).
   */
  ZONE_FLAG_UNDERGROUND = 4,
  /**
   * Zone contains an entrance to the tower.
   */
  ZONE_FLAG_TOWER_ENTRANCE = 8,
} WorldZoneFlag;

/**
 * A zone of the open world. Each zone is a hand-authored map that the player
 * can walk across. Zones connect to neighboring zones via exit tiles.
 */
typedef struct WorldZone {
  /**
   * Unique id of the zone (0 - 71).
   */
  uint8_t id;
  /**
   * Biome of the zone.
   */
  uint8_t biome;
  /**
   * Sub-biome variant of the zone (adds terrain variety).
   */
  uint8_t sub_biome;
  /**
   * Flags denoting the type of the zone.
   */
  uint8_t flags;
  /**
   * Pointer to the map system's floor data for this zone.
   */
  Floor *floor;
} WorldZone;

/**
 * A world route: a connection between two zones.
 */
typedef struct WorldRoute {
  /**
   * Id of the zone the connection starts at.
   */
  uint8_t from;
  /**
   * Id of the zone the connection ends at.
   */
  uint8_t to;
  /**
   * Type of connection (0 = route, 1 = trail).
   */
  uint8_t kind;
} WorldRoute;

/**
 * State for the tower: a tower with 1-100 floors entered via a zone entrance.
 */
typedef struct TowerState {
  /**
   * Whether the player is inside the tower.
   */
  bool active;
  /**
   * Current tower floor (1 - 100).
   */
  uint8_t floor;
  /**
   * Id of the zone with the tower entrance.
   */
  uint8_t entrance_zone_id;
} TowerState;

/**
 * The world: 72 hand-authored zones in a 9x8 grid.
 */
extern const WorldZone world_zones[WORLD_ZONE_COUNT];

/**
 * Floor banks for the world zones.
 */
extern const FloorBank world_zone_banks[WORLD_ZONE_COUNT];

/**
 * Floor banks for the tower floors (indices 1 - 100).
 */
extern const FloorBank tower_floor_banks[WORLD_TOWER_MAX_FLOOR + 1];

/**
 * World routes and trails.
 */
extern WorldRoute world_routes[WORLD_ROUTE_COUNT];
extern WorldRoute world_trails[WORLD_TRAIL_COUNT];

/**
 * The tower state.
 */
extern TowerState tower_state;

/**
 * Enters the zone with the given id, loading its map into the map system.
 */
void world_enter_zone(uint8_t zone_id);

/**
 * @return Id of the zone the player is currently in; 0xFF while in the tower.
 */
uint8_t world_current_zone(void);

/**
 * @return The biome of the given zone.
 */
WorldBiome world_zone_biome(uint8_t zone_id);

/**
 * Enters the tower. Activates tower mode and enters tower floor 1.
 */
void tower_enter(uint8_t zone_id);

/**
 * Exits the tower, returning to the tower entrance zone.
 */
void tower_exit(void);

/**
 * Moves down one tower floor.
 */
void tower_go_down(void);

/**
 * Moves up one tower floor.
 */
void tower_go_up(void);

/**
 * Called by the map system when a floor is loaded. Tracks the current zone.
 */
void world_on_load_floor(FloorBank *f);

#endif
