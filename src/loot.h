#ifndef _LOOT_H
#define _LOOT_H

/**
 * Items and economy for the ARPG systems (bank 25): weapons and armor with
 * material tiers for item levels 1-720, rarities, up to three random affixes,
 * forge upgrades (+1..+10), the bag, stash, equipment, gold, town shop,
 * quests and saving.
 */

#include <gb/gb.h>
#include <stdint.h>
#include <stdbool.h>

#define LOOT_BANK 25

#define LOOT_BAG_SIZE 16
#define LOOT_STASH_SIZE 24
#define LOOT_EQUIP_SLOTS 4
#define LOOT_SHOP_SIZE 4
#define LOOT_MAX_UPGRADE 10

/**
 * Equipment slots.
 */
typedef enum LootSlot {
  LOOT_SLOT_WEAPON,
  LOOT_SLOT_ARMOR,
  LOOT_SLOT_OFFHAND,
  LOOT_SLOT_TRINKET,
} LootSlot;

typedef enum Rarity {
  RARITY_WORN,
  RARITY_STURDY,
  RARITY_FINE,
  RARITY_ENCHANTED,
  RARITY_RARE,
  RARITY_EPIC,
  RARITY_LEGENDARY,
  RARITY_RELIC,
  RARITY_MYTHIC,
  RARITY_COUNT,
} Rarity;

/**
 * Item stat types. STR..SPI must stay in the same order as `Attr`.
 */
typedef enum Affix {
  AFF_NONE,
  AFF_ATK,
  AFF_MATK,
  AFF_DEF,
  AFF_MDEF,
  AFF_HP,
  AFF_SP,
  AFF_STR,
  AFF_DEX,
  AFF_INT,
  AFF_VIT,
  AFF_END,
  AFF_SPI,
  AFF_CRIT,
  AFF_CDMG,
  AFF_ASPD,
  AFF_DODGE,
  AFF_BLOCK,
  AFF_LEECH,
  AFF_GOLD,
  AFF_MF,
  AFF_REGEN,
  AFF_CDR,
  AFF_COUNT,
} Affix;

/**
 * A piece of equipment. `base` 0 means the slot is empty.
 */
typedef struct LootItem {
  uint8_t base;
  uint8_t rarity;
  uint16_t ilvl;
  uint8_t upgrade;
  uint8_t aff[3];
  uint16_t val[3];
} LootItem;

extern LootItem loot_bag[LOOT_BAG_SIZE];
extern LootItem loot_stash[LOOT_STASH_SIZE];
extern LootItem loot_equip[LOOT_EQUIP_SLOTS];
extern LootItem loot_shop[LOOT_SHOP_SIZE];
extern uint32_t loot_gold;
extern uint16_t total_kills;

/**
 * Resets items, gold and quests for a new game.
 */
void loot_new_game(void) BANKED;

/**
 * Hook called by the legacy stat code; recomputes the hero's stats.
 */
void loot_capture_base(void) BANKED;

/**
 * Sums every stat granted by equipped items into `totals[AFF_COUNT]`.
 */
void loot_gear_totals(uint16_t *totals) BANKED;

/**
 * Rolls a random item.
 * @param level Item level (1-720).
 * @param boost Rarity boost (0 normal, higher = rarer).
 */
void loot_generate(uint16_t level, uint8_t boost, LootItem *out) BANKED;

/**
 * Rolls an item of a fixed rarity (debug).
 */
void loot_generate_rarity(uint16_t level, uint8_t rarity, LootItem *out) BANKED;

/** Rolls equipment in the requested slot at a fixed rarity. */
void loot_generate_rarity_slot(uint16_t level, uint8_t rarity, uint8_t slot,
  LootItem *out) BANKED;

/**
 * Adds an item to the bag. @return `false` if the bag is full.
 */
bool loot_add_to_bag(const LootItem *it) BANKED;

/**
 * Writes the short item name ("Sword+3", max 12 chars) into buf.
 */
void loot_item_name(const LootItem *it, char *buf) BANKED;

/**
 * Writes the long item name ("Rare Mithril Sword+3") into buf.
 */
void loot_item_fullname(const LootItem *it, char *buf) BANKED;

/**
 * Writes one line of the item's stats (line 0 or 1, max 20 chars).
 */
void loot_item_stats(const LootItem *it, uint8_t line, char *buf) BANKED;

/**
 * @return The equipment slot for the item.
 */
uint8_t loot_item_slot(const LootItem *it) BANKED;

/**
 * @return Gold value of the item when sold.
 */
uint16_t loot_item_value(const LootItem *it) BANKED;

/**
 * Equips the bag item at `idx`, swapping with the currently equipped item.
 */
void loot_equip_from_bag(uint8_t idx) BANKED;

/**
 * Moves the equipped item in `slot` into the bag (if there is room).
 */
void loot_unequip(uint8_t slot) BANKED;

/**
 * Sells the bag item at `idx`.
 */
void loot_sell(uint8_t idx) BANKED;

/**
 * Moves bag[idx] into the stash (`to_stash`) or stash[idx] into the bag.
 */
void loot_stash_move(uint8_t idx, bool to_stash) BANKED;

/**
 * Restocks the town shop with items for the given level.
 */
void loot_restock_shop(uint16_t level) BANKED;

/**
 * Buys shop entry `idx` (0-3 gear, 4-7 consumables). @return success.
 */
bool loot_buy(uint8_t idx, uint16_t level) BANKED;

/**
 * @return Price of shop entry `idx`.
 */
uint16_t loot_price(uint8_t idx, uint16_t level) BANKED;

/**
 * @return Gold cost to upgrade the equipped item in `slot`, or 0 if it can't
 *   be upgraded.
 */
uint32_t loot_forge_cost(uint8_t slot) BANKED;

/**
 * @return Success chance (%) of the next upgrade for `slot`.
 */
uint8_t loot_forge_chance(uint8_t slot) BANKED;

/**
 * Tries to upgrade the equipped item in `slot`.
 * @return 0 not possible / not enough gold, 1 success, 2 failed (gold spent).
 */
uint8_t loot_forge(uint8_t slot) BANKED;

/**
 * Adds gold (applies gold find).
 */
void loot_add_gold(uint32_t gold) BANKED;

/**
 * Opens a treasure chest: grants gold + an item and writes a message.
 */
void loot_open_chest(uint16_t level, char *msg) BANKED;

/**
 * Rolls kill rewards (gold + possible item drop) and appends to `msg`.
 * @param rank 0 = normal, 1 = elite, 2 = boss.
 * @return number of items dropped.
 */
uint8_t loot_roll_kill(uint16_t level, uint8_t rank, char *msg) BANKED;

/**
 * Uses a consumable from the quick bar. @return `true` if used.
 */
bool loot_use_consumable(uint8_t hotbar_entry) BANKED;

//------------------------------------------------------------------------------
// Saving (battery backed cartridge RAM, bank 1)
//------------------------------------------------------------------------------

void save_game(void) BANKED;
bool save_exists(void) BANKED;
bool load_game(void) BANKED;

//------------------------------------------------------------------------------
// Quests
//------------------------------------------------------------------------------

#define QUEST_MAX 3

typedef enum QuestType {
  QUEST_NONE,
  QUEST_KILL,
  QUEST_ELITE,
  QUEST_ZONE,
  QUEST_TOWER,
  QUEST_BOSS,
  QUEST_RAID,
} QuestType;

typedef enum QuestEvent {
  QUEST_EVENT_KILL,
  QUEST_EVENT_ELITE,
  QUEST_EVENT_BOSS,
  QUEST_EVENT_ZONE,
  QUEST_EVENT_TOWER,
  QUEST_EVENT_RAID,
} QuestEvent;

typedef struct Quest {
  uint8_t type;
  uint8_t target;
  uint8_t progress;
  uint8_t goal;
  uint32_t gold;
  uint32_t xp;
} Quest;

extern Quest quests[QUEST_MAX];
extern Quest quest_offers[QUEST_MAX];
extern char quest_notice[64];
extern bool quest_notice_pending;

void quest_event(uint8_t event, uint8_t value) BANKED;
void quest_generate_offers(void) BANKED;
bool quest_accept(uint8_t idx) BANKED;
void quest_abandon(uint8_t idx) BANKED;
void quest_describe(const Quest *q, char *buf) BANKED;
void quest_reward_text(const Quest *q, char *buf) BANKED;

#endif
