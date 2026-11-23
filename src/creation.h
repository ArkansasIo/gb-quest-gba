#ifndef _CREATION_H
#define _CREATION_H

#include <stdint.h>

/**
 * Enumeration of the races available at character creation.
 */
typedef enum Race {
  RACE_HUMAN = 0,
  RACE_ELF,
  RACE_DWARF,
  RACE_ORC,
  RACE_HALFLING,
  RACE_GNOME,
  RACE_FAIRY,
  RACE_CENTAUR,
  RACE_DRAGONBORN,
  RACE_COUNT,
} Race;

/**
 * Data representation of a race: name and base stat bonuses applied when the
 * character is created.
 */
typedef struct RaceDef {
  /**
   * Name of the race.
   */
  const char *name;
  /**
   * Bonus to maximum HP.
   */
  uint8_t hp_bonus;
  /**
   * Bonus to attack.
   */
  uint8_t atk_bonus;
  /**
   * Bonus to defense.
   */
  uint8_t def_bonus;
  /**
   * Bonus to agility.
   */
  uint8_t agl_bonus;
  /**
   * Bonus to magical attack.
   */
  uint8_t matk_bonus;
  /**
   * Bonus to magical defense.
   */
  uint8_t mdef_bonus;
} RaceDef;

/**
 * All races.
 */
extern const RaceDef races[RACE_COUNT];

#endif
