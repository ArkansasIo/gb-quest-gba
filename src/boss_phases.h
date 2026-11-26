#ifndef _BOSS_PHASES_H
#define _BOSS_PHASES_H

#include <stdint.h>

/* Compact, derived phase state; Mob stores only this one-byte value. */
typedef enum BossPhase {
  BOSS_PHASE_NONE = 0,
  BOSS_PHASE_I = 1,
  BOSS_PHASE_II = 2,
  BOSS_PHASE_III = 3,
  BOSS_PHASE_IV = 4
} BossPhase;

/* HP thresholds are inclusive: 75%, 50%, then 25%. */
static uint8_t boss_phase_for_hp(uint16_t hp, uint16_t max_hp) {
  const uint32_t hp_quarters = (uint32_t)hp * 4UL;
  const uint32_t maximum = max_hp;
  if (!max_hp || hp >= max_hp)
    return BOSS_PHASE_I;
  if (hp_quarters <= maximum)
    return BOSS_PHASE_IV;
  if (hp_quarters <= maximum * 2UL)
    return BOSS_PHASE_III;
  if (hp_quarters <= maximum * 3UL)
    return BOSS_PHASE_II;
  return BOSS_PHASE_I;
}

/* Boss damage rises modestly in the last two phases; preserve uint16_t range. */
static uint16_t boss_phase_damage(uint16_t base, uint8_t phase) {
  uint32_t scaled = base;
  if (phase >= BOSS_PHASE_IV)
    scaled += scaled / 4;
  else if (phase >= BOSS_PHASE_III)
    scaled += scaled / 10;
  return scaled > 65535UL ? 65535U : (uint16_t)scaled;
}

static uint8_t boss_phase_move_delay(uint8_t frames, uint8_t phase) {
  if (phase >= BOSS_PHASE_III)
    frames >>= 1;
  else if (phase >= BOSS_PHASE_II)
    frames -= frames / 8;
  return frames ? frames : 1;
}

static uint8_t boss_phase_attack_delay(uint8_t frames, uint8_t phase) {
  if (phase >= BOSS_PHASE_IV)
    frames -= frames / 2;
  else if (phase >= BOSS_PHASE_III)
    frames -= frames / 3;
  else if (phase >= BOSS_PHASE_II)
    frames -= frames / 8;
  return frames ? frames : 1;
}

/* Longer windups offset the increased pressure in later phases. */
static uint8_t boss_phase_windup(uint8_t frames, uint8_t phase) {
  uint16_t total = frames;
  if (phase >= BOSS_PHASE_IV)
    total += 8;
  else if (phase >= BOSS_PHASE_III)
    total += 4;
  else if (phase >= BOSS_PHASE_II)
    total += 2;
  return total > 255U ? 255U : (uint8_t)total;
}

static uint8_t boss_phase_slam_period(uint8_t phase) {
  if (phase >= BOSS_PHASE_IV)
    return 1;
  if (phase >= BOSS_PHASE_II)
    return 2;
  return 3;
}

static uint8_t boss_phase_slam_radius(uint8_t phase) {
  return phase >= BOSS_PHASE_IV ? 3 : 2;
}

static uint8_t boss_phase_breath_range(uint8_t phase) {
  return phase >= BOSS_PHASE_IV ? 4 : 3;
}

static const char *boss_phase_title(uint8_t phase) {
  if (phase >= BOSS_PHASE_IV)
    return "Boss Final Phase";
  if (phase >= BOSS_PHASE_III)
    return "Boss Phase III";
  if (phase >= BOSS_PHASE_II)
    return "Boss Phase II";
  return "Boss Phase I";
}

#endif
