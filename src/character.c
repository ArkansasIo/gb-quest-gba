#pragma bank 20

#include <stdio.h>

#include "character.h"
#include "core.h"
#include "map.h"
#include "player.h"

/**
 * Draws a labeled stat value at the given window coordinates.
 */
static void draw_stat(const char *label, uint16_t value, uint8_t x, uint8_t y) {
  char buf[8];
  core.draw_text(VRAM_WINDOW_XY(x, y), label, 5);
  sprintf(buf, "%u", value);
  core.draw_text(VRAM_WINDOW_XY(x + 5, y), buf, 4);
}

/**
 * Draws the character screen.
 */
void draw_character_screen(void) {
  char buf[16];

  core.draw_text(VRAM_WINDOW_XY(2, 1), "CHARACTER", 9);

  sprintf(buf, "%s", player.name);
  core.draw_text(VRAM_WINDOW_XY(2, 2), buf, 8);

  sprintf(buf, "%s", player_class_names[player.player_class]);
  core.draw_text(VRAM_WINDOW_XY(12, 2), buf, 8);

  draw_stat("LVL", player.level, 2, 3);
  draw_stat("HP", player.hp, 2, 4);
  draw_stat("SP", player.sp, 2, 5);
  draw_stat("ATK", player.atk, 2, 6);
  draw_stat("DEF", player.def, 2, 7);
  draw_stat("MATK", player.matk, 2, 8);
  draw_stat("MDEF", player.mdef, 2, 9);
  draw_stat("AGL", player.agl, 2, 10);
}

/**
 * Initializes the character screen.
 */
void init_character_screen(void) {
  DISPLAY_OFF;
  draw_character_screen();
  DISPLAY_ON;
}

/**
 * Updates the character screen; returns to the world map on A/B/START.
 */
void update_character_screen(void) {
  if (was_pressed(J_A) || was_pressed(J_B) || was_pressed(J_START)) {
    map_state = MAP_STATE_WAITING;
    game_state = GAME_STATE_WORLD_MAP;
  }
}
