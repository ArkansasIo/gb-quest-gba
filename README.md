# Labyrinth of the Dragon (GBC)
An 8-bit Adventure RPG with D&D Monsters!

## Action RPG Mode

Combat happens in real time on the map. Monsters appear around you and fight
you directly; there is no separate battle screen.

### Controls

| Button | Action |
| --- | --- |
| D-pad | Move |
| B | Attack (a melee swing, or a magic bolt for spellcasters) |
| A | Talk / open / read. With nothing in front of you: use the selected hotbar skill or potion |
| SELECT | Dodge roll (two quick steps, briefly invulnerable) |
| Hold B + SELECT | Switch the selected hotbar slot |
| START | Pause menu (not available while enemies are near) |

On the title screen, START continues your saved game and SELECT + START starts
a new character.

### Features

* **Real-time combat.** Monsters chase you and flash before they attack, so you
  can step or dodge out of the way. Each monster type fights differently: fast
  and slow melee, ranged casters, ground slams and dragon breath. Hits push
  enemies back, crits deal double damage, and damage numbers float above
  targets. Some attacks poison or stun.
* **Elites and bosses.** Some monsters spawn as tougher elites. Every 5th tower
  floor has a boss, and bosses speed up when badly hurt.
* **Loot.** Monsters and treasure chests drop gear with five rarities (Common,
  Magic, Rare, Epic, Legendary) and random bonuses: ATK, DEF, MAT, MDF, AGL,
  HP, SP, critical chance, life steal, gold find and regeneration. You have four
  equipment slots: weapon, armor, off-hand and trinket.
* **Inventory and stash.** A 16-slot bag, plus a 24-slot stash you can use in
  any town. Sell items for gold from the ITEMS screen.
* **Skills and hotbar.** Each of the 18 classes has 6 skills, unlocked at levels
  1, 5, 8, 12, 16 and 20. Each level gives a skill point to learn a skill or
  rank it up (up to rank 5). You get 4 hotbar slots for skills and potions.
  Skills have SP costs and cooldowns.
* **Character growth.** Each level gives 3 stat points to spend on STR, DEX,
  INT, VIT, END and SPI. HP and SP regenerate over time, faster outside combat.
* **Towns.** A shop sells random gear and potions, and a quest board offers
  bounties: slay monsters, slay elites, explore a zone, climb the tower, or slay
  a boss. Quests pay gold and XP.
* **Open world.** A connected 9x8 grid of 72 named zones across nine biomes,
  with nine capital cities and nine smaller settlements. Zone names are
  available through the world-area lookup and appear on settlement signs;
  layouts are generated deterministically from each zone's ID. Monster
  strength rises with distance from Ashford. The
  100-floor tower and original eight-floor labyrinth have marked world gates.
* **Saving.** The game saves automatically to the cartridge's battery-backed
  save memory whenever you enter a town. If you are defeated, you lose 10% of
  your gold and wake up in the last town you visited.

See [the in-game menu and submenu system](docs/IN_GAME_MENU_SYSTEM.md) for the
Pokémon-era blue-panel navigation design, existing screens, controls, and the
planned Party, World Map, Guild, Save, and Settings branches.

See [the 72-zone world atlas](docs/WORLD_ATLAS_72_ZONES.md) for the region map,
settlements, underground frontier, and dungeon gateways. See [world tiers, raids,
trails, and monster taxonomy](docs/WORLD_TIERS_RAIDS_TRAILS_MONSTERS.md) for
level bands, instance routes, tower scaling, and D&D-style creature tags.
See [the online world server guide](docs/ONLINE_WORLD_SERVER.md) for the
authoritative realm, character, zone, chat, party, and guild backend.

## How to Build the ROM

### Depedencies
* [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020) - The Game Boy Development
  kit. Includes the libraries and binaries for C development on the Game Boy.
* [GNU Make](https://gnuwin32.sourceforge.net/packages/make.htm) - Build system
  tool (installation should only be required on Windows).
* [NodeJS](https://nodejs.org) - Used to run custom tools I made in the course
  of developing the game.

### Use Make to Build the ROM
Update the `MakeFile` or define a shell variable named `GBDK_HOME` pointing to
the directory where you installed GBDK.

To build the ROM run the following commands:

* `npm install`
* `make assets`
* `make`


## ARPG and MMORPG Systems

The system architecture and implementation boundaries are documented in:

- [ARPG + MMORPG Systems Master Specification](docs/ARPG_MMO_SYSTEMS_MASTER_PLAN.md) — character, combat, progression, loot, crafting, economy, multiplayer authority, persistence, and delivery phases.
- [Menu Navigation and Screen Contract](docs/MENU_NAVIGATION_CONTRACT.md) — root menu pages, submenu hierarchy, input rules, rendering constraints, and acceptance tests.
- [In-Game Menu System](docs/IN_GAME_MENU_SYSTEM.md) — current ROM menu behavior and existing screens.
- [Online World Server](docs/ONLINE_WORLD_SERVER.md) — currently implemented HTTP endpoints and online-play limitations.

The ROM remains playable offline. Internet multiplayer requires the separate server and a web/desktop client or a separately implemented hardware bridge; the current GBC ROM does not directly connect to the internet.


## 90-Zone Aelmoor World Design

- [90-Zone World Atlas](docs/WORLD_ATLAS_90_ZONES.md): nine original fantasy realms, 90 zones, 270 sub-zones, primary biomes, sub-biomes, settlements, landmarks, level bands, and map connections.
- [Canonical world data](data/world_atlas_90.json): machine-readable 10×9 world grid with stable IDs and neighbor links.
- The parchment-style Aelmoor map is a visual concept; the GBC interface should use a simplified tile-based map.

**Runtime status:** the current playable cartridge still uses its original 72-zone (9×8) grid. The 90-zone atlas and data model are committed as the next world-design foundation; the cartridge runtime, save bitset, zone transitions, biome scaling, and world-map UI have not yet been migrated or tested for 90 zones.


## 12-Act Campaign and Instance Atlas

- [12-act story, main quests, side quests, and instance catalog](docs/CAMPAIGN_AND_INSTANCE_ATLAS.md).
- [Canonical campaign and map data](data/campaign_and_instances.json): 60 main quests, 36 side quests, 42 dungeon maps, 9 raid maps, 42 trial maps, and 18 towers with 100-floor progression maps each.

These are original fantasy-game materials informed by broad fifth-edition tabletop play patterns, not copied D&D adventure text, settings, stat blocks, or published maps. Instance maps are currently logical node layouts and floor templates; finished GBC tilemaps and playable quest/instance runtime integration remain future implementation work.


## Character, Combat, Gear, Creatures & Boss Systems

- [Character and combat stat specification](docs/CHARACTER_COMBAT_ITEM_SYSTEM.md): primary attributes, derived stats, sub-stats, resources, damage, resistances, itemization, creature roles, and boss rank rules.
- [System formulas and schema](data/character_combat_item_system.json): current-runtime alignment, stat formulas/caps, affixes, rarities, equipment slots, damage types, and rank scaling targets.
- [Original content catalog](data/creature_gear_item_catalog.json): 32 realm creatures, 12 act/world/arc bosses, 12 weapons, 12 armor pieces, and 22 usable/material/key/currency items.

The catalog is design data. Existing six-attribute progression and four equipment slots are already represented in the GBC code; additional resources, damage types, expanded equipment slots, monster affixes, and world/arc boss scheduling still require runtime and server implementation.
