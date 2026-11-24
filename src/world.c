#pragma bank 23

/**
 * Open world: a 9x8 grid of zones plus a 100 floor tower. All zone and tower
 * maps are generated at runtime into RAM. Each zone's layout is derived from a
 * seed based on its id, so it is the same every time the player visits.
 *
 * Layout of the grid: the world is split into nine 3x3 regions, each with its
 * own biome. The center zone of every region is a town (no monsters, sign,
 * shop/stash/quests via the START menu).
 */

#include <gb/gb.h>
#include <stdio.h>
#include <string.h>

#include "world.h"
#include "core.h"
#include "encounter.h"
#include "floor.h"
#include "loot.h"
#include "map.h"
#include "player.h"
#include "progress.h"

TowerState tower_state;
RaidState raid_state;
uint8_t world_last_town = WORLD_START_ZONE;
uint8_t world_best_tower_floor;
uint8_t world_visited[9];

//------------------------------------------------------------------------------
// RAM floor data (the map system reads these through `world_fb`/`tower_fb`)
//------------------------------------------------------------------------------

static FloorBank world_fb;
static FloorBank tower_fb;
static FloorBank raid_fb;
static Floor gen_floor;
static Map gen_maps[2];
static Exit gen_exits[8];
static Sign gen_signs[2];
static uint8_t gen_map_data[WORLD_MAP_W * WORLD_MAP_H * 2];
static char sign_text[72];

static uint8_t current_zone = WORLD_START_ZONE;
static WorldLocation location = WORLD_LOC_ZONE;
static bool direct_entry;
static uint8_t gen_w;
static uint8_t gen_h;
static uint16_t area_level = 1;
static uint8_t area_biome;
static bool area_town;

/**
 * Treasure chest for the current area.
 */
static int8_t chest_x = -1;
static int8_t chest_y = -1;
static bool chest_opened;

/**
 * Whether the boss of the current tower floor has been defeated.
 */
static bool floor_boss_pending;
static bool raid_boss_pending;

// Empty object lists (read while this bank is mapped by the floor loader).
static const Chest no_chests[] = { { END } };
static const Lever no_levers[] = { { END } };
static const Door no_doors[] = { { END } };
static const Sconce no_sconces[] = { { END } };
static const NPC no_npcs[] = { { END } };

//------------------------------------------------------------------------------
// Tile ids (dungeon tileset metatiles)
//------------------------------------------------------------------------------

#define T_VOID 0x00
#define T_FLOOR 0x01
#define T_CHEST 0x1D
#define T_SIGN 0x0F
#define T_DOORWAY 0x17
#define T_STAIRS_DOWN 0x24
#define T_STAIRS_UP 0x17
#define T_TOWER 0x22
#define T_TRAIL 0x22

#define A_WALL 0
#define A_GROUND 1
#define A_EXIT 2

static const uint8_t biome_wall[WORLD_BIOME_COUNT] = {
  0x10, 0x11, 0x12, 0x10, 0x11, 0x12, 0x10, 0x11, 0x00
};

static const uint8_t biome_obstacle[WORLD_BIOME_COUNT] = {
  0x07, 0x30, 0x30, 0x2C, 0x30, 0x08, 0x07, 0x2D, 0x08
};

/*
 * World atlas names, in row-major order. Keeping names in ROM gives every
 * generated zone a stable identity without storing 72 separate maps.
 */
static const char *const zone_names[WORLD_ZONE_COUNT] = {
  // Row 0, zones 0-8
  "Mosswood", "Old King's Road", "Thornwatch", "Brightmeadow", "Windrun",
  "Sunfield", "Red Dunes", "Glasswind", "Suncleft",
  // Row 1, zones 9-17
  "Greenhollow", "Ashford", "Foxglove Vale", "Millcross", "Brightvale",
  "The Long Acre", "Mirage Run", "Sandmere", "Copper Flats",
  // Row 2, zones 18-26
  "Whisperfen", "Mossgate", "Reedmaze", "Kingsroad March", "Highgrass",
  "Barleycross", "Saltmarsh", "Reedhaven", "Blackwater Reach",
  // Row 3, zones 27-35
  "Gloamwood", "Briarwatch", "Hollowmere", "Old Stoneway", "Dragonwatch",
  "Eagle's Rest", "Grayridge", "Ironroot", "High Crags",
  // Row 4, zones 36-44
  "Cloudpass", "Mirefen", "Stormbreak Coast", "Old Salt Road", "Stonehelm",
  "Tideglass", "Shipwreck Strand", "Gullport", "Bluewater Bay",
  // Row 5, zones 45-53
  "Far Shoals", "Pinefall", "Frostmarch", "Whitecap Pass", "Icevein",
  "Northlight", "Saltwind", "Seawatch", "Beacon Isle",
  // Row 6, zones 54-62
  "Stormbreak Reach", "Pinefall", "Northlight Pass", "Emberfall",
  "Charstone", "Redglass Crater", "Rootdeep", "Sporelight", "Echoing Vault",
  // Row 7, zones 63-71
  "Frostmarch Gate", "Frosthold", "Whitecap Pass", "Cinderkeep Road",
  "Cinderkeep", "Wyrmstone", "The Deep Roads", "Underhall", "Abyssal Rift"
};

/* Nine regional capitals plus nine smaller settlements. */
static const uint8_t settlement_zones[18] = {
  10, 13, 16, 37, 40, 43, 64, 67, 70,
  19, 23, 25, 28, 32, 52, 55, 58, 61
};

typedef struct WorldTrail {
  uint8_t from;
  uint8_t to;
  const char *name;
} WorldTrail;

static const WorldTrail world_trails[WORLD_TRAIL_COUNT] = {
  { 1, 19, "King's Road Trail" },
  { 3, 22, "Sunfield Track" },
  { 5, 41, "Saltwind Way" },
  { 7, 48, "Glasswind Pass" },
  { 8, 53, "Beacon Path" },
  { 9, 27, "Mirefen Causeway" },
  { 11, 35, "Eagle Ridge Trail" },
  { 12, 39, "Old Salt Road" },
  { 14, 42, "Wreckers' Run" },
  { 15, 44, "Bluewater Track" },
  { 17, 26, "Copper Road" },
  { 18, 36, "Cloudpass Trail" },
  { 20, 45, "Frostmarch Way" },
  { 21, 33, "Grayridge Pass" },
  { 24, 50, "Northlight Path" },
  { 56, 69, "Deep Roads Spur" },
};

static const uint8_t raid_zones[WORLD_RAID_COUNT] = {
  10, 13, 16, 37, 40, 43, 64, 67, 70
};

static const char *const raid_names[WORLD_RAID_COUNT] = {
  "Root-Crown Raid", "Sunfield Citadel", "Glass Dune Vault",
  "Witchwater Depths", "Dragonwatch Bastion", "Tideglass Maw",
  "Frostmarch Hold", "Cinderkeep Caldera", "Abyssal Court"
};

static const uint8_t raid_bosses[WORLD_RAID_COUNT] = {
  MONSTER_OWLBEAR, MONSTER_GOBLIN, MONSTER_KOBOLD,
  MONSTER_WILL_O_WISP, MONSTER_DRAGON, MONSTER_DISPLACER_BEAST,
  MONSTER_DEATHKNIGHT, MONSTER_DRAGON, MONSTER_BEHOLDER
};

static const char *const raid_boss_titles[WORLD_RAID_COUNT] = {
  "Owlbear Matriarch", "Goblin War-Chief", "Kobold Sun-Seer",
  "Wisp Queen", "Ancient Dragon", "Displacer Stalker",
  "Death Knight Marshal", "Ashen Dragon", "Beholder Oracle"
};

static const char *const settlement_names[18] = {
  "Ashford", "Brightvale", "Sandmere", "Mirefen", "Stonehelm",
  "Gullport", "Frosthold", "Cinderkeep", "Underhall", "Mossgate",
  "Barleycross", "Reedhaven", "Briarwatch", "Eagle's Rest", "Seawatch",
  "Pinefall", "Charstone", "Sporelight"
};

/**
 * Monsters that roam each biome (3 per biome, weakest first).
 */
static const uint8_t biome_monsters[WORLD_BIOME_COUNT][3] = {
  { MONSTER_KOBOLD, MONSTER_GOBLIN, MONSTER_OWLBEAR },
  { MONSTER_GOBLIN, MONSTER_BUGBEAR, MONSTER_DISPLACER_BEAST },
  { MONSTER_KOBOLD, MONSTER_GELATINOUS_CUBE, MONSTER_WILL_O_WISP },
  { MONSTER_ZOMBIE, MONSTER_GELATINOUS_CUBE, MONSTER_WILL_O_WISP },
  { MONSTER_GOBLIN, MONSTER_BUGBEAR, MONSTER_OWLBEAR },
  { MONSTER_KOBOLD, MONSTER_ZOMBIE, MONSTER_DISPLACER_BEAST },
  { MONSTER_ZOMBIE, MONSTER_OWLBEAR, MONSTER_DEATHKNIGHT },
  { MONSTER_BUGBEAR, MONSTER_DEATHKNIGHT, MONSTER_DRAGON },
  { MONSTER_ZOMBIE, MONSTER_MINDFLAYER, MONSTER_BEHOLDER },
};

static const uint8_t tower_bosses[] = {
  MONSTER_DEATHKNIGHT, MONSTER_BUGBEAR, MONSTER_MINDFLAYER,
  MONSTER_DRAGON, MONSTER_DRAGON, MONSTER_DEATHKNIGHT,
  MONSTER_DISPLACER_BEAST, MONSTER_DEATHKNIGHT, MONSTER_BEHOLDER,
};

static const char *const tower_boss_titles[] = {
  "R1 Margit, Fell Omen",
  "R2 Godrick Grafted",
  "R3 Rennala, Full Moon",
  "R4 Starscourge Radahn",
  "R5 Rykard, Blasphemy",
  "R6 Morgott, Omen King",
  "R7 Malenia, Blade of Miquella",
  "R8 Maliketh, Black Blade",
  "R9 Radagon / Elden Beast",
};

/**
 * Background palette 0 for each biome (+ towns, + tower).
 */
static const palette_color_t biome_palettes[(WORLD_BIOME_COUNT + 2) * 4] = {
  // Forest
  RGB8(170, 205, 140), RGB8(80, 135, 70), RGB8(30, 72, 40), RGB8(8, 20, 8),
  // Plains
  RGB8(215, 215, 150), RGB8(145, 165, 80), RGB8(72, 92, 40), RGB8(20, 20, 0),
  // Desert
  RGB8(245, 225, 165), RGB8(205, 165, 95), RGB8(135, 92, 42), RGB8(40, 20, 0),
  // Swamp
  RGB8(155, 175, 120), RGB8(92, 112, 70), RGB8(42, 62, 40), RGB8(10, 20, 10),
  // Foothills
  RGB8(195, 182, 160), RGB8(132, 112, 90), RGB8(72, 60, 50), RGB8(20, 10, 0),
  // Coast
  RGB8(205, 232, 242), RGB8(112, 162, 202), RGB8(42, 82, 132), RGB8(0, 10, 40),
  // Snowfield
  RGB8(250, 250, 255), RGB8(182, 202, 232), RGB8(102, 122, 162), RGB8(20, 30, 60),
  // Volcano
  RGB8(225, 145, 92), RGB8(165, 62, 42), RGB8(82, 22, 20), RGB8(20, 0, 0),
  // Deeps
  RGB8(190, 200, 190), RGB8(100, 100, 140), RGB8(40, 60, 40), RGB8(24, 0, 0),
  // Town
  RGB8(235, 215, 175), RGB8(175, 135, 92), RGB8(92, 62, 42), RGB8(30, 10, 0),
  // Tower
  RGB8(200, 190, 230), RGB8(120, 100, 170), RGB8(55, 40, 90), RGB8(15, 0, 25),
};

/**
 * Palettes 1-6 shared by every generated area.
 */
static const palette_color_t shared_palettes[6 * 4] = {
  // Item chests
  RGB8(192, 138, 40), RGB8(100, 100, 140), RGB8(40, 60, 40), RGB8(24, 0, 0),
  // Special chests
  RGB8(195, 222, 180), RGB8(100, 100, 140), RGB8(40, 60, 40), RGB8(24, 0, 0),
  RGB_WHITE, RGB8(120, 120, 120), RGB8(60, 60, 60), RGB_BLACK,
  RGB_WHITE, RGB8(120, 120, 120), RGB8(60, 60, 60), RGB_BLACK,
  RGB_WHITE, RGB8(120, 120, 120), RGB8(60, 60, 60), RGB_BLACK,
  RGB_WHITE, RGB8(120, 120, 120), RGB8(60, 60, 60), RGB_BLACK,
};

/**
 * Palettes for the active floor (RAM so the loader can read them in any bank).
 */
static palette_color_t gen_palettes[7 * 4];

//------------------------------------------------------------------------------
// Random numbers (deterministic per area)
//------------------------------------------------------------------------------

static uint16_t seed;

static uint8_t rnd(void) {
  seed ^= seed << 7;
  seed ^= seed >> 9;
  seed ^= seed << 8;
  return (uint8_t)(seed ^ (seed >> 8));
}

//------------------------------------------------------------------------------
// Map building helpers
//------------------------------------------------------------------------------

static void put(uint8_t x, uint8_t y, uint8_t tile, uint8_t attr, uint8_t pal) {
  uint8_t *p = gen_map_data + 2 * (x + y * gen_w);
  p[0] = (attr << 6) | (tile & 0x3F);
  p[1] = pal;
}

static bool is_ground(uint8_t x, uint8_t y) {
  return (gen_map_data[2 * (x + y * gen_w)] >> 6) == A_GROUND;
}

static uint8_t floor_tile(void) {
  return T_FLOOR;
}

static void add_exit(
  uint8_t col, uint8_t row,
  uint8_t to_col, uint8_t to_row,
  Direction heading,
  ExitType type,
  FloorBank *to
) {
  Exit *e = gen_exits;
  while (e->map_id != END)
    e++;
  e->map_id = 0;
  e->col = col;
  e->row = row;
  e->to_map = 0;
  e->to_col = to_col;
  e->to_row = to_row;
  e->heading = heading;
  e->exit_type = type;
  e->to_floor = to;
  (e + 1)->map_id = END;
}

static void reset_floor(uint8_t w, uint8_t h, uint8_t id, uint8_t pal_index) {
  gen_w = w;
  gen_h = h;

  gen_maps[0].id = 0;
  gen_maps[0].bank = WORLD_BANK;
  gen_maps[0].data = gen_map_data;
  gen_maps[0].width = w;
  gen_maps[0].height = h;
  gen_maps[1].id = END;

  gen_exits[0].map_id = END;
  gen_signs[0].map_id = END;
  gen_signs[1].map_id = END;

  const palette_color_t *src = biome_palettes + 4 * pal_index;
  for (uint8_t k = 0; k < 4; k++)
    gen_palettes[k] = src[k];
  for (uint8_t k = 0; k < 24; k++)
    gen_palettes[4 + k] = shared_palettes[k];

  gen_floor.id = id;
  gen_floor.default_x = w / 2;
  gen_floor.default_y = h / 2;
  gen_floor.palettes = gen_palettes;
  gen_floor.maps = gen_maps;
  gen_floor.exits = gen_exits;
  gen_floor.chests = no_chests;
  gen_floor.signs = gen_signs;
  gen_floor.levers = no_levers;
  gen_floor.doors = no_doors;
  gen_floor.sconces = no_sconces;
  gen_floor.npcs = no_npcs;

  chest_x = -1;
  chest_y = -1;
  chest_opened = false;
}

/**
 * Places a treasure chest on a random free ground tile.
 */
static void place_chest(void) {
  for (uint8_t tries = 0; tries < 24; tries++) {
    uint8_t x = 2 + rnd() % (gen_w - 4);
    uint8_t y = 2 + rnd() % (gen_h - 4);
    if (x == gen_w / 2 || y == gen_h / 2)
      continue;
    if (!is_ground(x, y))
      continue;
    put(x, y, T_CHEST, A_WALL, 1);
    chest_x = x;
    chest_y = y;
    return;
  }
}

//------------------------------------------------------------------------------
// Zones
//------------------------------------------------------------------------------

static uint8_t zone_col(uint8_t z) { return z % WORLD_ZONE_COLS; }
static uint8_t zone_row(uint8_t z) { return z / WORLD_ZONE_COLS; }

static uint8_t zone_biome(uint8_t z) {
  return (zone_row(z) / 3) * 3 + (zone_col(z) / 3);
}

static bool zone_is_town(uint8_t z) {
  for (uint8_t i = 0; i < 18; i++)
    if (settlement_zones[i] == z)
      return true;
  return false;
}

static const char *zone_settlement_name(uint8_t z) {
  for (uint8_t i = 0; i < 18; i++)
    if (settlement_zones[i] == z)
      return settlement_names[i];
  return "Settlement";
}

static uint16_t zone_level(uint8_t z) {
  const uint8_t c = zone_col(z);
  const uint8_t r = zone_row(z);
  const uint8_t tier = zone_biome(z) + 1;
  const uint8_t dx = c % 3 > 1 ? c % 3 - 1 : 1 - c % 3;
  const uint8_t dy = r % 3 > 1 ? r % 3 - 1 : 1 - r % 3;
  const uint8_t local = dx + dy;
  return (tier - 1) * WORLD_LEVELS_PER_TIER + 1 + local * 39;
}

static uint8_t raid_id_for_zone(uint8_t zone_id) {
  for (uint8_t i = 0; i < WORLD_RAID_COUNT; i++)
    if (raid_zones[i] == zone_id)
      return i;
  return 0xFF;
}

static uint8_t trail_destination(uint8_t zone_id) {
  for (uint8_t i = 0; i < WORLD_TRAIL_COUNT; i++) {
    if (world_trails[i].from == zone_id)
      return world_trails[i].to;
    if (world_trails[i].to == zone_id)
      return world_trails[i].from;
  }
  return 0xFF;
}

static void generate_zone(uint8_t z) {
  const uint8_t biome = zone_biome(z);
  const bool town = zone_is_town(z);
  const uint8_t w = WORLD_MAP_W;
  const uint8_t h = WORLD_MAP_H;
  const uint8_t wall = biome_wall[biome];
  const uint8_t obstacle = biome_obstacle[biome];

  reset_floor(w, h, 0x40 + z, town ? WORLD_BIOME_COUNT : biome);
  seed = 0x5A17 + z * 0x0193;
  if (seed == 0) seed = 1;

  // Ground with a wall border.
  for (uint8_t y = 0; y < h; y++) {
    for (uint8_t x = 0; x < w; x++) {
      if (x == 0 || y == 0 || x == w - 1 || y == h - 1)
        put(x, y, wall, A_WALL, 0);
      else
        put(x, y, floor_tile(), A_GROUND, 0);
    }
  }

  const uint8_t mx = w / 2;
  const uint8_t my = h / 2;

  if (town) {
    // Four houses around the square.
    for (uint8_t q = 0; q < 4; q++) {
      const uint8_t bx = (q & 1) ? mx + 2 : 2;
      const uint8_t by = (q & 2) ? my + 2 : 2;
      for (uint8_t y = by; y < by + 3; y++)
        for (uint8_t x = bx; x < bx + 4; x++)
          put(x, y, 0x10 + ((x + y) & 1), A_WALL, 0);
      put(bx + 1, by + 2, T_DOORWAY, A_WALL, 0);
    }
    put(mx - 1, my - 1, T_SIGN, A_WALL, 0);
    gen_signs[0].map_id = 0;
    gen_signs[0].col = mx - 1;
    gen_signs[0].row = my - 1;
    gen_signs[0].facing = UP;
    gen_signs[0].message = sign_text;
    gen_signs[1].map_id = END;
    sprintf(sign_text, "Welcome to\n%s!\fPress START for\nthe SHOP, STASH\nand QUESTS.",
      zone_settlement_name(z));
  } else {
    // Scatter obstacles, keeping the center cross clear.
    for (uint8_t y = 2; y < h - 2; y++) {
      for (uint8_t x = 2; x < w - 2; x++) {
        if (x == mx || y == my || x == mx - 1 || y == my - 1)
          continue;
        if ((rnd() & 0x0F) < 2)
          put(x, y, obstacle, A_WALL, 0);
      }
    }
    if ((rnd() & 3) != 0)
      place_chest();
  }

  // Border exits to neighboring zones.
  const uint8_t c = zone_col(z);
  const uint8_t r = zone_row(z);
  if (c + 1 < WORLD_ZONE_COLS) {
    put(w - 1, my, T_FLOOR, A_EXIT, 0);
    add_exit(w - 1, my, 0, my, RIGHT, EXIT_DOOR, &world_fb);
  }
  if (c > 0) {
    put(0, my, T_FLOOR, A_EXIT, 0);
    add_exit(0, my, w - 1, my, LEFT, EXIT_DOOR, &world_fb);
  }
  if (r + 1 < WORLD_ZONE_ROWS) {
    put(mx, h - 1, T_FLOOR, A_EXIT, 0);
    add_exit(mx, h - 1, mx, 0, DOWN, EXIT_DOOR, &world_fb);
  }
  if (r > 0) {
    put(mx, 0, T_FLOOR, A_EXIT, 0);
    add_exit(mx, 0, mx, h - 1, UP, EXIT_DOOR, &world_fb);
  }

  // Special entrances.
  if (z == WORLD_TOWER_ZONE) {
    put(mx, my - 3, T_TOWER, A_EXIT, 0);
    add_exit(mx, my - 3, 2, 1, DOWN, EXIT_STAIRS, &tower_fb);
  }
  if (z == WORLD_DUNGEON_ZONE) {
    put(mx, 2, T_DOORWAY, A_EXIT, 0);
    add_exit(mx, 2, 12, 16, HERE, EXIT_STAIRS, (FloorBank *)&bank_floor1);
  }

  const uint8_t raid_id = raid_id_for_zone(z);
  if (raid_id != 0xFF) {
    put(mx, my - 3, T_TOWER, A_EXIT, 0);
    add_exit(mx, my - 3, 2, 1, DOWN, EXIT_STAIRS, &raid_fb);
  }

  const uint8_t trail_to = trail_destination(z);
  if (trail_to != 0xFF) {
    // Carve a visible path from the center to the trailhead.
    for (uint8_t x = mx; x <= mx + 2; x++)
      put(x, my + 2, T_FLOOR, A_GROUND, 0);
    for (uint8_t y = my; y <= my + 2; y++)
      put(mx, y, T_FLOOR, A_GROUND, 0);
    put(mx + 2, my + 2, T_TRAIL, A_EXIT, 0);
    add_exit(mx + 2, my + 2, WORLD_MAP_W / 2, WORLD_MAP_H / 2,
      HERE, EXIT_DOOR, &world_fb);

    if (!town && gen_signs[0].map_id == END) {
      for (uint8_t i = 0; i < WORLD_TRAIL_COUNT; i++) {
        if (world_trails[i].from == z || world_trails[i].to == z) {
          sprintf(sign_text, "%s\nShortcut to\n%s.", world_trails[i].name,
            zone_names[trail_to]);
          gen_signs[0].map_id = 0;
          gen_signs[0].col = mx - 1;
          gen_signs[0].row = my + 2;
          gen_signs[0].facing = UP;
          gen_signs[0].message = sign_text;
          gen_signs[1].map_id = END;
          break;
        }
      }
    }
  }

  area_biome = biome;
  area_town = town;
  area_level = zone_level(z);
  location = WORLD_LOC_ZONE;
  world_visited[z >> 3] |= 1 << (z & 7);
  if (town) {
    world_last_town = z;
    save_game();  // Autosave whenever a town is entered.
  }

  if (town)
    config_random_encounter(255, 0, 0, false);
  else
    config_random_encounter(6, 3, 3, false);
}

//------------------------------------------------------------------------------
// Tower
//------------------------------------------------------------------------------

#define TOWER_W 14
#define TOWER_H 14

static void generate_tower_floor(uint8_t n) {
  const uint8_t w = TOWER_W;
  const uint8_t h = TOWER_H;
  reset_floor(w, h, 0x90 + (n & 0x3F), WORLD_BIOME_COUNT + 1);
  seed = 0x7331 + n * 0x0BAD;
  if (seed == 0) seed = 1;

  for (uint8_t y = 0; y < h; y++) {
    for (uint8_t x = 0; x < w; x++) {
      if (x == 0 || y == 0 || x == w - 1 || y == h - 1)
        put(x, y, T_VOID, A_WALL, 0);
      else if (x % 4 == 0 && y % 4 == 0)
        put(x, y, 0x07, A_WALL, 0);
      else if (x > 3 && y > 3 && x < w - 4 && y < h - 4 && (rnd() & 0x0F) < 2)
        put(x, y, 0x08, A_WALL, 0);
      else
        put(x, y, floor_tile(), A_GROUND, 0);
    }
  }

  // Stairs up (to previous floor / world) and down (next floor).
  put(2, 1, T_STAIRS_UP, A_EXIT, 0);
  put(2, 2, T_FLOOR, A_GROUND, 0);
  if (n == 1) {
    add_exit(2, 1, WORLD_MAP_W / 2, WORLD_MAP_H / 2 - 3, DOWN, EXIT_STAIRS,
      &world_fb);
  } else {
    add_exit(2, 1, w - 3, h - 2, UP, EXIT_STAIRS, &tower_fb);
  }

  if (n < WORLD_TOWER_MAX_FLOOR) {
    put(w - 3, h - 2, T_STAIRS_DOWN, A_EXIT, 0);
    put(w - 3, h - 3, T_FLOOR, A_GROUND, 0);
    add_exit(w - 3, h - 2, 2, 1, DOWN, EXIT_STAIRS, &tower_fb);
  }

  gen_floor.default_x = 2;
  gen_floor.default_y = 2;

  if ((rnd() & 1) == 0)
    place_chest();

  area_biome = WORLD_BIOME_UNDERGROUND;
  area_town = false;
  area_level = 1 + ((uint32_t)(n - 1) * (MAX_LEVEL - 1)) /
    (WORLD_TOWER_MAX_FLOOR - 1);
  location = WORLD_LOC_TOWER;
  floor_boss_pending = (n % 5) == 0;

  if (n > world_best_tower_floor)
    world_best_tower_floor = n;

  config_random_encounter(5, 4, 4, false);
}

static void generate_raid_floor(uint8_t n) {
  const uint8_t w = TOWER_W;
  const uint8_t h = TOWER_H;
  const uint8_t biome = zone_biome(raid_state.entrance_zone_id);
  reset_floor(w, h, 0xB0 + raid_state.raid_id * WORLD_RAID_FLOORS + n,
    biome);
  seed = 0x4D21 + raid_state.raid_id * 0x0317 + n * 0x0193;
  if (seed == 0) seed = 1;

  for (uint8_t y = 0; y < h; y++) {
    for (uint8_t x = 0; x < w; x++) {
      if (x == 0 || y == 0 || x == w - 1 || y == h - 1)
        put(x, y, biome_wall[biome], A_WALL, 0);
      else if (x % 4 == 0 && y % 4 == 0)
        put(x, y, biome_obstacle[biome], A_WALL, 0);
      else if (x > 3 && y > 3 && x < w - 4 && y < h - 4 && (rnd() & 0x0F) < 2)
        put(x, y, biome_obstacle[biome], A_WALL, 0);
      else
        put(x, y, floor_tile(), A_GROUND, 0);
    }
  }

  put(2, 1, T_STAIRS_UP, A_EXIT, 0);
  put(2, 2, T_FLOOR, A_GROUND, 0);
  if (n == 1) {
    add_exit(2, 1, WORLD_MAP_W / 2, WORLD_MAP_H / 2 - 3,
      UP, EXIT_STAIRS, &world_fb);
  } else {
    add_exit(2, 1, w - 3, h - 2, UP, EXIT_STAIRS, &raid_fb);
  }

  if (n < WORLD_RAID_FLOORS) {
    put(w - 3, h - 2, T_STAIRS_DOWN, A_EXIT, 0);
    put(w - 3, h - 3, T_FLOOR, A_GROUND, 0);
    add_exit(w - 3, h - 2, 2, 1, DOWN, EXIT_STAIRS, &raid_fb);
  }

  gen_floor.default_x = 2;
  gen_floor.default_y = 2;
  if ((rnd() & 1) == 0)
    place_chest();

  area_biome = biome;
  area_town = false;
  area_level = zone_level(raid_state.entrance_zone_id) + (n - 1) * 12;
  location = WORLD_LOC_RAID;
  raid_boss_pending = n == WORLD_RAID_FLOORS;
  config_random_encounter(5, 4, 4, false);
}

//------------------------------------------------------------------------------
// Encounters
//------------------------------------------------------------------------------

static EncounterTable gen_table[2];

static PowerTier tier_for_level(uint16_t level) {
  if (level >= 200) return A_TIER;
  if (level >= 60) return B_TIER;
  return C_TIER;
}

static uint8_t vary_level(uint16_t base) {
  if (base > 99) base = 99;  // legacy monster generators use levels 1-99
  int8_t l = (int8_t)base + (int8_t)(d4()) - 1;
  if (l < 1) l = 1;
  if (l > 99) l = 99;
  return (uint8_t)l;
}

static void build_random_encounter(void) {
  const uint8_t *mons = biome_monsters[area_biome];
  const uint16_t lvl = area_level;
  EncounterTable *t = gen_table;

  // Stronger monster types show up as the area level rises.
  uint8_t max_kind = lvl < 10 ? 1 : (lvl < 30 ? 2 : 3);
  uint8_t count = 1 + (d4() % 3);
  if (lvl < 6 && count > 2)
    count = 2;

  t->odds = 254;  // ODDS_100P equals the END marker
  t->layout = count == 1 ? MONSTER_LAYOUT_1 :
    (count == 2 ? MONSTER_LAYOUT_2 : MONSTER_LAYOUT_3S);
  t->monster1 = mons[rand() % max_kind];
  t->monster1_level = vary_level(lvl);
  t->monster1_tier = tier_for_level(lvl);
  t->monster2 = mons[rand() % max_kind];
  t->monster2_level = vary_level(lvl);
  t->monster2_tier = tier_for_level(lvl);
  t->monster3 = mons[0];
  t->monster3_level = vary_level(lvl);
  t->monster3_tier = tier_for_level(lvl);
  (t + 1)->odds = END;

  generate_encounter(gen_table);
}

static void build_boss_encounter(void) {
  const uint8_t n = tower_state.floor;
  EncounterTable *t = gen_table;
  const uint8_t count = sizeof(tower_bosses);
  const uint8_t idx = ((n / 5) - 1) % count;
  t->odds = 254;  // ODDS_100P equals the END marker
  t->layout = MONSTER_LAYOUT_1;
  t->monster1 = tower_bosses[idx];
  t->monster1_level = vary_level(area_level);
  t->monster1_tier = S_TIER;
  (t + 1)->odds = END;
  generate_encounter(gen_table);
  // The boss name lives in RAM so it remains readable while ROM banks switch.
  strcpy(sign_text, tower_boss_titles[idx]);
  encounter.monsters[0].name = sign_text;
}

static void build_raid_boss_encounter(void) {
  EncounterTable *t = gen_table;
  t->odds = 254;
  t->layout = MONSTER_LAYOUT_1;
  t->monster1 = raid_bosses[raid_state.raid_id];
  t->monster1_level = vary_level(area_level);
  t->monster1_tier = S_TIER;
  (t + 1)->odds = END;
  generate_encounter(gen_table);
  sprintf(sign_text, "%s", raid_boss_titles[raid_state.raid_id]);
  encounter.monsters[0].name = sign_text;
}

/**
 * Floor callback: called after each step.
 */
static bool gen_on_move(void) {
  if (area_town)
    return false;
  if (!check_random_encounter())
    return false;
  build_random_encounter();
  start_battle();
  return true;
}

/**
 * Floor callback: called when the floor is initialized.
 */
static bool gen_on_init(void) {
  if (location == WORLD_LOC_TOWER) {
    quest_event(QUEST_EVENT_TOWER, tower_state.floor);
    if (floor_boss_pending) {
      floor_boss_pending = false;
      build_boss_encounter();
      start_battle();
      return true;
    }
  } else if (location == WORLD_LOC_RAID) {
    if (raid_boss_pending) {
      raid_boss_pending = false;
      build_raid_boss_encounter();
      start_battle();
      return true;
    }
  } else {
    quest_event(QUEST_EVENT_ZONE, current_zone);
  }
  return false;
}

/**
 * Floor callback: called when the player presses A with nothing else to do.
 */
static bool gen_on_action(void) {
  if (chest_x < 0 || chest_opened)
    return false;
  int8_t x = hero_x();
  int8_t y = hero_y();
  switch (hero_direction) {
  case UP: y--; break;
  case DOWN: y++; break;
  case LEFT: x--; break;
  case RIGHT: x++; break;
  }
  if (x != chest_x || y != chest_y)
    return false;

  chest_opened = true;
  set_tile_at(0, chest_x, chest_y, T_CHEST + 1);
  loot_open_chest(area_level, sign_text);
  map_textbox(sign_text);
  return true;
}

static void install_callbacks(void) {
  gen_floor.on_init = gen_on_init;
  gen_floor.on_special = NULL;
  gen_floor.on_move = gen_on_move;
  gen_floor.on_action = gen_on_action;
  gen_floor.on_load = NULL;
  gen_floor.on_draw = NULL;
}

//------------------------------------------------------------------------------
// Public API
//------------------------------------------------------------------------------

static void setup_banks(void) {
  world_fb.bank = WORLD_BANK;
  world_fb.floor = &gen_floor;
  tower_fb.bank = WORLD_BANK;
  tower_fb.floor = &gen_floor;
  raid_fb.bank = WORLD_BANK;
  raid_fb.floor = &gen_floor;
}

static uint8_t neighbor(uint8_t z, Direction d) {
  const uint8_t c = zone_col(z);
  const uint8_t r = zone_row(z);
  switch (d) {
  case RIGHT: return c + 1 < WORLD_ZONE_COLS ? z + 1 : z;
  case LEFT: return c > 0 ? z - 1 : z;
  case DOWN: return r + 1 < WORLD_ZONE_ROWS ? z + WORLD_ZONE_COLS : z;
  case UP: return r > 0 ? z - WORLD_ZONE_COLS : z;
  }
  return z;
}

void world_enter_zone(uint8_t zone_id) BANKED {
  setup_banks();
  if (zone_id >= WORLD_ZONE_COUNT)
    zone_id = WORLD_START_ZONE;
  current_zone = zone_id;
  tower_state.active = false;
  raid_state.active = false;
  direct_entry = true;
  set_active_floor(&world_fb);
}

FloorBank *world_prepare_floor(FloorBank *f) BANKED {
  setup_banks();

  if (f == &world_fb) {
    if (direct_entry) {
      direct_entry = false;
    } else if (raid_state.active && location == WORLD_LOC_RAID) {
      raid_state.active = false;
      current_zone = raid_state.entrance_zone_id;
    } else if (tower_state.active) {
      tower_state.active = false;
      current_zone = tower_state.entrance_zone_id;
    } else if (location == WORLD_LOC_ZONE) {
      if (active_exit.heading == HERE &&
        active_exit.col == WORLD_MAP_W / 2 + 2 &&
        active_exit.row == WORLD_MAP_H / 2 + 2) {
        const uint8_t trail_to = trail_destination(current_zone);
        if (trail_to != 0xFF)
          current_zone = trail_to;
      } else {
        current_zone = neighbor(current_zone, active_exit.heading);
      }
    }
    generate_zone(current_zone);
    install_callbacks();
    return f;
  }

  if (f == &tower_fb) {
    if (!tower_state.active) {
      tower_state.active = true;
      tower_state.entrance_zone_id = current_zone;
      tower_state.floor = 1;
    } else if (active_exit.heading == DOWN) {
      if (tower_state.floor < WORLD_TOWER_MAX_FLOOR)
        tower_state.floor++;
    } else if (tower_state.floor > 1) {
      tower_state.floor--;
    }
    generate_tower_floor(tower_state.floor);
    install_callbacks();
    return f;
  }

  if (f == &raid_fb) {
    if (!raid_state.active) {
      uint8_t id = raid_id_for_zone(current_zone);
      if (id == 0xFF) {
        generate_zone(current_zone);
        install_callbacks();
        return &world_fb;
      }
      raid_state.active = true;
      tower_state.active = false;
      raid_state.raid_id = id;
      raid_state.entrance_zone_id = current_zone;
      raid_state.floor = 1;
    } else if (active_exit.heading == DOWN) {
      if (raid_state.floor < WORLD_RAID_FLOORS)
        raid_state.floor++;
    } else if (raid_state.floor > 1) {
      raid_state.floor--;
    }
    generate_raid_floor(raid_state.floor);
    install_callbacks();
    return f;
  }

  // One of the original labyrinth floors.
  location = WORLD_LOC_DUNGEON;
  tower_state.active = false;
  raid_state.active = false;
  area_town = false;
  area_biome = WORLD_BIOME_UNDERGROUND;
  return f;
}

uint8_t world_current_zone(void) BANKED {
  return location == WORLD_LOC_ZONE ? current_zone : 0xFF;
}

WorldLocation world_location(void) BANKED {
  return location;
}

bool world_in_town(void) BANKED {
  return location == WORLD_LOC_ZONE && area_town;
}

uint16_t world_area_level(void) BANKED {
  if (location == WORLD_LOC_DUNGEON)
    return hero_level < 5 ? 5 : hero_level;
  return area_level;
}

uint8_t world_tier_for_level(uint16_t level) BANKED {
  if (level == 0)
    level = 1;
  if (level > MAX_LEVEL)
    level = MAX_LEVEL;
  return 1 + (level - 1) / WORLD_LEVELS_PER_TIER;
}

uint8_t world_current_tier(void) BANKED {
  if (location == WORLD_LOC_ZONE)
    return zone_biome(current_zone) + 1;
  if (location == WORLD_LOC_DUNGEON)
    return world_tier_for_level(hero_level);
  return world_tier_for_level(area_level);
}

uint8_t world_biome(void) BANKED {
  return area_biome;
}

void world_area_name(char *buf) BANKED {
  switch (location) {
  case WORLD_LOC_TOWER:
    sprintf(buf, "Tower F%u", tower_state.floor);
    break;
  case WORLD_LOC_RAID:
    sprintf(buf, "Raid %u F%u", raid_state.raid_id + 1, raid_state.floor);
    break;
  case WORLD_LOC_DUNGEON:
    sprintf(buf, "Labyrinth");
    break;
  default:
    sprintf(buf, "%s", zone_names[current_zone]);
  }
}

void world_respawn(void) BANKED {
  world_enter_zone(world_last_town);
}
