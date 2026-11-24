#pragma bank 26

/**
 * Skill core: class skill tables, ranks, unlocks, costs and cooldowns.
 */

#include <gb/gb.h>
#include <stdio.h>
#include <string.h>

#include "skills.h"
#include "item.h"
#include "player.h"
#include "progress.h"

uint8_t skill_rank[SKILL_COUNT];
uint8_t hotbar[HOTBAR_SLOTS];
uint8_t hotbar_sel;

typedef struct SkillDef {
  const char *name;
  uint8_t kind;
  uint16_t power;
} SkillDef;

static const uint8_t kind_cost[SK_KIND_COUNT] = {
  4, 6, 5, 8, 8, 10, 10, 10, 5, 7, 6, 4, 16
};

static const uint8_t kind_cooldown[SK_KIND_COUNT] = {
  40, 60, 30, 90, 240, 250, 250, 250, 80, 60, 60, 50, 250
};

static const char *const kind_info[SK_KIND_COUNT] = {
  "Strike ahead",
  "Hit all around",
  "Ranged bolt",
  "Blast nearby foes",
  "Restore HP",
  "Raise attack",
  "Defense+shield",
  "Act faster",
  "Strike and stun",
  "Bolt, steal HP",
  "Bolt, poisons",
  "Dash, dodge",
  "Hit every foe",
};

static const uint16_t unlock_levels[SKILL_COUNT] = { 1, 5, 15, 40, 100, 200 };

static const SkillDef class_skills[CLASS_COUNT][SKILL_COUNT] = {
  // Druid
  { { "Lightning", SK_BOLT, 115 }, { "Cure", SK_HEAL, 30 },
    { "Barkskin", SK_GUARD, 0 }, { "Swarm", SK_NOVA, 95 },
    { "Regrowth", SK_HEAL, 60 }, { "Storm", SK_QUAKE, 150 } },
  // Fighter
  { { "Power Hit", SK_STRIKE, 160 }, { "2nd Wind", SK_HEAL, 25 },
    { "Cleave", SK_CLEAVE, 115 }, { "Trip", SK_STUN, 100 },
    { "Surge", SK_HASTE, 0 }, { "Whirlwind", SK_NOVA, 140 } },
  // Monk
  { { "Palm", SK_STUN, 120 }, { "Flurry", SK_STRIKE, 200 },
    { "Evasion", SK_DASH, 0 }, { "Still Mind", SK_HEAL, 35 },
    { "Diamond", SK_GUARD, 0 }, { "Quivering", SK_STRIKE, 320 } },
  // Sorcerer
  { { "Fireball", SK_BOLT, 140 }, { "Haste", SK_HASTE, 0 },
    { "Sleet", SK_NOVA, 100 }, { "Darkness", SK_STUN, 90 },
    { "Disintegr", SK_BOLT, 260 }, { "Wild Magic", SK_QUAKE, 180 } },
  // Rogue
  { { "Backstab", SK_STRIKE, 180 }, { "Poison", SK_POISON, 80 },
    { "Tumble", SK_DASH, 0 }, { "Fan Knives", SK_NOVA, 90 },
    { "Shadow", SK_HASTE, 0 }, { "Assassin", SK_STRIKE, 300 } },
  // Ranger
  { { "Aimed Shot", SK_BOLT, 150 }, { "Volley", SK_NOVA, 100 },
    { "Snare", SK_STUN, 80 }, { "Mend", SK_HEAL, 30 },
    { "Hunter", SK_RAGE, 0 }, { "Arrow Rain", SK_QUAKE, 150 } },
  // Knight
  { { "Smite", SK_STRIKE, 150 }, { "Shield Up", SK_GUARD, 0 },
    { "Bash", SK_STUN, 110 }, { "Rally", SK_HEAL, 30 },
    { "Charge", SK_DASH, 0 }, { "Judgment", SK_CLEAVE, 180 } },
  // Wizard
  { { "M.Missile", SK_BOLT, 130 }, { "Frost Nova", SK_NOVA, 100 },
    { "Blink", SK_DASH, 0 }, { "Shield", SK_GUARD, 0 },
    { "Haste", SK_HASTE, 0 }, { "Meteor", SK_QUAKE, 200 } },
  // Paladin
  { { "Smite", SK_STRIKE, 150 }, { "Lay Hands", SK_HEAL, 45 },
    { "Aura", SK_GUARD, 0 }, { "Holy Bolt", SK_BOLT, 140 },
    { "Crusade", SK_RAGE, 0 }, { "Wrath", SK_NOVA, 160 } },
  // Cleric
  { { "Holy Bolt", SK_BOLT, 120 }, { "Heal", SK_HEAL, 40 },
    { "Bless", SK_RAGE, 0 }, { "Sanctuary", SK_NOVA, 110 },
    { "Ward", SK_GUARD, 0 }, { "Miracle", SK_HEAL, 90 } },
  // Bard
  { { "Dissonance", SK_BOLT, 110 }, { "Inspire", SK_RAGE, 0 },
    { "Lullaby", SK_STUN, 60 }, { "Ballad", SK_HEAL, 30 },
    { "Tempo", SK_HASTE, 0 }, { "Crescendo", SK_QUAKE, 160 } },
  // Necromancer
  { { "Drain", SK_DRAIN, 110 }, { "Bone Spear", SK_BOLT, 150 },
    { "Curse", SK_POISON, 90 }, { "Corpse Exp", SK_NOVA, 140 },
    { "Bone Armor", SK_GUARD, 0 }, { "Death", SK_QUAKE, 190 } },
  // Assassin
  { { "Stab", SK_STRIKE, 170 }, { "Venom", SK_POISON, 100 },
    { "Shadowstep", SK_DASH, 0 }, { "Blades", SK_CLEAVE, 120 },
    { "Fade", SK_HASTE, 0 }, { "Execute", SK_STRIKE, 330 } },
  // Dragoon
  { { "Lance", SK_STRIKE, 160 }, { "Jump", SK_DASH, 0 },
    { "Sweep", SK_CLEAVE, 120 }, { "Dragon Hrt", SK_RAGE, 0 },
    { "Wyvern", SK_BOLT, 160 }, { "Dragonfall", SK_QUAKE, 190 } },
  // Shaman
  { { "Spirit", SK_BOLT, 120 }, { "Totem", SK_GUARD, 0 },
    { "Chain Ltng", SK_NOVA, 120 }, { "Mend", SK_HEAL, 40 },
    { "Bloodlust", SK_HASTE, 0 }, { "Earthquake", SK_QUAKE, 180 } },
  // Warlock
  { { "Eldritch", SK_BOLT, 130 }, { "Hex", SK_POISON, 110 },
    { "Drain Life", SK_DRAIN, 120 }, { "Dark Pact", SK_RAGE, 0 },
    { "Doom", SK_NOVA, 150 }, { "Apocalypse", SK_QUAKE, 210 } },
  // Stalker
  { { "Rend", SK_STRIKE, 160 }, { "Pounce", SK_DASH, 0 },
    { "Snare", SK_STUN, 90 }, { "Vanish", SK_HASTE, 0 },
    { "Mark", SK_RAGE, 0 }, { "Kill Shot", SK_BOLT, 260 } },
  // Marauder
  { { "Crush", SK_STRIKE, 170 }, { "Rampage", SK_CLEAVE, 130 },
    { "Warcry", SK_RAGE, 0 }, { "Bloodlust", SK_DRAIN, 120 },
    { "Stomp", SK_NOVA, 140 }, { "Berserk", SK_HASTE, 0 } },
};

static const SkillDef *def_of(uint8_t k) {
  const uint8_t c = player.player_class < CLASS_COUNT ? player.player_class : 0;
  return &class_skills[c][k < SKILL_COUNT ? k : 0];
}

void skills_new_game(void) BANKED {
  memset(skill_rank, 0, sizeof(skill_rank));
  skill_rank[0] = 1;
  hotbar[0] = 0;
  hotbar[1] = HOTBAR_POTION;
  hotbar[2] = HOTBAR_ETHER;
  hotbar[3] = HOTBAR_EMPTY;
  hotbar_sel = 0;
}

void skill_get(uint8_t k, SkillInfo *out) BANKED {
  const SkillDef *s = def_of(k);
  const uint8_t rank = skill_rank[k] ? skill_rank[k] : 1;
  const uint32_t base = kind_cost[s->kind];
  uint32_t cost = base + (base * hero_level) / 25 + 2 * rank;
  uint32_t cd = kind_cooldown[s->kind];
  cd = cd * (100 - hs.cdr) / 100;
  const uint8_t haste = buff_power(HERO_BUFF_HASTE);
  if (haste)
    cd = cd * (100 - haste / 2) / 100;
  out->kind = s->kind;
  out->rank = rank;
  out->power = (uint16_t)(((uint32_t)s->power * (100 + 12 * (rank - 1))) / 100);
  out->cost = cost > 9999 ? 9999 : (uint16_t)cost;
  out->cooldown = cd < 8 ? 8 : (uint16_t)cd;
}

void skill_name(uint8_t k, char *buf) BANKED {
  strcpy(buf, def_of(k)->name);
}

void skill_info_text(uint8_t k, char *buf) BANKED {
  SkillInfo info;
  skill_get(k, &info);
  sprintf(buf, "%s %c%u", kind_info[info.kind],
    is_magic_class() ? 'M' : 'S', info.cost);
}

uint16_t skill_unlock_level(uint8_t k) BANKED {
  return k < SKILL_COUNT ? unlock_levels[k] : MAX_LEVEL;
}

uint8_t skill_max_rank(void) BANKED {
  const uint16_t r = 5 + hero_level / 30;
  return r > SKILL_MAX_RANK ? SKILL_MAX_RANK : (uint8_t)r;
}

bool skill_rank_up(uint8_t k) BANKED {
  if (k >= SKILL_COUNT || !skill_points)
    return false;
  if (hero_level < unlock_levels[k] || skill_rank[k] >= skill_max_rank())
    return false;
  skill_rank[k]++;
  skill_points--;
  return true;
}

void hotbar_entry_name(uint8_t entry, char *buf) BANKED {
  if (entry == HOTBAR_POTION)
    sprintf(buf, "Potion x%u", inventory[ITEM_POTION].quantity);
  else if (entry == HOTBAR_ETHER)
    sprintf(buf, "Ether x%u", inventory[ITEM_ETHER].quantity);
  else if (entry < SKILL_COUNT)
    strcpy(buf, def_of(entry)->name);
  else
    strcpy(buf, "-");
}
