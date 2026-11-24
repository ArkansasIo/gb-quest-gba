#pragma bank 24

/**
 * Real-time action combat. See arpg.h for an overview and controls.
 *
 * Sprite allocation while on the map:
 *   0-3    hero
 *   4-15   up to three monsters (16x16 each)
 *   16-19  effects (slash / nova / heal / buff / player bolt on 19)
 *   20-22  damage number popup
 *   23     enemy projectile
 *   24-31  torch gauge + keys HUD (map.c)
 *   32-39  sconce flames (map.c)
 */

#include <gb/gb.h>
#include <gb/cgb.h>
#include <rand.h>
#include <stdio.h>
#include <string.h>

#include "arpg.h"
#include "core.h"
#include "encounter.h"
#include "item.h"
#include "loot.h"
#include "map.h"
#include "monster.h"
#include "player.h"
#include "sound.h"
#include "stats.h"
#include "textbox.h"
#include "world.h"

extern Direction move_direction;
extern uint8_t move_step;

bool arpg_in_combat;
bool arpg_block_input;
bool arpg_respawn_request;
bool arpg_restore_npcs;
bool arpg_credits_pending;

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

#define MAX_MOBS 3
#define SPR_MOB0 4
#define SPR_FX 16
#define SPR_PBOLT 19
#define SPR_POP 20
#define SPR_EBOLT 23

#define PROP_BANK1 0x08
#define PROP_FLIPX 0x20
#define PROP_FLIPY 0x40

#define T_SLASH_R 0x40
#define T_SLASH_D 0x42
#define T_PBOLT 0x44
#define T_EBOLT 0x45
#define T_SPARK 0x46
#define T_HEAL 0x47
#define T_RING 0x48
#define T_ALERT 0x49
#define T_SHIELD 0x4A
#define T_POISON 0x4B
#define T_UP 0x4C
#define T_DIGIT0 0x30

#define PAL_FX 1
#define PAL_GREEN 2
#define PAL_POP 5

#define WINDUP_FRAMES 22
#define HERO_IFRAMES 45

#define HUD_Y 136

//------------------------------------------------------------------------------
// Skills
//------------------------------------------------------------------------------

typedef enum SkillKind {
  SK_STRIKE,
  SK_CLEAVE,
  SK_BOLT,
  SK_NOVA,
  SK_HEAL,
  SK_RAGE,
  SK_GUARD,
  SK_HASTE,
  SK_STUN,
  SK_DRAIN,
  SK_POISON,
  SK_DASH,
  SK_QUAKE,
  SK_KIND_COUNT,
} SkillKind;

typedef struct SkillDef {
  const char *name;
  uint8_t kind;
  uint16_t power;
} SkillDef;

static const uint8_t kind_cost[SK_KIND_COUNT] = {
  4, 6, 5, 8, 8, 10, 10, 10, 5, 7, 6, 4, 16
};

static const uint8_t kind_cooldown[SK_KIND_COUNT] = {
  40, 60, 30, 90, 240, 250, 250, 250, 80, 60, 60, 50, 250
};

static const char *const kind_info[SK_KIND_COUNT] = {
  "Strike ahead",
  "Hit all around",
  "Ranged bolt",
  "Blast nearby foes",
  "Restore HP",
  "Raise attack",
  "Raise defense",
  "Act faster",
  "Strike and stun",
  "Bolt, steal HP",
  "Bolt, poisons",
  "Dash, dodge",
  "Hit every foe",
};

static const uint8_t skill_levels[SKILL_COUNT] = { 1, 5, 8, 12, 16, 20 };

static const SkillDef class_skills[CLASS_COUNT][SKILL_COUNT] = {
  // Druid
  { { "Lightning", SK_BOLT, 115 }, { "Cure", SK_HEAL, 30 },
    { "Barkskin", SK_GUARD, 0 }, { "Swarm", SK_NOVA, 95 },
    { "Regrowth", SK_HEAL, 60 }, { "Storm", SK_QUAKE, 150 } },
  // Fighter
  { { "Power Hit", SK_STRIKE, 160 }, { "2nd Wind", SK_HEAL, 25 },
    { "Cleave", SK_CLEAVE, 115 }, { "Trip", SK_STUN, 100 },
    { "Surge", SK_HASTE, 0 }, { "Whirlwind", SK_NOVA, 140 } },
  // Monk
  { { "Palm", SK_STUN, 120 }, { "Flurry", SK_STRIKE, 200 },
    { "Evasion", SK_DASH, 0 }, { "Still Mind", SK_HEAL, 35 },
    { "Diamond", SK_GUARD, 0 }, { "Quivering", SK_STRIKE, 320 } },
  // Sorcerer
  { { "Fireball", SK_BOLT, 140 }, { "Haste", SK_HASTE, 0 },
    { "Sleet", SK_NOVA, 100 }, { "Darkness", SK_STUN, 90 },
    { "Disintegr", SK_BOLT, 260 }, { "Wild Magic", SK_QUAKE, 180 } },
  // Rogue
  { { "Backstab", SK_STRIKE, 180 }, { "Poison", SK_POISON, 80 },
    { "Tumble", SK_DASH, 0 }, { "Fan Knives", SK_NOVA, 90 },
    { "Shadow", SK_HASTE, 0 }, { "Assassin", SK_STRIKE, 300 } },
  // Ranger
  { { "Aimed Shot", SK_BOLT, 150 }, { "Volley", SK_NOVA, 100 },
    { "Snare", SK_STUN, 80 }, { "Mend", SK_HEAL, 30 },
    { "Hunter", SK_RAGE, 0 }, { "Arrow Rain", SK_QUAKE, 150 } },
  // Knight
  { { "Smite", SK_STRIKE, 150 }, { "Shield Up", SK_GUARD, 0 },
    { "Bash", SK_STUN, 110 }, { "Rally", SK_HEAL, 30 },
    { "Charge", SK_DASH, 0 }, { "Judgment", SK_CLEAVE, 180 } },
  // Wizard
  { { "M.Missile", SK_BOLT, 130 }, { "Frost Nova", SK_NOVA, 100 },
    { "Blink", SK_DASH, 0 }, { "Shield", SK_GUARD, 0 },
    { "Haste", SK_HASTE, 0 }, { "Meteor", SK_QUAKE, 200 } },
  // Paladin
  { { "Smite", SK_STRIKE, 150 }, { "Lay Hands", SK_HEAL, 45 },
    { "Aura", SK_GUARD, 0 }, { "Holy Bolt", SK_BOLT, 140 },
    { "Crusade", SK_RAGE, 0 }, { "Wrath", SK_NOVA, 160 } },
  // Cleric
  { { "Holy Bolt", SK_BOLT, 120 }, { "Heal", SK_HEAL, 40 },
    { "Bless", SK_RAGE, 0 }, { "Sanctuary", SK_NOVA, 110 },
    { "Ward", SK_GUARD, 0 }, { "Miracle", SK_HEAL, 90 } },
  // Bard
  { { "Dissonance", SK_BOLT, 110 }, { "Inspire", SK_RAGE, 0 },
    { "Lullaby", SK_STUN, 60 }, { "Ballad", SK_HEAL, 30 },
    { "Tempo", SK_HASTE, 0 }, { "Crescendo", SK_QUAKE, 160 } },
  // Necromancer
  { { "Drain", SK_DRAIN, 110 }, { "Bone Spear", SK_BOLT, 150 },
    { "Curse", SK_POISON, 90 }, { "Corpse Exp", SK_NOVA, 140 },
    { "Bone Armor", SK_GUARD, 0 }, { "Death", SK_QUAKE, 190 } },
  // Assassin
  { { "Stab", SK_STRIKE, 170 }, { "Venom", SK_POISON, 100 },
    { "Shadowstep", SK_DASH, 0 }, { "Blades", SK_CLEAVE, 120 },
    { "Fade", SK_HASTE, 0 }, { "Execute", SK_STRIKE, 330 } },
  // Dragoon
  { { "Lance", SK_STRIKE, 160 }, { "Jump", SK_DASH, 0 },
    { "Sweep", SK_CLEAVE, 120 }, { "Dragon Hrt", SK_RAGE, 0 },
    { "Wyvern", SK_BOLT, 160 }, { "Dragonfall", SK_QUAKE, 190 } },
  // Shaman
  { { "Spirit", SK_BOLT, 120 }, { "Totem", SK_GUARD, 0 },
    { "Chain Ltng", SK_NOVA, 120 }, { "Mend", SK_HEAL, 40 },
    { "Bloodlust", SK_HASTE, 0 }, { "Earthquake", SK_QUAKE, 180 } },
  // Warlock
  { { "Eldritch", SK_BOLT, 130 }, { "Hex", SK_POISON, 110 },
    { "Drain Life", SK_DRAIN, 120 }, { "Dark Pact", SK_RAGE, 0 },
    { "Doom", SK_NOVA, 150 }, { "Apocalypse", SK_QUAKE, 210 } },
  // Stalker
  { { "Rend", SK_STRIKE, 160 }, { "Pounce", SK_DASH, 0 },
    { "Snare", SK_STUN, 90 }, { "Vanish", SK_HASTE, 0 },
    { "Mark", SK_RAGE, 0 }, { "Kill Shot", SK_BOLT, 260 } },
  // Marauder
  { { "Crush", SK_STRIKE, 170 }, { "Rampage", SK_CLEAVE, 130 },
    { "Warcry", SK_RAGE, 0 }, { "Bloodlust", SK_DRAIN, 120 },
    { "Stomp", SK_NOVA, 140 }, { "Berserk", SK_HASTE, 0 } },
};

static const SkillDef *skill_def(uint8_t k) {
  uint8_t c = player.player_class < CLASS_COUNT ? player.player_class : 0;
  return &class_skills[c][k];
}

static uint8_t skill_cost(const SkillDef *s, uint8_t rank) {
  uint16_t c = kind_cost[s->kind];
  c += (c * player.level) / 20 + rank;
  return c > 250 ? 250 : (uint8_t)c;
}

//------------------------------------------------------------------------------
// State
//------------------------------------------------------------------------------

typedef enum AiKind {
  AI_MELEE,
  AI_FAST,
  AI_SLOW,
  AI_RANGED,
  AI_SLAM,
  AI_BREATH,
} AiKind;

typedef struct Mob {
  bool active;
  uint8_t slot;
  int8_t x;
  int8_t y;
  int8_t px;
  int8_t py;
  Direction dir;
  uint8_t kind;
  uint8_t rank;      // 0 normal, 1 elite, 2 boss
  uint8_t move_cd;
  uint8_t atk_cd;
  uint8_t windup;
  uint8_t flash;
  uint8_t stun;
  uint8_t poison;
  uint8_t attacks;
  uint8_t palette;
  uint8_t tile_root;
} Mob;

typedef struct Bolt {
  bool active;
  int8_t x;
  int8_t y;
  int8_t px;
  int8_t py;
  Direction dir;
  uint8_t range;
  uint8_t power;
  uint8_t kind;
} Bolt;

typedef enum FxType {
  FX_NONE,
  FX_SLASH,
  FX_NOVA,
  FX_ICON,
  FX_SPARK,
} FxType;

Mob mobs[MAX_MOBS];
static Bolt pbolt;
static Bolt ebolt;
static uint8_t ebolt_mob;

static uint8_t atk_cd;
static uint8_t skill_cd[SKILL_COUNT];
static uint8_t iframes;
static uint8_t hero_stun;
static uint8_t hero_poison;
static uint8_t tick;
static uint8_t dodge_cd;
static uint8_t forced_steps;
static Direction forced_dir;
static bool fast;

static uint8_t fx_type;
static uint8_t fx_t;
static int8_t fx_x;
static int8_t fx_y;
static Direction fx_dir;
static uint8_t fx_tile;
static uint8_t fx_pal;

static uint8_t pop_t;
static int16_t pop_x;
static int16_t pop_y;
static uint8_t pop_n;
static uint8_t pop_tiles[3];
static uint8_t pop_pal;

static uint8_t lost_timer;
static uint16_t xp_gain;
static uint8_t encounter_level;
static uint8_t kills;
static uint8_t drops;
static char msg[160];
static bool msg_pending;
static bool death_pending;

static uint8_t hud_saved[20];
static bool hud_visible;
static bool hud_dirty;
static uint16_t hud_hp;
static uint16_t hud_sp;
static uint8_t hud_sel;
static uint8_t hud_ready;

//------------------------------------------------------------------------------
// Utilities
//------------------------------------------------------------------------------

static int8_t absi(int8_t v) {
  return v < 0 ? -v : v;
}

static void step_dir(Direction d, int8_t *x, int8_t *y) {
  switch (d) {
  case UP: (*y)--; break;
  case DOWN: (*y)++; break;
  case LEFT: (*x)--; break;
  case RIGHT: (*x)++; break;
  }
}

static Direction dir_toward(int8_t dx, int8_t dy) {
  if (absi(dx) >= absi(dy))
    return dx > 0 ? RIGHT : LEFT;
  return dy > 0 ? DOWN : UP;
}

/**
 * Converts a map tile (+ pixel offset) to OAM coordinates.
 * @return `false` if the position is off screen.
 */
static bool to_screen(int8_t tx, int8_t ty, int8_t ox, int8_t oy,
  uint8_t *sx, uint8_t *sy) {
  int16_t x = ((int16_t)(tx - map_x) << 4) + 8 + ox;
  int16_t y = ((int16_t)(ty - map_y) << 4) + 16 + oy;
  if (map_state == MAP_STATE_MOVING) {
    const int8_t d = 15 - (int8_t)move_step;
    switch (move_direction) {
    case UP: y -= d; break;
    case DOWN: y += d; break;
    case LEFT: x -= d; break;
    case RIGHT: x += d; break;
    }
  }
  if (x < 1 || x > 167 || y < 1 || y > 159)
    return false;
  *sx = (uint8_t)x;
  *sy = (uint8_t)y;
  return true;
}

static void hide_spr(uint8_t first, uint8_t count) {
  for (uint8_t k = 0; k < count; k++)
    move_sprite(first + k, 0, 0);
}

static Mob *mob_at(int8_t x, int8_t y) {
  Mob *m = mobs;
  for (uint8_t k = 0; k < MAX_MOBS; k++, m++)
    if (m->active && m->x == x && m->y == y)
      return m;
  return NULL;
}

static bool hero_occupies(int8_t x, int8_t y) {
  const int8_t hx = hero_x();
  const int8_t hy = hero_y();
  if (x == hx && y == hy)
    return true;
  if (map_state == MAP_STATE_MOVING) {
    int8_t px = hx, py = hy;
    switch (move_direction) {
    case UP: py++; break;
    case DOWN: py--; break;
    case LEFT: px++; break;
    case RIGHT: px--; break;
    }
    return x == px && y == py;
  }
  return false;
}

static bool mob_can_enter(int8_t x, int8_t y) {
  if (map_attr_at(x, y) != MAP_GROUND)
    return false;
  if (hero_occupies(x, y))
    return false;
  return mob_at(x, y) == NULL;
}

static uint16_t rand_vary(uint16_t v) {
  // +/- ~12%
  int16_t delta = (int16_t)((v >> 3) * ((int8_t)d16() - 8)) / 8;
  int16_t r = (int16_t)v + delta;
  return r < 1 ? 1 : (uint16_t)r;
}

//------------------------------------------------------------------------------
// Effects and popups
//------------------------------------------------------------------------------

static void fx_start(uint8_t type, int8_t x, int8_t y, Direction d) {
  fx_type = type;
  fx_x = x;
  fx_y = y;
  fx_dir = d;
  fx_t = 0;
  fx_pal = PAL_FX;
}

static void fx_icon(uint8_t tile, uint8_t pal) {
  fx_start(FX_ICON, hero_x(), hero_y(), DOWN);
  fx_tile = tile;
  fx_pal = pal;
}

static void popup_number(int8_t tx, int8_t ty, uint16_t value, uint8_t pal) {
  uint8_t sx, sy;
  if (!to_screen(tx, ty, 0, -6, &sx, &sy))
    return;
  if (value > 999)
    value = 999;
  pop_n = value >= 100 ? 3 : (value >= 10 ? 2 : 1);
  uint8_t k = pop_n;
  do {
    pop_tiles[--k] = T_DIGIT0 + value % 10;
    value /= 10;
  } while (k);
  pop_x = sx + 8 - pop_n * 4;
  pop_y = sy;
  pop_t = 28;
  pop_pal = pal;
  for (k = 0; k < 3; k++) {
    set_sprite_tile(SPR_POP + k, pop_tiles[k]);
    set_sprite_prop(SPR_POP + k, PROP_BANK1 | pal);
  }
}

static void update_popup(void) {
  if (!pop_t) {
    hide_spr(SPR_POP, 3);
    return;
  }
  pop_t--;
  if ((pop_t & 1) == 0 && pop_t > 8)
    pop_y--;
  for (uint8_t k = 0; k < 3; k++) {
    if (k < pop_n && pop_y > 8)
      move_sprite(SPR_POP + k, pop_x + k * 8, pop_y);
    else
      move_sprite(SPR_POP + k, 0, 0);
  }
}

static void update_fx(void) {
  uint8_t sx, sy;
  const uint8_t prop = PROP_BANK1 | fx_pal;
  const uint8_t count = pbolt.active ? 3 : 4;

  if (fx_type == FX_NONE) {
    hide_spr(SPR_FX, count);
    return;
  }

  fx_t++;
  switch (fx_type) {
  case FX_SLASH: {
    if (fx_t > 9 || !to_screen(fx_x, fx_y, 0, 0, &sx, &sy)) {
      fx_type = FX_NONE;
      break;
    }
    const bool vertical = fx_dir == UP || fx_dir == DOWN;
    const uint8_t t = vertical ? T_SLASH_D : T_SLASH_R;
    uint8_t p = prop;
    uint8_t a = t, b = t + 1, c = t + 16, d = t + 17;
    if (fx_dir == LEFT) {
      p |= PROP_FLIPX;
      a = t + 1; b = t; c = t + 17; d = t + 16;
    } else if (fx_dir == UP) {
      p |= PROP_FLIPY;
      a = t + 16; b = t + 17; c = t; d = t + 1;
    }
    set_sprite_tile(SPR_FX, a);
    set_sprite_tile(SPR_FX + 1, b);
    set_sprite_tile(SPR_FX + 2, c);
    set_sprite_tile(SPR_FX + 3, d);
    for (uint8_t k = 0; k < 4; k++)
      set_sprite_prop(SPR_FX + k, p);
    move_sprite(SPR_FX, sx, sy);
    move_sprite(SPR_FX + 1, sx + 8, sy);
    move_sprite(SPR_FX + 2, sx, sy + 8);
    if (count == 4)
      move_sprite(SPR_FX + 3, sx + 8, sy + 8);
    break;
  }
  case FX_NOVA: {
    if (fx_t > 14 || !to_screen(fx_x, fx_y, 0, 0, &sx, &sy)) {
      fx_type = FX_NONE;
      break;
    }
    const uint8_t r = fx_t * 2;
    for (uint8_t k = 0; k < 4; k++) {
      set_sprite_tile(SPR_FX + k, T_RING);
      set_sprite_prop(SPR_FX + k, prop |
        ((k & 1) ? PROP_FLIPX : 0) | ((k & 2) ? PROP_FLIPY : 0));
    }
    move_sprite(SPR_FX, sx - r, sy - r);
    move_sprite(SPR_FX + 1, sx + 8 + r, sy - r);
    move_sprite(SPR_FX + 2, sx - r, sy + 8 + r);
    if (count == 4)
      move_sprite(SPR_FX + 3, sx + 8 + r, sy + 8 + r);
    break;
  }
  case FX_ICON:
  case FX_SPARK: {
    if (fx_t > 24 || !to_screen(fx_x, fx_y, 0, 0, &sx, &sy)) {
      fx_type = FX_NONE;
      break;
    }
    set_sprite_tile(SPR_FX, fx_type == FX_SPARK ? T_SPARK : fx_tile);
    set_sprite_prop(SPR_FX, prop);
    if (fx_type == FX_SPARK)
      move_sprite(SPR_FX, sx + 4, sy + 4);
    else
      move_sprite(SPR_FX, sx + 4, sy - 4 - (fx_t >> 1));
    hide_spr(SPR_FX + 1, count - 1);
    if (fx_type == FX_SPARK && fx_t > 8)
      fx_type = FX_NONE;
    break;
  }
  }

  if (fx_type == FX_NONE)
    hide_spr(SPR_FX, count);
}

//------------------------------------------------------------------------------
// HUD (window row 0, shown at the bottom of the screen)
//------------------------------------------------------------------------------

void arpg_hide_hud(void) BANKED {
  if (!hud_visible)
    return;
  hud_visible = false;
  uint8_t *vram = VRAM_WINDOW;
  VBK_REG = VBK_TILES;
  for (uint8_t k = 0; k < 20; k++)
    set_vram_byte(vram + k, hud_saved[k]);
  move_win(7, 144);
}

static void capture_hud_row(void) {
  uint8_t *vram = VRAM_WINDOW;
  VBK_REG = VBK_TILES;
  for (uint8_t k = 0; k < 20; k++)
    hud_saved[k] = get_vram_byte(vram + k);
}

void arpg_hotbar_name(uint8_t entry, char *buf) BANKED {
  if (entry == HOTBAR_POTION)
    sprintf(buf, "Potion x%u", inventory[ITEM_POTION].quantity);
  else if (entry == HOTBAR_ETHER)
    sprintf(buf, "Ether x%u", inventory[ITEM_ETHER].quantity);
  else if (entry < SKILL_COUNT)
    strcpy(buf, skill_def(entry)->name);
  else
    strcpy(buf, "-");
}

static void draw_hud(void) {
  char buf[24];
  char name[12];
  const uint8_t entry = hotbar[hotbar_sel];
  arpg_hotbar_name(entry, name);
  name[5] = 0;
  uint8_t ready = 1;
  if (entry < SKILL_COUNT) {
    const SkillDef *s = skill_def(entry);
    ready = skill_rank[entry] && !skill_cd[entry] &&
      player.sp >= skill_cost(s, skill_rank[entry]);
  }
  memset(buf, ' ', 20);
  sprintf(buf, "%u/%u", player.hp, player.max_hp);
  buf[strlen(buf)] = ' ';
  sprintf(buf + 10, "%c%u", is_magic_class() ? 'M' : 'S', player.sp);
  buf[10 + strlen(buf + 10)] = ' ';
  buf[14] = ready ? '>' : '.';
  memcpy(buf + 15, name, strlen(name));
  buf[20] = 0;
  core.draw_text(VRAM_WINDOW, buf, 20);

  hud_hp = player.hp;
  hud_sp = player.sp;
  hud_sel = hotbar_sel;
  hud_ready = ready;
  hud_dirty = false;
}

static void update_hud(void) {
  const bool can_show =
    (map_state == MAP_STATE_WAITING || map_state == MAP_STATE_MOVING) &&
    textbox.state == TEXT_BOX_CLOSED;

  if (!can_show) {
    return;
  }

  if (!hud_visible) {
    hud_visible = true;
    hud_dirty = true;
    move_win(7, HUD_Y);
  }

  if (!hud_dirty && (tick & 7) == 0) {
    uint8_t ready = 1;
    const uint8_t entry = hotbar[hotbar_sel];
    if (entry < SKILL_COUNT)
      ready = skill_rank[entry] && !skill_cd[entry] &&
        player.sp >= skill_cost(skill_def(entry), skill_rank[entry]);
    if (ready != hud_ready)
      hud_dirty = true;
  }

  if (hud_dirty || hud_hp != player.hp || hud_sp != player.sp ||
    hud_sel != hotbar_sel)
    draw_hud();
}

//------------------------------------------------------------------------------
// Monster setup
//------------------------------------------------------------------------------

static MonsterTiles tiles_for(MonsterType type) {
  switch (type) {
  case MONSTER_GOBLIN: return MONSTER_TILES_GOBLIN;
  case MONSTER_ZOMBIE: return MONSTER_TILES_ZOMBIE;
  case MONSTER_BUGBEAR: return MONSTER_TILES_BUGBEAR;
  case MONSTER_OWLBEAR: return MONSTER_TILES_OWLBEAR;
  case MONSTER_GELATINOUS_CUBE: return MONSTER_TILES_GELATINOUS_CUBE;
  case MONSTER_DISPLACER_BEAST: return MONSTER_TILES_DISPLACER_BEAST;
  case MONSTER_WILL_O_WISP: return MONSTER_TILES_WILL_O_WISP;
  case MONSTER_DEATHKNIGHT: return MONSTER_TILES_DEATHKNIGHT;
  case MONSTER_MINDFLAYER: return MONSTER_TILES_MINDFLAYER;
  case MONSTER_BEHOLDER: return MONSTER_TILES_BEHOLDER;
  case MONSTER_DRAGON: return MONSTER_TILES_DRAGON;
  default: return MONSTER_TILES_KOBOLD;
  }
}

static const palette_color_t *palette_for(MonsterType type) {
  switch (type) {
  case MONSTER_GOBLIN: return goblin_palettes;
  case MONSTER_ZOMBIE: return zombie_palettes;
  case MONSTER_BUGBEAR: return bugbear_palettes;
  case MONSTER_OWLBEAR: return owlbear_palettes;
  case MONSTER_GELATINOUS_CUBE: return gelatinous_cube_palettes;
  case MONSTER_DISPLACER_BEAST: return displacer_beast_palettes;
  case MONSTER_WILL_O_WISP: return will_o_wisp_palettes;
  case MONSTER_DEATHKNIGHT: return deathknight_palettes;
  case MONSTER_MINDFLAYER: return mindflayer_palettes;
  case MONSTER_BEHOLDER: return beholder_palettes;
  case MONSTER_DRAGON: return dragon_palettes;
  default: return kobold_palettes;
  }
}

static uint8_t ai_for(MonsterType type) {
  switch (type) {
  case MONSTER_GOBLIN:
  case MONSTER_DISPLACER_BEAST:
    return AI_FAST;
  case MONSTER_ZOMBIE:
  case MONSTER_GELATINOUS_CUBE:
    return AI_SLOW;
  case MONSTER_WILL_O_WISP:
  case MONSTER_MINDFLAYER:
  case MONSTER_BEHOLDER:
    return AI_RANGED;
  case MONSTER_DEATHKNIGHT:
  case MONSTER_OWLBEAR:
    return AI_SLAM;
  case MONSTER_DRAGON:
    return AI_BREATH;
  default:
    return AI_MELEE;
  }
}

static uint8_t move_delay(const Mob *m) {
  uint8_t d;
  switch (m->kind) {
  case AI_FAST: d = 4; break;
  case AI_SLOW: d = 22; break;
  case AI_RANGED: d = 14; break;
  default: d = 10;
  }
  if (m->rank == 2) {
    const Monster *mon = encounter.monsters + m->slot;
    if (mon->hp < mon->max_hp / 2)
      d >>= 1;
  }
  if (buffs.haste && d > 4)
    d += 4;
  return d;
}

static uint8_t attack_delay(const Mob *m) {
  const Monster *mon = encounter.monsters + m->slot;
  uint8_t base = m->kind == AI_SLOW ? 90 : 72;
  uint8_t agl = mon->agl > 60 ? 60 : mon->agl;
  uint8_t d = base - agl / 2;
  if (m->rank == 2 && mon->hp < mon->max_hp / 2)
    d -= d / 3;
  return d < 30 ? 30 : d;
}

/**
 * Finds a free spawn tile around the hero.
 */
static bool find_spawn(int8_t *ox, int8_t *oy, uint8_t index) {
  const int8_t hx = hero_x();
  const int8_t hy = hero_y();
  static const int8_t offs[][2] = {
    { 3, 0 }, { -3, 0 }, { 0, 3 }, { 0, -3 },
    { 3, 2 }, { -3, -2 }, { 2, -3 }, { -2, 3 },
    { 4, 1 }, { -4, -1 }, { 1, 4 }, { -1, -4 },
    { 2, 2 }, { -2, -2 }, { 2, -2 }, { -2, 2 },
    { 2, 0 }, { -2, 0 }, { 0, 2 }, { 0, -2 },
  };
  const uint8_t n = sizeof(offs) / sizeof(offs[0]);
  const uint8_t start = (index * 3 + (rand() & 3)) % n;
  for (uint8_t k = 0; k < n; k++) {
    const uint8_t i = (start + k) % n;
    const int8_t x = hx + offs[i][0];
    const int8_t y = hy + offs[i][1];
    if (mob_can_enter(x, y)) {
      *ox = x;
      *oy = y;
      return true;
    }
  }
  return false;
}

void arpg_begin_encounter(void) BANKED {
  uint8_t count = 0;
  bool boss_fight = encounter.is_final_boss;

  memset(mobs, 0, sizeof(mobs));
  pbolt.active = false;
  ebolt.active = false;

  wait_vbl_done();
  DISPLAY_OFF;

  Monster *mon = encounter.monsters;
  for (uint8_t k = 0; k < 3; k++, mon++) {
    if (!mon->active)
      continue;
    Mob *m = mobs + count;
    int8_t x, y;
    if (!find_spawn(&x, &y, count)) {
      mon->active = false;
      continue;
    }
    m->active = true;
    m->slot = k;
    m->x = x;
    m->y = y;
    m->dir = dir_toward(hero_x() - x, hero_y() - y);
    m->kind = ai_for(mon->type);
    m->rank = 0;
    if (mon->exp_tier == S_TIER || boss_fight) {
      m->rank = 2;
    } else if (mon->level >= 5 && (rand() & 7) == 0) {
      m->rank = 1;
    }

    // Real-time tuning: bosses and elites are much tougher.
    if (m->rank == 2) {
      mon->max_hp = mon->max_hp > 21000 ? 65000 : mon->max_hp * 3;
    } else if (m->rank == 1) {
      mon->max_hp = mon->max_hp > 30000 ? 65000 : mon->max_hp * 2;
      mon->atk += mon->atk / 4;
    }
    mon->hp = mon->max_hp;

    m->move_cd = 20 + count * 10;
    m->atk_cd = 40;
    m->palette = count == 2 ? 3 : 6 + count;
    m->tile_root = 0x20 + 0x20 * count;

    core.load_monster_tiles(tiles_for(mon->type), count);
    const palette_color_t *pal = palette_for(mon->type);
    const uint8_t tier = m->rank ? S_TIER : mon->exp_tier;
    core.load_sprite_palette(pal + tier * 4, m->palette, 1);
    count++;
  }

  DISPLAY_ON;

  xp_gain = 0;
  kills = 0;
  drops = 0;
  encounter_level = world_area_level();
  encounter.victory = false;
  strcpy(msg, "Victory!");

  if (count == 0) {
    arpg_in_combat = false;
    return;
  }

  arpg_in_combat = true;
  play_sound(sfx_start_battle);
}

//------------------------------------------------------------------------------
// Damage
//------------------------------------------------------------------------------

static void end_combat(bool victory);

static void hurt_hero(uint16_t dmg, bool magic) {
  if (iframes || player.hp == 0)
    return;
  uint8_t defense = magic ? player.mdef : player.def;
  int16_t d = (int16_t)dmg - defense / 3;
  if (d < 1) d = 1;
  if (buffs.def) d -= d / 4;
  if (d < 1) d = 1;
  player.hp = (uint16_t)d >= player.hp ? 0 : player.hp - d;
  iframes = HERO_IFRAMES;
  popup_number(hero_x(), hero_y(), d, PAL_FX);
  play_sound(magic ? sfx_monster_attack2 : sfx_monster_attack1);
  player_hp_and_sp_updated = true;
}

static uint16_t monster_damage(const Mob *m) {
  const Monster *mon = encounter.monsters + m->slot;
  uint16_t d = get_monster_dmg(mon->level, mon->exp_tier);
  d = d + mon->atk / 4;
  d = (d * 2) / 3;
  if (m->rank == 1) d += d / 4;
  if (m->rank == 2) d += d / 3;
  return rand_vary(d);
}

static void kill_mob(Mob *m) {
  Monster *mon = encounter.monsters + m->slot;
  m->active = false;
  mon->active = false;
  hide_spr(SPR_MOB0 + 4 * (m - mobs), 4);
  play_sound(sfx_monster_death);
  fx_start(FX_SPARK, m->x, m->y, DOWN);

  uint16_t xp = calc_monster_exp(mon->exp_level, mon->exp_tier);
  if (m->rank == 1) xp *= 2;
  if (m->rank == 2) xp *= 3;
  xp_gain = xp_gain + xp > 60000 ? 60000 : xp_gain + xp;
  kills++;

  quest_event(QUEST_EVENT_KILL, mon->type);
  if (m->rank == 1)
    quest_event(QUEST_EVENT_ELITE, mon->type);
  if (m->rank == 2)
    quest_event(QUEST_EVENT_BOSS, mon->type);

  drops += loot_roll_kill(encounter_level, m->rank, msg);
}

/**
 * Deals damage from the hero to a monster.
 */
static void hit_mob(Mob *m, uint8_t power, bool magic, uint8_t stun) {
  Monster *mon = encounter.monsters + m->slot;
  uint16_t base = get_player_damage(player.level, B_TIER);
  const uint8_t stat = magic ? player.matk : player.atk;
  const uint8_t res = magic ? mon->mdef : mon->def;
  int16_t d = (int16_t)base + stat / 2 - res / 4;
  if (d < 1) d = 1;
  uint32_t big = ((uint32_t)d * power) / 100;
  uint16_t dmg = big > 9999 ? 9999 : (uint16_t)big;
  dmg = rand_vary(dmg);

  bool crit = d128() < (uint16_t)loot_bonus.crit * 128 / 100;
  if (crit)
    dmg *= 2;

  mon->hp = dmg >= mon->hp ? 0 : mon->hp - dmg;
  popup_number(m->x, m->y, dmg, crit ? PAL_FX : PAL_POP);
  m->flash = 8;
  play_sound(crit ? sfx_monster_critical : (magic ? sfx_magic_missile :
    sfx_melee_attack));

  if (loot_bonus.leech) {
    uint16_t heal = (dmg * loot_bonus.leech) / 100 + 1;
    player.hp = player.hp + heal > player.max_hp ? player.max_hp :
      player.hp + heal;
  }

  if (mon->hp == 0) {
    kill_mob(m);
    return;
  }

  if (stun)
    m->stun = m->rank == 2 ? stun / 3 : stun;
  if (m->windup && m->rank != 2)
    m->windup = 0;

  // Knockback (not bosses).
  if (m->rank != 2 && m->px == 0 && m->py == 0) {
    int8_t nx = m->x, ny = m->y;
    step_dir(dir_toward(m->x - hero_x(), m->y - hero_y()), &nx, &ny);
    if (mob_can_enter(nx, ny)) {
      m->px = (m->x - nx) * 16;
      m->py = (m->y - ny) * 16;
      m->x = nx;
      m->y = ny;
      m->move_cd = 12;
    }
  }
}

static bool use_magic(void) {
  return player.matk > player.atk;
}

static uint8_t hit_area(int8_t cx, int8_t cy, uint8_t radius, uint8_t power,
  bool magic) {
  uint8_t hits = 0;
  Mob *m = mobs;
  for (uint8_t k = 0; k < MAX_MOBS; k++, m++) {
    if (!m->active)
      continue;
    const uint8_t dist = absi(m->x - cx) + absi(m->y - cy);
    const uint8_t cheb = absi(m->x - cx) > absi(m->y - cy) ?
      absi(m->x - cx) : absi(m->y - cy);
    if (radius == 1 ? cheb <= 1 : dist <= radius) {
      hit_mob(m, power, magic, 0);
      hits++;
    }
  }
  return hits;
}

//------------------------------------------------------------------------------
// Bolts
//------------------------------------------------------------------------------

static void fire_bolt(Bolt *b, int8_t x, int8_t y, Direction d, uint8_t power,
  uint8_t kind) {
  b->active = true;
  b->x = x;
  b->y = y;
  b->px = 0;
  b->py = 0;
  b->dir = d;
  b->range = 6;
  b->power = power;
  b->kind = kind;
}

/**
 * Advances a bolt by `speed` pixels. @return `true` when it enters a new tile.
 */
static bool advance_bolt(Bolt *b, uint8_t speed) {
  switch (b->dir) {
  case UP: b->py -= speed; break;
  case DOWN: b->py += speed; break;
  case LEFT: b->px -= speed; break;
  case RIGHT: b->px += speed; break;
  }
  if (absi(b->px) < 16 && absi(b->py) < 16)
    return false;
  step_dir(b->dir, &b->x, &b->y);
  b->px = 0;
  b->py = 0;
  if (b->range)
    b->range--;
  return true;
}

static void draw_bolt(const Bolt *b, uint8_t sprite, uint8_t tile, uint8_t pal) {
  uint8_t sx, sy;
  if (!b->active || !to_screen(b->x, b->y, b->px, b->py, &sx, &sy)) {
    move_sprite(sprite, 0, 0);
    return;
  }
  set_sprite_tile(sprite, tile);
  set_sprite_prop(sprite, PROP_BANK1 | pal);
  move_sprite(sprite, sx + 4, sy + 4);
}

static uint8_t kind_is_green(uint8_t kind) {
  return kind == SK_POISON ? PAL_GREEN : PAL_FX;
}

static void update_player_bolt(void) {
  if (!pbolt.active) {
    move_sprite(SPR_PBOLT, 0, 0);
    return;
  }
  if (advance_bolt(&pbolt, 4)) {
    Mob *m = mob_at(pbolt.x, pbolt.y);
    if (m) {
      const uint8_t kind = pbolt.kind;
      const uint8_t mi = m - mobs;
      hit_mob(m, pbolt.power, true, 0);
      if (mobs[mi].active && kind == SK_POISON)
        mobs[mi].poison = 180;
      if (kind == SK_DRAIN) {
        uint16_t heal = player.max_hp / 12 + 2;
        player.hp = player.hp + heal > player.max_hp ? player.max_hp :
          player.hp + heal;
      }
      pbolt.active = false;
    } else if (map_attr_at(pbolt.x, pbolt.y) == MAP_WALL || !pbolt.range) {
      pbolt.active = false;
    }
  }
  draw_bolt(&pbolt, SPR_PBOLT, T_PBOLT, kind_is_green(pbolt.kind));
}

static void update_enemy_bolt(void) {
  if (!ebolt.active) {
    move_sprite(SPR_EBOLT, 0, 0);
    return;
  }
  if (advance_bolt(&ebolt, 2)) {
    if (hero_occupies(ebolt.x, ebolt.y)) {
      const Mob *m = mobs + ebolt_mob;
      hurt_hero(ebolt.power ? monster_damage(m) : 1, true);
      if (encounter.monsters[m->slot].type == MONSTER_MINDFLAYER &&
        (rand() & 3) == 0 && !iframes)
        hero_stun = 40;
      ebolt.active = false;
    } else if (map_attr_at(ebolt.x, ebolt.y) == MAP_WALL || !ebolt.range) {
      ebolt.active = false;
    }
  }
  draw_bolt(&ebolt, SPR_EBOLT, T_EBOLT, PAL_GREEN);
}

//------------------------------------------------------------------------------
// Monster AI
//------------------------------------------------------------------------------

static bool try_step(Mob *m, Direction d) {
  int8_t nx = m->x, ny = m->y;
  step_dir(d, &nx, &ny);
  m->dir = d;
  if (!mob_can_enter(nx, ny))
    return false;
  m->px = (m->x - nx) * 16;
  m->py = (m->y - ny) * 16;
  m->x = nx;
  m->y = ny;
  return true;
}

static void step_toward(Mob *m, int8_t dx, int8_t dy, bool align) {
  Direction primary, secondary;
  bool horizontal_first;
  if (align)
    horizontal_first = (absi(dx) <= absi(dy) && dx != 0) || dy == 0;
  else
    horizontal_first = absi(dx) >= absi(dy);
  if (horizontal_first) {
    primary = dx > 0 ? RIGHT : LEFT;
    secondary = dy > 0 ? DOWN : UP;
  } else {
    primary = dy > 0 ? DOWN : UP;
    secondary = dx > 0 ? RIGHT : LEFT;
  }
  if ((horizontal_first ? dx : dy) != 0 && try_step(m, primary))
    return;
  if ((horizontal_first ? dy : dx) != 0 && try_step(m, secondary))
    return;
  // Blocked: walk around the obstacle (perpendicular to the primary axis).
  const bool flip = rand() & 1;
  if (horizontal_first) {
    if (try_step(m, flip ? UP : DOWN) || try_step(m, flip ? DOWN : UP))
      return;
  } else {
    if (try_step(m, flip ? LEFT : RIGHT) || try_step(m, flip ? RIGHT : LEFT))
      return;
  }
}

static void begin_attack(Mob *m, uint8_t windup) {
  m->windup = windup;
  m->dir = dir_toward(hero_x() - m->x, hero_y() - m->y);
}

static void resolve_attack(Mob *m) {
  const int8_t dx = hero_x() - m->x;
  const int8_t dy = hero_y() - m->y;
  const uint8_t dist = absi(dx) + absi(dy);
  const Monster *mon = encounter.monsters + m->slot;
  m->attacks++;
  m->atk_cd = attack_delay(m);

  switch (m->kind) {
  case AI_RANGED:
    if (dist > 1) {
      if (!ebolt.active) {
        fire_bolt(&ebolt, m->x, m->y, m->dir, 1, 0);
        ebolt_mob = m - mobs;
      }
      return;
    }
    break;
  case AI_SLAM:
    if (m->attacks % 3 == 0) {
      fx_start(FX_NOVA, m->x, m->y, DOWN);
      if (dist <= 2)
        hurt_hero(monster_damage(m) + monster_damage(m) / 2, false);
      return;
    }
    break;
  case AI_BREATH:
    if ((dx == 0 || dy == 0) && dist <= 3 && dist > 1) {
      fx_start(FX_NOVA, hero_x(), hero_y(), DOWN);
      hurt_hero(monster_damage(m) + monster_damage(m) / 2, true);
      return;
    }
    break;
  }

  if (dist == 1) {
    hurt_hero(monster_damage(m), false);
    if ((mon->type == MONSTER_ZOMBIE || mon->type == MONSTER_GELATINOUS_CUBE)
      && (rand() & 3) == 0)
      hero_poison = 150;
  } else {
    play_sound(sfx_miss);
  }
}

static void update_mob(Mob *m) {
  // Smooth movement between tiles.
  const int8_t speed = m->kind == AI_FAST ? 2 : 1;
  if (m->px > 0) m->px = m->px > speed ? m->px - speed : 0;
  else if (m->px < 0) m->px = -m->px > speed ? m->px + speed : 0;
  if (m->py > 0) m->py = m->py > speed ? m->py - speed : 0;
  else if (m->py < 0) m->py = -m->py > speed ? m->py + speed : 0;

  if (m->flash) m->flash--;

  if (m->poison && (tick & 31) == 0) {
    Monster *mon = encounter.monsters + m->slot;
    uint16_t d = mon->max_hp / 20 + 1;
    mon->hp = d >= mon->hp ? 0 : mon->hp - d;
    popup_number(m->x, m->y, d, PAL_GREEN);
    if (mon->hp == 0) {
      kill_mob(m);
      return;
    }
  }
  if (m->poison) m->poison--;

  if (m->stun) {
    m->stun--;
    return;
  }

  if (m->windup) {
    if (--m->windup == 0)
      resolve_attack(m);
    return;
  }

  if (m->atk_cd) m->atk_cd--;
  if (m->move_cd) m->move_cd--;

  if (m->px || m->py)
    return;

  const int8_t dx = hero_x() - m->x;
  const int8_t dy = hero_y() - m->y;
  const uint8_t dist = absi(dx) + absi(dy);
  const bool aligned = dx == 0 || dy == 0;

  if (dist == 1 && !m->atk_cd) {
    begin_attack(m, (m->kind == AI_SLAM && m->attacks % 3 == 2) ?
      WINDUP_FRAMES + 14 : WINDUP_FRAMES);
    return;
  }

  if (m->kind == AI_RANGED && aligned && dist >= 2 && dist <= 4 &&
    !m->atk_cd && !ebolt.active) {
    begin_attack(m, WINDUP_FRAMES);
    return;
  }

  if (m->kind == AI_BREATH && aligned && dist >= 2 && dist <= 3 &&
    !m->atk_cd) {
    begin_attack(m, WINDUP_FRAMES + 16);
    return;
  }

  if (m->move_cd)
    return;
  m->move_cd = move_delay(m);

  if (dist <= 1)
    return;

  if (m->kind == AI_RANGED && dist <= 2) {
    // Keep some distance.
    try_step(m, dir_toward(-dx, -dy));
    return;
  }

  step_toward(m, dx, dy, m->kind == AI_RANGED || m->kind == AI_BREATH);
}

static void draw_mob(const Mob *m, uint8_t index) {
  const uint8_t spr = SPR_MOB0 + 4 * index;
  uint8_t sx, sy;
  const bool blink = (m->windup && (tick & 4)) || (m->stun && (tick & 8));
  if (!m->active || !to_screen(m->x, m->y, m->px, m->py, &sx, &sy)) {
    hide_spr(spr, 4);
    return;
  }

  uint8_t offset = 0;
  switch (m->dir) {
  case UP: offset = 4; break;
  case LEFT:
  case RIGHT: offset = 8; break;
  }
  const uint8_t frame = m->tile_root + offset + (((tick >> 4) & 1) ? 2 : 0);
  uint8_t prop = m->flash ? PAL_POP : m->palette;
  if (blink)
    prop = PAL_FX;

  if (m->dir == LEFT) {
    prop |= PROP_FLIPX;
    set_sprite_tile(spr, frame + 1);
    set_sprite_tile(spr + 1, frame);
    set_sprite_tile(spr + 2, frame + 17);
    set_sprite_tile(spr + 3, frame + 16);
  } else {
    set_sprite_tile(spr, frame);
    set_sprite_tile(spr + 1, frame + 1);
    set_sprite_tile(spr + 2, frame + 16);
    set_sprite_tile(spr + 3, frame + 17);
  }
  for (uint8_t k = 0; k < 4; k++)
    set_sprite_prop(spr + k, prop);

  move_sprite(spr, sx, sy);
  move_sprite(spr + 1, sx + 8, sy);
  move_sprite(spr + 2, sx, sy + 8);
  move_sprite(spr + 3, sx + 8, sy + 8);
}

//------------------------------------------------------------------------------
// Hero actions
//------------------------------------------------------------------------------

static void facing_tile(int8_t *x, int8_t *y) {
  *x = hero_x();
  *y = hero_y();
  step_dir(hero_direction, x, y);
}

static void basic_attack(void) {
  const uint8_t agl = player.agl > 80 ? 80 : player.agl;
  atk_cd = 24 - agl / 8;
  if (buffs.haste) atk_cd >>= 1;
  if (atk_cd < 8) atk_cd = 8;

  int8_t tx, ty;
  facing_tile(&tx, &ty);

  if (use_magic()) {
    if (!pbolt.active) {
      fire_bolt(&pbolt, hero_x(), hero_y(), hero_direction, 100, SK_BOLT);
      play_sound(sfx_magic_missile);
    }
    return;
  }

  fx_start(FX_SLASH, tx, ty, hero_direction);
  Mob *m = mob_at(tx, ty);
  if (m)
    hit_mob(m, 100, false, 0);
  else
    play_sound(sfx_miss);
}

static void start_dodge(Direction d, uint8_t steps) {
  forced_dir = d;
  forced_steps = steps;
  fast = true;
  if (iframes < 28)
    iframes = 28;
  play_sound(sfx_evade);
}

static void heal_hero(uint16_t amount) {
  player.hp = player.hp + amount > player.max_hp ? player.max_hp :
    player.hp + amount;
  player_hp_and_sp_updated = true;
}

static bool cast_skill(uint8_t k) {
  const SkillDef *s = skill_def(k);
  const uint8_t rank = skill_rank[k];
  if (rank == 0 || skill_cd[k])
    return false;
  const uint8_t cost = skill_cost(s, rank);
  if (player.sp < cost)
    return false;

  const uint16_t scaled = (uint16_t)s->power * (100 + 15 * (rank - 1)) / 100;
  const uint8_t power = scaled > 255 ? 255 : (uint8_t)scaled;
  const bool magic = use_magic();
  int8_t tx, ty;
  facing_tile(&tx, &ty);

  switch (s->kind) {
  case SK_STRIKE:
  case SK_STUN: {
    fx_start(FX_SLASH, tx, ty, hero_direction);
    Mob *m = mob_at(tx, ty);
    if (m)
      hit_mob(m, power, magic, s->kind == SK_STUN ? 110 : 0);
    else
      play_sound(sfx_melee_attack);
    break;
  }
  case SK_CLEAVE:
    fx_start(FX_NOVA, hero_x(), hero_y(), DOWN);
    if (!hit_area(hero_x(), hero_y(), 1, power, magic))
      play_sound(sfx_melee_attack);
    break;
  case SK_BOLT:
  case SK_DRAIN:
  case SK_POISON:
    if (pbolt.active)
      return false;
    fire_bolt(&pbolt, hero_x(), hero_y(), hero_direction, power, s->kind);
    play_sound(s->kind == SK_POISON ? sfx_poison_spray : sfx_magic_missile);
    break;
  case SK_NOVA:
    fx_start(FX_NOVA, hero_x(), hero_y(), DOWN);
    if (!hit_area(hero_x(), hero_y(), 3, power, true))
      play_sound(sfx_magic_missile);
    break;
  case SK_QUAKE: {
    fx_start(FX_NOVA, hero_x(), hero_y(), DOWN);
    Mob *m = mobs;
    bool any = false;
    for (uint8_t i = 0; i < MAX_MOBS; i++, m++) {
      if (m->active) {
        hit_mob(m, power, true, 30);
        any = true;
      }
    }
    if (!any)
      play_sound(sfx_big_powerup);
    break;
  }
  case SK_HEAL:
    heal_hero((uint16_t)((uint32_t)player.max_hp * power / 100) + 5);
    fx_icon(T_HEAL, PAL_GREEN);
    play_sound(sfx_heal);
    break;
  case SK_RAGE:
    buffs.atk = 600 + 60 * rank;
    loot_recalc();
    fx_icon(T_UP, PAL_FX);
    play_sound(sfx_action_surge);
    break;
  case SK_GUARD:
    buffs.def = 600 + 60 * rank;
    loot_recalc();
    fx_icon(T_SHIELD, PAL_POP);
    play_sound(sfx_mid_powerup);
    break;
  case SK_HASTE:
    buffs.haste = 600 + 60 * rank;
    fx_icon(T_UP, PAL_GREEN);
    play_sound(sfx_action_surge);
    break;
  case SK_DASH:
    start_dodge(hero_direction, 3);
    iframes = 40;
    break;
  }

  player.sp -= cost;
  skill_cd[k] = kind_cooldown[s->kind];
  if (buffs.haste)
    skill_cd[k] >>= 1;
  player_hp_and_sp_updated = true;
  hud_dirty = true;
  return true;
}

bool arpg_on_action(void) BANKED {
  if (arpg_block_input)
    return true;
  const uint8_t entry = hotbar[hotbar_sel];
  if (entry == HOTBAR_EMPTY)
    return false;
  bool ok;
  if (entry >= SKILL_COUNT) {
    ok = loot_use_consumable(entry);
    if (ok) {
      fx_icon(T_HEAL, entry == HOTBAR_POTION ? PAL_GREEN : PAL_POP);
      play_sound(sfx_heal);
      player_hp_and_sp_updated = true;
    }
  } else {
    ok = cast_skill(entry);
  }
  if (!ok)
    play_sound(sfx_error);
  hud_dirty = true;
  return true;
}

static void cycle_hotbar(void) {
  for (uint8_t k = 0; k < HOTBAR_SLOTS; k++) {
    hotbar_sel = (hotbar_sel + 1) % HOTBAR_SLOTS;
    if (hotbar[hotbar_sel] != HOTBAR_EMPTY)
      break;
  }
  play_sound(sfx_menu_move);
  hud_dirty = true;
}

static void handle_input(void) {
  if (arpg_block_input || hero_stun)
    return;
  if (map_state != MAP_STATE_WAITING && map_state != MAP_STATE_MOVING)
    return;

  if (was_pressed(J_SELECT)) {
    if (is_down(J_B)) {
      cycle_hotbar();
    } else if (!dodge_cd) {
      dodge_cd = 50;
      start_dodge(hero_direction, 2);
    }
    return;
  }

  if (was_pressed(J_B) && !atk_cd)
    basic_attack();
}

//------------------------------------------------------------------------------
// Combat flow
//------------------------------------------------------------------------------

static void clear_mobs(void) {
  memset(mobs, 0, sizeof(mobs));
  pbolt.active = false;
  ebolt.active = false;
  hide_spr(SPR_MOB0, 12);
  move_sprite(SPR_EBOLT, 0, 0);
  move_sprite(SPR_PBOLT, 0, 0);
}

static void end_combat(bool victory) {
  arpg_in_combat = false;
  clear_mobs();
  arpg_restore_npcs = true;

  if (!victory)
    return;

  char *p = msg + strlen(msg);
  const uint8_t before = player.level;
  const bool leveled = level_up(xp_gain);
  if (leveled) {
    loot_on_level_up(player.level - before);
    sprintf(p, "\n+%uXP LEVEL %u!", xp_gain, player.level);
    play_sound(sfx_level_up);
    fx_icon(T_UP, PAL_FX);
  } else {
    sprintf(p, "\n+%uXP  %uG", xp_gain, loot_gold);
    play_sound(sfx_battle_success);
  }
  loot_recalc();
  player_hp_and_sp_updated = true;

  encounter.victory = true;
  if (encounter.is_final_boss)
    arpg_credits_pending = true;

  // Only interrupt play with a message if something notable happened.
  msg_pending = leveled || drops > 0 || encounter.on_victory != NULL ||
    encounter.is_final_boss;
}

static void player_defeated(void) {
  arpg_in_combat = false;
  clear_mobs();
  arpg_restore_npcs = true;
  encounter.on_victory = NULL;
  encounter.victory = false;
  uint16_t lost = loot_gold / 10;
  loot_gold -= lost;
  sprintf(msg, "You were defeated!\nLost %u gold.", lost);
  msg_pending = true;
  death_pending = true;
  play_sound(sfx_battle_death);
}

//------------------------------------------------------------------------------
// Public API
//------------------------------------------------------------------------------

static void reset_hero_state(void) {
  atk_cd = 0;
  memset(skill_cd, 0, sizeof(skill_cd));
  iframes = 0;
  hero_stun = 0;
  hero_poison = 0;
  dodge_cd = 0;
  forced_steps = 0;
  fast = false;
  fx_type = FX_NONE;
  pop_t = 0;
}

void arpg_world_init(void) BANKED {
  hud_visible = false;
  capture_hud_row();
  hud_dirty = true;
  arpg_menu_closed();
}

void arpg_menu_closed(void) BANKED {
  hide_spr(SPR_FX, 8);
  fx_type = FX_NONE;
  pop_t = 0;
  hud_dirty = true;
  if (!arpg_in_combat)
    hide_spr(SPR_MOB0, 12);
}

void arpg_floor_changed(void) BANKED {
  if (arpg_in_combat) {
    arpg_in_combat = false;
    encounter.on_victory = NULL;
  }
  clear_mobs();
  reset_hero_state();
  arpg_block_input = false;
  if (buffs.atk || buffs.def) {
    buffs.atk = 0;
    buffs.def = 0;
  }
}

bool arpg_tile_blocked(int8_t x, int8_t y) BANKED {
  return arpg_in_combat && mob_at(x, y) != NULL;
}

Direction arpg_forced_move(void) BANKED {
  if (forced_steps) {
    forced_steps--;
    return forced_dir;
  }
  fast = false;
  return HERE;
}

bool arpg_fast_move(void) BANKED {
  return fast;
}

static void update_buffs(void) {
  bool changed = false;
  if (buffs.atk && --buffs.atk == 0) changed = true;
  if (buffs.def && --buffs.def == 0) changed = true;
  if (buffs.haste) buffs.haste--;
  if (changed)
    loot_recalc();
}

static void update_regen(void) {
  if (player.hp == 0)
    return;
  const uint8_t hp_rate = arpg_in_combat ? 127 : 31;
  const uint8_t sp_rate = arpg_in_combat ? 127 : 63;
  if ((tick & hp_rate) == 0 && player.hp < player.max_hp)
    heal_hero(1 + (arpg_in_combat ? 0 : player.max_hp / 50));
  if ((tick & 127) == 64 && loot_bonus.regen)
    heal_hero(loot_bonus.regen);
  if ((tick & sp_rate) == 0 && player.sp < player.max_sp) {
    player.sp++;
    player_hp_and_sp_updated = true;
  }
  if (hero_poison) {
    hero_poison--;
    if ((hero_poison & 31) == 0 && player.hp > 1) {
      uint16_t d = player.max_hp / 40 + 1;
      player.hp = d >= player.hp ? 1 : player.hp - d;
      popup_number(hero_x(), hero_y(), d, PAL_GREEN);
      player_hp_and_sp_updated = true;
    }
  }
}

static void show_pending_messages(void) {
  if (map_state != MAP_STATE_WAITING || textbox.state != TEXT_BOX_CLOSED)
    return;

  if (death_pending && !msg_pending) {
    death_pending = false;
    player.hp = player.max_hp;
    player.sp = player.max_sp;
    hero_poison = 0;
    arpg_block_input = false;
    arpg_respawn_request = true;
    return;
  }

  if (msg_pending) {
    msg_pending = false;
    arpg_hide_hud();
    map_textbox(msg);
    return;
  }

  if (quest_notice_pending && !arpg_in_combat) {
    quest_notice_pending = false;
    arpg_hide_hud();
    play_sound(sfx_big_powerup);
    map_textbox(quest_notice);
  }
}

void arpg_update(void) BANKED {
  tick++;

  if (map_state == MAP_STATE_INACTIVE || map_state == MAP_STATE_MENU)
    return;

  if (atk_cd) atk_cd--;
  if (dodge_cd) dodge_cd--;
  if (hero_stun) hero_stun--;
  for (uint8_t k = 0; k < SKILL_COUNT; k++) {
    if (skill_cd[k]) {
      skill_cd[k]--;
      if (!skill_cd[k]) hud_dirty = true;
    }
  }

  update_buffs();
  update_regen();

  const bool active = map_state == MAP_STATE_WAITING ||
    map_state == MAP_STATE_MOVING;

  if (active) {
    handle_input();

    if (arpg_in_combat) {
      bool any = false;
      bool near = false;
      Mob *m = mobs;
      for (uint8_t k = 0; k < MAX_MOBS; k++, m++) {
        if (!m->active)
          continue;
        update_mob(m);
        if (m->active) {
          any = true;
          if (absi(m->x - hero_x()) + absi(m->y - hero_y()) <= 7)
            near = true;
        }
      }
      update_enemy_bolt();
      // Monsters lose track of a hero that runs far away.
      lost_timer = near ? 0 : lost_timer + 1;
      if (player.hp == 0) {
        player_defeated();
      } else if (!any) {
        end_combat(true);
      } else if (lost_timer > 180) {
        lost_timer = 0;
        end_combat(false);
        play_sound(sfx_evade);
      }
    }
    update_player_bolt();
  }

  // Rendering
  Mob *mr = mobs;
  for (uint8_t k = 0; k < MAX_MOBS; k++, mr++)
    draw_mob(mr, k);
  update_fx();
  update_popup();

  // Hero blink while invulnerable.
  if (iframes) {
    iframes--;
    if (iframes & 4)
      hide_spr(0, 4);
  }

  arpg_block_input = death_pending || player.hp == 0;

  update_hud();
  show_pending_messages();
}

//------------------------------------------------------------------------------
// Skill info (for menus)
//------------------------------------------------------------------------------

void arpg_skill_name(uint8_t k, char *buf) BANKED {
  strcpy(buf, k < SKILL_COUNT ? skill_def(k)->name : "-");
}

void arpg_skill_info(uint8_t k, char *buf) BANKED {
  if (k >= SKILL_COUNT) {
    buf[0] = 0;
    return;
  }
  const SkillDef *s = skill_def(k);
  const uint8_t rank = skill_rank[k] ? skill_rank[k] : 1;
  sprintf(buf, "%s %c%u", kind_info[s->kind], is_magic_class() ? 'M' : 'S',
    skill_cost(s, rank));
}

uint8_t arpg_skill_level(uint8_t k) BANKED {
  return k < SKILL_COUNT ? skill_levels[k] : 99;
}
