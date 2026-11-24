#pragma bank 1

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include <stdio.h>

#include "core.h"
#include "creation.h"
#include "gear.h"
#include "hero_select.h"
#include "player.h"
#include "map.h"
#include "loot.h"
#include "sound.h"
#include "world.h"

/**
 * Creation stages: race selection, then class selection.
 */
#define CREATION_STAGE_RACE 0
#define CREATION_STAGE_CLASS 1

#define CREATION_ROW_COUNT 16

static uint8_t creation_stage = CREATION_STAGE_RACE;
static uint8_t creation_race = RACE_HUMAN;
static uint8_t creation_class = 0;
static uint8_t creation_class_scroll = 0;

/**
 * Draws the header of the creation screen.
 */
static void draw_creation_header(void) {
  core.draw_text(VRAM_WINDOW_XY(2, 0), "CREATE CHARACTER", 16);
}

/**
 * Draws the race list.
 */
static void draw_race_list(void) {
  for (uint8_t k = 0; k < RACE_COUNT; k++) {
    char buf[16];
    get_race_name(k, buf, sizeof(buf));
    core.draw_text(VRAM_WINDOW_XY(2, 1 + k), buf, 12);
  }
}

/**
 * Draws the race cursor.
 */
static void draw_race_cursor(void) {
  move_sprite(16, 8, 24 + creation_race * 8);
  move_sprite(17, 0, 0);
}

/**
 * Palette for the creation screen text (glyphs use color 3 on color 1).
 */
static const palette_color_t creation_palette[4] = {
  RGB8(8, 8, 16), RGB8(24, 28, 56), RGB8(90, 100, 150), RGB8(235, 235, 220),
};

/**
 * Clears and shows the window layer used by the creation screen.
 */
static void setup_creation_window(void) {
  core.load_bg_palette(creation_palette, 7, 1);
  core.load_sprite_palette(creation_palette, 7, 1);
  core.fill(VRAM_WINDOW, 20, 18, FONT_SPACE, 0x0F);
  set_sprite_tile(16, '>' + FONT_OFFSET);
  set_sprite_prop(16, 0x0F);
  move_win(7, 0);
}

/**
 * Draws the class list (with scrolling).
 */
static void draw_class_list(void) {
  for (uint8_t r = 0; r < CREATION_ROW_COUNT; r++) {
    uint8_t idx = creation_class_scroll + r;
    char buf[16];
    if (idx >= CLASS_COUNT)
      break;
    get_class_name(idx, buf, sizeof(buf));
    core.draw_text(VRAM_WINDOW_XY(2, 1 + r), buf, 14);
  }
}

/**
 * Adjusts the class list scroll so the cursor is visible.
 */
static void creation_adjust_scroll(void) {
  if (creation_class < creation_class_scroll)
    creation_class_scroll = creation_class;
  else if (creation_class >= creation_class_scroll + CREATION_ROW_COUNT)
    creation_class_scroll = creation_class - CREATION_ROW_COUNT + 1;
}

/**
 * Draws the class cursor.
 */
static void draw_class_cursor(void) {
  creation_adjust_scroll();
  move_sprite(16, 8, 24 + (creation_class - creation_class_scroll) * 8);
  move_sprite(17, 0, 0);
}

/**
 * Initializes the character creation screen.
 */
void init_hero_select(void) NONBANKED {
  DISPLAY_OFF;

  core.load_font();
  core.load_all_heros();

  creation_stage = CREATION_STAGE_RACE;
  creation_race = RACE_HUMAN;
  creation_class = 0;
  creation_class_scroll = 0;

  setup_creation_window();
  draw_creation_header();
  draw_race_list();
  draw_race_cursor();

  DISPLAY_ON;
}

/**
 * Starts the game with the selected class and race.
 */
static void start_game(void) NONBANKED {
  DISPLAY_OFF;

  for (uint8_t k = 0; k < 16; k++)
    move_sprite(k, 0, 0);

  init_player(creation_class);
  player.race = creation_race;
  for (uint8_t k = 0; k < GEAR_SLOT_COUNT; k++)
    player.gear[k] = ITEM_INVALID;
  update_gear_stats();
  apply_race_bonuses();
  loot_new_game();
  loot_capture_base();
  full_heal_player();

  world_enter_zone(WORLD_START_ZONE);
  init_world_map();

  game_state = GAME_STATE_WORLD_MAP;
  DISPLAY_ON;
}

/**
 * Updates the character creation screen.
 */
bool hero_continue_requested;

void update_hero_select(void) NONBANKED {
  if (hero_continue_requested) {
    // Continue the saved game.
    hero_continue_requested = false;
    DISPLAY_OFF;
    if (load_game()) {
      world_enter_zone(world_last_town);
      init_world_map();
      game_state = GAME_STATE_WORLD_MAP;
      DISPLAY_ON;
    } else {
      init_hero_select();
    }
    return;
  }

  if (creation_stage == CREATION_STAGE_RACE) {
    if (was_pressed(J_UP)) {
      creation_race = (creation_race + RACE_COUNT - 1) % RACE_COUNT;
      play_sound(sfx_menu_move);
      draw_race_list();
      draw_race_cursor();
    } else if (was_pressed(J_DOWN)) {
      creation_race = (creation_race + 1) % RACE_COUNT;
      play_sound(sfx_menu_move);
      draw_race_list();
      draw_race_cursor();
    } else if (was_pressed(J_A) || was_pressed(J_B) || was_pressed(J_START)) {
      play_sound(sfx_hero_selected);
      creation_stage = CREATION_STAGE_CLASS;
      creation_class = 0;
      creation_class_scroll = 0;
      draw_class_list();
      draw_class_cursor();
    }
  } else {
    if (was_pressed(J_UP)) {
      creation_class = (creation_class + CLASS_COUNT - 1) % CLASS_COUNT;
      play_sound(sfx_menu_move);
      draw_class_list();
      draw_class_cursor();
    } else if (was_pressed(J_DOWN)) {
      creation_class = (creation_class + 1) % CLASS_COUNT;
      play_sound(sfx_menu_move);
      draw_class_list();
      draw_class_cursor();
    } else if (was_pressed(J_A) || was_pressed(J_B) || was_pressed(J_START)) {
      play_sound(sfx_hero_selected);
      start_game();
      return;
    }
  }
}
