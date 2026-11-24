#ifndef _ARPG_MENU_H
#define _ARPG_MENU_H

/**
 * Full in-game pause menu hub and feature screens. Code lives in bank 19.
 *
 * Screens draw into the dungeon background map (0x9800, hidden while the pause
 * menu is open) and display it through the window layer. When the pause menu
 * closes the map system redraws the dungeon (see `arpg_menu_used`).
 */

#include <gb/gb.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Six commands are shown on each of the two pause-hub pages.
 */
typedef enum ArpgMenuEntry {
  ARPG_MENU_SKILLS,
  ARPG_MENU_ITEMS,
  ARPG_MENU_QUESTS,
  ARPG_MENU_TOWN,
  ARPG_MENU_STATS,
  ARPG_MENU_MORE,
  ARPG_MENU_WORLD,
  ARPG_MENU_JOURNAL,
  ARPG_MENU_OPTIONS,
  ARPG_MENU_CONTROLS,
  ARPG_MENU_SAVE,
  ARPG_MENU_PROFESSIONS,
  ARPG_MENU_COUNT,
} ArpgMenuEntry;

#define ARPG_MENU_PAGE_SIZE 6
#define ARPG_MENU_PAGE_COUNT 2

/**
 * Set when a screen drew over the dungeon map (it must be redrawn).
 */
extern bool arpg_menu_used;

/**
 * Set when a screen closes and the pause menu should refresh.
 */
extern bool arpg_menu_returned;

/**
 * Draws the pause menu entries into the menu box.
 */
void arpg_menu_draw_entries(uint8_t page) BANKED;

/**
 * Positions the pause menu cursor for the given entry.
 */
void arpg_menu_draw_cursor(uint8_t entry) BANKED;

/**
 * Opens the screen for the given pause menu entry.
 * @return `false` if the entry cannot be opened right now.
 */
bool arpg_menu_open(uint8_t entry) BANKED;

/**
 * Per-frame update while a screen is open (GAME_STATE_ARPG_MENU).
 */
void arpg_menu_update(void) BANKED;

#endif
