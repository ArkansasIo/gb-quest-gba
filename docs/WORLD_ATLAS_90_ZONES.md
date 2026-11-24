# Aelmoor: 90-Zone World Atlas

An original fantasy setting for **Labyrinth of the Dragon (GBC)**. The world is organized as a 10-column by 9-row atlas: nine realm belts, ten connected zones per realm, and three named sub-zones per zone (270 sub-zones total). Each zone carries a primary biome, a more specific sub-biome, a level band, settlement classification, points of interest, and orthogonal travel links.

## World map layout

Each row is one realm, running west to east. Zone IDs are stable and row-major: `id = y * 10 + x`. Adjacent cells connect north, south, east, and west. The atlas JSON is the canonical source for names, sub-zones, biomes, coordinates, level bands, and links.

| Map row | Realm | Biome | Zone IDs |
|---:|---|---|---:|
| 1 | Verdant Crown | temperate forest | 0–9 |
| 2 | Sunward Marches | grassland and temperate plains | 10–19 |
| 3 | Saffron Expanse | desert and badlands | 20–29 |
| 4 | Mireveil Basin | swamp and marsh | 30–39 |
| 5 | Crownspine Highlands | mountains and foothills | 40–49 |
| 6 | Shattercoast | coastal and marine | 50–59 |
| 7 | Northfrost Reach | taiga and tundra | 60–69 |
| 8 | Emberfall Dominion | volcanic and ashland | 70–79 |
| 9 | The Underdeep | subterranean and cavern | 80–89 |

## Zone roster

### 1. Verdant Crown

**Biome:** temperate forest. **Sub-biomes:** ancient woodland, moss forest, faerie glade, riverwood.

| ID | Zone | Sub-biome | Level band | Settlement |
|---:|---|---|---:|---|
| 0 | Mosswood | ancient woodland | 1–80 | — |
| 1 | Thornwatch | moss forest | 1–80 | — |
| 2 | Greenhollow | faerie glade | 1–80 | — |
| 3 | Foxglove Vale | riverwood | 1–80 | — |
| 4 | Whisperfen | ancient woodland | 1–80 | — |
| 5 | Mossgate | moss forest | 1–80 | town |
| 6 | Gloamwood | faerie glade | 1–80 | — |
| 7 | Briarwatch | riverwood | 1–80 | town |
| 8 | Hollowmere | ancient woodland | 1–80 | — |
| 9 | Root-Crown Vale | moss forest | 1–80 | — |

### 2. Sunward Marches

**Biome:** grassland and temperate plains. **Sub-biomes:** golden prairie, farmland, rolling downs, wildflower steppe.

| ID | Zone | Sub-biome | Level band | Settlement |
|---:|---|---|---:|---|
| 10 | Brightmeadow | golden prairie | 81–160 | — |
| 11 | Windrun Downs | farmland | 81–160 | — |
| 12 | Sunfield | rolling downs | 81–160 | — |
| 13 | Ashford Fields | wildflower steppe | 81–160 | capital |
| 14 | Millcross | golden prairie | 81–160 | — |
| 15 | Brightvale | farmland | 81–160 | capital |
| 16 | Kingsroad March | rolling downs | 81–160 | — |
| 17 | Highgrass | wildflower steppe | 81–160 | — |
| 18 | Barleycross | golden prairie | 81–160 | town |
| 19 | Liongrass Reach | farmland | 81–160 | — |

### 3. Saffron Expanse

**Biome:** desert and badlands. **Sub-biomes:** red dunes, salt flats, glass desert, canyon oasis.

| ID | Zone | Sub-biome | Level band | Settlement |
|---:|---|---|---:|---|
| 20 | Red Dunes | red dunes | 161–240 | — |
| 21 | Glasswind | salt flats | 161–240 | — |
| 22 | Suncleft | glass desert | 161–240 | — |
| 23 | Mirage Run | canyon oasis | 161–240 | — |
| 24 | Sandmere | red dunes | 161–240 | capital |
| 25 | Copper Flats | salt flats | 161–240 | — |
| 26 | Saltmarsh Edge | glass desert | 161–240 | — |
| 27 | Reedhaven Oasis | canyon oasis | 161–240 | town |
| 28 | Blackwater Reach | red dunes | 161–240 | — |
| 29 | Sunken Ziggurat | salt flats | 161–240 | — |

### 4. Mireveil Basin

**Biome:** swamp and marsh. **Sub-biomes:** blackwater bog, reed marsh, mistfen, fungal wetland.

| ID | Zone | Sub-biome | Level band | Settlement |
|---:|---|---|---:|---|
| 30 | Reedmaze | blackwater bog | 241–320 | — |
| 31 | Mirefen | reed marsh | 241–320 | capital |
| 32 | Old Drowned Road | mistfen | 241–320 | — |
| 33 | Stillwater Fen | fungal wetland | 241–320 | — |
| 34 | Witchlight Marsh | blackwater bog | 241–320 | — |
| 35 | Sourwillow | reed marsh | 241–320 | — |
| 36 | Drowned Bell | mistfen | 241–320 | — |
| 37 | Wispwater | fungal wetland | 241–320 | — |
| 38 | Sinking Gardens | blackwater bog | 241–320 | — |
| 39 | The Pale Mire | reed marsh | 241–320 | — |

### 5. Crownspine Highlands

**Biome:** mountains and foothills. **Sub-biomes:** pine foothills, granite peaks, alpine meadow, cloud forest.

| ID | Zone | Sub-biome | Level band | Settlement |
|---:|---|---|---:|---|
| 40 | Old Stoneway | pine foothills | 321–400 | — |
| 41 | Dragonwatch | granite peaks | 321–400 | — |
| 42 | Eagle's Rest | alpine meadow | 321–400 | town |
| 43 | Grayridge | cloud forest | 321–400 | — |
| 44 | Ironroot | pine foothills | 321–400 | — |
| 45 | High Crags | granite peaks | 321–400 | — |
| 46 | Cloudpass | alpine meadow | 321–400 | — |
| 47 | Stonehelm | cloud forest | 321–400 | capital |
| 48 | Crownspine Pass | pine foothills | 321–400 | — |
| 49 | Wyrmwatch Heights | granite peaks | 321–400 | — |

### 6. Shattercoast

**Biome:** coastal and marine. **Sub-biomes:** rocky coast, tidal flats, island chain, storm cliffs.

| ID | Zone | Sub-biome | Level band | Settlement |
|---:|---|---|---:|---|
| 50 | Stormbreak Coast | rocky coast | 401–480 | — |
| 51 | Old Salt Road | tidal flats | 401–480 | — |
| 52 | Tideglass | island chain | 401–480 | — |
| 53 | Shipwreck Strand | storm cliffs | 401–480 | — |
| 54 | Gullport | rocky coast | 401–480 | capital |
| 55 | Bluewater Bay | tidal flats | 401–480 | — |
| 56 | Far Shoals | island chain | 401–480 | — |
| 57 | Saltwind | storm cliffs | 401–480 | — |
| 58 | Seawatch | rocky coast | 401–480 | town |
| 59 | Beacon Isle | tidal flats | 401–480 | — |

### 7. Northfrost Reach

**Biome:** taiga and tundra. **Sub-biomes:** snow pine forest, glacier field, frozen lake, ice shelf.

| ID | Zone | Sub-biome | Level band | Settlement |
|---:|---|---|---:|---|
| 60 | Pinefall | snow pine forest | 481–560 | town |
| 61 | Frostmarch | glacier field | 481–560 | — |
| 62 | Whitecap Pass | frozen lake | 481–560 | — |
| 63 | Icevein | ice shelf | 481–560 | — |
| 64 | Northlight | snow pine forest | 481–560 | — |
| 65 | Stormbreak Reach | glacier field | 481–560 | — |
| 66 | Northlight Pass | frozen lake | 481–560 | — |
| 67 | Frostmarch Gate | ice shelf | 481–560 | — |
| 68 | Frosthold | snow pine forest | 481–560 | capital |
| 69 | Whitecap Crown | glacier field | 481–560 | — |

### 8. Emberfall Dominion

**Biome:** volcanic and ashland. **Sub-biomes:** ash steppe, lava field, obsidian badlands, sulfur vent.

| ID | Zone | Sub-biome | Level band | Settlement |
|---:|---|---|---:|---|
| 70 | Emberfall | ash steppe | 561–640 | — |
| 71 | Charstone | lava field | 561–640 | town |
| 72 | Redglass Crater | obsidian badlands | 561–640 | — |
| 73 | Cinderkeep Road | sulfur vent | 561–640 | — |
| 74 | Cinderkeep | ash steppe | 561–640 | capital |
| 75 | Wyrmstone | lava field | 561–640 | — |
| 76 | Ashen Stair | obsidian badlands | 561–640 | — |
| 77 | Blackglass Reach | sulfur vent | 561–640 | — |
| 78 | Firefall Caldera | ash steppe | 561–640 | — |
| 79 | The Dragon's Maw | lava field | 561–640 | — |

### 9. The Underdeep

**Biome:** subterranean and cavern. **Sub-biomes:** root caverns, crystal grotto, fungal forest, abyssal chasm.

| ID | Zone | Sub-biome | Level band | Settlement |
|---:|---|---|---:|---|
| 80 | Rootdeep | root caverns | 641–720 | — |
| 81 | Sporelight | crystal grotto | 641–720 | town |
| 82 | Echoing Vault | fungal forest | 641–720 | — |
| 83 | The Deep Roads | abyssal chasm | 641–720 | — |
| 84 | Underhall | root caverns | 641–720 | capital |
| 85 | Abyssal Rift | crystal grotto | 641–720 | — |
| 86 | Gloam Galleries | fungal forest | 641–720 | — |
| 87 | Crystal Sepulcher | abyssal chasm | 641–720 | — |
| 88 | The Hollow Throne | root caverns | 641–720 | — |
| 89 | Nightwell | crystal grotto | 641–720 | — |

## Data contract

- Canonical machine-readable atlas: `data/world_atlas_90.json`.
- 90 unique zone IDs (0–89), 9 realms, 10 zones per realm.
- 270 sub-zone records, each with a stable zone-local ID and a sub-biome.
- The JSON contains four-way neighbor links and 10×9 map coordinates.
- The 9 level bands cover levels 1–720 in 80-level bands; encounter scaling remains a runtime responsibility.
- Settlements are classified as capital, town, or none. Existing named hubs are retained where possible.

## Important implementation boundary

The current cartridge runtime still uses its existing 72-zone, 9×8 world grid and 9-byte visited-zone bitset. This atlas adds the complete 90-zone world design and data contract, but does **not** silently claim the GBC runtime has been migrated. Runtime migration must update the grid dimensions, visited flags/save format, zone lookup, level/biome assignment, transitions, world map UI, and tests together. The current 72-zone ROM remains intact until that migration is implemented and verified.

The illustrated parchment map is a visual overview of Aelmoor. In-game GBC navigation should use a simplified 10×9 tile map with a cursor, realm names, visited markers, and a zone detail panel rather than attempting to render the full parchment artwork on the console.
