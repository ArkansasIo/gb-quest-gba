#pragma bank 19

/**
 * ARPG pause menu screens. See arpg_menu.h.
 */

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdio.h>
#include <string.h>

#include "arpg.h"
#include "arpg_menu.h"
#include "core.h"
#include "item.h"
#include "loot.h"
#include "map.h"
#include "player.h"
#include "progress.h"
#include "professions.h"
#include "settings.h"
#include "skills.h"
#include "sound.h"
#include "world.h"

bool arpg_menu_used;
bool arpg_menu_returned;

typedef enum Screen {
  SCREEN_SKILLS,
  SCREEN_ITEMS,
  SCREEN_QUESTS,
  SCREEN_TOWN,
  SCREEN_STATS,
  SCREEN_WORLD,
  SCREEN_JOURNAL,
  SCREEN_OPTIONS,
  SCREEN_CONTROLS,
  SCREEN_SAVE,
  SCREEN_PROFESSIONS,
} Screen;

static uint8_t screen;
static uint8_t cursor;
static uint8_t scroll;
static uint8_t tab;
static uint8_t hot_slot;
static uint8_t character_tab;
static uint8_t gear_tab;
static uint8_t detail_mode;
static bool profession_crafted;
static bool save_confirmed;
static uint8_t offers_zone = 0xFF;
static uint8_t shop_zone = 0xFF;

#define CURSOR_SPRITE 16
#define PAL_TEXT 7
#define PAL_HEAD 1
#define PAL_RARITY 2
#define MENU_CURSOR_TILE ((uint8_t)('>' + FONT_OFFSET))

#define SCR(x, y) VRAM_BACKGROUND_XY(x, y)

/**
 * Text palettes: glyphs use color 3 on background color 1.
 */
static const palette_color_t menu_palettes[6 * 4] = {
  // Pokémon-style blue menu panels with a light text field and dark heading.
  RGB_WHITE, RGB8(20, 48, 112), RGB8(65, 112, 176), RGB_WHITE,
  RGB_WHITE, RGB8(202, 218, 234), RGB8(20, 48, 112), RGB8(22, 6, 4),
  // Magic
  RGB_WHITE, RGB8(101, 128, 186), RGB8(3, 37, 135), RGB8(0, 40, 200),
  // Rare
  RGB_WHITE, RGB8(101, 128, 186), RGB8(3, 37, 135), RGB8(255, 230, 60),
  // Epic
  RGB_WHITE, RGB8(101, 128, 186), RGB8(3, 37, 135), RGB8(150, 20, 170),
  // Legendary
  RGB_WHITE, RGB8(101, 128, 186), RGB8(3, 37, 135), RGB8(255, 120, 20),
};

static const palette_color_t cursor_palette[4] = {
  RGB_WHITE, RGB8(101, 128, 186), RGB8(3, 37, 135), RGB_WHITE,
};

static const char *const slot_tags[LOOT_EQUIP_SLOTS] = { "W", "A", "O", "T" };
static const char *const rarity_names[RARITY_COUNT] = {
  "Worn", "Sturdy", "Fine", "Enchanted", "Rare", "Epic", "Legend",
  "Relic", "Mythic"
};
static const char *const alloc_names[ATTR_COUNT] = {
  "STR +ATK", "DEX +AGL", "INT +MAT", "VIT +HP ", "END +DEF", "SPI +MDF"
};

static uint8_t rarity_palette(uint8_t rarity) {
  static const uint8_t palettes[RARITY_COUNT] = {
    2, 2, 2, 3, 3, 4, 4, 5, 5
  };
  return palettes[rarity < RARITY_COUNT ? rarity : 0];
}

//------------------------------------------------------------------------------
// Drawing helpers
//------------------------------------------------------------------------------

/**
 * Pads `buf` with spaces up to `at` characters (GBDK sprintf has no field
 * widths). @return Pointer to the end of the string.
 */
static char *col(char *buf, uint8_t at) {
  uint8_t n = strlen(buf);
  while (n < at)
    buf[n++] = 0x20;
  buf[n] = 0;
  return buf + n;
}

static void text(uint8_t x, uint8_t y, const char *s, uint8_t max) {
  core.draw_text(SCR(x, y), s, max);
}

static void row_pal(uint8_t y, uint8_t pal) {
  uint8_t *vram = SCR(0, y);
  VBK_REG = VBK_ATTRIBUTES;
  for (uint8_t k = 0; k < 20; k++)
    set_vram_byte(vram + k, 0x08 | pal);
  VBK_REG = VBK_TILES;
}

static void line(uint8_t y, const char *s, uint8_t pal) {
  row_pal(y, pal);
  text(0, y, s, 20);
}

static void clear_rows(uint8_t from, uint8_t to) {
  for (uint8_t y = from; y <= to; y++)
    line(y, "", PAL_TEXT);
}

static void put_cursor(uint8_t row) {
  move_sprite(CURSOR_SPRITE, 8, 16 + row * 8);
}

static void item_line(uint8_t y, const char *prefix, const LootItem *it) {
  char buf[24];
  char name[14];
  if (it->base == 0) {
    sprintf(buf, "%s -", prefix);
    line(y, buf, PAL_TEXT);
    return;
  }
  loot_item_name(it, name);
  strcpy(buf, prefix);
  strcat(buf, name);
  sprintf(col(buf, strlen(prefix) + 10), " L%u", it->ilvl);
  line(y, buf, rarity_palette(it->rarity));
}

static void item_detail(uint8_t y, const LootItem *it) {
  char buf[24];
  if (it->base == 0) {
    clear_rows(y, y + 1);
    return;
  }
  sprintf(buf, "R%u %s", it->rarity + 1, rarity_names[it->rarity]);
  line(y, buf, rarity_palette(it->rarity));
  loot_item_stats(it, 0, buf);
  line(y + 1, buf, PAL_TEXT);
}

static void menu_title(const char *name) {
  char buf[32];
  sprintf(buf, "[ %s ]", name);
  line(0, buf, PAL_HEAD);
}

//------------------------------------------------------------------------------
// Pause menu entries
//------------------------------------------------------------------------------

static const uint8_t entry_col[3] = { 2, 8, 14 };

void arpg_menu_draw_entries(uint8_t page) BANKED {
  static const char *const names[ARPG_MENU_PAGE_COUNT][ARPG_MENU_PAGE_SIZE] = {
    { "SKILLS", "GEAR", "QUEST", "TOWN", "CHAR", "MORE" },
    { "WORLD", "JOURNAL", "OPTIONS", "CONTROLS", "SAVE", "PROF" }
  };
  uint8_t *vram = VRAM_WINDOW_XY(1, 29);
  for (uint8_t k = 0; k < 18; k++)
    set_vram_byte(vram + k, FONT_SPACE);
  if (page >= ARPG_MENU_PAGE_COUNT)
    page = 0;
  for (uint8_t k = 0; k < ARPG_MENU_PAGE_SIZE; k++)
    core.draw_text(VRAM_WINDOW_XY(entry_col[k % 3], k < 3 ? 28 : 30),
      names[page][k], 6);
  core.draw_text(VRAM_WINDOW_XY(1, 31), "A:Open SEL:Pg B:Exit", 20);
  core.load_sprite_palette(cursor_palette, 7, 1);
  set_sprite_tile(CURSOR_SPRITE, MENU_CURSOR_TILE);
  set_sprite_prop(CURSOR_SPRITE, 0x0F);
  for (uint8_t k = 1; k < 4; k++)
    move_sprite(CURSOR_SPRITE + k, 0, 0);
}

void arpg_menu_draw_cursor(uint8_t entry) BANKED {
  const uint8_t col = entry_col[entry % 3] - 1;
  const uint8_t row = entry < 3 ? 14 : 15;
  set_sprite_tile(CURSOR_SPRITE, MENU_CURSOR_TILE);
  set_sprite_prop(CURSOR_SPRITE, 0x0F);
  move_sprite(CURSOR_SPRITE, 8 + col * 8, 16 + row * 8);
  for (uint8_t k = 1; k < 4; k++)
    move_sprite(CURSOR_SPRITE + k, 0, 0);
}

//------------------------------------------------------------------------------
// Items screen
//------------------------------------------------------------------------------

#define ITEMS_BAG_ROWS 7

static uint8_t bag_count(void) {
  uint8_t n = 0;
  for (uint8_t k = 0; k < LOOT_BAG_SIZE; k++)
    if (loot_bag[k].base) n++;
  return n;
}

static void draw_items(void) {
  char buf[24];
  clear_rows(1, 16);
  if (detail_mode == 2) {
    const LootItem *it = gear_tab ? loot_bag + cursor : loot_equip + cursor;
    char name[14];
    menu_title("ITEM DETAILS");
    if (!it->base) {
      line(7, "Empty item slot", PAL_TEXT);
    } else {
      loot_item_name(it, name);
      sprintf(buf, "%s", name); line(2, buf, rarity_palette(it->rarity));
      sprintf(buf, "Rarity: %s", rarity_names[it->rarity]); line(4, buf, PAL_TEXT);
      sprintf(buf, "Item level: %u", it->ilvl); line(5, buf, PAL_TEXT);
      sprintf(buf, "Upgrade: +%u", it->upgrade); line(6, buf, PAL_TEXT);
      sprintf(buf, "Slot: %s", slot_tags[loot_item_slot(it)]); line(7, buf, PAL_TEXT);
      loot_item_stats(it, 0, buf); line(9, buf, PAL_TEXT);
      loot_item_stats(it, 1, buf); line(10, buf, PAL_TEXT);
      sprintf(buf, "Sell value: %uG", loot_item_value(it)); line(12, buf, PAL_TEXT);
    }
    line(17, gear_tab ? "A:Equip SEL:Sell B" : "A:Unequip B:Back", PAL_HEAD);
    return;
  }
  menu_title(gear_tab ? "GEAR: BAG < >" : "GEAR: EQUIP < >");
  if (gear_tab)
    sprintf(buf, "Bag %u/%u items", bag_count(), LOOT_BAG_SIZE);
  else
    sprintf(buf, "Gold:%u  EQUIPPED", loot_gold);
  line(1, buf, PAL_TEXT);
  const LootItem *sel;
  if (!gear_tab) {
    for (uint8_t s = 0; s < LOOT_EQUIP_SLOTS; s++) {
      sprintf(buf, " %s:", slot_tags[s]);
      item_line(2 + s, buf, loot_equip + s);
    }
    line(6, "GEAR SUMMARY", PAL_HEAD);
    uint16_t totals[AFF_COUNT];
    loot_gear_totals(totals);
    sprintf(buf, "ATK+%u MAT+%u", totals[AFF_ATK], totals[AFF_MATK]);
    line(7, buf, PAL_TEXT);
    sprintf(buf, "DEF+%u MDF+%u", totals[AFF_DEF], totals[AFF_MDEF]);
    line(8, buf, PAL_TEXT);
    sprintf(buf, "HP+%u SP+%u", totals[AFF_HP], totals[AFF_SP]);
    line(9, buf, PAL_TEXT);
    clear_rows(10, 13);
    sel = loot_equip + cursor;
    item_detail(14, sel);
    line(16, "Choose slot to remove", PAL_TEXT);
    line(17, "A:Unequip <>:Bag B", PAL_HEAD);
    put_cursor(2 + cursor);
    return;
  }
  line(6, "ITEMS IN BAG", PAL_HEAD);
  for (uint8_t r = 0; r < ITEMS_BAG_ROWS; r++)
    item_line(7 + r, "  ", loot_bag + scroll + r);
  sel = loot_bag + cursor;
  item_detail(14, sel);
  sprintf(buf, "Pot%u Eth%u Eli%u Rem%u",
    inventory[ITEM_POTION].quantity, inventory[ITEM_ETHER].quantity,
    inventory[ITEM_ELIXIR].quantity, inventory[ITEM_REMEDY].quantity);
  line(16, buf, PAL_TEXT);
  line(17, "A:Equip SEL:Sell B", PAL_HEAD);

  put_cursor(7 + cursor - scroll);
}

static void update_items(void) {
  bool redraw = false;
  if (detail_mode == 2) {
    if (was_pressed(J_A)) {
      if (!gear_tab) loot_unequip(cursor);
      else loot_equip_from_bag(cursor);
      detail_mode = 0;
      play_sound(sfx_open_chest);
    } else if (was_pressed(J_SELECT) && gear_tab) {
      loot_sell(cursor);
      detail_mode = 0;
      play_sound(sfx_open_chest);
    } else {
      return;
    }
    draw_items();
    return;
  }
  if (was_pressed(J_LEFT) || was_pressed(J_RIGHT)) {
    gear_tab ^= 1;
    cursor = 0;
    scroll = 0;
    draw_items();
    return;
  }
  const uint8_t max = gear_tab ? LOOT_BAG_SIZE : LOOT_EQUIP_SLOTS;
  if (was_pressed(J_UP) && cursor > 0) {
    cursor--;
    redraw = true;
  } else if (was_pressed(J_DOWN) && cursor + 1 < max) {
    cursor++;
    redraw = true;
  } else if (was_pressed(J_A)) {
    const LootItem *it = gear_tab ? loot_bag + cursor : loot_equip + cursor;
    if (it->base) {
      detail_mode = 2;
      play_sound(sfx_menu_move);
    } else {
      play_sound(sfx_error);
    }
    redraw = true;
  } else if (was_pressed(J_SELECT) && gear_tab) {
    const uint8_t idx = cursor;
    if (loot_bag[idx].base) {
      loot_sell(idx);
      play_sound(sfx_open_chest);
      redraw = true;
    }
  }
  if (!redraw)
    return;
  if (gear_tab) {
    const uint8_t idx = cursor;
    if (idx < scroll) scroll = idx;
    if (idx >= scroll + ITEMS_BAG_ROWS) scroll = idx - ITEMS_BAG_ROWS + 1;
  }
  if (was_pressed(J_UP) || was_pressed(J_DOWN))
    play_sound(sfx_menu_move);
  draw_items();
}

//------------------------------------------------------------------------------
// Skills screen
//------------------------------------------------------------------------------

static void hotbar_short(uint8_t entry, char *out) {
  char name[16];
  arpg_hotbar_name(entry, name);
  name[4] = 0;
  strcpy(out, name);
}

static void draw_skills(void) {
  char buf[24];
  char name[16];
  char desc[22];
  if (detail_mode == 1) {
    SkillInfo info;
    skill_get(cursor, &info);
    skill_name(cursor, name);
    menu_title("SKILL DETAILS");
    sprintf(buf, "%s  R%u/%u", name, skill_rank[cursor], skill_max_rank());
    line(2, buf, PAL_HEAD);
    skill_info_text(cursor, desc);
    sprintf(buf, "%s", desc); line(4, buf, PAL_TEXT);
    sprintf(buf, "Unlock level %u", skill_unlock_level(cursor)); line(6, buf, PAL_TEXT);
    sprintf(buf, "Power %u%%", info.power); line(7, buf, PAL_TEXT);
    sprintf(buf, "SP cost %u", info.cost); line(8, buf, PAL_TEXT);
    sprintf(buf, "Cooldown %us", info.cooldown / 60); line(9, buf, PAL_TEXT);
    sprintf(buf, "Skill points %u", skill_points); line(11, buf, PAL_TEXT);
    line(17, "A:Rank SEL:Hotbar B", PAL_HEAD);
    return;
  }
  menu_title("SKILLS");
  for (uint8_t k = 0; k < SKILL_COUNT; k++) {
    arpg_skill_name(k, name);
    const uint8_t req = arpg_skill_level(k);
    sprintf(buf, "  %s", name);
    char *e = col(buf, 12);
    if (skill_rank[k])
      sprintf(e, " Rank%u", skill_rank[k]);
    else if (player.level >= req)
      strcpy(e, " Learn");
    else
      sprintf(e, " Lv%u", req);
    line(1 + 2 * k, buf, skill_rank[k] ? PAL_RARITY + 2 :
      (player.level >= req ? PAL_RARITY + 1 : PAL_TEXT));
    arpg_skill_info(k, desc);
    sprintf(buf, "   %s", desc);
    line(2 + 2 * k, buf, PAL_TEXT);
  }
  sprintf(buf, "  Potion x%u", inventory[ITEM_POTION].quantity);
  line(13, buf, PAL_TEXT);
  sprintf(buf, "  Ether x%u", inventory[ITEM_ETHER].quantity);
  line(14, buf, PAL_TEXT);
  line(15, "HOTBAR  (<> slot)", PAL_HEAD);

  buf[0] = 0;
  for (uint8_t s = 0; s < HOTBAR_SLOTS; s++) {
    char shortn[8];
    hotbar_short(hotbar[s], shortn);
    char *p = col(buf, s * 5);
    *p++ = s == hot_slot ? '>' : 0x20;
    strcpy(p, shortn);
  }
  line(16, buf, PAL_TEXT);
  line(17, "A:Learn SEL:Set B", PAL_HEAD);

  put_cursor(cursor < SKILL_COUNT ? 1 + 2 * cursor : 13 + (cursor - SKILL_COUNT));
}

static void update_skills(void) {
  bool redraw = true;
  if (detail_mode == 1) {
    if (was_pressed(J_A)) {
      if (skill_rank_up(cursor))
        play_sound(sfx_mid_powerup);
      else
        play_sound(sfx_error);
    } else if (was_pressed(J_SELECT)) {
      for (uint8_t s = 0; s < HOTBAR_SLOTS; s++)
        if (hotbar[s] == cursor) hotbar[s] = HOTBAR_EMPTY;
      hotbar[hot_slot] = cursor;
      hotbar_sel = hot_slot;
      play_sound(sfx_menu_move);
    } else {
      return;
    }
    draw_skills();
    return;
  }
  if (was_pressed(J_UP) && cursor > 0) {
    cursor--;
  } else if (was_pressed(J_DOWN) && cursor < SKILL_COUNT + 1) {
    cursor++;
  } else if (was_pressed(J_LEFT)) {
    hot_slot = (hot_slot + HOTBAR_SLOTS - 1) % HOTBAR_SLOTS;
  } else if (was_pressed(J_RIGHT)) {
    hot_slot = (hot_slot + 1) % HOTBAR_SLOTS;
  } else if (was_pressed(J_A) && cursor < SKILL_COUNT) {
    detail_mode = 1;
  } else if (was_pressed(J_SELECT)) {
    uint8_t entry = cursor < SKILL_COUNT ? cursor :
      (cursor == SKILL_COUNT ? HOTBAR_POTION : HOTBAR_ETHER);
    if (entry < SKILL_COUNT && !skill_rank[entry]) {
      play_sound(sfx_error);
    } else {
      for (uint8_t s = 0; s < HOTBAR_SLOTS; s++)
        if (hotbar[s] == entry)
          hotbar[s] = HOTBAR_EMPTY;
      hotbar[hot_slot] = entry;
      hotbar_sel = hot_slot;
      play_sound(sfx_menu_move);
    }
  } else {
    redraw = false;
  }
  if (redraw)
    draw_skills();
}

//------------------------------------------------------------------------------
// Quests screen
//------------------------------------------------------------------------------

static void quest_rows(uint8_t y, const Quest *q) {
  char buf[24];
  char desc[28];
  quest_describe(q, desc);
  sprintf(buf, "  %s", q->type ? desc : "-");
  line(y, buf, q->type ? PAL_RARITY + 2 : PAL_TEXT);
  quest_reward_text(q, desc);
  line(y + 1, desc, PAL_TEXT);
}

static void draw_quests(void) {
  char buf[24];
  const bool town = world_in_town();
  if (detail_mode == 3) {
    const bool offer = cursor >= QUEST_MAX;
    const Quest *q = offer ? quest_offers + cursor - QUEST_MAX : quests + cursor;
    char desc[28];
    char number[12];
    menu_title(offer ? "QUEST OFFER" : "QUEST DETAILS");
    quest_describe(q, desc); line(3, desc, PAL_HEAD);
    sprintf(buf, "%s", offer ? "Not accepted" : "Progress"); line(5, buf, PAL_TEXT);
    sprintf(buf, "%u / %u complete", q->progress, q->goal); line(6, buf, PAL_TEXT);
    quest_reward_text(q, desc); line(9, desc, PAL_TEXT);
    u32_str(q->gold, number); sprintf(buf, "Gold %s", number); line(11, buf, PAL_TEXT);
    u32_str(q->xp, number); sprintf(buf, "XP %s", number); line(12, buf, PAL_TEXT);
    line(17, offer ? "A:Accept B:Back" : "SEL:Abandon B:Back", PAL_HEAD);
    return;
  }
  menu_title("QUEST LOG");
  for (uint8_t k = 0; k < QUEST_MAX; k++)
    quest_rows(1 + 2 * k, quests + k);
  line(7, town ? "QUEST BOARD" : "Visit a town for", PAL_HEAD);
  if (town) {
    for (uint8_t k = 0; k < QUEST_MAX; k++)
      quest_rows(8 + 2 * k, quest_offers + k);
  } else {
    line(8, "new quests.", PAL_TEXT);
    clear_rows(9, 13);
  }
  sprintf(buf, "Kills:%u Tower:F%u", total_kills, world_best_tower_floor);
  line(14, buf, PAL_TEXT);
  clear_rows(15, 16);
  line(17, "A:Take SEL:Drop B", PAL_HEAD);
  put_cursor(cursor < QUEST_MAX ? 1 + 2 * cursor : 8 + 2 * (cursor - QUEST_MAX));
}

static void update_quests(void) {
  const uint8_t max = world_in_town() ? QUEST_MAX * 2 : QUEST_MAX;
  bool redraw = true;
  if (detail_mode == 3) {
    if (was_pressed(J_A) && cursor >= QUEST_MAX) {
      if (quest_accept(cursor - QUEST_MAX)) play_sound(sfx_mid_powerup);
      else play_sound(sfx_error);
      detail_mode = 0;
    } else if (was_pressed(J_SELECT) && cursor < QUEST_MAX) {
      quest_abandon(cursor);
      detail_mode = 0;
      play_sound(sfx_menu_move);
    } else {
      return;
    }
    draw_quests();
    return;
  }
  if (was_pressed(J_UP) && cursor > 0) {
    cursor--;
  } else if (was_pressed(J_DOWN) && cursor + 1 < max) {
    cursor++;
  } else if (was_pressed(J_A)) {
    detail_mode = 3;
  } else if (was_pressed(J_SELECT) && cursor < QUEST_MAX &&
    quests[cursor].type) {
    quest_abandon(cursor);
    play_sound(sfx_menu_move);
  } else {
    redraw = false;
  }
  if (redraw)
    draw_quests();
}

//------------------------------------------------------------------------------
// Town screen (shop + stash)
//------------------------------------------------------------------------------

#define SHOP_ENTRIES 8
#define STASH_ROWS 12
static uint8_t forge_result;

static void draw_shop(void) {
  char buf[24];
  char name[14];
  const uint8_t lvl = world_area_level();
  menu_title("SHOP < > STASH > FORGE");
  sprintf(buf, "Gold:%u", loot_gold);
  line(1, buf, PAL_TEXT);
  for (uint8_t k = 0; k < LOOT_SHOP_SIZE; k++) {
    const LootItem *it = loot_shop + k;
    if (!it->base) {
      line(2 + k, "  (sold)", PAL_TEXT);
      continue;
    }
    loot_item_name(it, name);
    sprintf(buf, "  %s", name);
    sprintf(col(buf, 13), "%uG", loot_price(k, lvl));
    line(2 + k, buf, rarity_palette(it->rarity));
  }
  static const char *const consumables[4] = {
    "Potion", "Ether", "Elixir", "Remedy"
  };
  for (uint8_t k = 0; k < 4; k++) {
    sprintf(buf, "  %s", consumables[k]);
    sprintf(col(buf, 13), "%uG", loot_price(4 + k, lvl));
    line(7 + k, buf, PAL_TEXT);
  }
  clear_rows(11, 11);
  if (cursor < LOOT_SHOP_SIZE)
    item_detail(12, loot_shop + cursor);
  else
    clear_rows(12, 13);
  clear_rows(14, 16);
  line(17, "A:Buy <>:Tab B", PAL_HEAD);
  put_cursor(cursor < LOOT_SHOP_SIZE ? 2 + cursor : 3 + cursor);
}

static void draw_stash(void) {
  char buf[24];
  const bool bag = tab == 1;
  const LootItem *list = bag ? loot_bag : loot_stash;
  menu_title(bag ? "STASH BAG < >" : "STASH VAULT < >");
  sprintf(buf, "%s PAGE %u", bag ? "BAG" : "VAULT", scroll / STASH_ROWS + 1);
  line(1, buf, PAL_TEXT);
  for (uint8_t r = 0; r < STASH_ROWS; r++)
    item_line(2 + r, "  ", list + scroll + r);
  item_detail(14, list + cursor);
  line(16, bag ? "A:Store in stash" : "A:Take to bag", PAL_TEXT);
  line(17, "A:Move SEL:List B", PAL_HEAD);
  put_cursor(2 + cursor - scroll);
}

static void draw_forge(void) {
  char buf[24];
  char amount[12];
  menu_title("TOWN FORGE < >");
  u32_str(loot_gold, amount);
  sprintf(buf, "Gold: %s", amount); line(1, buf, PAL_TEXT);
  line(2, "Choose gear to upgrade", PAL_HEAD);
  for (uint8_t s = 0; s < LOOT_EQUIP_SLOTS; s++) {
    sprintf(buf, " %s:", slot_tags[s]);
    item_line(4 + s, buf, loot_equip + s);
  }
  if (loot_equip[cursor].base) {
    const uint32_t cost = loot_forge_cost(cursor);
    u32_str(cost, amount);
    sprintf(buf, "Next cost: %sG", amount); line(10, buf, PAL_TEXT);
    sprintf(buf, "Success: %u%%", loot_forge_chance(cursor)); line(11, buf, PAL_TEXT);
    sprintf(buf, "Current +%u", loot_equip[cursor].upgrade); line(12, buf, PAL_TEXT);
    if (forge_result == 1) line(14, "Upgrade succeeded!", PAL_HEAD);
    else if (forge_result == 2) line(14, "Upgrade failed.", PAL_HEAD);
    else if (!cost) line(14, "Max level or no item", PAL_TEXT);
    else if (loot_gold < cost) line(14, "Not enough gold", PAL_TEXT);
  } else {
    line(11, "No gear in this slot", PAL_TEXT);
  }
  line(17, "A:Upgrade <>:Service", PAL_HEAD);
  put_cursor(4 + cursor);
}

static void draw_town(void) {
  clear_rows(1, 16);
  if (tab == 0)
    draw_shop();
  else if (tab == 3)
    draw_forge();
  else
    draw_stash();
}

static void update_town(void) {
  bool redraw = true;
  if (was_pressed(J_LEFT) || was_pressed(J_RIGHT)) {
    if (tab == 0) tab = 1;
    else if (tab == 1 || tab == 2) tab = 3;
    else tab = 0;
    cursor = 0;
    scroll = 0;
    forge_result = 0;
  } else if (tab == 0) {
    if (was_pressed(J_UP) && cursor > 0) {
      cursor--;
    } else if (was_pressed(J_DOWN) && cursor + 1 < SHOP_ENTRIES) {
      cursor++;
    } else if (was_pressed(J_A)) {
      if (loot_buy(cursor, world_area_level()))
        play_sound(sfx_open_chest);
      else
        play_sound(sfx_error);
    } else {
      redraw = false;
    }
  } else if (tab == 1 || tab == 2) {
    const uint8_t max = tab == 1 ? LOOT_BAG_SIZE : LOOT_STASH_SIZE;
    if (was_pressed(J_UP) && cursor > 0) {
      cursor--;
    } else if (was_pressed(J_DOWN) && cursor + 1 < max) {
      cursor++;
    } else if (was_pressed(J_SELECT)) {
      tab = tab == 1 ? 2 : 1;
      cursor = 0;
      scroll = 0;
    } else if (was_pressed(J_A)) {
      loot_stash_move(cursor, tab == 1);
      play_sound(sfx_menu_move);
    } else {
      redraw = false;
    }
    if (cursor < scroll) scroll = cursor;
    if (cursor >= scroll + STASH_ROWS) scroll = cursor - STASH_ROWS + 1;
  } else {
    if (was_pressed(J_UP) && cursor > 0) {
      cursor--;
      forge_result = 0;
    } else if (was_pressed(J_DOWN) && cursor + 1 < LOOT_EQUIP_SLOTS) {
      cursor++;
      forge_result = 0;
    } else if (was_pressed(J_A)) {
      forge_result = loot_forge(cursor);
      play_sound(forge_result == 1 ? sfx_mid_powerup : sfx_error);
    } else {
      redraw = false;
    }
  }
  if (redraw)
    draw_town();
}

//------------------------------------------------------------------------------
// Gathering and crafting professions
//------------------------------------------------------------------------------

static void draw_professions(void) {
  char buf[24];
  char name[20];
  const uint8_t first = tab ? PROFESSION_GATHER_COUNT : 0;
  const uint8_t count = tab ? PROFESSION_COUNT - PROFESSION_GATHER_COUNT :
    PROFESSION_GATHER_COUNT;
  clear_rows(1, 16);
  menu_title(tab ? "CRAFTING JOBS" : "GATHERING JOBS");
  for (uint8_t row = 0; row < count; row++) {
    const uint8_t id = first + row;
    profession_name(id, name);
    sprintf(buf, "%c%s Lv%u", row == cursor ? '>' : ' ', name,
      profession_level[id]);
    line(2 + row, buf, row == cursor ? PAL_HEAD : PAL_TEXT);
  }
  if (!tab) {
    const uint8_t id = cursor;
    sprintf(buf, "XP %u/%u", profession_xp[id], profession_xp_to_next(id));
    line(9, buf, PAL_TEXT);
    sprintf(buf, "Ore%u Herbs%u Wood%u", profession_materials[MATERIAL_ORE],
      profession_materials[MATERIAL_HERBS], profession_materials[MATERIAL_WOOD]);
    line(11, buf, PAL_TEXT);
    sprintf(buf, "Hide%u Fish%u Relic%u", profession_materials[MATERIAL_HIDE],
      profession_materials[MATERIAL_FISH], profession_materials[MATERIAL_RELIC]);
    line(12, buf, PAL_TEXT);
    line(14, "Gather nodes in zones", PAL_TEXT);
    line(17, "<>:Craft B:Back", PAL_HEAD);
  } else {
    const uint8_t id = first + cursor;
    profession_recipe_text(id, buf); line(14, buf, PAL_TEXT);
    sprintf(buf, "Lv%u XP %u/%u", profession_level[id], profession_xp[id],
      profession_xp_to_next(id));
    line(15, buf, PAL_TEXT);
    if (profession_crafted) line(13, "Craft complete!", PAL_HEAD);
    else if (!world_in_town()) line(13, "Visit town to craft", PAL_TEXT);
    else line(13, "Select recipe, press A", PAL_TEXT);
    line(17, "<>:Gather A:Craft B", PAL_HEAD);
  }
  put_cursor(2 + cursor);
}

static void update_professions(void) {
  const uint8_t count = tab ? PROFESSION_COUNT - PROFESSION_GATHER_COUNT :
    PROFESSION_GATHER_COUNT;
  if (was_pressed(J_LEFT) || was_pressed(J_RIGHT)) {
    tab ^= 1;
    cursor = 0;
    profession_crafted = false;
    play_sound(sfx_menu_move);
    draw_professions();
  } else if (was_pressed(J_UP) && cursor > 0) {
    cursor--;
    profession_crafted = false;
    play_sound(sfx_menu_move);
    draw_professions();
  } else if (was_pressed(J_DOWN) && cursor + 1 < count) {
    cursor++;
    profession_crafted = false;
    play_sound(sfx_menu_move);
    draw_professions();
  } else if (was_pressed(J_A) && tab) {
    if (!world_in_town()) {
      profession_crafted = false;
      play_sound(sfx_error);
    } else {
      profession_crafted = profession_craft(
        PROFESSION_GATHER_COUNT + cursor, world_area_level());
      play_sound(profession_crafted ? sfx_open_chest : sfx_error);
    }
    draw_professions();
  }
}

//------------------------------------------------------------------------------
// Stats screen
//------------------------------------------------------------------------------

static void draw_stats(void) {
  char buf[24];
  char cls[14];
  char nm[12];
  static const char *const tabs[3] = { "OVERVIEW", "ATTRIBUTES", "COMBAT" };
  get_race_name(player.race, nm, sizeof(nm));
  get_class_name(player.player_class, cls, sizeof(cls));
  clear_rows(1, 16);
  sprintf(buf, "CHARACTER: %s", tabs[character_tab]);
  menu_title(buf);
  if (character_tab == 0) {
    sprintf(buf, "%s %s L%u", nm, cls, player.level);
    line(2, buf, PAL_HEAD);
    sprintf(buf, "XP %u/%u", player.exp, player.next_level_exp);
    line(4, buf, PAL_TEXT);
    sprintf(buf, "HP %u/%u", player.hp, player.max_hp);
    line(6, buf, PAL_TEXT);
    sprintf(buf, "%cP %u/%u", is_magic_class() ? 'M' : 'S', player.sp, player.max_sp);
    line(7, buf, PAL_TEXT);
    sprintf(buf, "Gold %u", loot_gold);
    line(9, buf, PAL_TEXT);
    sprintf(buf, "Kills %u", total_kills);
    line(10, buf, PAL_TEXT);
    sprintf(buf, "Stat points %u", stat_points);
    line(12, buf, PAL_TEXT);
    sprintf(buf, "Skill points %u", skill_points);
    line(13, buf, PAL_TEXT);
    line(17, "<>:Pages B:Back", PAL_HEAD);
  } else if (character_tab == 1) {
    sprintf(buf, "Points available: %u", stat_points);
    line(2, buf, PAL_HEAD);
    for (uint8_t k = 0; k < ATTR_COUNT; k++) {
      sprintf(buf, "%s %u +%u", alloc_names[k], attr_base(k), attr_alloc[k]);
      line(5 + k, buf, PAL_TEXT);
    }
    line(13, "A adds 1 point", PAL_TEXT);
    line(17, "UP/DN:Select <>:Page", PAL_HEAD);
    put_cursor(5 + cursor);
  } else {
    sprintf(buf, "ATK %u  MAT %u", hs.atk, hs.matk); line(2, buf, PAL_TEXT);
    sprintf(buf, "DEF %u  MDF %u", hs.def, hs.mdef); line(3, buf, PAL_TEXT);
    sprintf(buf, "Crit %u%%  CritDmg %u%%", hs.crit, hs.crit_dmg); line(5, buf, PAL_TEXT);
    sprintf(buf, "Speed +%u%%  Dodge %u%%", hs.aspd, hs.dodge); line(6, buf, PAL_TEXT);
    sprintf(buf, "Block %u%%  CDR %u%%", hs.block, hs.cdr); line(7, buf, PAL_TEXT);
    sprintf(buf, "Life leech %u%%", hs.leech); line(9, buf, PAL_TEXT);
    sprintf(buf, "Gold find +%u%%", hs.gold_find); line(10, buf, PAL_TEXT);
    sprintf(buf, "Magic find +%u%%", hs.magic_find); line(11, buf, PAL_TEXT);
    sprintf(buf, "Regen HP %u SP %u", hs.hp_regen, hs.sp_regen); line(12, buf, PAL_TEXT);
    line(17, "<>:Pages B:Back", PAL_HEAD);
  }
}

static void update_stats(void) {
  bool redraw = true;
  if (was_pressed(J_LEFT) || was_pressed(J_RIGHT)) {
    character_tab = (character_tab + (was_pressed(J_RIGHT) ? 1 : 2)) % 3;
    cursor = 0;
  } else if (character_tab == 1 && was_pressed(J_UP) && cursor > 0) {
    cursor--;
  } else if (character_tab == 1 && was_pressed(J_DOWN) && cursor + 1 < ATTR_COUNT) {
    cursor++;
  } else if (character_tab == 1 && was_pressed(J_A)) {
    if (cursor < ATTR_COUNT) {
      if (stat_points && attr_alloc[cursor] < 250) {
        attr_alloc[cursor]++;
        stat_points--;
        progress_recalc();
        play_sound(sfx_mid_powerup);
      } else {
        play_sound(sfx_error);
      }
    }
  } else {
    redraw = false;
  }
  if (redraw)
    draw_stats();
}

//------------------------------------------------------------------------------
// World atlas, journal, options, controls and save
//------------------------------------------------------------------------------

static const char *const biome_names[WORLD_BIOME_COUNT] = {
  "Forest", "Plains", "Desert", "Swamp", "Foothills",
  "Coast", "Snowfield", "Volcanic", "Underground"
};

static void draw_world(void) {
  char buf[24];
  char area[24];
  const uint8_t current_tier = world_current_tier();
  menu_title("WORLD ATLAS");
  world_area_name(area);
  sprintf(buf, "%s", area);
  line(1, buf, PAL_TEXT);
  sprintf(buf, "Area Lv%u  Tier %u", world_area_level(), current_tier);
  line(2, buf, PAL_TEXT);
  line(3, "REGION LEVEL BANDS", PAL_HEAD);
  for (uint8_t k = 0; k < WORLD_BIOME_COUNT; k++) {
    sprintf(buf, "%s%u %s", cursor == k ? ">" : " ", k + 1, biome_names[k]);
    line(4 + k, buf, cursor == k ? PAL_HEAD : PAL_TEXT);
  }
  sprintf(buf, "Tier %u  Lv%u-%u", cursor + 1,
    cursor * WORLD_LEVELS_PER_TIER + 1,
    (cursor + 1) * WORLD_LEVELS_PER_TIER);
  line(14, buf, PAL_TEXT);
  if (world_current_zone() == 0xFF)
    line(15, "Explore the world to", PAL_TEXT);
  else {
    sprintf(buf, "Zone %u  Town %u", world_current_zone() + 1,
      world_last_town + 1);
    line(15, buf, PAL_TEXT);
  }
  line(17, "UP/DOWN:Region B:Back", PAL_HEAD);
  put_cursor(4 + cursor);
}

static void draw_journal(void) {
  char buf[24];
  char area[24];
  menu_title("JOURNAL");
  world_area_name(area);
  line(1, area, PAL_TEXT);
  sprintf(buf, "Kills:%u  Tier:%u", total_kills, world_current_tier());
  line(2, buf, PAL_TEXT);
  sprintf(buf, "Tower best:%u/%u", world_best_tower_floor,
    WORLD_TOWER_MAX_FLOOR);
  line(3, buf, PAL_TEXT);
  sprintf(buf, "Last town zone:%u", world_last_town + 1);
  line(4, buf, PAL_TEXT);
  line(5, "ACTIVE BOUNTIES", PAL_HEAD);
  for (uint8_t k = 0; k < QUEST_MAX; k++)
    quest_rows(6 + k * 2, quests + k);
  if (world_location() == WORLD_LOC_RAID)
    sprintf(buf, "Raid %u Floor %u/%u", raid_state.raid_id + 1,
      raid_state.floor, WORLD_RAID_FLOORS);
  else if (world_location() == WORLD_LOC_TOWER)
    sprintf(buf, "Tower Floor %u", tower_state.floor);
  else if (world_location() == WORLD_LOC_INSTANCE)
    sprintf(buf, "Dungeon F%u/%u", deep_dungeon_state.floor,
      WORLD_DEEP_DUNGEON_FLOORS);
  else
    sprintf(buf, "Current level:%u", world_area_level());
  line(13, buf, PAL_TEXT);
  line(17, "B:Back", PAL_HEAD);
}

static void draw_options(void) {
  char buf[24];
  menu_title("OPTIONS");
  line(2, "SOUND", PAL_HEAD);
  sprintf(buf, "%s SFX: %s", cursor == 0 ? ">" : " ",
    settings_sfx_enabled ? "ON" : "OFF");
  line(4, buf, cursor == 0 ? PAL_HEAD : PAL_TEXT);
  sprintf(buf, "%s MUSIC: %s", cursor == 1 ? ">" : " ",
    settings_music_enabled ? "ON" : "OFF");
  line(5, buf, cursor == 1 ? PAL_HEAD : PAL_TEXT);
  line(17, "A:Toggle UP/DOWN B", PAL_HEAD);
  put_cursor(4 + cursor);
}

static void draw_controls(void) {
  menu_title("CONTROLS");
  line(1, "FIELD", PAL_HEAD);
  line(2, "D-PAD  Move", PAL_TEXT);
  line(3, "B      Attack", PAL_TEXT);
  line(4, "A      Talk/Use", PAL_TEXT);
  line(5, "SELECT Dodge", PAL_TEXT);
  line(6, "B+SEL  Hotbar slot", PAL_TEXT);
  line(7, "START  Pause menu", PAL_TEXT);
  line(9, "MENUS", PAL_HEAD);
  line(10, "D-PAD  Move cursor", PAL_TEXT);
  line(11, "A      Open/Inspect", PAL_TEXT);
  line(12, "B      Back/Cancel", PAL_TEXT);
  line(13, "SELECT Context/Page", PAL_TEXT);
  line(14, "LEFT/RIGHT Tabs", PAL_TEXT);
  line(17, "B:Back", PAL_HEAD);
}

static void draw_save(void) {
  menu_title("SAVE");
  if (save_confirmed) {
    line(6, "Progress saved.", PAL_HEAD);
    line(8, "Press A to save again.", PAL_TEXT);
  } else {
    line(5, "Save current progress", PAL_TEXT);
    line(6, "to cartridge memory?", PAL_TEXT);
  }
  line(17, save_confirmed ? "A:Save again B:Back" : "A:Save B:Cancel", PAL_HEAD);
}

static void update_world(void) {
  if (was_pressed(J_UP) && cursor > 0) {
    cursor--;
  } else if (was_pressed(J_DOWN) && cursor + 1 < WORLD_BIOME_COUNT) {
    cursor++;
  } else {
    return;
  }
  play_sound(sfx_menu_move);
  draw_world();
}

static void update_options(void) {
  if (was_pressed(J_UP) || was_pressed(J_DOWN)) {
    cursor ^= 1;
    play_sound(sfx_menu_move);
  } else if (was_pressed(J_A)) {
    if (cursor == 0)
      settings_sfx_enabled = !settings_sfx_enabled;
    else
      settings_music_enabled = !settings_music_enabled;
    play_sound(sfx_menu_move);
  } else {
    return;
  }
  draw_options();
}

static void update_save(void) {
  if (!was_pressed(J_A))
    return;
  save_game();
  save_confirmed = true;
  play_sound(sfx_level_up);
  draw_save();
}

//------------------------------------------------------------------------------
// Screen lifecycle
//------------------------------------------------------------------------------

static void draw_screen(void);

static void draw_screen(void) {
  switch (screen) {
  case SCREEN_SKILLS: draw_skills(); break;
  case SCREEN_ITEMS: draw_items(); break;
  case SCREEN_QUESTS: draw_quests(); break;
  case SCREEN_TOWN: draw_town(); break;
  case SCREEN_STATS: draw_stats(); break;
  case SCREEN_WORLD: draw_world(); break;
  case SCREEN_JOURNAL: draw_journal(); break;
  case SCREEN_OPTIONS: draw_options(); break;
  case SCREEN_CONTROLS: draw_controls(); break;
  case SCREEN_SAVE: draw_save(); break;
  case SCREEN_PROFESSIONS: draw_professions(); break;
  }
}

bool arpg_menu_open(uint8_t entry) BANKED {
  if (entry == ARPG_MENU_TOWN && !world_in_town())
    return false;
  if (entry == ARPG_MENU_MORE || entry >= ARPG_MENU_COUNT)
    return false;

  screen = entry < ARPG_MENU_WORLD ? entry : entry - 1;
  cursor = 0;
  scroll = 0;
  tab = 0;
  profession_crafted = false;
  character_tab = 0;
  gear_tab = 0;
  detail_mode = 0;
  save_confirmed = false;
  if (screen == SCREEN_WORLD) {
    uint8_t tier = world_current_tier();
    cursor = tier ? tier - 1 : 0;
  }

  if (world_in_town()) {
    const uint8_t z = world_current_zone();
    if (offers_zone != z) {
      offers_zone = z;
      quest_generate_offers();
    }
    if (shop_zone != z) {
      shop_zone = z;
      loot_restock_shop(world_area_level());
    }
  }

  arpg_menu_used = true;
  game_state = GAME_STATE_ARPG_MENU;

  DISPLAY_OFF;
  core.load_bg_palette(menu_palettes, 1, 6);
  core.load_sprite_palette(cursor_palette, 7, 1);
  core.fill(SCR(0, 0), 20, 18, FONT_SPACE, 0x08 | PAL_TEXT);
  LCDC_REG &= ~LCDCF_WIN9C00;
  move_win(7, 0);
  draw_screen();
  DISPLAY_ON;
  return true;
}

static void close_screen(void) {
  move_win(7, 144);
  LCDC_REG |= LCDCF_WIN9C00;
  game_state = GAME_STATE_WORLD_MAP;
  arpg_menu_returned = true;
  play_sound(sfx_menu_move);
}

void arpg_menu_update(void) BANKED {
  if (was_pressed(J_B) || was_pressed(J_START)) {
    if (detail_mode) {
      detail_mode = 0;
      draw_screen();
      play_sound(sfx_menu_move);
      return;
    }
    close_screen();
    return;
  }
  switch (screen) {
  case SCREEN_SKILLS: update_skills(); break;
  case SCREEN_ITEMS: update_items(); break;
  case SCREEN_QUESTS: update_quests(); break;
  case SCREEN_TOWN: update_town(); break;
  case SCREEN_STATS: update_stats(); break;
  case SCREEN_WORLD: update_world(); break;
  case SCREEN_OPTIONS: update_options(); break;
  case SCREEN_SAVE: update_save(); break;
  case SCREEN_PROFESSIONS: update_professions(); break;
  case SCREEN_JOURNAL:
  case SCREEN_CONTROLS:
    break;
  }
}
