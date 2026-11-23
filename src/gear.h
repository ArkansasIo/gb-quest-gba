#ifndef _GEAR_H
#define _GEAR_H

#include <stdint.h>

#include "item.h"

/**
 * Enumeration of gear slots on the character.
 */
typedef enum GearSlot {
  GEAR_SLOT_WEAPON = 0,
  GEAR_SLOT_ARMOR,
  GEAR_SLOT_SHIELD,
  GEAR_SLOT_ACCESSORY,
  GEAR_SLOT_COUNT,
} GearSlot;

/**
 * Number of weapons and armors in the gear tables.
 */
#define GEAR_WEAPONS_COUNT 6
#define GEAR_ARMORS_COUNT 6

/**
 * A weapon or armor definition: name, title, rank (Elden-Ring style), rarity,
 * and stat bonuses.
 */
typedef struct GearItemDef {
  const char *name;
  const char *title;
  const char *rank;
  uint8_t rarity;
  uint8_t atk;
  uint8_t def;
  uint8_t matk;
  uint8_t mdef;
  uint8_t agl;
} GearItemDef;

/**
 * Weapons available for the WEAPON gear slot.
 */
extern const GearItemDef gear_weapons[GEAR_WEAPONS_COUNT];

/**
 * Armor available for the ARMOR gear slot.
 */
extern const GearItemDef gear_armors[GEAR_ARMORS_COUNT];

/**
 * Stat bonuses granted by equipping an item in a gear slot.
 */
typedef struct GearGrant {
  uint8_t atk;
  uint8_t def;
  uint8_t matk;
  uint8_t mdef;
  uint8_t agl;
} GearGrant;

/**
 * Grants for each gear slot.
 */
extern const GearGrant gear_grants[GEAR_SLOT_COUNT];

/**
 * @param id Id of the item.
 * @return The gear slot the item equips into, or `GEAR_SLOT_COUNT` if the
 * item is not equippable.
 */
GearSlot gear_slot_for_item(ItemId id);

/**
 * Toggles the gear slot: equips the first available equippable item, or
 * unequips the slot if it is occupied.
 */
void gear_toggle(GearSlot slot);

/**
 * Recomputes the player's current stats from base stats and equipped gear.
 */
void update_gear_stats(void);

/**
 * Opens the gear screen.
 */
void gear_open(void);

/**
 * Updates the gear screen.
 */
void gear_update(void);

/**
 * Draws the gear screen.
 */
void gear_draw(void);

#endif
