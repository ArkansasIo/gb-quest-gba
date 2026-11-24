#ifndef _PROFESSIONS_H
#define _PROFESSIONS_H

#include <gb/gb.h>
#include <stdint.h>
#include <stdbool.h>

#define PROFESSION_COUNT 18
#define PROFESSION_GATHER_COUNT 6
#define PROFESSION_MATERIAL_COUNT 6

typedef enum Profession {
  PROF_MINING, PROF_HERBALISM, PROF_LOGGING, PROF_SKINNING, PROF_FISHING,
  PROF_ARCHAEOLOGY, PROF_BLACKSMITHING, PROF_WEAPONSMITHING,
  PROF_ARMORSMITHING, PROF_LEATHERWORKING, PROF_TAILORING,
  PROF_JEWELCRAFTING, PROF_ALCHEMY, PROF_ENCHANTING, PROF_ENGINEERING,
  PROF_COOKING, PROF_RUNECRAFTING, PROF_INSCRIPTION
} Profession;

typedef enum ProfessionMaterial {
  MATERIAL_ORE, MATERIAL_HERBS, MATERIAL_WOOD, MATERIAL_HIDE,
  MATERIAL_FISH, MATERIAL_RELIC
} ProfessionMaterial;

extern uint8_t profession_level[PROFESSION_COUNT];
extern uint16_t profession_xp[PROFESSION_COUNT];
extern uint8_t profession_materials[PROFESSION_MATERIAL_COUNT];

void professions_new_game(void) BANKED;
void profession_name(uint8_t profession, char *buf) BANKED;
void profession_material_name(uint8_t material, char *buf) BANKED;
void profession_recipe_text(uint8_t profession, char *buf) BANKED;
uint16_t profession_xp_to_next(uint8_t profession) BANKED;
bool profession_craft(uint8_t profession, uint16_t item_level) BANKED;
bool profession_gather_zone(uint8_t biome, uint16_t item_level, char *message) BANKED;

#endif
