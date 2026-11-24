#pragma bank 27

#include <rand.h>
#include <stdio.h>
#include <string.h>

#include "professions.h"
#include "item.h"
#include "loot.h"
#include "world.h"

uint8_t profession_level[PROFESSION_COUNT];
uint16_t profession_xp[PROFESSION_COUNT];
uint8_t profession_materials[PROFESSION_MATERIAL_COUNT];

static const char *const profession_names[PROFESSION_COUNT] = {
  "Mining", "Herbalism", "Logging", "Skinning", "Fishing", "Archaeology",
  "Blacksmithing", "Weaponsmithing", "Armorsmithing", "Leatherworking",
  "Tailoring", "Jewelcrafting", "Alchemy", "Enchanting", "Engineering",
  "Cooking", "Runecrafting", "Inscription"
};

static const char *const material_names[PROFESSION_MATERIAL_COUNT] = {
  "Ore", "Herbs", "Wood", "Hide", "Fish", "Relics"
};

typedef struct Recipe {
  uint8_t first;
  uint8_t first_count;
  uint8_t second;
  uint8_t second_count;
  uint8_t slot;
  uint8_t item;
  uint8_t rarity_bonus;
} Recipe;

#define NO_MATERIAL 0xFF
#define NO_EQUIP_SLOT 0xFF
#define NO_ITEM 0xFF
static const Recipe recipes[PROFESSION_COUNT - PROFESSION_GATHER_COUNT] = {
  { MATERIAL_ORE, 4, MATERIAL_WOOD, 1, LOOT_SLOT_ARMOR, NO_ITEM, 0 },
  { MATERIAL_ORE, 4, MATERIAL_WOOD, 2, LOOT_SLOT_WEAPON, NO_ITEM, 0 },
  { MATERIAL_ORE, 3, MATERIAL_HIDE, 3, LOOT_SLOT_ARMOR, NO_ITEM, 0 },
  { MATERIAL_HIDE, 4, MATERIAL_HERBS, 1, LOOT_SLOT_ARMOR, NO_ITEM, 0 },
  { MATERIAL_HIDE, 2, MATERIAL_HERBS, 3, LOOT_SLOT_ARMOR, NO_ITEM, 0 },
  { MATERIAL_ORE, 2, MATERIAL_RELIC, 2, LOOT_SLOT_TRINKET, NO_ITEM, 0 },
  { MATERIAL_HERBS, 3, MATERIAL_FISH, 1, NO_EQUIP_SLOT, ITEM_ELIXIR, 0 },
  { MATERIAL_RELIC, 2, MATERIAL_HERBS, 2, NO_EQUIP_SLOT, NO_ITEM, 2 },
  { MATERIAL_ORE, 3, MATERIAL_WOOD, 3, LOOT_SLOT_OFFHAND, NO_ITEM, 0 },
  { MATERIAL_FISH, 2, MATERIAL_HERBS, 2, NO_EQUIP_SLOT, ITEM_REGEN, 0 },
  { MATERIAL_RELIC, 3, MATERIAL_ORE, 1, LOOT_SLOT_TRINKET, NO_ITEM, 1 },
  { MATERIAL_HERBS, 2, MATERIAL_WOOD, 2, NO_EQUIP_SLOT, NO_ITEM, 1 },
};

void professions_new_game(void) BANKED {
  memset(profession_level, 1, sizeof(profession_level));
  memset(profession_xp, 0, sizeof(profession_xp));
  memset(profession_materials, 0, sizeof(profession_materials));
}

void profession_name(uint8_t profession, char *buf) BANKED {
  strcpy(buf, profession < PROFESSION_COUNT ? profession_names[profession] : "Unknown");
}

void profession_material_name(uint8_t material, char *buf) BANKED {
  strcpy(buf, material < PROFESSION_MATERIAL_COUNT ? material_names[material] : "?");
}

void profession_recipe_text(uint8_t profession, char *buf) BANKED {
  if (profession < PROFESSION_GATHER_COUNT || profession >= PROFESSION_COUNT) {
    strcpy(buf, "Gather in the field");
    return;
  }
  const Recipe *recipe = recipes + profession - PROFESSION_GATHER_COUNT;
  char first[12];
  char second[12];
  if (recipe->first == NO_MATERIAL) strcpy(first, "-");
  else profession_material_name(recipe->first, first);
  if (recipe->second == NO_MATERIAL) strcpy(second, "-");
  else profession_material_name(recipe->second, second);
  sprintf(buf, "%u %s + %u %s", recipe->first_count, first,
    recipe->second_count, second);
}

uint16_t profession_xp_to_next(uint8_t profession) BANKED {
  if (profession >= PROFESSION_COUNT || profession_level[profession] >= 100)
    return 0;
  return (uint16_t)profession_level[profession] * 40;
}

static void award_profession_xp(uint8_t profession, uint16_t amount) {
  if (profession >= PROFESSION_COUNT || profession_level[profession] >= 100)
    return;
  profession_xp[profession] += amount;
  while (profession_level[profession] < 100 &&
    profession_xp[profession] >= profession_xp_to_next(profession)) {
    profession_xp[profession] -= profession_xp_to_next(profession);
    profession_level[profession]++;
  }
}

static void add_material(uint8_t material, uint8_t count) {
  uint16_t total = profession_materials[material] + count;
  profession_materials[material] = total > 99 ? 99 : total;
}

bool profession_gather_zone(uint8_t biome, uint16_t item_level, char *message) BANKED {
  static const uint8_t gather_pair[WORLD_BIOME_COUNT][2] = {
    { PROF_HERBALISM, PROF_LOGGING }, { PROF_LOGGING, PROF_FISHING },
    { PROF_MINING, PROF_ARCHAEOLOGY }, { PROF_HERBALISM, PROF_FISHING },
    { PROF_MINING, PROF_SKINNING }, { PROF_FISHING, PROF_SKINNING },
    { PROF_SKINNING, PROF_LOGGING }, { PROF_MINING, PROF_ARCHAEOLOGY },
    { PROF_MINING, PROF_ARCHAEOLOGY }
  };
  static const uint8_t gather_material[PROFESSION_GATHER_COUNT] = {
    MATERIAL_ORE, MATERIAL_HERBS, MATERIAL_WOOD, MATERIAL_HIDE,
    MATERIAL_FISH, MATERIAL_RELIC
  };
  if (biome >= WORLD_BIOME_COUNT)
    return false;
  const uint8_t profession = gather_pair[biome][rand() & 1];
  const uint8_t material = gather_material[profession];
  const uint8_t amount = 1 + (profession_level[profession] - 1) / 30;
  add_material(material, amount);
  const uint16_t xp = 10 + (item_level > 720 ? 35 : item_level / 20);
  award_profession_xp(profession, xp);
  sprintf(message, "Gathered %u %s.", amount, material_names[material]);
  return true;
}

bool profession_craft(uint8_t profession, uint16_t item_level) BANKED {
  if (profession < PROFESSION_GATHER_COUNT || profession >= PROFESSION_COUNT)
    return false;
  const Recipe *recipe = recipes + profession - PROFESSION_GATHER_COUNT;
  if (recipe->first != NO_MATERIAL &&
    profession_materials[recipe->first] < recipe->first_count)
    return false;
  if (recipe->second != NO_MATERIAL &&
    profession_materials[recipe->second] < recipe->second_count)
    return false;
  if (recipe->item != NO_ITEM && inventory[recipe->item].quantity >= 99)
    return false;

  LootItem output;
  if (recipe->item == NO_ITEM) {
    const uint8_t level = profession_level[profession];
    const uint8_t rarity = level >= 75 ? RARITY_EPIC :
      (level >= 40 ? RARITY_ENCHANTED : RARITY_FINE) + recipe->rarity_bonus;
    if (recipe->slot == NO_EQUIP_SLOT)
      loot_generate_rarity(item_level, rarity, &output);
    else
      loot_generate_rarity_slot(item_level, rarity, recipe->slot, &output);
    if (!loot_add_to_bag(&output))
      return false;
  }

  if (recipe->first != NO_MATERIAL)
    profession_materials[recipe->first] -= recipe->first_count;
  if (recipe->second != NO_MATERIAL)
    profession_materials[recipe->second] -= recipe->second_count;
  if (recipe->item != NO_ITEM)
    add_items(recipe->item, 1);
  award_profession_xp(profession, 25 + profession_level[profession] / 2);
  return true;
}
