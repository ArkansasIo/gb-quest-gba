#pragma bank 1

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "core.h"
#include "hero_select.h"
#include "loot.h"
#include "sound.h"

#define MAX_ANIMATION_SPRITES 20

void init_dragon_eyes(void);
void update_dragon_eyes(void);

void init_main_title(void);
void update_main_title(void);

void init_fire_animation(void);
void update_fire_animation(void);

void init_smoke_animation(void);
void stop_smoke_animation(void);
void update_smoke_animation(void);


/**
 * Clears all sprites used by a title screen animation.
 */
static void clear_sprites(void) {
  for (uint8_t k = 0; k < MAX_ANIMATION_SPRITES; k++) {
    set_sprite_tile(k, 0);
    set_sprite_prop(k, 0);
    move_sprite(k, 0, 0);
  }
}

//------------------------------------------------------------------------------
// Title Screen Main State Controller
//------------------------------------------------------------------------------

typedef enum TitleState {
  TITLE_DRAGON_EYES,
  TITLE_MAIN,
} TitleState;


const palette_color_t default_palette[] = {
  RGB_BLACK, RGB_BLACK, RGB_BLACK, RGB_BLACK,
  RGB_BLACK, RGB_BLACK, RGB_BLACK, RGB_BLACK,
  RGB_BLACK, RGB_BLACK, RGB_BLACK, RGB_BLACK,
  RGB_BLACK, RGB_BLACK, RGB_BLACK, RGB_BLACK,
  RGB_BLACK, RGB_BLACK, RGB_BLACK, RGB_BLACK,
  RGB_BLACK, RGB_BLACK, RGB_BLACK, RGB_BLACK,
  RGB_BLACK, RGB_BLACK, RGB_BLACK, RGB_BLACK,
  RGB_BLACK, RGB_BLACK, RGB_BLACK, RGB_BLACK,
};

const palette_color_t test_palette[] = {
  RGB_BLACK, RGB8(50, 50, 50), RGB8(160, 160, 160), RGB_WHITE,
  RGB_BLACK, RGB8(50, 50, 50), RGB8(160, 160, 160), RGB_WHITE,
  RGB_BLACK, RGB8(50, 50, 50), RGB8(160, 160, 160), RGB_WHITE,
  RGB_BLACK, RGB8(50, 50, 50), RGB8(160, 160, 160), RGB_WHITE,
  RGB_BLACK, RGB8(50, 50, 50), RGB8(160, 160, 160), RGB_WHITE,
  RGB_BLACK, RGB8(50, 50, 50), RGB8(160, 160, 160), RGB_WHITE,
  RGB_BLACK, RGB8(50, 50, 50), RGB8(160, 160, 160), RGB_WHITE,
  RGB_BLACK, RGB8(50, 50, 50), RGB8(160, 160, 160), RGB_WHITE,
};


TitleState title_state;
Tilemap title_screen_tilemap = { 20, 18, 1, tilemap_title_screen };

void init_title_screen(void) BANKED {
  DISPLAY_OFF;

  scroll_bkg(0, 0);

  // Clear out palettes and tile memory
  core.load_bg_palette(default_palette, 0, 8);
  core.load_sprite_palette(default_palette, 0, 8);
  core.fill_bg(0xFF, 0b00001010);
  core.fill(VRAM_WINDOW, 32, 32, 0xFF, 0b00001010);

  // Load the title screen tile into VRAM
  core.load_title_tiles();

  // Draw the main splash screen tiles
  core.draw_tilemap(title_screen_tilemap, VRAM_BACKGROUND);

  init_dragon_eyes();
  title_state = TITLE_DRAGON_EYES;

  DISPLAY_ON;
}

void update_title_screen(void) BANKED {
  switch (title_state) {
  case TITLE_DRAGON_EYES:
    update_dragon_eyes();
    break;
  case TITLE_MAIN:
    update_main_title();
    break;
  }
}

//------------------------------------------------------------------------------
// Dragon Eyes
//------------------------------------------------------------------------------

typedef enum DragonEyesState {
  DE_PRE_DELAY,
  DE_FADE_IN,
  DE_SMILE_DELAY,
  DE_FADE_OUT_DELAY,
  DE_FADE_OUT,
  DE_POST_DELAY,
  DE_DONE,
} DragonEyesState;

const uint8_t eyes_x[] = {
  64,         88,
  64, 72, 80, 88,
      72, 80,
};

const uint8_t eyes_y[] = {
   88,            88,
  104, 104, 104, 104,
       112, 112,
};

const palette_color_t eyes_palette[] = {
  RGB_BLACK, RGB_BLACK, RGB_BLACK, RGB8(0, 0, 0), // Frame 1
  RGB_BLACK, RGB_BLACK, RGB_BLACK, RGB8(50, 0, 0), // Frame 2
  RGB_BLACK, RGB_BLACK, RGB_BLACK, RGB8(100, 36, 0), // Frame 3
  RGB_BLACK, RGB_BLACK, RGB_BLACK, RGB8(160, 110, 0), // Frame 4
  RGB_BLACK, RGB_BLACK, RGB_BLACK, RGB8(252, 216, 0), // Frame 5
};

DragonEyesState de_state = DE_DONE;
uint8_t de_palette_idx = 0;
Timer de_timer;

void init_dragon_eyes(void) {
  core.load_sprite_palette(eyes_palette, 0, 1);

  for (uint8_t k = 0; k < 8; k++) {
    set_sprite_tile(k, 0xE0 + k);
    set_sprite_prop(k, 0x07);
    move_sprite(k, eyes_x[k] + 8, eyes_y[k] + 16);
  }

  de_state = DE_PRE_DELAY;
  de_palette_idx = 0;
  init_timer(de_timer, 5);
}

void update_dragon_eyes(void) {
  switch (de_state) {
  case DE_PRE_DELAY:
    if (!update_timer(de_timer))
      return;
    init_timer(de_timer, 25);
    de_state = DE_FADE_IN;
    break;
  case DE_FADE_IN:
    if (!update_timer(de_timer))
      return;

    de_palette_idx++;
    if (de_palette_idx == 5) {
      init_timer(de_timer, 45);
      de_state = DE_SMILE_DELAY;
      return;
    }

    reset_timer(de_timer);
    core.load_sprite_palette(eyes_palette + de_palette_idx * 4, 7, 1);

    break;
  case DE_SMILE_DELAY:
    if (!update_timer(de_timer))
      return;

    for (uint8_t k = 0; k < 8; k++)
      set_sprite_tile(k, 0xE8 + k);

    de_state = DE_FADE_OUT_DELAY;
    init_timer(de_timer, 15);

    break;
  case DE_FADE_OUT_DELAY:
    if (!update_timer(de_timer))
      return;
    de_palette_idx = 4;
    init_timer(de_timer, 6);
    de_state = DE_FADE_OUT;
    break;
  case DE_FADE_OUT:
    if (!update_timer(de_timer))
      return;

    de_palette_idx--;
    reset_timer(de_timer);
    core.load_sprite_palette(eyes_palette + de_palette_idx * 4, 7, 1);

    if (de_palette_idx == 0) {
      de_state = DE_POST_DELAY;
      init_timer(de_timer, 16);

      for (uint8_t j = 0; j < 8; j++) {
        move_sprite(j, 0, 0);
      }

      return;
    }

    break;
  case DE_POST_DELAY:
    if (!update_timer(de_timer))
      return;

    title_state = TITLE_MAIN;
    init_main_title();

    break;
  case DE_DONE:
    break;
  }
}

//------------------------------------------------------------------------------
// Main Title (Dragon Flame, Smoke, Press Start Animations w/ Joypad Input)
//------------------------------------------------------------------------------

typedef enum MainTitleState {
  MAIN_FIRE,
  MAIN_WAIT_FOR_INPUT,
} MainTitleState;


static const palette_color_t main_fg_palettes[] = {
  // 0 - Fire
  RGB_BLACK, RGB8(252, 216, 0), RGB8(252, 108, 0), RGB_WHITE,
  // 1 - Smoke
  RGB_BLACK, RGB8(91, 91, 91), RGB8(156, 156, 156), RGB_WHITE,
};

static const palette_color_t main_bg_palettes[] = {
   // Palette 1 - Title
  RGB8(251, 242, 54), RGB8(48, 96, 130), RGB8(172, 50, 50), RGB_BLACK,
  // Palette 2 - Face
  RGB8(252, 216, 0), RGB8(154, 32, 24), RGB8(54, 11, 13), RGB8(25, 8, 10),
  // Palette 3 - Wings
  RGB8(154, 32, 24), RGB8(54, 11, 13), RGB8(25, 8, 10), RGB_BLACK,
  // Palette 4 - Outer Body
  RGB8(54, 11, 13), RGB8(25, 8, 10), RGB8(9, 4, 4), RGB_BLACK,
  // Palette 5 - Head
  RGB8(154, 32, 24), RGB8(54, 11, 13), RGB8(25, 8, 10), RGB_BLACK,
  // Palette 6 - PRESS START
  RGB8(251, 242, 54), RGB_BLACK, RGB_BLACK, RGB_BLACK,
};

static const palette_color_t dragon_palette_frames[] = {
  // FRAME 1 -------------------------------------------------------------------
  // Palette 2 - Face
  RGB8(252, 216, 0), RGB8(154, 32, 24), RGB8(54, 11, 13), RGB8(25, 8, 10),
  // Palette 3 - Wings
  RGB8(154, 32, 24), RGB8(54, 11, 13), RGB8(25, 8, 10), RGB_BLACK,
  // Palette 4 - Outer Body
  RGB8(54, 11, 13), RGB8(25, 8, 10), RGB8(9, 4, 4), RGB_BLACK,
  // Palette 5 - Head
  RGB8(154, 32, 24), RGB8(54, 11, 13), RGB8(25, 8, 10), RGB_BLACK,

  // FRAME 2 -------------------------------------------------------------------
  // Palette 2 - Face
  RGB8(255, 230, 0), RGB8(174, 52, 54), RGB8(74, 31, 43), RGB8(45, 28, 30),
  // Palette 3 - Wings
  RGB8(174, 52, 54), RGB8(74, 31, 43), RGB8(45, 28, 30), RGB_BLACK,
  // Palette 4 - Outer Body
  RGB8(74, 31, 43), RGB8(45, 28, 30), RGB8(39, 24, 24), RGB_BLACK,
  // Palette 5 - Head
  RGB8(174, 52, 54), RGB8(74, 31, 43), RGB8(45, 28, 30), RGB_BLACK,
};

MainTitleState main_title_state;

uint8_t flame_palette_idx;
Timer flame_palette_timer;

void init_main_title(void) {
  move_win(0, 144);
  core.load_bg_palette(main_bg_palettes, 0, 6);
  core.load_sprite_palette(main_fg_palettes, 0, 2);

  play_sound(sfx_title_fire);
  main_title_state = MAIN_FIRE;
  init_fire_animation();

  init_timer(flame_palette_timer, 3);
}

void update_main_title(void) NONBANKED {
  switch (main_title_state) {
  case MAIN_FIRE:
    update_fire_animation();
    if (!update_timer(flame_palette_timer))
      return;
    reset_timer(flame_palette_timer);

    flame_palette_idx++;
    flame_palette_idx &= 0x01;
    core.load_bg_palette(dragon_palette_frames + 16*flame_palette_idx, 1, 4);

    break;
  case MAIN_WAIT_FOR_INPUT:
    core.load_bg_palette(dragon_palette_frames, 1, 4);
    update_smoke_animation();

    if (was_pressed(J_START)) {
      DISPLAY_OFF;
      clear_sprites();
      game_state = GAME_STATE_HERO_SELECT;
      // START continues a saved game; SELECT+START starts a new one.
      if (!is_down(J_SELECT) && save_exists()) {
        hero_continue_requested = true;
        return;
      }
      init_hero_select();
      return;
    }

    break;
  }
}

//------------------------------------------------------------------------------
// Fire Animation
//------------------------------------------------------------------------------

#define FIRE_FRAME_SPRITE_0 0
#define FIRE_FRAME_SPRITES 20

Timer fire_timer;
uint8_t fire_frame_idx = 0;

const uint8_t fire_animation_tiles[] = {
  // Frame 1
                    0x00, 0x01,
                    0x02, 0x03,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  // Frame 2
                    0x08, 0x09,
                    0x0A, 0x0B,
  0x07, 0x07, 0x07, 0x0C, 0x0D, 0x07, 0x07, 0x07,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  // Frame 3
                    0x10, 0x11,
                    0x12, 0x13,
  0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
  0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23,
  // Frame 4
                    0x24, 0x25,
                    0x26, 0x27,
  0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  // Frame 5
                    0x38, 0x39,
                    0x3A, 0x3B,
  0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43,
  0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B,
};

const uint8_t fire_sprite_x[FIRE_FRAME_SPRITES] = {
  72, 80,
  72, 80,
  48, 56, 64, 72, 80, 88, 96, 104,
  48, 56, 64, 72, 80, 88, 96, 104,
};

const uint8_t fire_sprite_y[FIRE_FRAME_SPRITES] = {
  112, 112,
  120, 120,
  128, 128, 128, 128, 128, 128, 128, 128,
  136, 136, 136, 136, 136, 136, 136, 136,
};

const uint8_t fire_frames[] = {
  0, 1,
  2, 3, 4, 2, 3, 4, 2, 3, 4, 2, 3, 4, 3, 2, 1, 0,
  // 2, 3, 4, 2, 3, 4, 2, 3, 4,
  // 2, 3, 4, 2, 3, 4, 2, 3, 4,
  END
};

void init_fire_animation(void) {
  init_timer(fire_timer, 6);
  fire_frame_idx = 0;

  for (uint8_t k = 0; k < FIRE_FRAME_SPRITES; k++) {
    const uint8_t id = FIRE_FRAME_SPRITE_0 + k;
    set_sprite_tile(id, fire_animation_tiles[k]);
    set_sprite_prop(id, 0x08);
    move_sprite(id, fire_sprite_x[k] + 8, fire_sprite_y[k] + 16);
  }
}

void update_fire_animation(void) {
  if (fire_frame_idx == END)
    return;

  if (!update_timer(fire_timer))
    return;
  reset_timer(fire_timer);
  fire_frame_idx++;

  uint8_t fire_frame_offset = fire_frames[fire_frame_idx];
  if (fire_frame_offset == END) {
    fire_frame_idx = END;
    clear_sprites();
    init_smoke_animation();
    main_title_state = MAIN_WAIT_FOR_INPUT;
    return;
  }

  for (uint8_t k = 0; k < FIRE_FRAME_SPRITES; k++) {
    const uint8_t tile_offset = fire_frame_offset * FIRE_FRAME_SPRITES;
    set_sprite_tile(FIRE_FRAME_SPRITE_0 + k, fire_animation_tiles[tile_offset + k]);
  }
}

//------------------------------------------------------------------------------
// Smoke Animation
//------------------------------------------------------------------------------

#define SMOKE_FRAMES 6
#define SMOKE_SPRITE_COUNT 18
#define SMOKE_TILE_OFFSET 0x50

const uint8_t smoke_tiles[] = {
  // Frame 1
  0x29, 0x29, 0x00, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29,
  0x00, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29,

  // Frame 2
  0x29, 0x01, 0x02, 0x29, 0x03, 0x04, 0x29, 0x29, 0x29,
  0x02, 0x01, 0x29, 0x04, 0x03, 0x29, 0x29, 0x29, 0x29,

  // Frame 3
  0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
  0x07, 0x06, 0x05, 0x0A, 0x09, 0x08, 0x0D, 0x0C, 0x0B,

  // Frame 4
  0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
  0x10, 0x0F, 0x0E, 0x13, 0x12, 0x11, 0x16, 0x15, 0x14,

  // Frame 5
  0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
  0x19, 0x18, 0x17, 0x1C, 0x1B, 0x1A, 0x1F, 0x1E, 0x1D,

  // Frame 6
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
  0x21, 0x21, 0x20, 0x25, 0x24, 0x23, 0x28, 0x27, 0x26
};

typedef enum SmokeAnimationState {
  SMOKE_ANIMATION_SMOKE,
  SMOKE_ANIMATION_PAUSE,
} SmokeAnimationState;

Timer smoke_timer;
uint8_t smoke_timer_idx = 0;
SmokeAnimationState smoke_animation_state;

const uint8_t smoke_sprite_x[] = {
  0, 8, 16, 0, 8, 16, 0, 8, 16,
  24, 32, 40, 24, 32, 40, 24, 32, 40,
};
const uint8_t smoke_sprite_y[] = {
  0, 0, 0, 8, 8, 8, 16, 16, 16,
  0, 0, 0, 8, 8, 8, 16, 16, 16,
};

#define SMOKE_OFFSET_X 7 * 8 - 6
#define SMOKE_OFFSET_Y 12 * 8 + 6


void stop_smoke_animation(void) {
  smoke_timer_idx = END;
}

void init_smoke_animation(void) {
  init_timer(smoke_timer, 6);
  smoke_animation_state = SMOKE_ANIMATION_SMOKE;
  smoke_timer_idx = 0;

  for (uint8_t k = 0; k < SMOKE_SPRITE_COUNT; k++) {
    set_sprite_tile(k, smoke_tiles[k] + 0x50);
    if (k >= 9)
      set_sprite_prop(k, 0x29);
    else
      set_sprite_prop(k, 0x09);

    uint8_t x_offset = k >= 9 ? 11 : 0;
    uint8_t y_offset = k >= 9 ? 1 : 0;

    move_sprite(k,
      smoke_sprite_x[k] + 8 + SMOKE_OFFSET_X + x_offset,
      smoke_sprite_y[k] + 16 + SMOKE_OFFSET_Y + y_offset);
  }
}

void update_smoke_animation(void) {
  if (smoke_timer_idx == END)
    return;

  switch(smoke_animation_state) {
  case SMOKE_ANIMATION_SMOKE:
    if (!update_timer(smoke_timer))
      return;
    reset_timer(smoke_timer);

    smoke_timer_idx++;

    if (smoke_timer_idx >= 6) {
      init_timer(smoke_timer, 100);
      smoke_animation_state = SMOKE_ANIMATION_PAUSE;
      for (uint8_t k = 0; k < SMOKE_SPRITE_COUNT; k++) {
        set_sprite_tile(k, 0x29 + 0x50);
      }
      return;
    }

    uint8_t frame_offset = smoke_timer_idx * 18;
    for (uint8_t k = 0; k < SMOKE_SPRITE_COUNT; k++) {
      set_sprite_tile(k, smoke_tiles[k + frame_offset] + 0x50);
    }

    break;
  case SMOKE_ANIMATION_PAUSE:
    if (!update_timer(smoke_timer))
      return;
      init_smoke_animation();
    break;
  }
}
