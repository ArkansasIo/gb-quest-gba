#pragma bank 25

/**
 * Loot, equipment, progression and quests for the ARPG systems.
 */

#include <gb/gb.h>
#include <rand.h>
#include <stdio.h>
#include <string.h>

#include "loot.h"
#include "item.h"
#include "map.h"
#include "player.h"
#include "sound.h"
#include "stats.h"
#include "world.h"

LootItem loot_bag[LOOT_BAG_SIZE];
LootItem loot_stash[LOOT_STASH_SIZE];
LootItem loot_equip[LOOT_EQUIP_SLOTS];
LootItem loot_shop[LOOT_SHOP_SIZE];
uint16_t loot_gold;
LootBonuses loot_bonus;
BuffState buffs;

uint8_t stat_points;
uint8_t skill_points;
uint8_t stat_alloc[ALLOC_COUNT];
uint8_t skill_rank[6];
uint8_t hotbar[HOTBAR_SLOTS];
uint8_t hotbar_sel;
uint16_t total_kills;

Quest quests[QUEST_MAX];
Quest quest_offers[QUEST_MAX];
char quest_notice[64];
bool quest_notice_pending;

static uint16_t base_max_hp;
static uint16_t base_max_sp;

//------------------------------------------------------------------------------
// Item data
//------------------------------------------------------------------------------

typedef struct LootBase {
  const char *name;
  uint8_t slot;
  uint8_t implicit;
  uint8_t scale;
} LootBase;

#define LOOT_BASE_COUNT 19

static const LootBase loot_bases[LOOT_BASE_COUNT] = {
  { "", 0, AFF_NONE, 0 },
  { "Dagger", LOOT_SLOT_WEAPON, AFF_ATK, 3 },
  { "Sword", LOOT_SLOT_WEAPON, AFF_ATK, 4 },
  { "Axe", LOOT_SLOT_WEAPON, AFF_ATK, 5 },
  { "Mace", LOOT_SLOT_WEAPON, AFF_ATK, 4 },
  { "Spear", LOOT_SLOT_WEAPON, AFF_ATK, 4 },
  { "Bow", LOOT_SLOT_WEAPON, AFF_ATK, 4 },
  { "Staff", LOOT_SLOT_WEAPON, AFF_MATK, 5 },
  { "Wand", LOOT_SLOT_WEAPON, AFF_MATK, 4 },
  { "Robe", LOOT_SLOT_ARMOR, AFF_MDEF, 3 },
  { "Leather", LOOT_SLOT_ARMOR, AFF_DEF, 3 },
  { "Chainmail", LOOT_SLOT_ARMOR, AFF_DEF, 4 },
  { "Plate", LOOT_SLOT_ARMOR, AFF_DEF, 5 },
  { "Buckler", LOOT_SLOT_OFFHAND, AFF_DEF, 2 },
  { "Kite Shld", LOOT_SLOT_OFFHAND, AFF_DEF, 3 },
  { "Tome", LOOT_SLOT_OFFHAND, AFF_MATK, 3 },
  { "Ring", LOOT_SLOT_TRINKET, AFF_AGL, 2 },
  { "Amulet", LOOT_SLOT_TRINKET, AFF_MDEF, 2 },
  { "Charm", LOOT_SLOT_TRINKET, AFF_HP, 4 },
};

static const char *const rarity_names[RARITY_COUNT] = {
  "Common", "Magic", "Rare", "Epic", "Legend"
};

static const char *const affix_codes[AFF_COUNT] = {
  "", "ATK", "DEF", "MAT", "MDF", "AGL", "HP", "SP", "CRT", "LCH", "GLD", "RGN"
};

static const uint8_t rarity_mult[RARITY_COUNT] = { 8, 9, 10, 12, 14 };

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

static uint8_t clamp8(uint16_t v) {
  return v > 255 ? 255 : (uint8_t)v;
}

static void clear_item(LootItem *it) {
  memset(it, 0, sizeof(LootItem));
}

static uint8_t implicit_value(const LootItem *it) {
  const LootBase *b = &loot_bases[it->base];
  uint16_t v = b->scale + ((uint16_t)it->ilvl * b->scale) / 6;
  v = (v * rarity_mult[it->rarity]) / 8;
  return clamp8(v);
}

static uint8_t roll_affix_value(uint8_t affix, uint8_t ilvl, uint8_t rarity) {
  switch (affix) {
  case AFF_HP: return clamp8(4 + ilvl / 2 + rarity * 3 + d4());
  case AFF_SP: return clamp8(2 + ilvl / 4 + rarity + d4());
  case AFF_CRIT: return 2 + rarity * 2 + (d4() & 1);
  case AFF_LEECH: return 1 + rarity;
  case AFF_GOLD: return 5 + rarity * 5 + d4();
  case AFF_REGEN: return 1 + ilvl / 20 + (rarity >> 1);
  default: return clamp8(1 + ilvl / 5 + rarity + (d4() & 1));
  }
}

static uint8_t roll_rarity(uint8_t boost) {
  uint16_t r = (uint16_t)d256() + boost;
  if (r < 150) return RARITY_COMMON;
  if (r < 215) return RARITY_MAGIC;
  if (r < 245) return RARITY_RARE;
  if (r < 262) return RARITY_EPIC;
  return RARITY_LEGENDARY;
}

//------------------------------------------------------------------------------
// Stats
//------------------------------------------------------------------------------

void loot_new_game(void) BANKED {
  memset(loot_bag, 0, sizeof(loot_bag));
  memset(loot_stash, 0, sizeof(loot_stash));
  memset(loot_equip, 0, sizeof(loot_equip));
  memset(loot_shop, 0, sizeof(loot_shop));
  memset(stat_alloc, 0, sizeof(stat_alloc));
  memset(skill_rank, 0, sizeof(skill_rank));
  memset(quests, 0, sizeof(quests));
  memset(quest_offers, 0, sizeof(quest_offers));
  memset(&buffs, 0, sizeof(buffs));
  loot_gold = 25;
  stat_points = 0;
  skill_points = 1;
  skill_rank[0] = 1;
  hotbar[0] = 0;
  hotbar[1] = HOTBAR_POTION;
  hotbar[2] = HOTBAR_ETHER;
  hotbar[3] = HOTBAR_EMPTY;
  hotbar_sel = 0;
  total_kills = 0;
  quest_notice_pending = false;

  // Starter gear.
  loot_equip[LOOT_SLOT_WEAPON].base = is_magic_class() ? 8 : 2;
  loot_equip[LOOT_SLOT_WEAPON].ilvl = 1;
  loot_equip[LOOT_SLOT_ARMOR].base = is_magic_class() ? 9 : 10;
  loot_equip[LOOT_SLOT_ARMOR].ilvl = 1;

  add_items(ITEM_POTION, 3);
  add_items(ITEM_ETHER, 1);
}

void loot_capture_base(void) BANKED {
  base_max_hp = player.max_hp;
  base_max_sp = player.max_sp;
  loot_recalc();
}

void loot_recalc(void) BANKED {
  uint16_t g[AFF_COUNT];
  memset(g, 0, sizeof(g));

  for (uint8_t s = 0; s < LOOT_EQUIP_SLOTS; s++) {
    const LootItem *it = loot_equip + s;
    if (it->base == 0 || it->base >= LOOT_BASE_COUNT)
      continue;
    g[loot_bases[it->base].implicit] += implicit_value(it);
    for (uint8_t a = 0; a < 2; a++)
      if (it->aff[a] && it->aff[a] < AFF_COUNT)
        g[it->aff[a]] += it->val[a];
  }

  uint16_t atk = player.atk_base + g[AFF_ATK] + stat_alloc[ALLOC_STR];
  uint16_t def = player.def_base + g[AFF_DEF] + stat_alloc[ALLOC_END];
  if (buffs.atk) atk += atk / 4 + 2;
  if (buffs.def) def += def / 4 + 2;

  player.atk = clamp8(atk);
  player.def = clamp8(def);
  player.matk = clamp8(player.matk_base + g[AFF_MATK] + stat_alloc[ALLOC_INT] +
    (buffs.atk ? player.matk_base / 4 + 2 : 0));
  player.mdef = clamp8(player.mdef_base + g[AFF_MDEF] + stat_alloc[ALLOC_SPI]);
  player.agl = clamp8(player.agl_base + g[AFF_AGL] + stat_alloc[ALLOC_DEX]);

  if (base_max_hp) {
    player.max_hp = base_max_hp + g[AFF_HP] + 6 * stat_alloc[ALLOC_VIT];
    player.max_sp = base_max_sp + g[AFF_SP] + 2 * stat_alloc[ALLOC_SPI];
  }
  if (player.hp > player.max_hp) player.hp = player.max_hp;
  if (player.sp > player.max_sp) player.sp = player.max_sp;

  loot_bonus.crit = clamp8(g[AFF_CRIT] + player.agl / 16);
  if (loot_bonus.crit > 60) loot_bonus.crit = 60;
  loot_bonus.leech = clamp8(g[AFF_LEECH]);
  loot_bonus.gold_find = clamp8(g[AFF_GOLD]);
  loot_bonus.regen = clamp8(g[AFF_REGEN]);
}

void loot_on_level_up(uint8_t levels) BANKED {
  stat_points += 3 * levels;
  skill_points += levels;
}

//------------------------------------------------------------------------------
// Items
//------------------------------------------------------------------------------

void loot_generate(uint8_t level, uint8_t boost, LootItem *out) BANKED {
  clear_item(out);
  out->base = 1 + rand() % (LOOT_BASE_COUNT - 1);
  out->ilvl = level == 0 ? 1 : level;
  out->rarity = roll_rarity(boost);

  uint8_t n = out->rarity == RARITY_COMMON ? 0 :
    (out->rarity == RARITY_MAGIC ? 1 : 2);
  for (uint8_t a = 0; a < n; a++) {
    uint8_t affix;
    do {
      affix = 1 + rand() % (AFF_COUNT - 1);
    } while (a == 1 && affix == out->aff[0]);
    out->aff[a] = affix;
    out->val[a] = roll_affix_value(affix, out->ilvl, out->rarity);
    if (out->rarity == RARITY_LEGENDARY)
      out->val[a] = clamp8((uint16_t)out->val[a] * 3 / 2);
  }
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
  if (it->base == 0 || it->base >= LOOT_BASE_COUNT) {
    buf[0] = 0;
    return;
  }
  strcpy(buf, loot_bases[it->base].name);
}

void loot_item_stats(const LootItem *it, char *buf) BANKED {
  buf[0] = 0;
  if (it->base == 0 || it->base >= LOOT_BASE_COUNT)
    return;
  const LootBase *b = &loot_bases[it->base];
  char *p = buf;
  sprintf(p, "%s%u", affix_codes[b->implicit], implicit_value(it));
  p += strlen(p);
  for (uint8_t a = 0; a < 2; a++) {
    if (!it->aff[a] || it->aff[a] >= AFF_COUNT)
      continue;
    sprintf(p, " %s%u", affix_codes[it->aff[a]], it->val[a]);
    p += strlen(p);
  }
}

uint8_t loot_item_slot(const LootItem *it) BANKED {
  return loot_bases[it->base < LOOT_BASE_COUNT ? it->base : 0].slot;
}

uint16_t loot_item_value(const LootItem *it) BANKED {
  if (it->base == 0)
    return 0;
  return 4 + it->ilvl + (uint16_t)it->ilvl * it->rarity * 2 +
    (uint16_t)it->rarity * it->rarity * 10;
}

void loot_equip_from_bag(uint8_t idx) BANKED {
  if (idx >= LOOT_BAG_SIZE || loot_bag[idx].base == 0)
    return;
  const uint8_t slot = loot_item_slot(loot_bag + idx);
  LootItem tmp = loot_equip[slot];
  loot_equip[slot] = loot_bag[idx];
  loot_bag[idx] = tmp;
  loot_recalc();
}

void loot_unequip(uint8_t slot) BANKED {
  if (slot >= LOOT_EQUIP_SLOTS || loot_equip[slot].base == 0)
    return;
  if (loot_add_to_bag(loot_equip + slot))
    clear_item(loot_equip + slot);
  loot_recalc();
}

void loot_sell(uint8_t idx) BANKED {
  if (idx >= LOOT_BAG_SIZE || loot_bag[idx].base == 0)
    return;
  uint16_t v = loot_item_value(loot_bag + idx);
  loot_gold = loot_gold + v > 60000 ? 60000 : loot_gold + v;
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
// Shop
//------------------------------------------------------------------------------

void loot_restock_shop(uint8_t level) BANKED {
  for (uint8_t k = 0; k < LOOT_SHOP_SIZE; k++)
    loot_generate(level, 30, loot_shop + k);
}

uint16_t loot_price(uint8_t idx, uint8_t level) BANKED {
  switch (idx) {
  case 0: case 1: case 2: case 3:
    return loot_shop[idx].base ? loot_item_value(loot_shop + idx) * 3 : 0;
  case 4: return 10 + level;          // Potion
  case 5: return 15 + level;          // Ether
  case 6: return 60 + 4 * level;      // Elixir
  default: return 8;                  // Remedy
  }
}

bool loot_buy(uint8_t idx, uint8_t level) BANKED {
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
    Item *item = inventory + ids[idx - 4];
    if (item->quantity >= 99)
      return false;
    add_items(ids[idx - 4], 1);
  }
  loot_gold -= price;
  return true;
}

//------------------------------------------------------------------------------
// Rewards
//------------------------------------------------------------------------------

static void add_gold(uint16_t g) {
  g += (g * loot_bonus.gold_find) / 100;
  loot_gold = loot_gold + g > 60000 ? 60000 : loot_gold + g;
}

static char *append_item(char *p, const LootItem *it, bool kept) {
  sprintf(p, kept ? "\n%s %s" : "\n%s %s (full)",
    rarity_names[it->rarity], loot_bases[it->base].name);
  return p + strlen(p);
}

void loot_open_chest(uint8_t level, char *msg) BANKED {
  LootItem it;
  uint16_t g = (uint16_t)level * 5 + d16() + 5;
  add_gold(g);
  loot_generate(level, 25, &it);
  bool kept = loot_add_to_bag(&it);
  sprintf(msg, "Treasure!\n+%u gold", g);
  append_item(msg + strlen(msg), &it, kept);
  play_sound(sfx_level_up);
}

uint8_t loot_roll_kill(uint8_t level, uint8_t boss, char *msg) BANKED {
  char *p = msg + strlen(msg);
  uint8_t drops = 0;

  uint16_t g = (uint16_t)level * 2 + d8() + 1;
  if (boss == 1) g *= 3;
  if (boss == 2) g *= 8;
  add_gold(g);

  uint8_t chance = boss ? 255 : 70;
  uint8_t rolls = boss == 2 ? 2 : 1;
  for (uint8_t k = 0; k < rolls; k++) {
    if (d256() > chance)
      continue;
    LootItem it;
    loot_generate(level, boss == 2 ? 60 : (boss ? 35 : 0), &it);
    bool kept = loot_add_to_bag(&it);
    if (drops < 2)
      p = append_item(p, &it, kept);
    drops++;
  }

  // Occasional potion drop.
  if ((d16()) == 0) {
    add_items(ITEM_POTION, 1);
  }
  return drops;
}

bool loot_use_consumable(uint8_t entry) BANKED {
  if (entry == HOTBAR_POTION) {
    if (inventory[ITEM_POTION].quantity == 0 || player.hp >= player.max_hp)
      return false;
    remove_item(ITEM_POTION);
    uint16_t heal = player.max_hp / 3 + 10;
    player.hp = player.hp + heal > player.max_hp ? player.max_hp : player.hp + heal;
    return true;
  }
  if (entry == HOTBAR_ETHER) {
    if (inventory[ITEM_ETHER].quantity == 0 || player.sp >= player.max_sp)
      return false;
    remove_item(ITEM_ETHER);
    uint16_t heal = player.max_sp / 3 + 5;
    player.sp = player.sp + heal > player.max_sp ? player.max_sp : player.sp + heal;
    return true;
  }
  return false;
}

//------------------------------------------------------------------------------
// Saving
//------------------------------------------------------------------------------

#define SAVE_ADDR ((uint8_t *)0xA000)
#define SAVE_DATA ((uint8_t *)0xA008)
#define SAVE_VERSION 1

static uint8_t *save_ptr;
static uint8_t save_sum;
static bool save_write;

/**
 * Streams one block to (or from) the save area, updating the checksum.
 */
static void stream(void *data, uint16_t n) {
  uint8_t *d = (uint8_t *)data;
  while (n--) {
    if (save_write)
      *save_ptr = *d;
    else if (data)
      *d = *save_ptr;
    save_sum += *save_ptr++;
    d++;
  }
}

/**
 * Streams every piece of saved state. With `save_write` false and `load`
 * false it only computes the checksum.
 */
static void stream_all(bool load) {
  save_ptr = SAVE_DATA;
  save_sum = 0;
  stream(load || save_write ? &player : NULL, sizeof(Player));
  for (uint8_t k = 0; k < INVENTORY_LEN; k++)
    stream(load || save_write ? &inventory[k].quantity : NULL, 1);
  stream(load || save_write ? loot_bag : NULL, sizeof(loot_bag));
  stream(load || save_write ? loot_stash : NULL, sizeof(loot_stash));
  stream(load || save_write ? loot_equip : NULL, sizeof(loot_equip));
  stream(load || save_write ? &loot_gold : NULL, sizeof(loot_gold));
  stream(load || save_write ? &stat_points : NULL, 1);
  stream(load || save_write ? &skill_points : NULL, 1);
  stream(load || save_write ? stat_alloc : NULL, sizeof(stat_alloc));
  stream(load || save_write ? skill_rank : NULL, sizeof(skill_rank));
  stream(load || save_write ? hotbar : NULL, sizeof(hotbar));
  stream(load || save_write ? &total_kills : NULL, sizeof(total_kills));
  stream(load || save_write ? quests : NULL, sizeof(quests));
  stream(load || save_write ? &base_max_hp : NULL, sizeof(base_max_hp));
  stream(load || save_write ? &base_max_sp : NULL, sizeof(base_max_sp));
  stream(load || save_write ? &world_last_town : NULL, 1);
  stream(load || save_write ? &world_best_tower_floor : NULL, 1);
  stream(load || save_write ? world_visited : NULL, sizeof(world_visited));
}

void save_game(void) BANKED {
  SWITCH_RAM(1);
  save_write = true;
  stream_all(false);
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
  stream_all(false);
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
  save_write = false;
  stream_all(true);
  SWITCH_RAM(0);
  memset(&buffs, 0, sizeof(buffs));
  hotbar_sel = 0;
  loot_recalc();
  return true;
}

//------------------------------------------------------------------------------
// Quests
//------------------------------------------------------------------------------

static uint16_t capped_mul(uint16_t a, uint16_t b) {
  uint32_t v = (uint32_t)a * b;
  return v > 60000 ? 60000 : (uint16_t)v;
}

static void make_offer(Quest *q) {
  const uint8_t lvl = player.level;
  const uint16_t mxp = calc_monster_exp(lvl, B_TIER);
  memset(q, 0, sizeof(Quest));
  switch (rand() % 5) {
  case 0:
  case 1:
    q->type = QUEST_KILL;
    q->goal = 8 + (rand() & 7);
    q->gold = capped_mul(lvl, q->goal * 2);
    q->xp = capped_mul(mxp, q->goal / 2 + 1);
    break;
  case 2:
    q->type = QUEST_ELITE;
    q->goal = 2 + (rand() & 1);
    q->gold = capped_mul(lvl, 20 * q->goal);
    q->xp = capped_mul(mxp, 3 * q->goal);
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
      q->gold = capped_mul(lvl, 25);
      q->xp = capped_mul(mxp, 4);
      return;
    }
  }
    // fall through
  default:
    if (rand() & 1) {
      q->type = QUEST_TOWER;
      q->target = world_best_tower_floor + 3;
      if (q->target > WORLD_TOWER_MAX_FLOOR)
        q->target = WORLD_TOWER_MAX_FLOOR;
      q->goal = 1;
      q->gold = capped_mul(q->target, 30);
      q->xp = capped_mul(mxp, 5);
    } else {
      q->type = QUEST_BOSS;
      q->goal = 1;
      q->gold = capped_mul(lvl, 50);
      q->xp = capped_mul(mxp, 8);
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
  default:
    sprintf(buf, "-");
  }
}

void quest_reward_text(const Quest *q, char *buf) BANKED {
  if (q->type == QUEST_NONE)
    buf[0] = 0;
  else
    sprintf(buf, " %uG %uXP", q->gold, q->xp);
}

static void complete_quest(Quest *q) {
  char desc[20];
  const uint8_t before = player.level;
  quest_describe(q, desc);
  add_gold(q->gold);
  if (level_up(q->xp))
    loot_on_level_up(player.level - before);
  loot_recalc();
  sprintf(quest_notice, "Quest complete!\n%s\n+%uG +%uXP", desc, q->gold, q->xp);
  quest_notice_pending = true;
  q->type = QUEST_NONE;
}

void quest_event(uint8_t event, uint8_t value) BANKED {
  if (event == QUEST_EVENT_KILL)
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
    }
    if (!hit)
      continue;
    if (q->progress < 255)
      q->progress++;
    if (q->progress >= q->goal)
      complete_quest(q);
  }
}
