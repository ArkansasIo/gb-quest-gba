#ifndef _LOOT_H
#define _LOOT_H

/**
 * ARPG progression systems: randomized loot (rarities + affixes), the bag,
 * stash, equipment, gold, stat/skill points, the skill hotbar and quests.
 * Code lives in bank 25 (loot.c).
 */

#include <gb/gb.h>
#include <stdint.h>
#include <stdbool.h>

#define LOOT_BANK 25

#define LOOT_BAG_SIZE 16
#define LOOT_STASH_SIZE 24
#define LOOT_EQUIP_SLOTS 4
#define LOOT_SHOP_SIZE 4

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
  RARITY_COMMON,
  RARITY_MAGIC,
  RARITY_RARE,
  RARITY_EPIC,
  RARITY_LEGENDARY,
  RARITY_COUNT,
} Rarity;

typedef enum Affix {
  AFF_NONE,
  AFF_ATK,
  AFF_DEF,
  AFF_MATK,
  AFF_MDEF,
  AFF_AGL,
  AFF_HP,
  AFF_SP,
  AFF_CRIT,
  AFF_LEECH,
  AFF_GOLD,
  AFF_REGEN,
  AFF_COUNT,
} Affix;

/**
 * A piece of loot. `base` 0 means the slot is empty.
 */
typedef struct LootItem {
  uint8_t base;
  uint8_t rarity;
  uint8_t ilvl;
  uint8_t aff[2];
  uint8_t val[2];
} LootItem;

/**
 * Character stats that can be raised with stat points.
 */
typedef enum StatAlloc {
  ALLOC_STR,  // +ATK
  ALLOC_DEX,  // +AGL
  ALLOC_INT,  // +MATK
  ALLOC_VIT,  // +HP
  ALLOC_END,  // +DEF
  ALLOC_SPI,  // +MDEF +SP
  ALLOC_COUNT,
} StatAlloc;

/**
 * Derived combat bonuses computed from equipment.
 */
typedef struct LootBonuses {
  uint8_t crit;       // % chance for double damage
  uint8_t leech;      // % of damage dealt returned as HP
  uint8_t gold_find;  // % bonus gold
  uint8_t regen;      // HP regenerated every 2 seconds
} LootBonuses;

/**
 * Hotbar entries beyond the six class skills.
 */
#define HOTBAR_EMPTY 0xFF
#define HOTBAR_POTION 6
#define HOTBAR_ETHER 7
#define HOTBAR_SLOTS 4

/**
 * Timed buffs (in frames) applied by skills / consumables.
 */
typedef struct BuffState {
  uint16_t atk;
  uint16_t def;
  uint16_t haste;
} BuffState;

extern LootItem loot_bag[LOOT_BAG_SIZE];
extern LootItem loot_stash[LOOT_STASH_SIZE];
extern LootItem loot_equip[LOOT_EQUIP_SLOTS];
extern LootItem loot_shop[LOOT_SHOP_SIZE];
extern uint16_t loot_gold;
extern LootBonuses loot_bonus;
extern BuffState buffs;

extern uint8_t stat_points;
extern uint8_t skill_points;
extern uint8_t stat_alloc[ALLOC_COUNT];
extern uint8_t skill_rank[6];
extern uint8_t hotbar[HOTBAR_SLOTS];
extern uint8_t hotbar_sel;
extern uint16_t total_kills;

/**
 * Resets all progression state for a new game.
 */
void loot_new_game(void) BANKED;

/**
 * Captures the class/level base HP and SP (call after the base stats change).
 */
void loot_capture_base(void) BANKED;

/**
 * Recomputes the player's effective stats from base stats, equipment, stat
 * allocation and buffs.
 */
void loot_recalc(void) BANKED;

/**
 * Grants stat and skill points for gained levels.
 */
void loot_on_level_up(uint8_t levels) BANKED;

/**
 * Rolls a random item.
 * @param level Item level.
 * @param boost Rarity boost (0 normal, higher = rarer).
 */
void loot_generate(uint8_t level, uint8_t boost, LootItem *out) BANKED;

/**
 * Adds an item to the bag. @return `false` if the bag is full.
 */
bool loot_add_to_bag(const LootItem *it) BANKED;

/**
 * Writes the item name (max 12 chars) into buf.
 */
void loot_item_name(const LootItem *it, char *buf) BANKED;

/**
 * Writes a short stat line for the item (max 18 chars).
 */
void loot_item_stats(const LootItem *it, char *buf) BANKED;

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
 * Sells (salvages) the bag item at `idx`.
 */
void loot_sell(uint8_t idx) BANKED;

/**
 * Moves an item between the bag and the stash.
 * @param to_stash `true` to move bag[idx] into the stash, else stash[idx]
 *   into the bag.
 */
void loot_stash_move(uint8_t idx, bool to_stash) BANKED;

/**
 * Restocks the town shop with items for the given level.
 */
void loot_restock_shop(uint8_t level) BANKED;

/**
 * Buys shop entry `idx` (0-3 gear, 4.. consumables). @return `true` on success.
 */
bool loot_buy(uint8_t idx, uint8_t level) BANKED;

/**
 * @return Price of shop entry `idx`.
 */
uint16_t loot_price(uint8_t idx, uint8_t level) BANKED;

/**
 * Opens a treasure chest: grants gold + an item and writes a message.
 */
void loot_open_chest(uint8_t level, char *msg) BANKED;

/**
 * Rolls kill rewards (gold + possible item drop) and appends to `msg`.
 * @param boss 0 = normal, 1 = elite, 2 = boss.
 * @return number of items dropped.
 */
uint8_t loot_roll_kill(uint8_t level, uint8_t boss, char *msg) BANKED;

/**
 * Uses a consumable from the quick bar. @return `true` if used.
 */
bool loot_use_consumable(uint8_t hotbar_entry) BANKED;

//------------------------------------------------------------------------------
// Saving (battery backed cartridge RAM, bank 1)
//------------------------------------------------------------------------------

/**
 * Saves the game (character, loot, quests and world progress).
 */
void save_game(void) BANKED;

/**
 * @return `true` if a valid save exists.
 */
bool save_exists(void) BANKED;

/**
 * Loads the saved game. @return `false` if there is no valid save.
 */
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
} QuestType;

typedef enum QuestEvent {
  QUEST_EVENT_KILL,
  QUEST_EVENT_ELITE,
  QUEST_EVENT_BOSS,
  QUEST_EVENT_ZONE,
  QUEST_EVENT_TOWER,
} QuestEvent;

typedef struct Quest {
  uint8_t type;
  uint8_t target;
  uint8_t progress;
  uint8_t goal;
  uint16_t gold;
  uint16_t xp;
} Quest;

extern Quest quests[QUEST_MAX];
extern Quest quest_offers[QUEST_MAX];

/**
 * Message describing a completed quest; shown by the ARPG system when the map
 * is idle.
 */
extern char quest_notice[64];
extern bool quest_notice_pending;

/**
 * Reports a game event to the quest log.
 */
void quest_event(uint8_t event, uint8_t value) BANKED;

/**
 * Generates fresh quest offers for the current town.
 */
void quest_generate_offers(void) BANKED;

/**
 * Accepts quest offer `idx`. @return `false` if the log is full.
 */
bool quest_accept(uint8_t idx) BANKED;

/**
 * Abandons active quest `idx`.
 */
void quest_abandon(uint8_t idx) BANKED;

/**
 * Writes a description of the quest (max 18 chars) into buf.
 */
void quest_describe(const Quest *q, char *buf) BANKED;

/**
 * Writes the reward line of the quest (max 18 chars) into buf.
 */
void quest_reward_text(const Quest *q, char *buf) BANKED;

#endif
