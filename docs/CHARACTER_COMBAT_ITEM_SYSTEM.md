# Character, Combat, Gear, Creature & Boss Systems

## Existing implementation alignment

This specification aligns with the current GBC code in `src/progress.c/.h`, `src/loot.c/.h`, `src/gear.h`, `src/stats.h`, and `src/monster.h`.

The runtime already has six primary attributes (STR, DEX, INT, VIT, END, SPI), derived ATK/MATK/DEF/MDEF/HP/SP, critical chance and damage, attack speed, dodge, block, cooldown reduction, life steal, gold find, magic find, HP/SP regeneration, timed buffs, four equipment slots, nine rarity tiers, 23 affix types, and normal/elite/boss monster scaling.

## Attributes and formulas

Each level grants 3 allocatable attribute points and 1 skill point. Class-specific growth contributes to all six attributes.

| Attribute | Role |
|---|---|
| STR | Physical attack, critical damage, carry capacity |
| DEX | Critical chance, attack speed, dodge, accuracy |
| INT | Magic attack, cooldown reduction, skill power |
| VIT | Maximum HP, health regeneration, status resistance |
| END | Physical defense, block chance, stamina |
| SPI | Magic defense, SP, resource regeneration |

Derived formulas match the current progression implementation:

- ATK = STR × 2 + DEX ÷ 2 + gear
- MATK = INT × 2 + SPI ÷ 2 + gear
- DEF = END × 2 + STR ÷ 2 + gear
- MDEF = SPI × 2 + INT ÷ 2 + gear
- HP = 30 + VIT × 8 + END × 2 + level × 4 + gear
- SP = 10 + SPI × 2 + INT + level + gear

Sub-stat caps: critical chance 75%, critical damage 250% bonus, attack speed 100%, dodge 40%, block 50%, cooldown reduction 40%, life steal 25%, gold find 500%, and magic find 500%.

## Resources and damage types

HP and SP are active runtime resources. STA, RAGE, ENERGY, FOCUS, FAITH, AETHER, HEAT, and SANITY are designed expansion pools; they are not claimed to be implemented until the player structure, costs, UI, persistence, and class rules support them.

Current damage aspects are physical, magical, earth, water, air, fire, light, and dark. The extended catalog adds poison, bleed, frost, lightning, arcane, void, chaos, and true. Ordinary resistance mitigation is `rawDamage * (100 - resistancePercent) / 100`, with resistance clamped from -100% to 80%. True damage bypasses ordinary resistance.

## Weapons, armor, and itemization

Current equipment slots are weapon, armor, offhand, and trinket. The data contract defines weapon archetypes (sword, greatsword, spear, dual blades, staff, bow, mace, wand, scythe, axe, rapier, hammer), armor weights (light, medium, heavy, robe), and future slots (head, chest, hands, legs, feet, back, neck, two rings, two relics).

Nine rarities: Worn, Sturdy, Fine, Enchanted, Rare, Epic, Legendary, Relic, Mythic. Up to three affixes are supported by the current loot structure. Forge upgrades are +0 through +10. Item categories include consumables, food, runes, crafting materials, keys, currencies, quest items, relics, artifacts, and gems.

## Creatures, monsters, and bosses

Creature taxonomy: aberration, beast, celestial, construct, dragon, elemental, fey, fiend, giant, humanoid, monstrosity, ooze, plant, undead. Combat roles: skirmisher, brute, lurker, controller, soldier, artillery, support, boss.

Ranks:
- Normal: baseline zone enemy.
- Elite: increased health and attack plus one monster modifier.
- Boss: multi-phase arena encounter.
- World boss: shared regional encounter for 12–24 players, contribution tracking, telegraphed area mechanics, persistent health, and server-validated rewards.
- Arc boss: 8–12 player multi-stage encounter spanning realm or campaign progression.

Monster modifier families: Enraged, Armored, Spellbound, Vampiric, Swift, Plaguebearer, Warden, Voidtouched.

## Runtime boundary

The catalog and formulas describe the intended system, not a completed replacement combat engine. The existing GBC runtime mirrors some stats into 8-bit legacy fields and its visible player level is capped at 99 even though progression data defines 720. Save/UI compatibility must be resolved before claiming the full 720-level system is playable.

Implementation order: connect catalog records to data tables; complete weapon/armor tooltips and affix rolls; add resistances and extended damage; add monster affix modifiers; implement boss phase state machines; add server-side world-boss scheduling and contribution ledger; update character and gear screens; migrate saves; test levels 1, 60, 120, 360, and 720.
