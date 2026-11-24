#ifndef _PROGRESS_H
#define _PROGRESS_H

/**
 * Character progression for the ARPG systems (bank 18):
 *
 * - Levels 1-720 with a formula based experience curve (32-bit XP).
 * - Six primary attributes (STR, DEX, INT, VIT, END, SPI). Each class grows
 *   them at its own rate per level; players add 3 points per level; gear adds
 *   more.
 * - Derived stats (ATK, MATK, DEF, MDEF, HP, SP) and sub-stats (crit chance,
 *   crit damage, attack speed, dodge, block, cooldown reduction, life steal,
 *   gold find, magic find, HP/SP regeneration).
 * - Timed buffs (attack, defense, haste, regen, crit, shield).
 * - Monster scaling and experience rewards for monster levels 1-720.
 */

#include <gb/gb.h>
#include <stdint.h>
#include <stdbool.h>

#define PROGRESS_BANK 18
#define MAX_LEVEL 720

/* Monster rank values accepted by monster_stats and monster_xp. */
typedef enum MonsterRank {
  MONSTER_RANK_NORMAL = 0,
  MONSTER_RANK_ELITE = 1,
  MONSTER_RANK_BOSS = 2,
  MONSTER_RANK_WORLD_BOSS = 3,
  MONSTER_RANK_ARC_BOSS = 4,
} MonsterRank;

typedef enum Attr {
  ATTR_STR,  // Strength: attack, crit damage, a little defense
  ATTR_DEX,  // Dexterity: crit chance, attack speed, dodge
  ATTR_INT,  // Intelligence: magic attack, cooldown reduction, SP
  ATTR_VIT,  // Vitality: max HP, HP regeneration
  ATTR_END,  // Endurance: defense, block, some HP
  ATTR_SPI,  // Spirit: magic defense, max SP, SP regeneration
  ATTR_COUNT,
} Attr;

/**
 * The hero's computed stats (see `progress_recalc`).
 */
typedef struct HeroStats {
  uint16_t attr[ATTR_COUNT];
  uint16_t atk;
  uint16_t matk;
  uint16_t def;
  uint16_t mdef;
  uint16_t max_hp;
  uint16_t max_sp;
  uint8_t crit;        // % chance
  uint8_t crit_dmg;    // % extra damage on a crit
  uint8_t aspd;        // % faster basic attacks
  uint8_t dodge;       // % chance to avoid a hit
  uint8_t block;       // % chance to halve a hit
  uint8_t cdr;         // % cooldown reduction
  uint8_t leech;       // % of damage dealt returned as HP
  uint16_t gold_find;  // % bonus gold
  uint16_t magic_find; // % better item rarity
  uint16_t hp_regen;   // HP per 2 seconds
  uint16_t sp_regen;   // SP per 2 seconds
} HeroStats;

typedef enum BuffType {
  HERO_BUFF_NONE,
  HERO_BUFF_ATK,     // +power % attack and magic attack
  HERO_BUFF_DEF,     // +power % defense and magic defense
  HERO_BUFF_HASTE,   // +power % attack speed, faster cooldowns
  HERO_BUFF_REGEN,   // +power % of max HP every 2 seconds
  HERO_BUFF_CRIT,    // +power % crit chance
  HERO_BUFF_SHIELD,  // absorbs damage (power % of max HP)
  HERO_BUFF_TYPE_COUNT,
} BuffType;

typedef struct Buff {
  uint8_t type;
  uint8_t power;
  uint16_t timer;
} Buff;

#define MAX_BUFFS 6

/**
 * Computed monster stats for a level.
 */
typedef struct MobStats {
  uint16_t hp;
  uint16_t atk;
  uint16_t def;
  uint16_t mdef;
} MobStats;

extern uint16_t hero_level;
extern uint32_t hero_xp;
extern HeroStats hs;
extern uint16_t attr_alloc[ATTR_COUNT];
extern uint16_t stat_points;
extern uint16_t skill_points;
extern Buff active_buffs[MAX_BUFFS];
extern uint16_t shield_hp;
extern bool debug_god_mode;

/**
 * Resets progression for a new character (level 1).
 */
void progress_new_game(void) BANKED;

/**
 * @return Experience needed to advance from `level` to the next level.
 */
uint32_t xp_to_next(uint16_t level) BANKED;

/**
 * Adds experience. @return The number of levels gained.
 */
uint8_t progress_add_xp(uint32_t xp) BANKED;

/**
 * Sets the hero level directly (debug), granting points for gained levels.
 */
void progress_set_level(uint16_t level) BANKED;

/**
 * Recomputes all hero stats from level, attributes, gear and buffs, and
 * mirrors them into the legacy `player` struct.
 */
void progress_recalc(void) BANKED;

/**
 * @return The class/level part of an attribute (without points or gear).
 */
uint16_t attr_base(uint8_t attr) BANKED;

/**
 * Refunds every allocated attribute point (debug / respec).
 */
void progress_respec(void) BANKED;

/**
 * @return `true` if the hero fights with magic (bolts) by default.
 */
bool hero_uses_magic(void) BANKED;

/**
 * Applies (or refreshes) a timed buff.
 */
void buff_apply(uint8_t type, uint8_t power, uint16_t frames) BANKED;

/**
 * Advances buff timers by one frame. Recomputes stats when a buff ends.
 */
void buff_tick(void) BANKED;

/**
 * @return The power of an active buff, or 0.
 */
uint8_t buff_power(uint8_t type) BANKED;

/**
 * Removes every buff.
 */
void buff_clear(void) BANKED;

/**
 * Writes a short label for active buff `index` ("ATK+20 12s"), or "".
 */
void buff_label(uint8_t index, char *buf) BANKED;

/**
 * Computes stats for a monster.
 * @param type MonsterType.
 * @param tier PowerTier.
 * @param rank 0 normal, 1 elite, 2 boss.
 */
void monster_stats(uint8_t type, uint8_t tier, uint8_t rank, uint16_t level,
  MobStats *out) BANKED;

/**
 * @return Experience for defeating a monster (includes level difference).
 */
uint32_t monster_xp(uint16_t level, uint8_t tier, uint8_t rank) BANKED;

/**
 * Writes an unsigned 32-bit number as decimal text.
 */
void u32_str(uint32_t value, char *buf) BANKED;

#endif
