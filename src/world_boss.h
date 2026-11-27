#ifndef _WORLD_BOSS_H
#define _WORLD_BOSS_H

#include <stdbool.h>
#include <stdint.h>

/* Wyrmstone is a level-600 overworld zone. The event is repeatable on re-entry. */
#define WORLD_BOSS_ZONE 68
#define WORLD_BOSS_LEVEL_GAP 80
#define WORLD_BOSS_MIN_AREA_LEVEL 200

static inline bool world_boss_zone_eligible(uint8_t zone, uint16_t hero_level,
  uint16_t area_level) {
  if (zone != WORLD_BOSS_ZONE || area_level < WORLD_BOSS_MIN_AREA_LEVEL)
    return false;
  return hero_level >= area_level - WORLD_BOSS_LEVEL_GAP;
}

/* World-boss bonuses use wide intermediates and retain legacy stat caps. */
static inline uint16_t world_boss_attack(uint16_t base) {
  const uint32_t scaled = (uint32_t)base * 3UL / 2UL;
  return scaled > 60000UL ? 60000U : (uint16_t)scaled;
}

static inline uint16_t world_boss_defense(uint16_t base) {
  const uint32_t scaled = (uint32_t)base * 5UL / 4UL;
  return scaled > 60000UL ? 60000U : (uint16_t)scaled;
}

/* The extra reward doubles XP without wrapping the 32-bit progression value. */
static inline uint32_t world_boss_reward_xp(uint32_t base) {
  return base > 0x7FFFFFFFUL ? 0xFFFFFFFFUL : base * 2UL;
}

#endif
