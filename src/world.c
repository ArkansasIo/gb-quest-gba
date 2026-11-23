#pragma bank 18

#include "world.h"
#include "map.h"
#include "core.h"

/**
 * Id of the zone the player is currently in (0xFF while in the tower).
 */
static uint8_t current_zone_id = 0;

TowerState tower_state;

WorldBiome world_zone_biome(uint8_t zone_id) {
  return (WorldBiome)world_zones[zone_id].biome;
}

void world_enter_zone(uint8_t zone_id) {
  current_zone_id = zone_id;
  set_active_floor(&world_zone_banks[zone_id]);
}

uint8_t world_current_zone(void) {
  if (tower_state.active)
    return 0xFF;
  return current_zone_id;
}

void world_on_load_floor(FloorBank *f) {
  for (uint8_t i = 0; i < WORLD_ZONE_COUNT; i++) {
    if (f == &world_zone_banks[i]) {
      current_zone_id = i;
      tower_state.active = false;
      return;
    }
  }
  for (uint8_t n = 1; n <= WORLD_TOWER_MAX_FLOOR; n++) {
    if (f == &tower_floor_banks[n]) {
      tower_state.floor = n;
      return;
    }
  }
}

void tower_enter(uint8_t zone_id) {
  tower_state.active = true;
  tower_state.entrance_zone_id = zone_id;
  tower_state.floor = 1;
  set_active_floor(&tower_floor_banks[1]);
}

void tower_exit(void) {
  tower_state.active = false;
  set_active_floor(&world_zone_banks[tower_state.entrance_zone_id]);
}

void tower_go_down(void) {
  if (tower_state.floor >= WORLD_TOWER_MAX_FLOOR)
    return;
  tower_state.floor++;
  set_active_floor(&tower_floor_banks[tower_state.floor]);
}

void tower_go_up(void) {
  if (tower_state.floor > 1) {
    tower_state.floor--;
    set_active_floor(&tower_floor_banks[tower_state.floor]);
  } else {
    tower_exit();
  }
}
