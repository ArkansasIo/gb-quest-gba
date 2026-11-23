#pragma bank 22

#include <stdio.h>

#include "settings.h"
#include "core.h"
#include "map.h"
#include "player.h"
#include "sound.h"

bool settings_sfx_enabled = true;
bool settings_music_enabled = true;

#define SETTINGS_OPTIONS_COUNT 2

static uint8_t settings_cursor = 0;

/**
 * @return The name of the option at the given index.
 */
static const char *settings_option_name(uint8_t idx) {
  switch (idx) {
  case 0: return "SFX";
  default: return "MUSIC";
  }
}

/**
 * Draws the option line for the given index.
 */
static void settings_draw_option(uint8_t idx) {
  char buf[24];
  bool value = idx == 0 ? settings_sfx_enabled : settings_music_enabled;
  sprintf(buf, "%s: %s", settings_option_name(idx), value ? "ON" : "OFF");
  core.draw_text(VRAM_WINDOW_XY(2, idx + 1), buf, 20);
}

/**
 * Draws the settings screen cursor.
 */
static void settings_draw_cursor(void) {
  move_sprite(16, 8, 8 + settings_cursor * 8);
  move_sprite(17, 16, 8 + settings_cursor * 8);
}

/**
 * Draws the settings screen.
 */
void settings_draw(void) {
  core.draw_text(VRAM_WINDOW_XY(2, 0), "SETTINGS", 8);
  for (uint8_t k = 0; k < SETTINGS_OPTIONS_COUNT; k++)
    settings_draw_option(k);
  settings_draw_cursor();
}

/**
 * Opens the settings screen.
 */
void settings_open(void) {
  game_state = GAME_STATE_SETTINGS;
  settings_draw();
}

/**
 * Updates the settings screen; toggles the selected option or returns.
 */
void settings_update(void) {
  if (was_pressed(J_UP)) {
    settings_cursor = (settings_cursor + SETTINGS_OPTIONS_COUNT - 1) % SETTINGS_OPTIONS_COUNT;
    play_sound(sfx_menu_move);
    settings_draw_cursor();
  } else if (was_pressed(J_DOWN)) {
    settings_cursor = (settings_cursor + 1) % SETTINGS_OPTIONS_COUNT;
    play_sound(sfx_menu_move);
    settings_draw_cursor();
  } else if (was_pressed(J_A)) {
    play_sound(sfx_menu_move);
    if (settings_cursor == 0)
      settings_sfx_enabled = !settings_sfx_enabled;
    else
      settings_music_enabled = !settings_music_enabled;
    settings_draw_option(settings_cursor);
  } else if (was_pressed(J_START) || was_pressed(J_B)) {
    map_state = MAP_STATE_WAITING;
    game_state = GAME_STATE_WORLD_MAP;
  }
}
