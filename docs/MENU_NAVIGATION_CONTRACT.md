# Menu Navigation and Screen Contract

Implementation companion to ARPG_MMO_SYSTEMS_MASTER_PLAN.md.

## Navigation state
Every screen declares a stable screen ID, parent ID, selected row, scroll offset, availability predicate/reason, input mapping, render/update handlers, and exit cleanup. Use a bounded parent stack. B pops one screen; START closes the root only after pending-action confirmation; SELECT invokes only the current screen's documented context action.

## Root pages
The current ROM root is Skills, Bag, Quests, Town, Status, Back. Preserve it until replacement routes are implemented and regression-tested. The expanded page-based layout is:

**Page 1 — Adventure:** Character, Party, Inventory, Skills, Talents, Quests.

**Page 2 — World:** World Map, Journal, Social, Guild, Services (town-only), Settings.

**Footer:** Save / Return; A: Open; B: Back; SELECT: Context action.

## Screen contracts

| Screen | Child views | Actions |
|---|---|---|
| Character | Overview, Attributes, Combat, Progression, Reputation | Inspect, allocate points |
| Party | Roster, Member Details, Invites, Formation | Invite, inspect, leave |
| Inventory | Equipment, Bag, Consumables, Materials, Key Items, Item Details | Use, equip, compare, sell |
| Skills | Learned, Available, Skill Details, Hotbar | Learn, rank, assign |
| Talents | Tree, Node Details, Respec Preview | Allocate, confirm respec |
| Quests | Active, Available, Completed, Quest Details | Accept, abandon, turn in |
| World Map | World, Region, Zone, Local Map | Inspect, waypoint |
| Journal | Bestiary, Lore, Recipes, Achievements | Inspect |
| Social | Friends, Chat Channels, Block List | Whisper, mute, block, report |
| Guild | Overview, Roster, Invites, Ranks, Events, Storage | Invite, leave, manage |
| Services | Shop, Stash, Forge, Crafting, Runes, Auction | Buy, move, craft, list |
| Settings | Audio, Controls, Display, Accessibility, Account | Change, reset |
| Save / Return | Save Slot, Confirm, Logout, Title | Save, logout |

## Visual rules
- Existing blue/light-blue palette and high-contrast dark text.
- One visible selection cursor; never rely on color alone.
- 20 columns maximum for GBC text; truncate safely and show full details on inspect.
- Reserve the last two rows for help and action prompts.
- Show page number and category title on every paged screen.
- Disabled entries show a short reason.
- Empty lists show an explicit empty state, never stale content.
- Closing a screen restores window position, background bank, palettes, sprites and map redraw flags.

## Input and error rules
- D-pad: move selection or change tabs only when documented.
- A: confirm / primary action.
- B: return to parent.
- START: close root / open pause root.
- SELECT: contextual secondary action only.
- Invalid action: error sound and concise reason.
- Network action: pending state until server acknowledgement; on timeout preserve authoritative state and offer retry.
- Never optimistically grant items, gold, XP, guild membership or quest rewards without server confirmation.

## Test checklist
- Every screen opens and exits without corrupting map/window state.
- Cursor and scroll restore after inspecting details.
- Empty/full inventories render safely.
- Invalid actions do not mutate state.
- Town-only screens cannot open outside towns.
- Network failures do not duplicate transactions or corrupt displayed state.
- Every visible action has a handler or is explicitly disabled.
