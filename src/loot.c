#pragma bank 25

/**
 * Items, economy, quests and saving for the ARPG systems. See loot.h.
 */

#include <gb/gb.h>
#include <rand.h>
#include <stdio.h>
#include <string.h>

#include "loot.h"
#include "item.h"
#include "map.h"
#include "player.h"
#include "progress.h"
#include "skills.h"
#include "sound.h"
#include "stats.h"
#include "world.h"

LootItem loot_bag[LOOT_BAG_SIZE];
LootItem loot_stash[LOOT_STASH_SIZE];
LootItem loot_equip[LOOT_EQUIP_SLOTS];
LootItem loot_shop[LOOT_SHOP_SIZE];
uint32_t loot_gold;
uint16_t total_kills;

Quest quests[QUEST_MAX];
Quest quest_offers[QUEST_MAX];
char quest_notice[64];
bool quest_notice_pending;

#define GOLD_CAP 99999999UL

//------------------------------------------------------------------------------
// Item data
//------------------------------------------------------------------------------

typedef struct LootBase {
  const char *name;
  uint8_t slot;
  uint8_t implicit;   // scaled stat
  uint8_t scale;
  uint8_t bonus;      // fixed secondary stat (0 = none)
  uint8_t bonus_val;
} LootBase;

#define LOOT_BASE_COUNT 28

static const LootBase loot_bases[LOOT_BASE_COUNT] = {
  { "", 0, AFF_NONE, 0, AFF_NONE, 0 },
  { "Uchigatana", LOOT_SLOT_WEAPON, AFF_ATK, 4, AFF_ASPD, 5 },
  { "Bloodhound", LOOT_SLOT_WEAPON, AFF_ATK, 5, AFF_CDMG, 15 },
  { "Moonveil", LOOT_SLOT_WEAPON, AFF_MATK, 5, AFF_CDR, 5 },
  { "Rivers Blood", LOOT_SLOT_WEAPON, AFF_ATK, 5, AFF_LEECH, 2 },
  { "Blasphemous", LOOT_SLOT_WEAPON, AFF_ATK, 6, AFF_HP, 10 },
  { "Dark Moon GS", LOOT_SLOT_WEAPON, AFF_MATK, 6, AFF_CDR, 8 },
  { "Night+Flame", LOOT_SLOT_WEAPON, AFF_MATK, 6, AFF_CRIT, 5 },
  { "Malenia Hand", LOOT_SLOT_WEAPON, AFF_ATK, 5, AFF_ASPD, 10 },
  { "Starscourge", LOOT_SLOT_WEAPON, AFF_ATK, 6, AFF_CDMG, 20 },
  { "Carian Regal", LOOT_SLOT_WEAPON, AFF_MATK, 5, AFF_SPI, 3 },
  { "Sacred Relic", LOOT_SLOT_WEAPON, AFF_ATK, 6, AFF_GOLD, 5 },
  { "Raging Wolf", LOOT_SLOT_ARMOR, AFF_DEF, 4, AFF_DODGE, 3 },
  { "Blaidd Armor", LOOT_SLOT_ARMOR, AFF_DEF, 5, AFF_ATK, 2 },
  { "Malenia Armor", LOOT_SLOT_ARMOR, AFF_DEF, 4, AFF_ASPD, 5 },
  { "Radahn Armor", LOOT_SLOT_ARMOR, AFF_DEF, 6, AFF_HP, 10 },
  { "Cleanrot Set", LOOT_SLOT_ARMOR, AFF_DEF, 5, AFF_MDEF, 3 },
  { "Black Knife", LOOT_SLOT_ARMOR, AFF_DEF, 3, AFF_DODGE, 6 },
  { "Brass Shield", LOOT_SLOT_OFFHAND, AFF_DEF, 4, AFF_BLOCK, 15 },
  { "Carian Shield", LOOT_SLOT_OFFHAND, AFF_MDEF, 4, AFF_BLOCK, 12 },
  { "Erdtree Staff", LOOT_SLOT_OFFHAND, AFF_MATK, 5, AFF_SPI, 4 },
  { "Godslayer", LOOT_SLOT_OFFHAND, AFF_MATK, 5, AFF_CRIT, 4 },
  { "Golden Order", LOOT_SLOT_OFFHAND, AFF_MATK, 4, AFF_CDR, 6 },
  { "Great Rune", LOOT_SLOT_TRINKET, AFF_STR, 0, AFF_HP, 8 },
  { "Radagon Seal", LOOT_SLOT_TRINKET, AFF_ATK, 0, AFF_CDMG, 8 },
  { "Erdtree Favor", LOOT_SLOT_TRINKET, AFF_HP, 4, AFF_REGEN, 2 },
  { "Dragoncrest", LOOT_SLOT_TRINKET, AFF_DEF, 3, AFF_MDEF, 3 },
  { "Crimson Amber", LOOT_SLOT_TRINKET, AFF_HP, 5, AFF_VIT, 2 },
};

static const char *const rarity_names[RARITY_COUNT] = {
  "Worn", "Sturdy", "Fine", "Enchanted", "Rare", "Epic", "Legend",
  "Relic", "Mythic"
};

static const char *const affix_codes[AFF_COUNT] = {
  "", "ATK", "MAT", "DEF", "MDF", "HP", "SP", "STR", "DEX", "INT", "VIT",
  "END", "SPI", "CRT", "CDM", "SPD", "DGE", "BLK", "LCH", "GLD", "MF", "RGN",
  "CDR"
};

static const uint8_t rarity_mult[RARITY_COUNT] = {
  7, 8, 9, 10, 12, 14, 17, 20, 24
};
static const uint8_t rarity_affixes[RARITY_COUNT] = {
  0, 0, 1, 1, 2, 2, 3, 3, 3
};
static const uint8_t forge_chance[6] = { 80, 65, 50, 35, 25, 0 };

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

static uint16_t cap16(uint32_t v) {
  return v > 60000 ? 60000 : (uint16_t)v;
}

static void clear_item(LootItem *it) {
  memset(it, 0, sizeof(LootItem));
}

static bool valid(const LootItem *it) {
  return it->base && it->base < LOOT_BASE_COUNT && it->rarity < RARITY_COUNT;
}

static uint16_t implicit_value(const LootItem *it) {
  const LootBase *b = &loot_bases[it->base];
  uint32_t v;
  if (b->implicit >= AFF_STR && b->implicit <= AFF_SPI)
    v = 2 + it->ilvl / 3;
  else
    v = ((uint32_t)b->scale * (4 + (uint32_t)it->ilvl * 2)) / 4;
  v = v * rarity_mult[it->rarity] / 8;
  v = v * (100 + 10 * it->upgrade) / 100;
  return cap16(v);
}

static uint16_t roll_affix_value(uint8_t affix, uint16_t ilvl, uint8_t rarity) {
  const uint32_t l = ilvl;
  uint32_t v;
  switch (affix) {
  case AFF_ATK: case AFF_MATK: case AFF_DEF: case AFF_MDEF:
    v = 2 + l / 2 + rarity * l / 10; break;
  case AFF_HP: v = 5 + l * 2 + rarity * l / 4; break;
  case AFF_SP: v = 3 + l / 2 + rarity * l / 20; break;
  case AFF_STR: case AFF_DEX: case AFF_INT:
  case AFF_VIT: case AFF_END: case AFF_SPI:
    v = 1 + l / 6 + rarity * l / 40; break;
  case AFF_CRIT: v = 2 + rarity * 2 + (d4() & 1); break;
  case AFF_CDMG: v = 10 + rarity * 10; break;
  case AFF_ASPD: v = 3 + rarity * 3; break;
  case AFF_DODGE: v = 2 + rarity * 2; break;
  case AFF_BLOCK: v = 3 + rarity * 3; break;
  case AFF_LEECH: v = 1 + rarity; break;
  case AFF_GOLD: case AFF_MF: v = 10 + rarity * 10; break;
  case AFF_REGEN: v = 1 + l / 15 + rarity * 2; break;
  default: v = 2 + rarity * 2; break;  // CDR
  }
  v += v * (d4()) / 8;  // small spread
  if (rarity >= RARITY_RELIC)
    v = v * (rarity == RARITY_RELIC ? 3 : 4) / 2;
  return cap16(v);
}

static uint8_t roll_rarity(uint16_t boost) {
  uint16_t r = (uint16_t)d256() + boost;
  if (r < 105) return RARITY_WORN;
  if (r < 165) return RARITY_STURDY;
  if (r < 205) return RARITY_FINE;
  if (r < 230) return RARITY_ENCHANTED;
  if (r < 245) return RARITY_RARE;
  if (r < 252) return RARITY_EPIC;
  if (r < 255) return RARITY_LEGENDARY;
  if (r < 258) return RARITY_RELIC;
  return RARITY_MYTHIC;
}

//------------------------------------------------------------------------------
// Stats
//------------------------------------------------------------------------------

void loot_new_game(void) BANKED {
  memset(loot_bag, 0, sizeof(loot_bag));
  memset(loot_stash, 0, sizeof(loot_stash));
  memset(loot_equip, 0, sizeof(loot_equip));
  memset(loot_shop, 0, sizeof(loot_shop));
  memset(quests, 0, sizeof(quests));
  memset(quest_offers, 0, sizeof(quest_offers));
  loot_gold = 50;
  total_kills = 0;
  quest_notice_pending = false;

  // Starter gear.
  loot_equip[LOOT_SLOT_WEAPON].base = is_magic_class() ? 10 : 1;
  loot_equip[LOOT_SLOT_WEAPON].ilvl = 1;
  loot_equip[LOOT_SLOT_ARMOR].base = 12;
  loot_equip[LOOT_SLOT_ARMOR].ilvl = 1;

  add_items(ITEM_POTION, 3);
  add_items(ITEM_ETHER, 1);
}

void loot_capture_base(void) BANKED {
  progress_recalc();
}

void loot_gear_totals(uint16_t *t) BANKED {
  memset(t, 0, sizeof(uint16_t) * AFF_COUNT);
  for (uint8_t s = 0; s < LOOT_EQUIP_SLOTS; s++) {
    const LootItem *it = loot_equip + s;
    if (!valid(it))
      continue;
    const LootBase *b = &loot_bases[it->base];
    t[b->implicit] = cap16((uint32_t)t[b->implicit] + implicit_value(it));
    if (b->bonus && b->bonus_val)
      t[b->bonus] = cap16((uint32_t)t[b->bonus] + b->bonus_val);
    for (uint8_t a = 0; a < 3; a++)
      if (it->aff[a] && it->aff[a] < AFF_COUNT)
        t[it->aff[a]] = cap16((uint32_t)t[it->aff[a]] + it->val[a]);
  }
}

//------------------------------------------------------------------------------
// Items
//------------------------------------------------------------------------------

static void fill_item(uint16_t level, uint8_t rarity, LootItem *out) {
  clear_item(out);
  out->base = 1 + rand() % (LOOT_BASE_COUNT - 1);
  out->ilvl = level == 0 ? 1 : (level > MAX_LEVEL ? MAX_LEVEL : level);
  out->rarity = rarity;

  const uint8_t n = rarity_affixes[rarity];
  for (uint8_t a = 0; a < n; a++) {
    uint8_t affix;
    bool dup;
    do {
      affix = 1 + rand() % (AFF_COUNT - 1);
      dup = false;
      for (uint8_t b = 0; b < a; b++)
        if (out->aff[b] == affix) dup = true;
    } while (dup);
    out->aff[a] = affix;
    out->val[a] = roll_affix_value(affix, out->ilvl, rarity);
  }
}

void loot_generate(uint16_t level, uint8_t boost, LootItem *out) BANKED {
  uint16_t b = boost + hs.magic_find / 5;
  fill_item(level, roll_rarity(b), out);
}

void loot_generate_rarity(uint16_t level, uint8_t rarity, LootItem *out) BANKED {
  fill_item(level, rarity < RARITY_COUNT ? rarity : RARITY_MYTHIC, out);
}

bool loot_add_to_bag(const LootItem *it) BANKED {
  for (uint8_t k = 0; k < LOOT_BAG_SIZE; k++) {
    if (loot_bag[k].base == 0) {
      loot_bag[k] = *it;
      return true;
    }
  }
  return false;
}

void loot_item_name(const LootItem *it, char *buf) BANKED {
  if (!valid(it)) {
    buf[0] = 0;
    return;
  }
  if (it->upgrade)
    sprintf(buf, "%s+%u", loot_bases[it->base].name, it->upgrade);
  else
    strcpy(buf, loot_bases[it->base].name);
}

void loot_item_fullname(const LootItem *it, char *buf) BANKED {
  char name[16];
  if (!valid(it)) {
    buf[0] = 0;
    return;
  }
  loot_item_name(it, name);
  sprintf(buf, "R%u %s %s", it->rarity + 1,
    rarity_names[it->rarity], name);
}

static char *stat_piece(char *p, uint8_t affix, uint16_t value) {
  if (!affix || affix >= AFF_COUNT || !value)
    return p;
  sprintf(p, p[0] ? " %s%u" : "%s%u", affix_codes[affix], value);
  return p;
}

void loot_item_stats(const LootItem *it, uint8_t line, char *buf) BANKED {
  buf[0] = 0;
  if (!valid(it))
    return;
  const LootBase *b = &loot_bases[it->base];
  char *p = buf;
  if (line == 0) {
    stat_piece(p, b->implicit, implicit_value(it));
    p += strlen(p);
    p[0] = 0;
    if (b->bonus && b->bonus_val) {
      sprintf(p, buf[0] ? " %s%u" : "%s%u", affix_codes[b->bonus], b->bonus_val);
      p += strlen(p);
    }
    if (it->aff[0])
      sprintf(p, buf[0] ? " %s%u" : "%s%u", affix_codes[it->aff[0]], it->val[0]);
  } else {
    if (it->aff[1]) {
      sprintf(p, "%s%u", affix_codes[it->aff[1]], it->val[1]);
      p += strlen(p);
    }
    if (it->aff[2])
      sprintf(p, buf[0] ? " %s%u" : "%s%u", affix_codes[it->aff[2]], it->val[2]);
  }
}

uint8_t loot_item_slot(const LootItem *it) BANKED {
  return loot_bases[it->base < LOOT_BASE_COUNT ? it->base : 0].slot;
}

uint16_t loot_item_value(const LootItem *it) BANKED {
  if (!valid(it))
    return 0;
  const uint32_t l = it->ilvl;
  const uint32_t r = it->rarity;
  return cap16(5 + l * 2 + l * r * 3 + r * r * 20 + (uint32_t)it->upgrade * l);
}

void loot_equip_from_bag(uint8_t idx) BANKED {
  if (idx >= LOOT_BAG_SIZE || loot_bag[idx].base == 0)
    return;
  const uint8_t slot = loot_item_slot(loot_bag + idx);
  LootItem tmp = loot_equip[slot];
  loot_equip[slot] = loot_bag[idx];
  loot_bag[idx] = tmp;
  progress_recalc();
}

void loot_unequip(uint8_t slot) BANKED {
  if (slot >= LOOT_EQUIP_SLOTS || loot_equip[slot].base == 0)
    return;
  if (loot_add_to_bag(loot_equip + slot))
    clear_item(loot_equip + slot);
  progress_recalc();
}

static void gain_gold(uint32_t g) {
  loot_gold = loot_gold + g > GOLD_CAP ? GOLD_CAP : loot_gold + g;
}

void loot_add_gold(uint32_t g) BANKED {
  g += (g * hs.gold_find) / 100;
  gain_gold(g);
}

void loot_sell(uint8_t idx) BANKED {
  if (idx >= LOOT_BAG_SIZE || loot_bag[idx].base == 0)
    return;
  gain_gold(loot_item_value(loot_bag + idx));
  clear_item(loot_bag + idx);
}

void loot_stash_move(uint8_t idx, bool to_stash) BANKED {
  if (to_stash) {
    if (idx >= LOOT_BAG_SIZE || loot_bag[idx].base == 0)
      return;
    for (uint8_t k = 0; k < LOOT_STASH_SIZE; k++) {
      if (loot_stash[k].base == 0) {
        loot_stash[k] = loot_bag[idx];
        clear_item(loot_bag + idx);
        return;
      }
    }
  } else {
    if (idx >= LOOT_STASH_SIZE || loot_stash[idx].base == 0)
      return;
    if (loot_add_to_bag(loot_stash + idx))
      clear_item(loot_stash + idx);
  }
}

//------------------------------------------------------------------------------
// Shop and forge
//------------------------------------------------------------------------------

void loot_restock_shop(uint16_t level) BANKED {
  for (uint8_t k = 0; k < LOOT_SHOP_SIZE; k++)
    loot_generate(level, 30, loot_shop + k);
}

uint16_t loot_price(uint8_t idx, uint16_t level) BANKED {
  const uint32_t l = level;
  switch (idx) {
  case 0: case 1: case 2: case 3:
    return loot_shop[idx].base ?
      cap16((uint32_t)loot_item_value(loot_shop + idx) * 3) : 0;
  case 4: return cap16(10 + l * 2);   // Potion
  case 5: return cap16(15 + l * 2);   // Ether
  case 6: return cap16(60 + l * 8);   // Elixir
  default: return cap16(8 + l / 4);   // Remedy
  }
}

bool loot_buy(uint8_t idx, uint16_t level) BANKED {
  const uint16_t price = loot_price(idx, level);
  if (price == 0 || loot_gold < price)
    return false;
  if (idx < LOOT_SHOP_SIZE) {
    if (!loot_add_to_bag(loot_shop + idx))
      return false;
    clear_item(loot_shop + idx);
  } else {
    static const uint8_t ids[4] = {
      ITEM_POTION, ITEM_ETHER, ITEM_ELIXIR, ITEM_REMEDY
    };
    if (inventory[ids[idx - 4]].quantity >= 99)
      return false;
    add_items(ids[idx - 4], 1);
  }
  loot_gold -= price;
  return true;
}

uint32_t loot_forge_cost(uint8_t slot) BANKED {
  const LootItem *it = loot_equip + slot;
  if (slot >= LOOT_EQUIP_SLOTS || !valid(it) || it->upgrade >= LOOT_MAX_UPGRADE)
    return 0;
  return (uint32_t)loot_item_value(it) * (it->upgrade + 1);
}

uint8_t loot_forge_chance(uint8_t slot) BANKED {
  const LootItem *it = loot_equip + slot;
  if (slot >= LOOT_EQUIP_SLOTS || !valid(it) || it->upgrade >= LOOT_MAX_UPGRADE)
    return 0;
  return it->upgrade < 5 ? 100 : forge_chance[it->upgrade - 5];
}

uint8_t loot_forge(uint8_t slot) BANKED {
  const uint32_t cost = loot_forge_cost(slot);
  if (!cost || loot_gold < cost)
    return 0;
  loot_gold -= cost;
  if (d128() >= (uint16_t)loot_forge_chance(slot) * 128 / 100)
    return 2;
  loot_equip[slot].upgrade++;
  progress_recalc();
  return 1;
}

//------------------------------------------------------------------------------
// Rewards
//------------------------------------------------------------------------------

static char *append_item(char *p, const LootItem *it, bool kept) {
  char name[24];
  loot_item_fullname(it, name);
  sprintf(p, kept ? "\n%s" : "\n%s (full)", name);
  return p + strlen(p);
}

void loot_open_chest(uint16_t level, char *msg) BANKED {
  LootItem it;
  char num[12];
  uint32_t g = (uint32_t)level * 5 + d16() + 5;
  g += (g * hs.gold_find) / 100;
  gain_gold(g);
  loot_generate(level, 25, &it);
  bool kept = loot_add_to_bag(&it);
  u32_str(g, num);
  sprintf(msg, "Treasure!\n+%s gold", num);
  append_item(msg + strlen(msg), &it, kept);
  play_sound(sfx_level_up);
}

uint8_t loot_roll_kill(uint16_t level, uint8_t rank, char *msg) BANKED {
  char *p = msg + strlen(msg);
  uint8_t drops = 0;

  uint32_t g = (uint32_t)level * 2 + d8() + 1;
  if (rank == 1) g *= 3;
  if (rank == 2) g *= 8;
  loot_add_gold(g);

  uint16_t chance = rank ? 255 : 70 + hs.magic_find / 10;
  uint8_t rolls = rank == 2 ? 2 : 1;
  for (uint8_t k = 0; k < rolls; k++) {
    if (d256() > chance)
      continue;
    LootItem it;
    loot_generate(level, rank == 2 ? 60 : (rank ? 35 : 0), &it);
    bool kept = loot_add_to_bag(&it);
    if (drops < 2)
      p = append_item(p, &it, kept);
    drops++;
  }

  if (d16() == 0)
    add_items(ITEM_POTION, 1);
  return drops;
}

bool loot_use_consumable(uint8_t entry) BANKED {
  if (entry == HOTBAR_POTION) {
    if (inventory[ITEM_POTION].quantity == 0 || player.hp >= player.max_hp)
      return false;
    remove_item(ITEM_POTION);
    uint32_t hp = (uint32_t)player.hp + player.max_hp / 3 + 10;
    player.hp = hp > player.max_hp ? player.max_hp : (uint16_t)hp;
    return true;
  }
  if (entry == HOTBAR_ETHER) {
    if (inventory[ITEM_ETHER].quantity == 0 || player.sp >= player.max_sp)
      return false;
    remove_item(ITEM_ETHER);
    uint32_t sp = (uint32_t)player.sp + player.max_sp / 3 + 5;
    player.sp = sp > player.max_sp ? player.max_sp : (uint16_t)sp;
    return true;
  }
  return false;
}

//------------------------------------------------------------------------------
// Saving
//------------------------------------------------------------------------------

#define SAVE_ADDR ((uint8_t *)0xA000)
#define SAVE_DATA ((uint8_t *)0xA008)
#define SAVE_VERSION 2

static uint8_t *save_ptr;
static uint8_t save_sum;
static bool save_write;
static bool save_load;

/**
 * Streams one block to (or from) the save area, updating the checksum.
 */
static void stream(void *data, uint16_t n) {
  uint8_t *d = (uint8_t *)data;
  while (n--) {
    if (save_write)
      *save_ptr = *d;
    else if (save_load)
      *d = *save_ptr;
    save_sum += *save_ptr++;
    d++;
  }
}

static void stream_all(void) {
  save_ptr = SAVE_DATA;
  save_sum = 0;
  stream(&player, sizeof(Player));
  for (uint8_t k = 0; k < INVENTORY_LEN; k++)
    stream(&inventory[k].quantity, 1);
  stream(loot_bag, sizeof(loot_bag));
  stream(loot_stash, sizeof(loot_stash));
  stream(loot_equip, sizeof(loot_equip));
  stream(&loot_gold, sizeof(loot_gold));
  stream(&total_kills, sizeof(total_kills));
  stream(quests, sizeof(quests));
  stream(&world_last_town, 1);
  stream(&world_best_tower_floor, 1);
  stream(world_visited, sizeof(world_visited));
  stream(&hero_level, sizeof(hero_level));
  stream(&hero_xp, sizeof(hero_xp));
  stream(attr_alloc, sizeof(attr_alloc));
  stream(&stat_points, sizeof(stat_points));
  stream(&skill_points, sizeof(skill_points));
  stream(skill_rank, sizeof(skill_rank));
  stream(hotbar, sizeof(hotbar));
}

void save_game(void) BANKED {
  SWITCH_RAM(1);
  save_write = true;
  save_load = false;
  stream_all();
  save_write = false;
  SAVE_ADDR[0] = 'L';
  SAVE_ADDR[1] = 'D';
  SAVE_ADDR[2] = SAVE_VERSION;
  SAVE_ADDR[3] = save_sum;
  SWITCH_RAM(0);
}

static bool valid_save(void) {
  if (SAVE_ADDR[0] != 'L' || SAVE_ADDR[1] != 'D' ||
    SAVE_ADDR[2] != SAVE_VERSION)
    return false;
  save_write = false;
  save_load = false;
  stream_all();
  return save_sum == SAVE_ADDR[3];
}

bool save_exists(void) BANKED {
  SWITCH_RAM(1);
  bool ok = valid_save();
  SWITCH_RAM(0);
  return ok;
}

bool load_game(void) BANKED {
  SWITCH_RAM(1);
  if (!valid_save()) {
    SWITCH_RAM(0);
    return false;
  }
  // Read the class first so the class abilities can be set up.
  const uint8_t player_class = SAVE_DATA[8];
  SWITCH_RAM(0);
  init_player(player_class);
  SWITCH_RAM(1);
  save_load = true;
  stream_all();
  save_load = false;
  SWITCH_RAM(0);
  buff_clear();
  hotbar_sel = 0;
  progress_recalc();
  return true;
}

//------------------------------------------------------------------------------
// Quests
//------------------------------------------------------------------------------

static void make_offer(Quest *q) {
  const uint16_t lvl = hero_level;
  const uint32_t mxp = monster_xp(lvl, B_TIER, 0);
  memset(q, 0, sizeof(Quest));
  switch (rand() % 6) {
  case 0:
  case 1:
    q->type = QUEST_KILL;
    q->goal = 8 + (rand() & 7);
    q->gold = (uint32_t)lvl * q->goal * 2 + 20;
    q->xp = mxp * (q->goal / 2 + 1);
    break;
  case 2:
    q->type = QUEST_ELITE;
    q->goal = 2 + (rand() & 1);
    q->gold = (uint32_t)lvl * 20 * q->goal + 50;
    q->xp = mxp * 3 * q->goal;
    break;
  case 3: {
    // Explore an unvisited zone (falls back to a tower quest).
    uint8_t start = rand() % WORLD_ZONE_COUNT;
    for (uint8_t k = 0; k < WORLD_ZONE_COUNT; k++) {
      uint8_t z = (start + k) % WORLD_ZONE_COUNT;
      if (world_visited[z >> 3] & (1 << (z & 7)))
        continue;
      q->type = QUEST_ZONE;
      q->target = z;
      q->goal = 1;
      q->gold = (uint32_t)lvl * 25 + 50;
      q->xp = mxp * 4;
      return;
    }
  }
    // fall through when every zone has already been visited
  case 4:
    q->type = QUEST_RAID;
    q->target = rand() % WORLD_RAID_COUNT;
    q->goal = 1;
    q->gold = (uint32_t)(q->target + 1) * 500 + (uint32_t)lvl * 10;
    q->xp = mxp * (q->target + 3);
    break;
  default:
    if (rand() & 1) {
      q->type = QUEST_TOWER;
      q->target = world_best_tower_floor + 3;
      if (q->target > WORLD_TOWER_MAX_FLOOR)
        q->target = WORLD_TOWER_MAX_FLOOR;
      q->goal = 1;
      q->gold = (uint32_t)q->target * 40 + 100;
      q->xp = mxp * 5;
    } else {
      q->type = QUEST_BOSS;
      q->goal = 1;
      q->gold = (uint32_t)lvl * 50 + 100;
      q->xp = mxp * 8;
    }
  }
}

void quest_generate_offers(void) BANKED {
  for (uint8_t k = 0; k < QUEST_MAX; k++)
    make_offer(quest_offers + k);
}

bool quest_accept(uint8_t idx) BANKED {
  if (idx >= QUEST_MAX || quest_offers[idx].type == QUEST_NONE)
    return false;
  for (uint8_t k = 0; k < QUEST_MAX; k++) {
    if (quests[k].type == QUEST_NONE) {
      quests[k] = quest_offers[idx];
      quest_offers[idx].type = QUEST_NONE;
      return true;
    }
  }
  return false;
}

void quest_abandon(uint8_t idx) BANKED {
  if (idx < QUEST_MAX)
    quests[idx].type = QUEST_NONE;
}

void quest_describe(const Quest *q, char *buf) BANKED {
  switch (q->type) {
  case QUEST_KILL:
    sprintf(buf, "Slay foes %u/%u", q->progress, q->goal);
    break;
  case QUEST_ELITE:
    sprintf(buf, "Slay elites %u/%u", q->progress, q->goal);
    break;
  case QUEST_ZONE:
    sprintf(buf, "Explore zone %u-%u",
      (q->target % WORLD_ZONE_COLS) + 1, (q->target / WORLD_ZONE_COLS) + 1);
    break;
  case QUEST_TOWER:
    sprintf(buf, "Reach Tower F%u", q->target);
    break;
  case QUEST_BOSS:
    sprintf(buf, "Slay a boss");
    break;
  case QUEST_RAID:
    sprintf(buf, "Clear raid %u", q->target + 1);
    break;
  default:
    sprintf(buf, "-");
  }
}

void quest_reward_text(const Quest *q, char *buf) BANKED {
  char g[12];
  char x[12];
  if (q->type == QUEST_NONE) {
    buf[0] = 0;
    return;
  }
  u32_str(q->gold, g);
  u32_str(q->xp, x);
  sprintf(buf, " %sG %sXP", g, x);
}

static void complete_quest(Quest *q) {
  char desc[20];
  char g[12];
  char x[12];
  quest_describe(q, desc);
  gain_gold(q->gold);
  progress_add_xp(q->xp);
  u32_str(q->gold, g);
  u32_str(q->xp, x);
  sprintf(quest_notice, "Quest complete!\n%s\n+%sG +%sXP", desc, g, x);
  quest_notice_pending = true;
  q->type = QUEST_NONE;
}

void quest_event(uint8_t event, uint8_t value) BANKED {
  if (event == QUEST_EVENT_KILL && total_kills < 65000)
    total_kills++;

  for (uint8_t k = 0; k < QUEST_MAX; k++) {
    Quest *q = quests + k;
    bool hit = false;
    switch (q->type) {
    case QUEST_KILL:
      hit = event == QUEST_EVENT_KILL;
      break;
    case QUEST_ELITE:
      hit = event == QUEST_EVENT_ELITE || event == QUEST_EVENT_BOSS;
      break;
    case QUEST_BOSS:
      hit = event == QUEST_EVENT_BOSS;
      break;
    case QUEST_ZONE:
      hit = event == QUEST_EVENT_ZONE && value == q->target;
      break;
    case QUEST_TOWER:
      hit = event == QUEST_EVENT_TOWER && value >= q->target;
      break;
    case QUEST_RAID:
      hit = event == QUEST_EVENT_RAID && value == q->target;
      break;
    }
    if (!hit)
      continue;
    if (q->progress < 255)
      q->progress++;
    if (q->progress >= q->goal)
      complete_quest(q);
  }
}
