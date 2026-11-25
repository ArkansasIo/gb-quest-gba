#ifndef _ELITE_AFFIXES_H
#define _ELITE_AFFIXES_H

#include <stdint.h>

/* One compact affix is stored per elite Mob instance. */
typedef enum EliteAffix {
  ELITE_AFFIX_NONE,
  ELITE_AFFIX_BERSERKER,
  ELITE_AFFIX_IRONHIDE,
  ELITE_AFFIX_VENOMOUS,
  ELITE_AFFIX_LEECHING,
  ELITE_AFFIX_COUNT
} EliteAffix;

static uint16_t elite_affix_attack(uint16_t value, uint8_t affix) {
  uint32_t scaled = value;
  if (affix == ELITE_AFFIX_BERSERKER)
    scaled += scaled / 4;
  return scaled > 65535UL ? 65535U : (uint16_t)scaled;
}

static uint16_t elite_affix_defense(uint16_t value, uint8_t affix) {
  uint32_t scaled = value;
  if (affix == ELITE_AFFIX_IRONHIDE)
    scaled += scaled / 4;
  return scaled > 65535UL ? 65535U : (uint16_t)scaled;
}

static uint8_t elite_affix_delay(uint8_t frames, uint8_t affix) {
  if (affix == ELITE_AFFIX_BERSERKER) {
    frames -= frames / 4;
    if (frames == 0)
      frames = 1;
  }
  return frames;
}

static uint16_t elite_affix_leech(uint16_t damage, uint8_t affix) {
  return affix == ELITE_AFFIX_LEECHING ? damage / 5 : 0;
}

static const char *elite_affix_title(uint8_t affix) {
  switch (affix) {
  case ELITE_AFFIX_BERSERKER: return "Berserker Elite";
  case ELITE_AFFIX_IRONHIDE: return "Ironhide Elite";
  case ELITE_AFFIX_VENOMOUS: return "Venomous Elite";
  case ELITE_AFFIX_LEECHING: return "Leeching Elite";
  default: return "Elite";
  }
}

#endif
