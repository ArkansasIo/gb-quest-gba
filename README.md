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
| START | Open the pause menu (not available while enemies are near) |
| SELECT in pause menu | Switch between menu pages |

On the title screen, START continues your saved game and SELECT + START starts
a new character.

### Features

* **Real-time combat.** Monsters chase you and flash before they attack, so you
  can step or dodge out of the way. Each monster type fights differently: fast
  and slow melee, ranged casters, ground slams and dragon breath. Hits push
  enemies back, crits deal double damage, and damage numbers float above
  targets. Some attacks poison or stun.
* **Elites and bosses.** Some monsters spawn as tougher elites with one random
  combat affix: Berserker (stronger and faster), Ironhide (higher physical and
  magical defense), Venomous (poison on contact), or Leeching (heals from HP
  damage dealt). The elite affix is shown in the HUD. Every 5th tower floor has
  a boss. Bosses transition through four HP phases at 75%, 50% and 25% health;
  phase changes are announced in the HUD. Later phases shorten attack cooldowns,
  increase damage, make slams more frequent and wider, and extend dragon-breath
  reach. Boss windups lengthen with each phase and flash faster, preserving a
  dodge window before these stronger attacks.
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
* **Full in-game menus.** The two-page pause hub opens skills, gear, quests,
  town services, character pages, the world atlas, journal, options, controls
  and manual save. Character pages separate overview, base and allocated
  attributes, and derived combat statistics. Gear pages separate equipped
  slots and their combined bonuses from bag items, with item rarity and stat
  details, equip, unequip and sell actions. Skill, item and quest selections
  open detail submenus with rank, stat, progress and reward information. Town
  services include a shop, bag and vault storage, and a gear upgrade forge.
* **Professions.** Six gathering and twelve crafting professions progress to
  level 100. Harvest field nodes for materials, then craft weapons, armor,
  trinkets, and consumables in town.
* **Towns.** A shop sells random gear and potions, and a quest board offers
  bounties: slay monsters, slay elites, explore a zone, climb the tower, or slay
  a boss. Quests pay gold and XP.
* **Open world.** A connected 9x8 grid of 72 named zones across nine biomes,
  with nine capital cities and nine smaller settlements. Zone names are
  available through the world-area lookup and appear on settlement signs;
  layouts are generated deterministically from each zone's ID. Monster
  strength rises with distance from Ashford. The
  100-floor tower, original eight-floor labyrinth, nine regional raids, and
  five-floor Echoing Vault, Deep Roads, and Abyssal Rift instances have marked
  world gates and named boss encounters.
* **Saving.** The game saves automatically to the cartridge's battery-backed
  save memory whenever you enter a town. If you are defeated, you lose 10% of
  your gold and wake up in the last town you visited.

See [the 72-zone world atlas](docs/WORLD_ATLAS_72_ZONES.md) for the region map,
settlements, underground frontier, and dungeon gateways. See [world tiers, raids,
trails, and monster taxonomy](docs/WORLD_TIERS_RAIDS_TRAILS_MONSTERS.md) for
level bands, instance routes, tower scaling, and D&D-style creature tags.
See [the online world server guide](docs/ONLINE_WORLD_SERVER.md) for the
authoritative realm, character, zone, chat, party, and guild backend.
See [professions and gathering](docs/PROFESSIONS_AND_GATHERING.md) for the
profession list, gathering regions, and craft recipes.

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
