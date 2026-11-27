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
#include "boss_phases.h"
#include "core.h"
#include "encounter.h"
#include "elite_affixes.h"
#include "item.h"
#include "loot.h"
#include "map.h"
#include "monster.h"
#include "player.h"
#include "progress.h"
#include "skills.h"
#include "sound.h"
#include "stats.h"
#include "textbox.h"
#include "world.h"
#include "world_boss.h"

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
#define T_SHIELD 0x4A
#define T_UP 0x4C
#define T_DIGIT0 0x30

#define PAL_FX 1
#define PAL_GREEN 2
#define PAL_POP 5

#define WINDUP_FRAMES 22
#define HERO_IFRAMES 45

#define HUD_Y 136

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
  uint8_t affix;     // one compact combat modifier for elite monsters
  uint8_t phase;     // 0 normal/elite, 1-4 boss HP phases
  bool world_boss;    // receives the overworld event bonus and reward
  uint8_t move_cd;
  uint8_t atk_cd;
  uint8_t windup;
  uint8_t flash;
  uint8_t stun;
  uint8_t poison;
  uint8_t attacks;
  uint8_t palette;
  uint8_t tile_root;
  uint16_t level;
  uint16_t atk;
  uint16_t def;
  uint16_t mdef;
} Mob;

typedef struct Bolt {
  bool active;
  int8_t x;
  int8_t y;
  int8_t px;
  int8_t py;
  Direction dir;
  uint8_t range;
  uint16_t power;
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
static uint16_t skill_cd[SKILL_COUNT];
static uint8_t iframes;
static uint8_t hero_stun;
static uint8_t hero_poison;
static uint8_t tick;
static uint8_t dodge_cd;
static uint8_t boss_title_timer;
static const char *boss_phase_label;
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

static uint8_t lost_timer;
static uint32_t xp_gain;
static uint16_t encounter_level;
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

static uint16_t rand_vary(uint32_t v) {
  // +/- ~12%
  uint32_t r = v - v / 8 + (v * d32()) / 128;
  if (r < 1) r = 1;
  return r > 60000 ? 60000 : (uint16_t)r;
}

static uint16_t cap16(uint32_t v) {
  return v > 65000 ? 65000 : (uint16_t)v;
}

static void heal_hero(uint16_t amount) {
  const uint32_t hp = (uint32_t)player.hp + amount;
  player.hp = hp > player.max_hp ? player.max_hp : (uint16_t)hp;
  player_hp_and_sp_updated = true;
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

/**
 * Shows a damage number. Values of 1000+ show as thousands with a "K".
 */
static void popup_number(int8_t tx, int8_t ty, uint16_t value, uint8_t pal) {
  uint8_t sx, sy;
  if (!to_screen(tx, ty, 0, -6, &sx, &sy))
    return;
  bool thousands = value >= 1000;
  if (thousands)
    value /= 1000;
  if (value > 999)
    value = 999;
  uint8_t digits = value >= 100 ? 3 : (value >= 10 ? 2 : 1);
  if (thousands && digits > 2)
    digits = 2, value = 99;
  pop_n = digits + (thousands ? 1 : 0);
  uint8_t k = digits;
  do {
    pop_tiles[--k] = T_DIGIT0 + value % 10;
    value /= 10;
  } while (k);
  if (thousands)
    pop_tiles[digits] = 'K' + FONT_OFFSET;
  pop_x = sx + 8 - pop_n * 4;
  pop_y = sy;
  pop_t = 28;
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

static uint8_t entry_ready(uint8_t entry) {
  if (entry >= SKILL_COUNT)
    return 1;
  SkillInfo info;
  skill_get(entry, &info);
  return skill_rank[entry] && !skill_cd[entry] && player.sp >= info.cost;
}

static void draw_hud(void) {
  char buf[24];
  char name[12];
  if (boss_title_timer) {
    if (boss_phase_label) {
      strncpy(buf, boss_phase_label, 20);
      buf[20] = 0;
      core.draw_text(VRAM_WINDOW, buf, 20);
      hud_dirty = false;
      return;
    }
    if (mobs[0].rank == 2 && encounter.monsters[0].name) {
      strncpy(buf, encounter.monsters[0].name, 20);
      buf[20] = 0;
      core.draw_text(VRAM_WINDOW, buf, 20);
      hud_dirty = false;
      return;
    }
    for (uint8_t k = 0; k < MAX_MOBS; k++) {
      if (mobs[k].active && mobs[k].rank == 1) {
        strncpy(buf, elite_affix_title(mobs[k].affix), 20);
        buf[20] = 0;
        core.draw_text(VRAM_WINDOW, buf, 20);
        hud_dirty = false;
        return;
      }
    }
  }
  const uint8_t entry = hotbar[hotbar_sel];
  hotbar_entry_name(entry, name);
  name[4] = 0;
  const uint8_t ready = entry_ready(entry);
  memset(buf, ' ', 20);
  // HP (current/max, or current only when long)
  sprintf(buf, "%u/%u", player.hp, player.max_hp);
  if (strlen(buf) > 9)
    sprintf(buf, "HP%u", player.hp);
  buf[strlen(buf)] = ' ';
  sprintf(buf + 10, "%c%u", is_magic_class() ? 'M' : 'S', player.sp);
  buf[10 + strlen(buf + 10)] = ' ';
  buf[15] = ready ? '>' : '.';
  memcpy(buf + 16, name, strlen(name));
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

  if (!can_show)
    return;

  if (boss_title_timer) {
    boss_title_timer--;
    if (!boss_title_timer)
      hud_dirty = true;
    if (hud_dirty)
      draw_hud();
    if (boss_title_timer)
      return;
  }

  if (!hud_visible) {
    hud_visible = true;
    hud_dirty = true;
    move_win(7, HUD_Y);
  }

  if (!hud_dirty && (tick & 7) == 0 &&
    entry_ready(hotbar[hotbar_sel]) != hud_ready)
    hud_dirty = true;

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
  if (m->rank == 2)
    d = boss_phase_move_delay(d, m->phase);
  return elite_affix_delay(d, m->affix);
}

static uint8_t attack_delay(const Mob *m) {
  uint8_t d = m->kind == AI_SLOW ? 90 : 72;
  if (m->rank == 2)
    d = boss_phase_attack_delay(d, m->phase);
  return elite_affix_delay(d, m->affix);
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
  const bool boss_fight = encounter.is_final_boss;
  const bool dungeon = world_location() == WORLD_LOC_DUNGEON;
  const WorldLocation location = world_location();
  const uint16_t area = world_area_level();
  const bool world_boss = location == WORLD_LOC_ZONE &&
    world_boss_zone_eligible(world_current_zone(), hero_level, area) &&
    encounter.monsters[0].exp_tier == S_TIER;
  const bool named_boss = world_boss || ((location == WORLD_LOC_TOWER ||
    location == WORLD_LOC_RAID) && encounter.monsters[0].exp_tier == S_TIER);

  memset(mobs, 0, sizeof(mobs));
  boss_title_timer = named_boss ? 120 : 0;
  boss_phase_label = NULL;
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
    if (mon->exp_tier == S_TIER || boss_fight)
      m->rank = 2;
    else if ((rand() & 7) == 0 && (area >= 5 || dungeon))
      m->rank = 1;

    m->phase = m->rank == 2 ? BOSS_PHASE_I : BOSS_PHASE_NONE;
    m->world_boss = world_boss && k == 0;
    m->affix = ELITE_AFFIX_NONE;
    if (m->rank == 1) {
      m->affix = (uint8_t)(ELITE_AFFIX_BERSERKER + (rand() & 3));
      if (boss_title_timer < 90)
        boss_title_timer = 90;
    }

    // Monster level: the area level (scripted labyrinth monsters keep the
    // level they were designed for, but never fall far behind the hero).
    uint16_t lvl = area;
    if (dungeon) {
      lvl = mon->level;
      if (hero_level > 20 && lvl < hero_level - 20)
        lvl = hero_level - 20;
    } else if (lvl > 2) {
      lvl = lvl - 1 + (d4() % 3);
    }
    if (lvl > MAX_LEVEL) lvl = MAX_LEVEL;
    m->level = lvl;

    MobStats st;
    monster_stats(mon->type, mon->exp_tier, m->rank, lvl, &st);
    if (m->world_boss) {
      st.atk = world_boss_attack(st.atk);
      st.def = world_boss_defense(st.def);
      st.mdef = world_boss_defense(st.mdef);
    }
    mon->max_hp = st.hp;
    mon->hp = st.hp;
    m->atk = elite_affix_attack(st.atk, m->affix);
    m->def = elite_affix_defense(st.def, m->affix);
    m->mdef = elite_affix_defense(st.mdef, m->affix);

    m->move_cd = elite_affix_delay(20 + count * 10, m->affix);
    m->atk_cd = elite_affix_delay(40, m->affix);
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
  drops = 0;
  encounter_level = area;
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

/**
 * Deals damage from a monster to the hero (defense, dodge, block, shield).
 */
static uint16_t hurt_hero(uint16_t raw, bool magic) {
  if (iframes || player.hp == 0)
    return 0;
  if (debug_god_mode) {
    iframes = HERO_IFRAMES;
    return 0;
  }
  if (d128() < (uint16_t)hs.dodge * 128 / 100) {
    iframes = 20;
    play_sound(sfx_evade);
    return 0;
  }
  const uint16_t defense = magic ? hs.mdef : hs.def;
  uint32_t d = raw > defense / 2 ? raw - defense / 2 : 0;
  if (d < raw / 8 + 1)
    d = raw / 8 + 1;
  if (d128() < (uint16_t)hs.block * 128 / 100)
    d /= 2;
  if (shield_hp) {
    if (d <= shield_hp) {
      shield_hp -= d;
      d = 0;
    } else {
      d -= shield_hp;
      shield_hp = 0;
    }
  }
  iframes = HERO_IFRAMES;
  if (d == 0) {
    play_sound(sfx_mid_powerup);
    return 0;
  }
  const uint16_t applied = d >= player.hp ? player.hp : (uint16_t)d;
  player.hp -= applied;
  popup_number(hero_x(), hero_y(), cap16(d), PAL_FX);
  play_sound(magic ? sfx_monster_attack2 : sfx_monster_attack1);
  player_hp_and_sp_updated = true;
  return applied;
}

static uint16_t hurt_hero_from_mob(Mob *m, uint16_t raw, bool magic) {
  const uint16_t dealt = hurt_hero(raw, magic);
  const uint16_t healed = elite_affix_leech(dealt, m->affix);
  if (healed) {
    Monster *mon = encounter.monsters + m->slot;
    if (mon->hp < mon->max_hp) {
      const uint16_t room = mon->max_hp - mon->hp;
      const uint16_t restored = healed < room ? healed : room;
      mon->hp += restored;
      if (restored)
        popup_number(m->x, m->y, restored, PAL_GREEN);
    }
  }
  return dealt;
}

static uint16_t monster_damage(const Mob *m) {
  const uint16_t attack = m->rank == 2 ?
    boss_phase_damage(m->atk, m->phase) : m->atk;
  return rand_vary(attack);
}

static void kill_mob(Mob *m) {
  Monster *mon = encounter.monsters + m->slot;
  m->active = false;
  mon->active = false;
  hide_spr(SPR_MOB0 + 4 * (m - mobs), 4);
  play_sound(sfx_monster_death);
  fx_start(FX_SPARK, m->x, m->y, DOWN);

  uint32_t xp = monster_xp(m->level, mon->exp_tier, m->rank);
  if (m->world_boss)
    xp = world_boss_reward_xp(xp);
  xp_gain += xp;

  quest_event(QUEST_EVENT_KILL, mon->type);
  if (m->rank == 1)
    quest_event(QUEST_EVENT_ELITE, mon->type);
  if (m->rank == 2) {
    quest_event(QUEST_EVENT_BOSS, mon->type);
    if (world_location() == WORLD_LOC_RAID)
      quest_event(QUEST_EVENT_RAID, raid_state.raid_id);
  }

  drops += loot_roll_kill(m->level, m->rank, msg);
}

/**
 * Deals damage from the hero to a monster.
 */
static void hit_mob(Mob *m, uint16_t power, bool magic, uint8_t stun) {
  Monster *mon = encounter.monsters + m->slot;
  const uint32_t stat = magic ? hs.matk : hs.atk;
  const uint32_t res = magic ? m->mdef : m->def;
  uint32_t raw = stat * power / 100;
  uint32_t d = raw > res / 2 ? raw - res / 2 : 0;
  if (d < raw / 8 + 1)
    d = raw / 8 + 1;
  uint16_t dmg = rand_vary(d);

  const bool crit = d128() < (uint16_t)hs.crit * 128 / 100;
  if (crit)
    dmg = cap16((uint32_t)dmg * (100 + hs.crit_dmg) / 100);

  mon->hp = dmg >= mon->hp ? 0 : mon->hp - dmg;
  popup_number(m->x, m->y, dmg, crit ? PAL_FX : PAL_POP);
  m->flash = 8;
  play_sound(crit ? sfx_monster_critical : (magic ? sfx_magic_missile :
    sfx_melee_attack));

  if (hs.leech)
    heal_hero((uint16_t)((uint32_t)dmg * hs.leech / 100) + 1);

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

static uint8_t hit_area(int8_t cx, int8_t cy, uint8_t radius, uint16_t power,
  bool magic) {
  uint8_t hits = 0;
  Mob *m = mobs;
  for (uint8_t k = 0; k < MAX_MOBS; k++, m++) {
    if (!m->active)
      continue;
    const uint8_t ax = absi(m->x - cx);
    const uint8_t ay = absi(m->y - cy);
    const bool in = radius == 1 ? (ax <= 1 && ay <= 1) : (ax + ay <= radius);
    if (in) {
      hit_mob(m, power, magic, 0);
      hits++;
    }
  }
  return hits;
}

//------------------------------------------------------------------------------
// Bolts
//------------------------------------------------------------------------------

static void fire_bolt(Bolt *b, int8_t x, int8_t y, Direction d, uint16_t power,
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
      if (kind == SK_DRAIN)
        heal_hero(player.max_hp / 12 + 2);
      pbolt.active = false;
    } else if (map_attr_at(pbolt.x, pbolt.y) == MAP_WALL || !pbolt.range) {
      pbolt.active = false;
    }
  }
  draw_bolt(&pbolt, SPR_PBOLT, T_PBOLT,
    pbolt.kind == SK_POISON ? PAL_GREEN : PAL_FX);
}

static void update_enemy_bolt(void) {
  if (!ebolt.active) {
    move_sprite(SPR_EBOLT, 0, 0);
    return;
  }
  if (advance_bolt(&ebolt, 2)) {
    if (hero_occupies(ebolt.x, ebolt.y)) {
      Mob *m = mobs + ebolt_mob;
      const uint16_t dealt = hurt_hero_from_mob(m, monster_damage(m), true);
      if (dealt && encounter.monsters[m->slot].type == MONSTER_MINDFLAYER &&
        (rand() & 3) == 0 && !debug_god_mode)
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

static uint8_t mob_windup(const Mob *m, uint8_t frames) {
  return m->rank == 2 ? boss_phase_windup(frames, m->phase) : frames;
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
  case AI_SLAM: {
    const uint8_t period = m->rank == 2 ?
      boss_phase_slam_period(m->phase) : 3;
    if (m->attacks % period == 0) {
      const uint8_t radius = m->rank == 2 ?
        boss_phase_slam_radius(m->phase) : 2;
      fx_start(FX_NOVA, m->x, m->y, DOWN);
      if (dist <= radius)
        hurt_hero_from_mob(m, cap16((uint32_t)monster_damage(m) * 3 / 2), false);
      return;
    }
    break;
  }
  case AI_BREATH: {
    const uint8_t range = m->rank == 2 ?
      boss_phase_breath_range(m->phase) : 3;
    if ((dx == 0 || dy == 0) && dist <= range && dist > 1) {
      fx_start(FX_NOVA, hero_x(), hero_y(), DOWN);
      hurt_hero_from_mob(m, cap16((uint32_t)monster_damage(m) * 3 / 2), true);
      return;
    }
    break;
  }
  }

  if (dist == 1) {
    const uint16_t dealt = hurt_hero_from_mob(m, monster_damage(m), false);
    if (dealt &&
      (mon->type == MONSTER_ZOMBIE || mon->type == MONSTER_GELATINOUS_CUBE ||
        m->affix == ELITE_AFFIX_VENOMOUS) &&
      (rand() & 3) == 0 && !debug_god_mode)
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

  if (m->rank == 2) {
    const Monster *mon = encounter.monsters + m->slot;
    const uint8_t phase = boss_phase_for_hp(mon->hp, mon->max_hp);
    if (phase > m->phase) {
      m->phase = phase;
      boss_phase_label = boss_phase_title(phase);
      boss_title_timer = 72;
      hud_dirty = true;
    }
  }

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
    uint8_t windup = WINDUP_FRAMES;
    if (m->kind == AI_SLAM) {
      const uint8_t period = m->rank == 2 ?
        boss_phase_slam_period(m->phase) : 3;
      if (m->attacks % period == period - 1)
        windup += 14;
    }
    begin_attack(m, mob_windup(m, windup));
    return;
  }

  if (m->kind == AI_RANGED && aligned && dist >= 2 && dist <= 4 &&
    !m->atk_cd && !ebolt.active) {
    begin_attack(m, mob_windup(m, WINDUP_FRAMES));
    return;
  }

  if (m->kind == AI_BREATH && aligned && dist >= 2 &&
    dist <= (m->rank == 2 ? boss_phase_breath_range(m->phase) : 3) &&
    !m->atk_cd) {
    begin_attack(m, mob_windup(m, WINDUP_FRAMES + 16));
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
  const bool blink = (m->windup &&
    (m->rank == 2 ? (tick & 2) : (tick & 4))) || (m->stun && (tick & 8));
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
  // 24 frames at 0% attack speed, down to 8 frames at 100%.
  atk_cd = 24 - (uint8_t)((uint16_t)hs.aspd * 16 / 100);
  if (atk_cd < 8) atk_cd = 8;

  int8_t tx, ty;
  facing_tile(&tx, &ty);

  if (hero_uses_magic()) {
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

static bool cast_skill(uint8_t k) {
  SkillInfo s;
  if (skill_rank[k] == 0 || skill_cd[k])
    return false;
  skill_get(k, &s);
  if (player.sp < s.cost)
    return false;

  const bool magic = hero_uses_magic();
  const uint8_t rank = s.rank;
  int8_t tx, ty;
  facing_tile(&tx, &ty);

  switch (s.kind) {
  case SK_STRIKE:
  case SK_STUN: {
    fx_start(FX_SLASH, tx, ty, hero_direction);
    Mob *m = mob_at(tx, ty);
    if (m)
      hit_mob(m, s.power, magic, s.kind == SK_STUN ? 110 : 0);
    else
      play_sound(sfx_melee_attack);
    break;
  }
  case SK_CLEAVE:
    fx_start(FX_NOVA, hero_x(), hero_y(), DOWN);
    if (!hit_area(hero_x(), hero_y(), 1, s.power, magic))
      play_sound(sfx_melee_attack);
    break;
  case SK_BOLT:
  case SK_DRAIN:
  case SK_POISON:
    if (pbolt.active)
      return false;
    fire_bolt(&pbolt, hero_x(), hero_y(), hero_direction, s.power, s.kind);
    play_sound(s.kind == SK_POISON ? sfx_poison_spray : sfx_magic_missile);
    break;
  case SK_NOVA:
    fx_start(FX_NOVA, hero_x(), hero_y(), DOWN);
    if (!hit_area(hero_x(), hero_y(), 3, s.power, true))
      play_sound(sfx_magic_missile);
    break;
  case SK_QUAKE: {
    fx_start(FX_NOVA, hero_x(), hero_y(), DOWN);
    Mob *m = mobs;
    bool any = false;
    for (uint8_t i = 0; i < MAX_MOBS; i++, m++) {
      if (m->active) {
        hit_mob(m, s.power, true, 30);
        any = true;
      }
    }
    if (!any)
      play_sound(sfx_big_powerup);
    break;
  }
  case SK_HEAL:
    heal_hero(cap16((uint32_t)player.max_hp * s.power / 100 + 5));
    fx_icon(T_HEAL, PAL_GREEN);
    play_sound(sfx_heal);
    break;
  case SK_RAGE:
    buff_apply(HERO_BUFF_ATK, 20 + 2 * rank, 600 + 30 * rank);
    fx_icon(T_UP, PAL_FX);
    play_sound(sfx_action_surge);
    break;
  case SK_GUARD:
    buff_apply(HERO_BUFF_DEF, 20 + 2 * rank, 600 + 30 * rank);
    buff_apply(HERO_BUFF_SHIELD, 10 + rank, 600);
    fx_icon(T_SHIELD, PAL_POP);
    play_sound(sfx_mid_powerup);
    break;
  case SK_HASTE:
    buff_apply(HERO_BUFF_HASTE, 30 + 2 * rank, 600 + 30 * rank);
    fx_icon(T_UP, PAL_GREEN);
    play_sound(sfx_action_surge);
    break;
  case SK_DASH:
    start_dodge(hero_direction, 3);
    iframes = 40;
    break;
  }

  player.sp -= s.cost;
  skill_cd[k] = s.cooldown;
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
  char num[12];
  arpg_in_combat = false;
  clear_mobs();
  arpg_restore_npcs = true;

  if (!victory)
    return;

  char *p = msg + strlen(msg);
  u32_str(xp_gain, num);
  const uint8_t gained = progress_add_xp(xp_gain);
  if (gained) {
    sprintf(p, "\n+%sXP LEVEL %u!", num, hero_level);
    play_sound(sfx_level_up);
    fx_icon(T_UP, PAL_FX);
  } else {
    sprintf(p, "\n+%sXP", num);
    play_sound(sfx_battle_success);
  }
  player_hp_and_sp_updated = true;

  encounter.victory = true;
  if (encounter.is_final_boss)
    arpg_credits_pending = true;

  // Only interrupt play with a message if something notable happened.
  msg_pending = gained || drops > 0 || encounter.on_victory != NULL ||
    encounter.is_final_boss;
}

static void player_defeated(void) {
  char num[12];
  arpg_in_combat = false;
  clear_mobs();
  arpg_restore_npcs = true;
  encounter.on_victory = NULL;
  encounter.victory = false;
  const uint32_t lost = loot_gold / 10;
  loot_gold -= lost;
  buff_clear();
  u32_str(lost, num);
  sprintf(msg, "You were defeated!\nLost %s gold.", num);
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
}

void arpg_reset_cooldowns(void) BANKED {
  memset(skill_cd, 0, sizeof(skill_cd));
  hero_poison = 0;
  hero_stun = 0;
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

static void update_regen(void) {
  if (player.hp == 0)
    return;
  // Every 2 seconds in combat, every half second outside of it.
  const uint8_t rate = arpg_in_combat ? 127 : 31;
  if ((tick & rate) == 0) {
    if (player.hp < player.max_hp)
      heal_hero(hs.hp_regen);
    if (player.sp < player.max_sp) {
      const uint32_t sp = (uint32_t)player.sp + hs.sp_regen;
      player.sp = sp > player.max_sp ? player.max_sp : (uint16_t)sp;
      player_hp_and_sp_updated = true;
    }
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

  buff_tick();
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

void arpg_hotbar_name(uint8_t entry, char *buf) BANKED {
  hotbar_entry_name(entry, buf);
}

void arpg_skill_name(uint8_t k, char *buf) BANKED {
  skill_name(k, buf);
}

void arpg_skill_info(uint8_t k, char *buf) BANKED {
  skill_info_text(k, buf);
}

uint8_t arpg_skill_level(uint8_t k) BANKED {
  const uint16_t level = skill_unlock_level(k);
  return level > 255 ? 255 : (uint8_t)level;
}
