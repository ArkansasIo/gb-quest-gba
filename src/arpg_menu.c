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
} Screen;

static uint8_t screen;
static uint8_t cursor;
static uint8_t scroll;
static uint8_t tab;
static uint8_t hot_slot;
static uint8_t offers_zone = 0xFF;
static uint8_t shop_zone = 0xFF;

#define CURSOR_SPRITE 16
#define PAL_TEXT 7
#define PAL_HEAD 1
#define PAL_RARITY 2

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
  char name[14];
  if (it->base == 0) {
    clear_rows(y, y + 1);
    return;
  }
  loot_item_name(it, name);
  sprintf(buf, "R%u %s %s", it->rarity + 1, rarity_names[it->rarity], name);
  line(y, buf, rarity_palette(it->rarity));
  loot_item_stats(it, 0, buf);
  line(y + 1, buf, PAL_TEXT);
}

static void menu_title(const char *name, uint8_t page) {
  char buf[24];
  sprintf(buf, "[%s] %u/5", name, page + 1);
  line(0, buf, PAL_HEAD);
}

//------------------------------------------------------------------------------
// Pause menu entries
//------------------------------------------------------------------------------

static const uint8_t entry_col[3] = { 2, 8, 14 };

void arpg_menu_draw_entries(void) BANKED {
  static const char *const names[ARPG_MENU_COUNT] = {
    "SKILLS", "BAG", "QUEST", "TOWN", "STATUS", "BACK"
  };
  uint8_t *vram = VRAM_WINDOW_XY(1, 29);
  for (uint8_t k = 0; k < 18; k++)
    set_vram_byte(vram + k, FONT_SPACE);
  for (uint8_t k = 0; k < ARPG_MENU_COUNT; k++)
    core.draw_text(VRAM_WINDOW_XY(entry_col[k % 3], k < 3 ? 28 : 30),
      names[k], 6);
  core.draw_text(VRAM_WINDOW_XY(1, 31), "A:OPEN      B:CLOSE", 19);
  core.load_sprite_palette(cursor_palette, 7, 1);
  set_sprite_tile(CURSOR_SPRITE, '>' + FONT_OFFSET);
  set_sprite_prop(CURSOR_SPRITE, 0x0F);
  for (uint8_t k = 1; k < 4; k++)
    move_sprite(CURSOR_SPRITE + k, 0, 0);
}

void arpg_menu_draw_cursor(uint8_t entry) BANKED {
  const uint8_t col = entry_col[entry % 3] - 1;
  const uint8_t row = entry < 3 ? 14 : 16;
  set_sprite_tile(CURSOR_SPRITE, '>' + FONT_OFFSET);
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
  menu_title("BAG", 1);
  sprintf(buf, "Gold:%u  EQUIPPED", loot_gold);
  line(1, buf, PAL_TEXT);
  for (uint8_t s = 0; s < LOOT_EQUIP_SLOTS; s++) {
    sprintf(buf, " %s:", slot_tags[s]);
    item_line(2 + s, buf, loot_equip + s);
  }
  sprintf(buf, "BAG %u/%u", bag_count(), LOOT_BAG_SIZE);
  line(6, buf, PAL_HEAD);
  for (uint8_t r = 0; r < ITEMS_BAG_ROWS; r++)
    item_line(7 + r, "  ", loot_bag + scroll + r);

  const LootItem *sel = cursor < LOOT_EQUIP_SLOTS ? loot_equip + cursor :
    loot_bag + (cursor - LOOT_EQUIP_SLOTS);
  item_detail(14, sel);
  sprintf(buf, "Pot%u Eth%u Eli%u Rem%u",
    inventory[ITEM_POTION].quantity, inventory[ITEM_ETHER].quantity,
    inventory[ITEM_ELIXIR].quantity, inventory[ITEM_REMEDY].quantity);
  line(16, buf, PAL_TEXT);
  line(17, "A:Equip SEL:Sell B", PAL_HEAD);

  if (cursor < LOOT_EQUIP_SLOTS)
    put_cursor(2 + cursor);
  else
    put_cursor(7 + (cursor - LOOT_EQUIP_SLOTS) - scroll);
}

static void update_items(void) {
  const uint8_t max = LOOT_EQUIP_SLOTS + LOOT_BAG_SIZE;
  bool redraw = false;
  if (was_pressed(J_UP) && cursor > 0) {
    cursor--;
    redraw = true;
  } else if (was_pressed(J_DOWN) && cursor + 1 < max) {
    cursor++;
    redraw = true;
  } else if (was_pressed(J_A)) {
    if (cursor < LOOT_EQUIP_SLOTS)
      loot_unequip(cursor);
    else
      loot_equip_from_bag(cursor - LOOT_EQUIP_SLOTS);
    play_sound(sfx_menu_move);
    redraw = true;
  } else if (was_pressed(J_SELECT) && cursor >= LOOT_EQUIP_SLOTS) {
    const uint8_t idx = cursor - LOOT_EQUIP_SLOTS;
    if (loot_bag[idx].base) {
      loot_sell(idx);
      play_sound(sfx_open_chest);
      redraw = true;
    }
  }
  if (!redraw)
    return;
  if (cursor >= LOOT_EQUIP_SLOTS) {
    const uint8_t idx = cursor - LOOT_EQUIP_SLOTS;
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
  menu_title("SKILLS", 0);
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
    arpg_skill_info(k, name);
    sprintf(buf, "   %s", name);
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
  if (was_pressed(J_UP) && cursor > 0) {
    cursor--;
  } else if (was_pressed(J_DOWN) && cursor < SKILL_COUNT + 1) {
    cursor++;
  } else if (was_pressed(J_LEFT)) {
    hot_slot = (hot_slot + HOTBAR_SLOTS - 1) % HOTBAR_SLOTS;
  } else if (was_pressed(J_RIGHT)) {
    hot_slot = (hot_slot + 1) % HOTBAR_SLOTS;
  } else if (was_pressed(J_A) && cursor < SKILL_COUNT) {
    if (skill_points && skill_rank[cursor] < 5 &&
      player.level >= arpg_skill_level(cursor)) {
      skill_rank[cursor]++;
      skill_points--;
      play_sound(sfx_mid_powerup);
    } else {
      play_sound(sfx_error);
    }
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
  char desc[22];
  quest_describe(q, desc);
  sprintf(buf, "  %s", q->type ? desc : "-");
  line(y, buf, q->type ? PAL_RARITY + 2 : PAL_TEXT);
  quest_reward_text(q, desc);
  sprintf(buf, "  %s", desc);
  line(y + 1, buf, PAL_TEXT);
}

static void draw_quests(void) {
  char buf[24];
  const bool town = world_in_town();
  menu_title("QUESTS", 2);
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
  if (was_pressed(J_UP) && cursor > 0) {
    cursor--;
  } else if (was_pressed(J_DOWN) && cursor + 1 < max) {
    cursor++;
  } else if (was_pressed(J_A) && cursor >= QUEST_MAX) {
    if (quest_accept(cursor - QUEST_MAX))
      play_sound(sfx_mid_powerup);
    else
      play_sound(sfx_error);
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

static void draw_shop(void) {
  char buf[24];
  char name[14];
  const uint8_t lvl = world_area_level();
  menu_title("SHOP < > STASH", 3);
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
  menu_title("STASH < > SHOP", 3);
  sprintf(buf, "%s PAGE %u", bag ? "BAG" : "VAULT", scroll / STASH_ROWS + 1);
  line(1, buf, PAL_TEXT);
  for (uint8_t r = 0; r < STASH_ROWS; r++)
    item_line(2 + r, "  ", list + scroll + r);
  item_detail(14, list + cursor);
  line(16, bag ? "A:Store in stash" : "A:Take to bag", PAL_TEXT);
  line(17, "A:Move SEL:List B", PAL_HEAD);
  put_cursor(2 + cursor - scroll);
}

static void draw_town(void) {
  clear_rows(1, 16);
  if (tab == 0)
    draw_shop();
  else
    draw_stash();
}

static void update_town(void) {
  bool redraw = true;
  if (was_pressed(J_LEFT) || was_pressed(J_RIGHT)) {
    tab = tab == 0 ? 1 : 0;
    cursor = 0;
    scroll = 0;
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
  } else {
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
  }
  if (redraw)
    draw_town();
}

//------------------------------------------------------------------------------
// Stats screen
//------------------------------------------------------------------------------

static void draw_stats(void) {
  char buf[24];
  char cls[14];
  char nm[12];
  get_race_name(player.race, nm, sizeof(nm));
  get_class_name(player.player_class, cls, sizeof(cls));
  menu_title("STATUS", 4);
  sprintf(buf, "%s %s L%u", nm, cls, player.level);
  line(1, buf, PAL_TEXT);
  sprintf(buf, "XP %u/%u", player.exp, player.next_level_exp);
  line(2, buf, PAL_TEXT);
  sprintf(buf, "HP %u/%u %cP %u/%u", player.hp, player.max_hp,
    is_magic_class() ? 'M' : 'S', player.sp, player.max_sp);
  line(3, buf, PAL_TEXT);
  sprintf(buf, "ATK %u", player.atk);
  sprintf(col(buf, 9), "DEF %u", player.def);
  line(4, buf, PAL_TEXT);
  sprintf(buf, "MAT %u", player.matk);
  sprintf(col(buf, 9), "MDF %u", player.mdef);
  line(5, buf, PAL_TEXT);
  sprintf(buf, "AGL %u", player.agl);
  sprintf(col(buf, 9), "CRIT %u", hs.crit);
  line(6, buf, PAL_TEXT);
  sprintf(buf, "LEECH %u GOLD+%u", hs.leech, hs.gold_find);
  line(7, buf, PAL_TEXT);
  sprintf(buf, "Gold %u Kills %u", loot_gold, total_kills);
  line(8, buf, PAL_TEXT);
  sprintf(buf, "STAT POINTS: %u", stat_points);
  line(9, buf, PAL_HEAD);
  for (uint8_t k = 0; k < ATTR_COUNT; k++) {
    sprintf(buf, "  %s %u", alloc_names[k], attr_alloc[k]);
    line(10 + k, buf, PAL_TEXT);
  }
  sprintf(buf, "SET SFX:%s MUSIC:%s", settings_sfx_enabled ? "ON " : "OFF",
    settings_music_enabled ? "ON " : "OFF");
  line(16, buf, PAL_HEAD);
  line(17, "A:Raise/Toggle B", PAL_HEAD);
  put_cursor(cursor < ATTR_COUNT ? 10 + cursor : 16);
}

static void update_stats(void) {
  bool redraw = true;
  if (was_pressed(J_UP) && cursor > 0) {
    cursor--;
  } else if (was_pressed(J_DOWN) && cursor < ATTR_COUNT + 1) {
    cursor++;
  } else if (was_pressed(J_A)) {
    if (cursor < ATTR_COUNT) {
      if (stat_points && attr_alloc[cursor] < 250) {
        attr_alloc[cursor]++;
        stat_points--;
        progress_recalc();
        play_sound(sfx_mid_powerup);
      } else {
        play_sound(sfx_error);
      }
    } else if (cursor == ATTR_COUNT) {
      settings_sfx_enabled = !settings_sfx_enabled;
      play_sound(sfx_menu_move);
    } else {
      settings_music_enabled = !settings_music_enabled;
      play_sound(sfx_menu_move);
    }
  } else {
    redraw = false;
  }
  if (cursor > ATTR_COUNT) {
    // Second settings entry shares the row: move cursor to the right.
    move_sprite(CURSOR_SPRITE, 8 + 9 * 8, 16 + 16 * 8);
  }
  if (redraw)
    draw_stats();
  if (cursor > ATTR_COUNT)
    move_sprite(CURSOR_SPRITE, 8 + 9 * 8, 16 + 16 * 8);
}

//------------------------------------------------------------------------------
// Screen lifecycle
//------------------------------------------------------------------------------

static void draw_screen(void) {
  switch (screen) {
  case SCREEN_SKILLS: draw_skills(); break;
  case SCREEN_ITEMS: draw_items(); break;
  case SCREEN_QUESTS: draw_quests(); break;
  case SCREEN_TOWN: draw_town(); break;
  case SCREEN_STATS: draw_stats(); break;
  }
}

bool arpg_menu_open(uint8_t entry) BANKED {
  if (entry == ARPG_MENU_TOWN && !world_in_town())
    return false;
  if (entry >= ARPG_MENU_RETURN)
    return false;

  screen = entry;
  cursor = 0;
  scroll = 0;
  tab = 0;

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
    close_screen();
    return;
  }
  switch (screen) {
  case SCREEN_SKILLS: update_skills(); break;
  case SCREEN_ITEMS: update_items(); break;
  case SCREEN_QUESTS: update_quests(); break;
  case SCREEN_TOWN: update_town(); break;
  case SCREEN_STATS: update_stats(); break;
  }
}
