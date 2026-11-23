#pragma bank 21

#include <stdio.h>

#include "gear.h"
#include "player.h"
#include "core.h"
#include "sound.h"
#include "map.h"

const GearGrant gear_grants[GEAR_SLOT_COUNT] = {
  { 0, 0, 0, 0, 0 }, // GEAR_SLOT_WEAPON
  { 0, 2, 0, 0, 0 }, // GEAR_SLOT_ARMOR
  { 0, 0, 0, 0, 1 }, // GEAR_SLOT_SHIELD
  { 0, 0, 0, 1, 0 }, // GEAR_SLOT_ACCESSORY
};

const GearItemDef gear_weapons[GEAR_WEAPONS_COUNT] = {
  { "Longsword", "Blade of the Iron Guard", "Sharp", 0, 6, 0, 0, 0, 0 },
  { "Mace", "Morning Star", "Blessed", 1, 5, 0, 0, 0, 0 },
  { "Greataxe", "Executioner", "Heavy", 2, 8, 0, 0, 0, 0 },
  { "Scepter", "Amber Scepter", "Arcane", 3, 2, 4, 0, 0, 0 },
  { "Dagger", "Poison Fang", "Ancient", 4, 3, 0, 0, 5, 0 },
  { "Bow", "Longbow", "Expert", 5, 4, 0, 0, 0, 0 },
};

const GearItemDef gear_armors[GEAR_ARMORS_COUNT] = {
  { "Cloth Robe", "Silk of the festivities", "Blessed", 0, 0, 2, 0, 0, 0 },
  { "Leather Armor", "Cured Hide", "Common", 1, 0, 3, 0, 0, 0 },
  { "Chain Mail", "Riveted Mail", "Sharp", 2, 0, 5, 0, 0, 0 },
  { "Plate Armor", "Black Iron Plate", "Heavy", 3, 0, 8, 0, 0, 0 },
  { "Shield", "Round Shield", "Ancient", 4, 0, 0, 0, 2, 0 },
  { "Vambraces", "Gauntlets of the Wolf", "Galeforce", 5, 0, 0, 0, 0, 3 },
};

GearSlot gear_slot_for_item(ItemId id) {
  switch (id) {
  case ITEM_ATK_UP:
    return GEAR_SLOT_WEAPON;
  case ITEM_DEF_UP:
    return GEAR_SLOT_ARMOR;
  default:
    return GEAR_SLOT_COUNT;
  }
}

/**
 * Applies the stat bonus of every equipped gear item to the player's current
 * stats.
 */
void update_gear_stats(void) {
  uint8_t atk = 0;
  uint8_t def = 0;
  uint8_t matk = 0;
  uint8_t mdef = 0;
  uint8_t agl = 0;

  for (uint8_t k = 0; k < GEAR_SLOT_COUNT; k++) {
    if (player.gear[k] != ITEM_INVALID) {
      atk += gear_grants[k].atk;
      def += gear_grants[k].def;
      matk += gear_grants[k].matk;
      mdef += gear_grants[k].mdef;
      agl += gear_grants[k].agl;
    }
  }

  player.atk = player.atk_base + atk;
  player.def = player.def_base + def;
  player.matk = player.matk_base + matk;
  player.mdef = player.mdef_base + mdef;
  player.agl = player.agl_base + agl;
}

/**
 * Toggles a gear slot between equipped and unequipped.
 */
void gear_toggle(GearSlot slot) {
  if (player.gear[slot] != ITEM_INVALID) {
    player.gear[slot] = ITEM_INVALID;
    play_sound(sfx_menu_move);
  } else {
    for (uint8_t i = 0; i < INVENTORY_LEN; i++) {
      if (inventory[i].quantity == 0)
        continue;
      if (gear_slot_for_item(inventory[i].id) == slot) {
        player.gear[slot] = inventory[i].id;
        play_sound(sfx_menu_move);
        break;
      }
    }
  }
  update_gear_stats();
}

/**
 * @return The name of the gear slot.
 */
static const char *gear_slot_name(GearSlot slot) {
  switch (slot) {
  case GEAR_SLOT_WEAPON: return "WEAPON";
  case GEAR_SLOT_ARMOR: return "ARMOR";
  case GEAR_SLOT_SHIELD: return "SHIELD";
  case GEAR_SLOT_ACCESSORY: return "ACCESSORY";
  }
  return "";
}

/**
 * Draws a single gear slot line.
 */
static void gear_draw_slot(GearSlot slot) {
  char buf[32];

  if (slot == GEAR_SLOT_WEAPON) {
    const GearItemDef *w = &gear_weapons[0];
    if (player.gear[slot] != ITEM_INVALID)
      sprintf(buf, "WEAPON: ATK_UP (%s)", inventory[player.gear[slot]].name);
    else
      sprintf(buf, "WEAPON: %s - %s (%s)", w->name, w->title, w->rank);
  } else if (slot == GEAR_SLOT_ARMOR) {
    const GearItemDef *a = &gear_armors[0];
    if (player.gear[slot] != ITEM_INVALID)
      sprintf(buf, "ARMOR: DEF_UP (%s)", inventory[player.gear[slot]].name);
    else
      sprintf(buf, "ARMOR: %s - %s (%s)", a->name, a->title, a->rank);
  } else if (player.gear[slot] != ITEM_INVALID) {
    sprintf(buf, "%s: %s", gear_slot_name(slot), inventory[player.gear[slot]].name);
  } else {
    sprintf(buf, "%s: (none)", gear_slot_name(slot));
  }

  core.draw_text(VRAM_WINDOW_XY(2, slot + 1), buf, 28);
}

/**
 * Draws the gear screen contents.
 */
void gear_draw(void) {
  for (uint8_t k = 0; k < GEAR_SLOT_COUNT; k++)
    gear_draw_slot((GearSlot)k);
}

/**
 * Opens the gear screen.
 */
void gear_open(void) {
  game_state = GAME_STATE_GEAR;
  gear_draw();
  update_gear_stats();
}

/**
 * Updates the gear screen: moves the cursor, toggles gear, or shows details.
 */
void gear_update(void) {
  static uint8_t gear_cursor = 0;

  if (was_pressed(J_UP)) {
    gear_cursor = (gear_cursor + GEAR_SLOT_COUNT - 1) % GEAR_SLOT_COUNT;
    play_sound(sfx_menu_move);
  } else if (was_pressed(J_DOWN)) {
    gear_cursor = (gear_cursor + 1) % GEAR_SLOT_COUNT;
    play_sound(sfx_menu_move);
  } else if (was_pressed(J_A)) {
    gear_toggle((GearSlot)gear_cursor);
    gear_draw_slot((GearSlot)gear_cursor);
  } else if (was_pressed(J_B)) {
    char buf[24];
    if (player.gear[gear_cursor] != ITEM_INVALID)
      sprintf(buf, "%s: atk+%d def+%d",
        inventory[player.gear[gear_cursor]].name,
        gear_grants[gear_cursor].atk, gear_grants[gear_cursor].def);
    else
      sprintf(buf, "%s: (none)", gear_slot_name((GearSlot)gear_cursor));
    core.draw_text(VRAM_WINDOW_XY(2, 6), buf, 20);
  } else if (was_pressed(J_START)) {
    map_state = MAP_STATE_WAITING;
    game_state = GAME_STATE_WORLD_MAP;
  }
}
