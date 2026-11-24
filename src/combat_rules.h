#ifndef _COMBAT_RULES_H
#define _COMBAT_RULES_H

#include <stdint.h>

/*
 * Shared damage-aspect resolution. DamageAspect values are single-bit masks
 * from stats.h. Immunity is checked by the caller so it can show the correct
 * battle message; resistance and vulnerability are resolved here.
 *
 * The uint32_t intermediate prevents overflow when vulnerability doubles a
 * large level-scaled hit.
 */
static uint16_t combat_apply_aspect(uint16_t raw_damage,
  uint8_t resist_mask, uint8_t vuln_mask, uint8_t aspect) {
  uint32_t damage = raw_damage;
  if (resist_mask & aspect) {
    damage /= 2;
  } else if (vuln_mask & aspect) {
    damage *= 2;
  }
  return damage > 65535UL ? 65535U : (uint16_t)damage;
}

#endif
