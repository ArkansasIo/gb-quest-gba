# Labyrinth of the Dragon: 72-Zone World Atlas

The overworld is a connected 9-column by 8-row grid. Zone IDs increase from
west to east, then north to south: `id = row * 9 + column`. The game generates
each zone from a stable seed, so the atlas can name and classify all 72 spaces
without storing 72 full maps in cartridge ROM.

## World map

Each cell shows `ID Name`. Capitals are marked **City**, settlements are
marked **Town**. Zone 1 contains the original Labyrinth entrance; Dragonwatch
(31) is the tower gateway.

| Row | West → East |
|---|---|
| 0 | `00 Mosswood` · `01 Old King's Road` · `02 Thornwatch` · `03 Brightmeadow` · `04 Windrun` · `05 Sunfield` · `06 Red Dunes` · `07 Glasswind` · `08 Suncleft` |
| 1 | `09 Greenhollow` · `10 Ashford — City` · `11 Foxglove Vale` · `12 Millcross` · `13 Brightvale — City` · `14 The Long Acre` · `15 Mirage Run` · `16 Sandmere — City` · `17 Copper Flats` |
| 2 | `18 Whisperfen` · `19 Mossgate — Town` · `20 Reedmaze` · `21 Kingsroad March` · `22 Highgrass` · `23 Barleycross — Town` · `24 Saltmarsh` · `25 Reedhaven — Town` · `26 Blackwater Reach` |
| 3 | `27 Gloamwood` · `28 Briarwatch — Town` · `29 Hollowmere` · `30 Old Stoneway` · `31 Dragonwatch — Tower Gate` · `32 Eagle's Rest — Town` · `33 Grayridge` · `34 Ironroot` · `35 High Crags` |
| 4 | `36 Cloudpass` · `37 Mirefen — City` · `38 Stormbreak Coast` · `39 Old Salt Road` · `40 Stonehelm — City` · `41 Tideglass` · `42 Shipwreck Strand` · `43 Gullport — City` · `44 Bluewater Bay` |
| 5 | `45 Far Shoals` · `46 Pinefall` · `47 Frostmarch` · `48 Whitecap Pass` · `49 Icevein` · `50 Northlight` · `51 Saltwind` · `52 Seawatch — Town` · `53 Beacon Isle` |
| 6 | `54 Stormbreak Reach` · `55 Pinefall — Town` · `56 Northlight Pass` · `57 Emberfall` · `58 Charstone — Town` · `59 Redglass Crater` · `60 Rootdeep` · `61 Sporelight — Town` · `62 Echoing Vault` |
| 7 | `63 Frostmarch Gate` · `64 Frosthold — City` · `65 Whitecap Pass` · `66 Cinderkeep Road` · `67 Cinderkeep — City` · `68 Wyrmstone` · `69 The Deep Roads` · `70 Underhall — City` · `71 Abyssal Rift` |

## Biomes and regions

| Biome | World character | Native threats |
|---|---|---|
| Forest | Mosswood canopy, old roads, and the Ashford valley | Kobolds, goblins, owlbears |
| Plains | Farm belts, grasslands, and the Brightvale trade roads | Goblins, bugbears, displacer beasts |
| Desert | Sandmere, glass dunes, and mirage trails | Kobolds, gelatinous cubes, will-o'-wisps |
| Swamp | Mirefen, Gloamwood, and flooded ruins | Zombies, gelatinous cubes, will-o'-wisps |
| Foothills | Stonehelm, Dragonwatch, and the tower approach | Goblins, bugbears, owlbears |
| Coast | Gullport, shipwrecks, salt roads, and sea cliffs | Kobolds, zombies, displacer beasts |
| Snowfield | Frosthold, pine valleys, and the northern passes | Zombies, owlbears, death knights |
| Volcanic | Cinderkeep, ash steppe, and the redglass crater | Bugbears, death knights, dragons |
| Underground | Rootdeep, the Deep Roads, buried ruins, and the Abyssal Rift | Zombies, mind flayers, beholders |

The original nine-biome encounter and palette systems remain in use. The
generated Deeps form an underground frontier at zones 60–62 and 69–71.

## Cities and towns

| Region | Capital city | Town / outpost |
|---|---|---|
| Forest | Ashford (10) | Mossgate (19) |
| Plains | Brightvale (13) | Barleycross (23) |
| Desert | Sandmere (16) | Reedhaven (25) |
| Swamp | Mirefen (37) | Briarwatch (28) |
| Foothills | Stonehelm (40) | Eagle's Rest (32) |
| Coast | Gullport (43) | Seawatch (52) |
| Snowfield | Frosthold (64) | Pinefall (55) |
| Volcanic | Cinderkeep (67) | Charstone (58) |
| Underground | Underhall (70) | Sporelight (61) |

The starting location remains Ashford. All 18 settlements use the existing
safe map, sign, shop, stash, quest board, autosave, and respawn behavior.

## Dungeons and underground zones

| Zone | Name | Map role |
|---:|---|---|
| 01 | Old King's Road | Surface entrance to the original eight-floor Labyrinth |
| 31 | Dragonwatch | Entrance to the generated 100-floor tower |
| 60 | Rootdeep | Root-choked underground approach |
| 61 | Sporelight | Underground town and resupply point |
| 62 | Echoing Vault | Hostile ancient halls |
| 69 | The Deep Roads | Underground travel corridor |
| 70 | Underhall | Deep city and hub |
| 71 | Abyssal Rift | Deepest hostile frontier |

The Labyrinth and tower are playable through their existing entrance systems.
The six Deeps cells are playable generated biome zones. New bespoke multi-floor
dungeon instances for the Deep Roads, Echoing Vault, and Abyssal Rift are
planned content; this pass does not claim those separate instances exist yet.

## Runtime implementation

- Added a stable display name for every one of the 72 zone IDs.
- Replaced the nine center-only settlement check with 18 named city and town
  zones. Their safe-map and town-service behavior uses the existing system.
- Changed settlement signs to use the named city and town entries; the area
  name lookup now returns the zone's atlas name.
- Kept procedural map generation, zone connections, monster tables, the
  eight-floor Labyrinth, and 100-floor tower architecture intact.

Zone layouts remain generated from the existing Game Boy tile set; this atlas
does not imply 72 bespoke maps or new art assets.
