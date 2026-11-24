#ifndef _SKILLS_H
#define _SKILLS_H

/**
 * Skill core (bank 26): six active skills per class, ranks 1-20, unlock
 * levels across 1-720, SP costs and cooldowns that scale with level, rank,
 * cooldown reduction and haste, plus the four slot hotbar.
 */

#include <gb/gb.h>
#include <stdint.h>
#include <stdbool.h>

#define SKILLS_BANK 26
#define SKILL_COUNT 6
#define SKILL_MAX_RANK 20

/**
 * Hotbar entries beyond the six class skills.
 */
#define HOTBAR_EMPTY 0xFF
#define HOTBAR_POTION 6
#define HOTBAR_ETHER 7
#define HOTBAR_SLOTS 4

typedef enum SkillKind {
  SK_STRIKE,   // hit the tile ahead
  SK_CLEAVE,   // hit all adjacent foes
  SK_BOLT,     // magic projectile
  SK_NOVA,     // magic blast around the hero
  SK_HEAL,     // restore HP
  SK_RAGE,     // attack buff
  SK_GUARD,    // defense buff + shield
  SK_HASTE,    // haste buff
  SK_STUN,     // hit + stun
  SK_DRAIN,    // bolt that heals
  SK_POISON,   // bolt that poisons
  SK_DASH,     // dash with invulnerability
  SK_QUAKE,    // hit every foe
  SK_KIND_COUNT,
} SkillKind;

/**
 * Computed values for one skill at its current rank.
 */
typedef struct SkillInfo {
  uint8_t kind;
  uint8_t rank;
  uint16_t power;     // % of attack
  uint16_t cost;      // SP
  uint16_t cooldown;  // frames
} SkillInfo;

extern uint8_t skill_rank[SKILL_COUNT];
extern uint8_t hotbar[HOTBAR_SLOTS];
extern uint8_t hotbar_sel;

/**
 * Resets skills and the hotbar for a new character.
 */
void skills_new_game(void) BANKED;

/**
 * Fills `out` with the current values of class skill `k`.
 */
void skill_get(uint8_t k, SkillInfo *out) BANKED;

/**
 * Writes the name of class skill `k` (max 10 chars).
 */
void skill_name(uint8_t k, char *buf) BANKED;

/**
 * Writes a short description of class skill `k` (max 18 chars).
 */
void skill_info_text(uint8_t k, char *buf) BANKED;

/**
 * @return The hero level needed to learn class skill `k`.
 */
uint16_t skill_unlock_level(uint8_t k) BANKED;

/**
 * @return The highest rank skills can currently reach (grows with level).
 */
uint8_t skill_max_rank(void) BANKED;

/**
 * Tries to learn / rank up skill `k` with a skill point.
 * @return `true` on success.
 */
bool skill_rank_up(uint8_t k) BANKED;

/**
 * Writes the display name of a hotbar entry (skill or consumable).
 */
void hotbar_entry_name(uint8_t entry, char *buf) BANKED;

#endif
