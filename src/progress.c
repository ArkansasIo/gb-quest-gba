#pragma bank 18

/**
 * Character progression: levels 1-720, attributes, derived stats, sub-stats,
 * buffs and monster scaling. See progress.h.
 */

#include <gb/gb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "progress.h"
#include "loot.h"
#include "monster.h"
#include "player.h"
#include "stats.h"

uint16_t hero_level = 1;
uint32_t hero_xp;
HeroStats hs;
uint16_t attr_alloc[ATTR_COUNT];
uint16_t stat_points;
uint16_t skill_points;
Buff active_buffs[MAX_BUFFS];
uint16_t shield_hp;
bool debug_god_mode;

/**
 * Attribute growth per level (x10) for each class:
 * STR, DEX, INT, VIT, END, SPI.
 */
static const uint8_t class_growth[CLASS_COUNT][ATTR_COUNT] = {
  { 6, 8, 14, 9, 8, 15 },    // Druid
  { 16, 9, 4, 13, 13, 5 },   // Fighter
  { 12, 16, 5, 10, 9, 8 },   // Monk
  { 4, 8, 17, 7, 6, 14 },    // Sorcerer
  { 9, 17, 6, 8, 7, 6 },     // Rogue
  { 10, 16, 6, 9, 8, 7 },    // Ranger
  { 15, 7, 4, 14, 15, 5 },   // Knight
  { 4, 7, 18, 7, 6, 14 },    // Wizard
  { 14, 7, 8, 13, 14, 9 },   // Paladin
  { 8, 6, 13, 11, 10, 15 },  // Cleric
  { 7, 12, 12, 9, 7, 12 },   // Bard
  { 5, 7, 17, 9, 7, 13 },    // Necromancer
  { 11, 18, 5, 8, 6, 6 },    // Assassin
  { 15, 12, 5, 12, 11, 5 },  // Dragoon
  { 9, 8, 14, 11, 9, 13 },   // Shaman
  { 5, 7, 18, 8, 7, 13 },    // Warlock
  { 12, 17, 5, 9, 7, 6 },    // Stalker
  { 18, 9, 3, 14, 11, 4 },   // Marauder
};

static uint8_t hero_class(void) {
  return player.player_class < CLASS_COUNT ? player.player_class : 0;
}

static uint16_t cap16(uint32_t v, uint16_t max) {
  return v > max ? max : (uint16_t)v;
}

static uint8_t cap8(uint16_t v, uint8_t max) {
  return v > max ? max : (uint8_t)v;
}

//------------------------------------------------------------------------------
// Levels
//------------------------------------------------------------------------------

uint32_t xp_to_next(uint16_t level) BANKED {
  const uint32_t l = level;
  return 20 + 8 * l + (l * l) / 2;
}

static void grant_level(void) {
  hero_level++;
  stat_points += 3;
  skill_points += 1;
}

uint8_t progress_add_xp(uint32_t xp) BANKED {
  uint8_t gained = 0;
  if (hero_level >= MAX_LEVEL) {
    hero_xp = 0;
    return 0;
  }
  hero_xp += xp;
  while (hero_level < MAX_LEVEL && hero_xp >= xp_to_next(hero_level)) {
    hero_xp -= xp_to_next(hero_level);
    grant_level();
    if (gained < 255) gained++;
  }
  if (hero_level >= MAX_LEVEL)
    hero_xp = 0;
  if (gained) {
    progress_recalc();
    player.hp = player.max_hp;
    player.sp = player.max_sp;
  }
  return gained;
}

void progress_set_level(uint16_t level) BANKED {
  if (level < 1) level = 1;
  if (level > MAX_LEVEL) level = MAX_LEVEL;
  while (hero_level < level)
    grant_level();
  if (level < hero_level)
    hero_level = level;
  hero_xp = 0;
  progress_recalc();
  player.hp = player.max_hp;
  player.sp = player.max_sp;
}

void progress_new_game(void) BANKED {
  hero_level = 1;
  hero_xp = 0;
  memset(attr_alloc, 0, sizeof(attr_alloc));
  stat_points = 0;
  skill_points = 0;
  shield_hp = 0;
  debug_god_mode = false;
  buff_clear();
}

void progress_respec(void) BANKED {
  for (uint8_t a = 0; a < ATTR_COUNT; a++) {
    stat_points += attr_alloc[a];
    attr_alloc[a] = 0;
  }
  progress_recalc();
}

//------------------------------------------------------------------------------
// Stats
//------------------------------------------------------------------------------

uint16_t attr_base(uint8_t attr) BANKED {
  const uint16_t l = hero_level ? hero_level : 1;
  return 5 + (uint16_t)(((uint32_t)(l - 1) * class_growth[hero_class()][attr]) / 10);
}

bool hero_uses_magic(void) BANKED {
  const uint8_t *g = class_growth[hero_class()];
  return g[ATTR_INT] > g[ATTR_STR];
}

void progress_recalc(void) BANKED {
  uint16_t g[AFF_COUNT];
  loot_gear_totals(g);

  const uint16_t l = hero_level ? hero_level : 1;
  for (uint8_t a = 0; a < ATTR_COUNT; a++) {
    uint32_t v = (uint32_t)attr_base(a) + attr_alloc[a] + g[AFF_STR + a];
    hs.attr[a] = cap16(v, 60000);
  }
  const uint32_t str = hs.attr[ATTR_STR];
  const uint32_t dex = hs.attr[ATTR_DEX];
  const uint32_t intl = hs.attr[ATTR_INT];
  const uint32_t vit = hs.attr[ATTR_VIT];
  const uint32_t endu = hs.attr[ATTR_END];
  const uint32_t spi = hs.attr[ATTR_SPI];

  uint32_t atk = str * 2 + dex / 2 + g[AFF_ATK];
  uint32_t matk = intl * 2 + spi / 2 + g[AFF_MATK];
  uint32_t def = endu * 2 + str / 2 + g[AFF_DEF];
  uint32_t mdef = spi * 2 + intl / 2 + g[AFF_MDEF];

  const uint8_t batk = buff_power(HERO_BUFF_ATK);
  const uint8_t bdef = buff_power(HERO_BUFF_DEF);
  atk += atk * batk / 100;
  matk += matk * batk / 100;
  def += def * bdef / 100;
  mdef += mdef * bdef / 100;

  hs.atk = cap16(atk, 60000);
  hs.matk = cap16(matk, 60000);
  hs.def = cap16(def, 60000);
  hs.mdef = cap16(mdef, 60000);
  hs.max_hp = cap16(30 + vit * 8 + endu * 2 + (uint32_t)l * 4 + g[AFF_HP], 65000);
  hs.max_sp = cap16(10 + spi * 2 + intl + l + g[AFF_SP], 9999);

  hs.crit = cap8(5 + dex / 40 + g[AFF_CRIT] + buff_power(HERO_BUFF_CRIT), 75);
  hs.crit_dmg = cap8(50 + str / 30 + g[AFF_CDMG], 250);
  hs.aspd = cap8(dex / 30 + g[AFF_ASPD] + buff_power(HERO_BUFF_HASTE), 100);
  hs.dodge = cap8(dex / 60 + g[AFF_DODGE], 40);
  hs.block = cap8(endu / 60 + g[AFF_BLOCK], 50);
  hs.cdr = cap8(intl / 60 + g[AFF_CDR], 40);
  hs.leech = cap8(g[AFF_LEECH], 25);
  hs.gold_find = cap16(g[AFF_GOLD], 500);
  hs.magic_find = cap16(g[AFF_MF], 500);
  hs.hp_regen = cap16(1 + vit / 25 + g[AFF_REGEN] +
    ((uint32_t)hs.max_hp * buff_power(HERO_BUFF_REGEN)) / 100, 9999);
  hs.sp_regen = cap16(1 + spi / 30, 999);

  // Mirror into the legacy player struct (8-bit stats are capped).
  player.max_hp = hs.max_hp;
  player.max_sp = hs.max_sp;
  if (player.hp > player.max_hp) player.hp = player.max_hp;
  if (player.sp > player.max_sp) player.sp = player.max_sp;
  player.atk = cap8(hs.atk > 255 ? 255 : hs.atk, 255);
  player.def = cap8(hs.def > 255 ? 255 : hs.def, 255);
  player.matk = cap8(hs.matk > 255 ? 255 : hs.matk, 255);
  player.mdef = cap8(hs.mdef > 255 ? 255 : hs.mdef, 255);
  player.agl = cap8(dex > 255 ? 255 : (uint8_t)dex, 255);
  player.level = l > 99 ? 99 : (uint8_t)l;
}

//------------------------------------------------------------------------------
// Buffs
//------------------------------------------------------------------------------

uint8_t buff_power(uint8_t type) BANKED {
  for (uint8_t k = 0; k < MAX_BUFFS; k++)
    if (active_buffs[k].type == type && active_buffs[k].timer)
      return active_buffs[k].power;
  return 0;
}

void buff_apply(uint8_t type, uint8_t power, uint16_t frames) BANKED {
  Buff *slot = NULL;
  for (uint8_t k = 0; k < MAX_BUFFS; k++) {
    Buff *b = active_buffs + k;
    if (b->type == type && b->timer) {
      slot = b;
      break;
    }
    if (!slot && (b->type == HERO_BUFF_NONE || !b->timer))
      slot = b;
  }
  if (!slot)
    slot = active_buffs;  // replace the oldest slot
  slot->type = type;
  if (power > slot->power || slot->timer == 0)
    slot->power = power;
  slot->timer = frames;
  if (type == HERO_BUFF_SHIELD)
    shield_hp = cap16(((uint32_t)hs.max_hp * power) / 100, 65000);
  progress_recalc();
}

void buff_tick(void) BANKED {
  bool expired = false;
  for (uint8_t k = 0; k < MAX_BUFFS; k++) {
    Buff *b = active_buffs + k;
    if (!b->timer)
      continue;
    if (--b->timer == 0) {
      if (b->type == HERO_BUFF_SHIELD)
        shield_hp = 0;
      b->type = HERO_BUFF_NONE;
      b->power = 0;
      expired = true;
    }
  }
  if (shield_hp == 0) {
    // Shield broke early: drop the buff.
    for (uint8_t k = 0; k < MAX_BUFFS; k++)
      if (active_buffs[k].type == HERO_BUFF_SHIELD && active_buffs[k].timer) {
        active_buffs[k].timer = 0;
        active_buffs[k].type = HERO_BUFF_NONE;
      }
  }
  if (expired)
    progress_recalc();
}

void buff_clear(void) BANKED {
  memset(active_buffs, 0, sizeof(active_buffs));
  shield_hp = 0;
}

void buff_label(uint8_t index, char *buf) BANKED {
  static const char *const names[HERO_BUFF_TYPE_COUNT] = {
    "", "ATK", "DEF", "HASTE", "REGEN", "CRIT", "SHIELD"
  };
  const Buff *b = active_buffs + index;
  if (index >= MAX_BUFFS || !b->timer || b->type >= HERO_BUFF_TYPE_COUNT) {
    buf[0] = 0;
    return;
  }
  sprintf(buf, "%s+%u %us", names[b->type], b->power, b->timer / 60);
}

//------------------------------------------------------------------------------
// Monsters
//------------------------------------------------------------------------------

/**
 * HP and attack multipliers (%) for each monster type.
 */
static const uint8_t type_hp[12] = {
  70, 80, 120, 110, 140, 160, 110, 80, 150, 110, 160, 220
};
static const uint8_t type_atk[12] = {
  80, 90, 90, 110, 120, 90, 130, 120, 140, 140, 150, 170
};
static const uint8_t tier_mult[4] = { 100, 115, 130, 160 };
static const uint16_t tier_xp[4] = { 100, 120, 150, 300 };

void monster_stats(uint8_t type, uint8_t tier, uint8_t rank, uint16_t level,
  MobStats *out) BANKED {
  const uint32_t l = level ? level : 1;
  if (type >= 12) type = 0;
  if (tier > 3) tier = 3;

  uint32_t hp = 30 + 10 * l + (l * l) / 40;
  hp = hp * type_hp[type] / 100 * tier_mult[tier] / 100;
  uint32_t atk = (10 + 5 * l) * type_atk[type] / 100 * tier_mult[tier] / 100;
  uint32_t def = (2 + 2 * l) * tier_mult[tier] / 100;

  if (rank == 1) {
    hp *= 2;
    atk = atk * 5 / 4;
  } else if (rank == MONSTER_RANK_BOSS) {
    hp *= 3;
    atk = atk * 13 / 10;
  } else if (rank == MONSTER_RANK_WORLD_BOSS) {
    /* Public encounter target: durable against a coordinated raid group. */
    hp *= 12;
    atk = atk * 16 / 10;
    def = def * 12 / 10;
  } else if (rank == MONSTER_RANK_ARC_BOSS) {
    /* Campaign pinnacle encounter: tuned for an organized 8-12 player raid. */
    hp *= 20;
    atk = atk * 18 / 10;
    def = def * 14 / 10;
  }

  out->hp = cap16(hp, 65000);
  out->atk = cap16(atk, 60000);
  out->def = cap16(def, 60000);
  out->mdef = cap16(def * 9 / 10, 60000);
}

uint32_t monster_xp(uint16_t level, uint8_t tier, uint8_t rank) BANKED {
  if (tier > 3) tier = 3;
  uint32_t xp = xp_to_next(level) / (8 + level / 40);
  xp = xp * tier_xp[tier] / 100;
  if (rank == MONSTER_RANK_ELITE) xp *= 2;
  else if (rank == MONSTER_RANK_BOSS) xp *= 4;
  else if (rank == MONSTER_RANK_WORLD_BOSS) xp *= 12;
  else if (rank == MONSTER_RANK_ARC_BOSS) xp *= 20;
  if (hero_level > level + 15)
    xp /= 5;
  return xp ? xp : 1;
}

void u32_str(uint32_t value, char *buf) BANKED {
  ultoa(value, buf, 10);
}
