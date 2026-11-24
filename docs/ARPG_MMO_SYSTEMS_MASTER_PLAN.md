# ARPG + MMORPG Systems Master Specification

This repository contains a Game Boy Color ROM and a separate Node.js online-world prototype. Keep that boundary explicit: the ROM runs locally; online multiplayer is provided by the server and a future web/desktop client or supported bridge. A menu entry is not a feature implementation until it has real data, validation, persistence, and tests.

## 1. Player-facing menu hierarchy

The visual language is a compact Pokémon-era blue panel: 20x18 tile viewport, readable headings, one selection cursor, short labels, contextual help, D-pad navigation, A confirm, B back, START close, and SELECT reserved for context actions. Menus pause the local world simulation. Never let an unavailable feature appear to work.

### Root menu
1. Character
2. Party
3. Inventory
4. Skills
5. Talents
6. Quests
7. World Map
8. Journal
9. Social
10. Guild
11. Services (town only)
12. Settings
13. Save / Return

The current ROM root is a 2x3 grid: Skills, Bag, Quests, Town, Status, Back. Preserve those working routes and migrate to the expanded root incrementally. On a 160x144 GBC display, use pages or category tabs rather than squeezing 13 entries onto one screen.

### Character
- Overview: name, race, class, level, XP, title, current zone.
- Attributes: STR, DEX, INT, VIT, END, SPI; allocated and derived values.
- Combat: HP, SP/MP, ATK, DEF, MATK, MDEF, AGL, critical, resistances.
- Progression: level history, stat points, skill points, unlocked abilities.
- Reputation: faction standings and PvP status (online client/server only).
- Appearance: palette, outfit, portrait, cosmetic slots (where supported).

### Party
- Roster: up to five members in the current server prototype.
- Member inspect: level, class, HP status, zone, online state.
- Invite by character name; accept/decline; leave; leader transfer.
- Formation, loot rules, ready check, party finder and raid roles are future server features.
- Group chat is membership-gated; the server must authorize every action.

### Inventory
- Equipment: weapon, armor, off-hand/shield, accessory slots supported by the current loot system.
- Bag: item list, stack count, item level, rarity, durability if later implemented.
- Filters: consumables, weapons, armor, accessories, materials, quest/key items.
- Item inspect: description, requirements, base stats, affixes, sell value.
- Context: use, equip, unequip, compare, split, discard, sell, trade.
- Storage: town stash; later shared guild bank.
- All item mutation must be atomic and server-authoritative for online characters.

### Skills
- Class skills: unlock level, rank, cost, cooldown, target rules, description.
- Hotbar: four current slots; assign only learned/usable skills and valid consumables.
- Skill details: scaling, damage/healing type, range, status effects.
- Cooldown and resource state must be shown without implying a cast succeeded before gameplay confirms it.

### Talents
- Passive trees, branches, node prerequisites, point cost, rank cap.
- Respec preview, cost confirmation, and derived-stat preview.
- Persist allocation by character; validate points and prerequisites on the authority.
- Do not show unimplemented trees as selectable nodes.

### Quests
- Active, available, completed, failed, daily, weekly, seasonal categories.
- Objective tracker, progress, location hints, reward preview.
- Accept, abandon, turn in, claim reward.
- Server validates completion and reward grants for online quests.
- Current server prototype has quest acceptance only; progression, completion, and reward APIs remain to be built.

### World Map
- World / realm / region / zone / subzone hierarchy.
- Current location, discovered locations, exits, roads, gates, dungeon entrances.
- Party member markers and world-event markers (online client).
- Waypoints and fast travel require discovery, unlock, and cost validation.
- Map UI can show local ROM map data offline; shared markers require server state.

### Journal
- Bestiary, lore, crafting recipes, discovered items, achievements, tutorials.
- Entry state is per character; server sync is required for online persistence.
- Avoid making collection completion grant rewards until achievement rules exist.

### Social
- Friends, blocks, whispers, zone chat, local/say chat, party chat, guild chat.
- Presence, mute, report, ignore, notification controls.
- Moderation, spam controls and privacy settings are required before public deployment.

### Guild
- Create, invite, join, leave, member list, leader and officer roles, guild message of the day.
- Future: ranks/permissions, guild bank, roster search, guild quests, territory, guild events.
- Current server prototype supports create/invite/leave and basic membership; permissions and persistent rank systems are not yet complete.

### Services (town)
- Shop: browse, compare, buy, sell, restock.
- Stash: deposit and withdraw.
- Forge: upgrade, repair, salvage, socket, enchant.
- Crafting: recipes, ingredients, profession levels, output preview.
- Rune / gem services: insert, extract, combine.
- Auction house and player trade are future online services; both require escrow/transaction safety and server validation.

### Settings
- Audio, controls, text speed, display options, accessibility, language.
- Account settings and connection diagnostics belong to the online client.
- Apply settings immediately where safe; persist them in the correct local or account scope.

### Save / Return
- Save slot, timestamp, character/realm summary, overwrite confirmation.
- Local cartridge save must use the actual battery-backed save implementation.
- Online characters are persisted by the server; client save UI must report server acknowledgements and offline state accurately.
- Return to title / logout must confirm unsaved or pending actions.

## 2. ARPG gameplay systems

| System | Required behavior | Authority |
|---|---|---|
| Movement | Tile movement, collision, transitions, dodge | ROM locally; server validates online movement |
| Combat | Targeting, hit checks, damage, mitigation, cooldowns | Server for online play |
| Skills | Resource cost, cooldown, range, effects | Server for online play |
| Stats | Base, allocated, gear, buff/debuff derived stats | Shared formulas; server validates online |
| Loot | Drop tables, rarity, affixes, item rolls | Server for online play |
| Equipment | Slot compatibility, stat recalculation, requirements | Server for online play |
| Progression | XP, level caps, rewards, stat/skill points | Server for online play |
| Quests | Objectives, state transitions, rewards | Server for online play |
| Encounters | Spawn tables, elite modifiers, boss phases | Server for online play |
| Instances | Dungeon/raid IDs, lockouts, completion state | Server |
| Crafting | Recipes, ingredients, output rolls | Server for online play |
| Economy | Currency ledger, vendor prices, transaction history | Server |
| Death | Checkpoint, penalties, resurrection | Server for online play |
| PvP | Consent flag, matchmaking, combat validation | Server |

### Combat pipeline
1. Client sends intent (ability ID, target ID/position, sequence number).
2. Server verifies session, character, zone, range, cooldown, resources, and target state.
3. Server resolves hit, damage, mitigation, critical, status effects, death and rewards.
4. Server emits an authoritative event/state update.
5. Client animates the confirmed result; rejected actions show a reason.

Never accept client-submitted damage, loot, currency, XP, inventory contents, or final character coordinates as trusted facts.

### Item model
An item instance needs a stable instance ID, definition ID, quantity, item level, rarity, required level/class, rolled affixes, binding/trade state, and source metadata. Item definitions are immutable data; player inventory stores instance references. Enforce stack limits, slot compatibility, and unique instance ownership.

Rarity and affix display must come from item data. A tooltip must include name, rarity, item level, requirements, base stats, each affix, and comparison delta where a compatible equipped item exists.

## 3. MMORPG services and persistence

### Existing online prototype
The current Node service exposes realm discovery, account registration/login/logout, character creation, world enter/state/move/zone, chat, party create/invite/leave, guild create/invite/leave, quest acceptance, character read, and social state. It stores JSON data and uses in-memory sessions. The online server guide is the source of truth for the current routes.

### Required production services
- Authentication: secure password hashing, session rotation/revocation, expiry, account recovery, optional MFA.
- Character: create/list/select/delete with ownership and realm constraints.
- Realm: status, capacity, maintenance, version compatibility.
- Presence: reconnect-safe sessions, heartbeats, zone channels, bounded event replay.
- Groups: party lifecycle, roles, invite expiry, leader changes, raid groups.
- Guilds: roles, permission matrix, audit log, guild events.
- Chat: channel ACLs, moderation, mute/block/report, rate limits.
- Gameplay authority: combat, inventory, equipment, quest, progression, crafting.
- Economy: atomic trade escrow, auction listings/bids, currency ledger, anti-duplication.
- Persistence: relational database, migrations, backups, transaction boundaries.
- Operations: structured logs, metrics, tracing, health checks, abuse detection, moderation tooling.

### Data ownership
Accounts own characters. Characters belong to one realm. Realm-scoped character IDs and names must be unique according to the chosen policy. Inventory, equipment, quest state, talent allocations, currency, achievements and progression belong to the character. Guilds and parties reference character IDs, never client-provided display names as authority. Transactions must be idempotent and auditable.

## 4. GBC constraints and client architecture

The Game Boy Color ROM cannot directly connect to the internet using the current cartridge implementation. Keep the ROM as a fully playable offline ARPG. Online MMORPG play requires a web/desktop client or a separately implemented hardware/transport bridge. Do not link the current sketch protocol to the HTTP server and call it functional multiplayer.

The GBC UI has a 20x18 tile display and tight ROM/RAM/bank budgets. Keep menu data compact, render one page at a time, avoid large dynamic strings, keep screen code bank-safe, and preserve existing bank declarations. The online client may expose richer pages, party frames, chat and map overlays than the ROM.

## 5. Delivery phases and acceptance criteria

### Phase A — menu foundation
- Define one menu-state enum and a parent/back stack.
- Implement root pages, submenu navigation, disabled-state reasons, cursor/scroll restoration.
- Preserve existing Skills, Bag, Quests, Town and Status behavior.
- Test every menu path with D-pad, A, B, START and SELECT.

### Phase B — offline ARPG completeness
- Connect Character, Inventory, Skills, Quests, Services and Settings to existing ROM systems.
- Add missing item details, compare views, equipment filters, and save confirmation only where underlying systems exist.
- Add unit/manual test cases for purchase, sell, equip, stat allocation, quest accept/abandon and screen restoration.

### Phase C — online social client
- Add realm/login/character selection UI.
- Integrate presence, party, guild and chat with the existing HTTP API.
- Add reconnect handling, server error display and membership authorization tests.

### Phase D — authoritative MMORPG gameplay
- Add database migrations and server-owned inventory, combat, progression, quest completion, crafting and economy APIs.
- Add idempotency keys, transaction logs, abuse/rate controls, integration tests and backup/restore tests.
- Only then enable online trading, auction house, raids and competitive PvP.

### Definition of done
A system is complete only when its UI, state model, validation, persistence, failure states and tests are implemented. A design document or disabled menu item is not a completed gameplay system.
