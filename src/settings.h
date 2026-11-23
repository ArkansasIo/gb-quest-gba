#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <stdbool.h>

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
void settings_open(void);

/**
 * Draws the settings screen.
 */
void settings_draw(void);

/**
 * Updates the settings screen.
 */
void settings_update(void);

#endif
