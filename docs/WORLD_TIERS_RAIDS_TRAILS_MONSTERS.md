# World Tiers, Raids, Trails, and Monster Taxonomy

This document describes the implemented world progression layer for Labyrinth of the Dragon. The nine regions each occupy a 3×3 block of the 9×8 overworld. Their biome order is forest, plains, desert, swamp, foothills, coast, snowfield, volcanic, and underground. Each region is a world tier with eight levels per tier band (1–80, 81–160, …, 641–720); zone placement within a region adds local level steps so the center settlement is the safest route and the outer corners are more dangerous. Character levels remain capped by the existing 1–720 progression.

## Nine World Tiers

| Tier | Region / biome | Level band | Theme |
|---:|---|---:|---|
| 1 | Mosswood — forest | 1–80 | Starting woods, roads, and the Ashford frontier |
| 2 | Brightmeadow — plains | 81–160 | Farms, grasslands, and open approach routes |
| 3 | Red Dunes — desert | 161–240 | Glasswind wastes and buried ruins |
| 4 | Whisperfen — swamp | 241–320 | Mire paths, drowned places, and will-o-wisps |
| 5 | Gloamwood — foothills | 321–400 | Stone roads, ridges, and Dragonwatch |
| 6 | Cloudpass — coast | 401–480 | Stormbreak shores, shipwrecks, and Tideglass |
| 7 | Far Shoals — snowfield | 481–560 | Frostmarch, ice passes, and Frosthold |
| 8 | Stormbreak Reach — volcanic | 561–640 | Emberfall, Cinderkeep, and the craterlands |
| 9 | The Deep Roads — underground | 641–720 | Rootdeep, Underhall, and the Abyssal Rift |

Zone level is generated from tier and distance from that region's central settlement. A zone's map is deterministic from its ID, so its layout is stable across visits. Region capitals are the center zones: Ashford (10), Brightvale (13), Sandmere (16), Mirefen (37), Stonehelm (40), Gullport (43), Frosthold (64), Cinderkeep (67), and Underhall (70).

## Raids

Each regional capital has a marked raid gate. A raid is a three-floor generated instance with ordinary encounters and a named boss on floor 3. The boss clear completes a matching raid bounty when one is active.

| Raid | Capital / zone | Final boss |
|---|---|---|
| Root-Crown Raid | Ashford (10) | Owlbear Matriarch |
| Sunfield Citadel | Brightvale (13) | Goblin War-Chief |
| Glass Dune Vault | Sandmere (16) | Kobold Sun-Seer |
| Witchwater Depths | Mirefen (37) | Wisp Queen |
| Dragonwatch Bastion | Stonehelm (40) | Ancient Dragon |
| Tideglass Maw | Gullport (43) | Displacer Stalker |
| Frostmarch Hold | Frosthold (64) | Death Knight Marshal |
| Cinderkeep Caldera | Cinderkeep (67) | Ashen Dragon |
| Abyssal Court | Underhall (70) | Beholder Oracle |

Raid encounters scale from the entrance zone; later raid floors add 12 levels per floor. Use the entrance in the capital to start, descend to progress, and use the return stairs to leave.

## Trails

Trails are shortcut gates between the paired zones. The sign beside a trail names its destination; use the marked path to travel directly between endpoints.

| Trail | Zone endpoints |
|---|---|
| King's Road Trail | 1 ↔ 19 |
| Sunfield Track | 3 ↔ 22 |
| Saltwind Way | 5 ↔ 41 |
| Glasswind Pass | 7 ↔ 48 |
| Beacon Path | 8 ↔ 53 |
| Mirefen Causeway | 9 ↔ 27 |
| Eagle Ridge Trail | 11 ↔ 35 |
| Old Salt Road | 12 ↔ 39 |
| Wreckers' Run | 14 ↔ 42 |
| Bluewater Track | 15 ↔ 44 |
| Copper Road | 17 ↔ 26 |
| Cloudpass Trail | 18 ↔ 36 |
| Frostmarch Way | 20 ↔ 45 |
| Grayridge Pass | 21 ↔ 33 |
| Northlight Path | 24 ↔ 50 |
| Deep Roads Spur | 56 ↔ 69 |

## Tower

The tower contains 100 generated floors. Its level progression spans levels 1–720: floor 1 is level 1, floor 100 is level 720, with intermediate floors interpolated evenly. Every fifth floor has a boss encounter. This gives a repeatable vertical challenge that crosses all nine world tiers.

## Monster Types, Subtypes, and Classes

Creature type and subtype are separate fields on each monster instance. The creature types follow the 2014 D&D 5e/SRD taxonomy: Aberration, Beast, Celestial, Construct, Dragon, Elemental, Fey, Fiend, Giant, Humanoid, Monstrosity, Ooze, Plant, and Undead. A subtype is a descriptive tag such as goblinoid; the tag itself does not add rules. The game's monster class is a separate gameplay role used to describe combat behavior, not an official D&D creature type.

| Existing monster | D&D creature type | Subtype/tag | Game role |
|---|---|---|---|
| Kobold | Humanoid | Kobold | Skirmisher |
| Goblin | Humanoid | Goblinoid | Skirmisher |
| Zombie | Undead | Zombie | Brute |
| Bugbear | Humanoid | Goblinoid | Brute |
| Owlbear | Monstrosity | Owlbear | Brute |
| Gelatinous Cube | Ooze | Gelatinous | Controller |
| Displacer Beast | Monstrosity | Displacer beast | Lurker |
| Will-o'-wisp | Undead | Will-o'-wisp | Controller |
| Death Knight | Undead | Death knight | Soldier |
| Mind Flayer | Aberration | Mind flayer | Controller |
| Beholder | Aberration | Beholder | Artillery |
| Dragon | Dragon | Dragon | Boss |

Only the types represented by the current monster roster are assigned in runtime data; the enum includes all 14 SRD creature types for future content. Roles currently include skirmisher, brute, lurker, controller, soldier, artillery, and boss.

## Source

The D&D type names and tag convention are based on the [D&D 5.1 System Reference Document](https://media.dndbeyond.com/compendium-images/srd/5.1/SRD-OGL_V5.1.pdf). This taxonomy does not reproduce monster stat blocks; this game's monster stats and combat roles are its own implementation.
