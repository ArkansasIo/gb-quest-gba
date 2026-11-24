#ifndef _CHARACTER_H
#define _CHARACTER_H

#include <gb/gb.h>

/**
 * Initializes the character screen.
 */
void init_character_screen(void) BANKED;

/**
 * Updates the character screen.
 */
void update_character_screen(void) BANKED;

/**
 * Draws the character screen.
 */
void draw_character_screen(void) BANKED;

#endif
