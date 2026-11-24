#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <stdbool.h>
#include <gb/gb.h>

/**
 * Whether sound effects are enabled.
 */
extern bool settings_sfx_enabled;

/**
 * Whether the music is enabled.
 */
extern bool settings_music_enabled;

/**
 * Opens the settings screen.
 */
void settings_open(void) BANKED;

/**
 * Draws the settings screen.
 */
void settings_draw(void) BANKED;

/**
 * Updates the settings screen.
 */
void settings_update(void) BANKED;

#endif
