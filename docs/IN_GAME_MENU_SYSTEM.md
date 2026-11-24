# In-Game Menu System

Pokémon-style tile menu architecture for the existing GBC action RPG.

## Main menu

The current top-level menu in `src/arpg_menu.c` has Skills, Bag, Quests, Town, Status, and Back. Preserve these working entries while adding branches only when their backing systems exist.

## Controls

- START opens the menu from the world.
- D-pad moves the cursor; A confirms; B returns one level.
- SELECT remains a context action (selling, list switching, or hotbar assignment), not a universal back button.
- Opening a menu pauses world simulation, enemy updates, movement, and combat timers.
- Preserve cursor and scroll position when moving between sibling screens.

## Existing submenus

### Bag
- Equipped slots: weapon, armor, off-hand, trinket.
- Bag contents with scrolling and selected-item details.
- Equip/unequip with A; sell with SELECT where supported.
- Future filters should be views over the existing inventory, not duplicate item storage: Consumables, Weapons, Armor, Accessories, Materials, Key Items, Quest Items.

### Skills
- Skill list, unlock level, rank, and description.
- A spends a skill point to learn/rank up (maximum rank 5).
- Left/Right selects one of four hotbar slots; SELECT assigns the highlighted learned skill or consumable.

### Quests
- Active quests and town-board offers.
- A accepts an offer; SELECT abandons an active quest.
- Show objectives and rewards; generate town offers on entering a new town zone.

### Town
- Left/Right switches Shop and Stash.
- Purchases use existing gold/stock checks; stash moves items between bag and vault.
- Only available in towns.

### Status
- Race, class, level, XP, HP/SP, combat attributes, currency, kills, and unspent stat points.
- A allocates a stat point; sound toggles remain accessible.

## Visual requirements

- Retain the blue Pokémon-era panel palette, dark-blue headings, pale-blue rows, and clear `>` cursor.
- Keep text within the 20-column tile width and safely truncate long names.
- Hide unused cursor sprites and restore the map/window configuration when closing a submenu.
- Every screen documents controls on its final row and uses existing navigation/error sounds.

## Future branches (not yet implemented)

Party, World Map, Guild, explicit Save-slot selection, and expanded Settings require real data models, persistence, and game-loop integration. Do not present these as working actions until those systems exist.