# Open World — Implementation Plan

> **For agentic workers:** implement this plan task-by-task, in order, checking off each step as it lands.

**Goal:** The world is an open world of hand-authored zones: a 9x8 grid = 72 zones, each a generated map with a biome (forest, plains, desert, swamp, foothills, coastal, snowfield, volcanic, underground). Towns/cities/villages and hubs are designated settlement zones. Other zones include Entered via entrances to existing floors (floor1-8). A tower entrance zone leads to a tower with 1-100 floors. 18 routes + 16 trails defined as zone-pair connections. Movement between zones reuses the existing map/exit/teleport machinery.

**Architecture:** World zones are `Floor` structs (so the existing map system handles movement, exits, battles, etc.). `world.c` holds the world state: the 72 zone floors, biome tables, routes/trails tables, and tower state. Zone maps are procedurally generated at runtime into RAM.

**Tech Stack:** Game Boy Development Kit (GBDK-2020), C, the game's existing map system.

---

## Task 1: Data structures (world.h, world.c skeleton)

### Files:
- Create: `src/world.h`
- Create: `src/world.c`
- Modify: `src/map.h` (add `Floor *world_floor(void)` accessors? — see Task 3)

- [ ] Step 1: Add to `src/world.h`:
```c
#ifndef _WORLD_H
#define _WORLD_H

#include <stdint.h>
#include "map.h"

#define WORLD_ZONE_COUNT 72
#define WORLD_GRID_COLS 9
#define WORLD_GRID_ROWS 8

#define WORLD_ZONE_MAP_W 14
#define WORLD_ZONE_MAP_H 14
#define WORLD_ZONE_MAP_BYTES (WORLD_ZONE_MAPW * WORLD_ZONE_MAP_H)  /* 196 */

typedef enum WorldBiome {
  WORLD_BIOME_FOREST,
  WORLD_BIOME_PLAINS,
  WORLD_BIOME_DESERT,
  WORLD_BIOME_SWAMP,
  WORLD_BIOME_FOOTHILLS,
  WORLD_BIOME_COASTAL,
  WORLD_BIOME_SNOWFIELD,
  WORLD_BIOME_VOLCANIC,
  WORLD_BIOME_UNDERGROUND,
  WORLD_BIOME_COUNT,
} WorldBiome;

typedef enum WorldZoneFlag {
  ZONE_FLAG_TOWN = FLAG(0),   /* settlement: towns, cities, villages, hubs */
  ZONE_FLAG_HUB = FLAG(1),    /* major connection hub */
  ZONE_FLAG_UNDERGROUND = FLAG(2),
  ZONE_FLAG_TOWER_ENTRANCE = FLAG(3),
} WorldZoneFlag;

typedef struct WorldZone {
  uint8_t id;
  WorldBiome biome;
  uint8_t flags;
  Floor floor;  /* the map system treats zones as floors */
  Map map;      /* points into zone map data */
  uint8_t map_data[WORLD_ZONE_MAP_BYTES];
  int8_t tower_entrance_col; /* where the tower entrance square is */
  int8_t tower_entrance_row;
} WorldZone;

typedef struct WorldRoute {
  uint8_t from;  /* zone id */
  uint8_t to;    /* zone id */
  uint8_t kind;  /* 0 = route, 1 = trail */
} WorldRoute;

#define WORLD_ROUTE_COUNT 18
#define WORLD_TRAIL_COUNT 16

void world_generate(void) NONBANKED;
void world_enter_zone(uint8_t zone_id) NONBANKED;
const WorldZone *world_zone(uint8_t zone_id);
uint8_t world_current_zone(void);
extern WorldRoute world_routes[WORLD_ROUTE_COUNT];
extern WorldRoute world_trails[WORLD_TRAIL_COUNT];

#endif
```

- [ ] Step 2: Add tower state to world.c skeleton (included in final world.c, created now):
```c
/* Tower: floors 1..100 */
#define TOWER_MAX_FLOOR 100
typedef struct TowerState {
  bool active;
  uint8_t floor;
} TowerState;
extern TowerState tower_state;
```

## Task 2: World generation — zones, biomes, towns, underground, tower

### Files:
- Create: `src/world_gen.c`? or `src/world.c` — implement here (merge into world.c).

- [ ] Step 1: Implement zone map generation in world.c:
  - `generate_zone_map(WorldZone *zone)`:
    - Fill with biome base tile (reuse `dungeon.bin` tiles? — use tiles from res/tiles: use the game's `MapTile` palette + tile ids from the world_map asset or existing map data. For MVP, use tile ids from `world_map.png` (first tile) as generic terrain tile, and `dungeon.png` for underground zones.)
    - Add random terrain patches (biome-specific: forests add tree/grass tiles, deserts add sand, swamps add murky water tiles, etc. — use the same tile ids).
    - Place exits at map edges: for each of N/E/S/W, if the grid neighbor exists, place an exit square at the zone edge (tile id from `dungeon.png` edge tile or a portal tile) with `to_floor = &world_zones[neighbor].floor`.
    - For towns: place signs/NPCs markers? MVP: use `Sign`-less; add town-flag decorative tiles.
    - Underground zones: always use dungeon-style tiles.
  - Use the game RNG: `rand()`? The game uses arand (`initarand(RANDOM_SEED)`, `d8()`, `d16()` — battle uses those? — grep for `d8` def. Use `d16()`-style RNG or `rand()` — check quickly; use `arand()`? I'll use `rand()` from GBDK if available — map.c uses? — grep later; simple: `rand()`.)

- [ ] Step 2: `world_generate(void)`:
  - For each zone 0..71: set id, grid position (col = id % 9, row = id / 9), biome = zone_biome(id) (deterministic pattern: forests first, plains, etc.), flags (towns: pick ~8 zones, some hubs; underground: some zones; tower entrance: the town "Hub" zone).
  - Generate each zone's map via `generate_zone_map`.
  - Set `world_zones[i].floor` fields: id, default_x/y (center), palettes (nil), maps = &world_zones[i].map, exits = world_zones[i].floor.exits? — the Floor struct needs exits: the map system looks up exits via floors.exits? — I need to check how exits are stored/used — Floor has `exits` array and map system uses `active_floor->exits`? For generated zones, exits are placed into the zone map data as exit tiles; the Floor.exits array can be empty for zones — the map system handles map-based exits via hash? — I need to check `check_map_move` / `load_exit` quickly (map.c). If exits are Floor.exits — generate: `zone->floor.exits = world_zones[i].exits_array` — build a static exits array per zone pointing to neighbor zone floors.
  - Set routes/trails tables: routes between designated zone pairs (list 18), trails (16).

- [ ] Step 3: Tower: `tower_enter(WorldZone *zone)` — sets tower_state.active = true, floor = 1, switches to tower floor: generate tower floor map into `tower_floors[1]` (a static Floor), set_active_floor. Tower floors: static array `Floor tower_floors[TOWER_MAX_FLOOR + 1]` generated on demand (`tower_generate_floor(n)`).

## Task 3: Integration with the map system (hero_select/world map entry)

### Files:
- Modify: `src/hero_select.c` — after class select, start game: set active floor to zone 0 instead of floor1.
- Modify: `src/map.c` — `init_world_map` entry points: allow world zones as active floors.

- [ ] Step 1: Check `load_exit`/`check_map_move` in map.c to confirm zone exits (to_floor) work as-is.
- [ ] Step 2: In `start_game` (hero_select.c): `set_active_floor(&world_zones[0].floor)` instead of `&bank_floor1`; generate world: `world_generate()` at game start (from title? — generate at creation).
- [ ] Step 3: Zones as Floors: ensure `set_active_floor` handles non-bank Floors — check implementation; adjust to use FloorBank (create `const FloorBank world_zone_banks[72]` pointing to zone floors).

## Task 4: Tests (smoke tests via mGBA + build)

- [ ] Step 1: Build the ROM (powershell build loop from before).
- [ ] Step 2: Run mGBA, check: title → hero select → world map = zone 0 of the new open world; walk to an edge, confirm exit to neighbor zone works; confirm tower entrance reachable and towers work; confirm towns are visitable.

---

*Phase 2 continues after: routes/trails travel menu wiring, underground entrance icons, etc. (future cycles)*
