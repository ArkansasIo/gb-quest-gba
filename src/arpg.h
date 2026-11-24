#ifndef _ARPG_H
#define _ARPG_H

/**
 * Real-time action combat on the world map. Encounters that used to open the
 * turn-based battle screen now spawn their monsters directly on the map, where
 * they chase and attack the hero in real time.
 *
 * Controls (on the map):
 *   B             Basic attack (melee swing, or magic bolt for casters)
 *   A             Interact; with nothing to interact with, use the selected
 *                 hotbar skill / potion
 *   SELECT        Dodge roll (two quick steps with invulnerability)
 *   B + SELECT    Cycle the hotbar selection
 *   START         Menu (not available while enemies are near)
 *
 * Code lives in bank 24 (arpg.c).
 */

#include <gb/gb.h>
#include <stdint.h>
#include <stdbool.h>

#include "core.h"

#define ARPG_BANK 24

/**
 * Number of class skills.
 */
#define SKILL_COUNT 6

/**
 * `true` while monsters are active on the map.
 */
extern bool arpg_in_combat;

/**
 * Set by the ARPG system to block map input (stunned, defeated).
 */
extern bool arpg_block_input;

/**
 * Requests that the map system respawn the player (after defeat).
 */
extern bool arpg_respawn_request;

/**
 * Requests that the map system reload NPC/flame sprites (after combat).
 */
extern bool arpg_restore_npcs;

/**
 * Requests the end credits (final boss defeated).
 */
extern bool arpg_credits_pending;

/**
 * Initializes ARPG sprites and the HUD after the world map has been
 * (re)initialized. Must be called after the textbox is initialized.
 */
void arpg_world_init(void) BANKED;

/**
 * Restores ARPG sprites after the map menu closes.
 */
void arpg_menu_closed(void) BANKED;

/**
 * Per-frame update (call at the end of the map update).
 */
void arpg_update(void) BANKED;

/**
 * Spawns the monsters in the current `encounter` onto the map.
 */
void arpg_begin_encounter(void) BANKED;

/**
 * @return `true` if a monster occupies the given map tile.
 */
bool arpg_tile_blocked(int8_t x, int8_t y) BANKED;

/**
 * Handles the A button when there is nothing to interact with.
 * @return `true` if an action was taken.
 */
bool arpg_on_action(void) BANKED;

/**
 * @return A direction the hero is being forced to move (dodge), or `HERE`.
 */
Direction arpg_forced_move(void) BANKED;

/**
 * @return `true` if the hero should move at double speed (dodge).
 */
bool arpg_fast_move(void) BANKED;

/**
 * Called whenever a new floor is loaded.
 */
void arpg_floor_changed(void) BANKED;

/**
 * Hides the HUD (before the textbox or menu uses the window).
 */
void arpg_hide_hud(void) BANKED;

/**
 * Writes the display name of a hotbar entry (skill or consumable) into buf.
 */
void arpg_hotbar_name(uint8_t entry, char *buf) BANKED;

/**
 * Writes the full name of class skill `k` into buf (max 10 chars).
 */
void arpg_skill_name(uint8_t k, char *buf) BANKED;

/**
 * Writes a short description of class skill `k` into buf (max 18 chars).
 */
void arpg_skill_info(uint8_t k, char *buf) BANKED;

/**
 * @return The level required to learn class skill `k`.
 */
uint8_t arpg_skill_level(uint8_t k) BANKED;

#endif
