# Online World Server

The ROM is a single-player Game Boy Color program. Online play therefore runs through a separate Node.js service; the GBC cartridge does not currently make internet connections. This server is an authoritative starter backend for a future web/desktop client or a network bridge. It is a small persistent-world foundation, not a production MMO deployment.

## Run locally

Requirements: Node.js 20 or newer. No npm packages are required.

```sh
cd server
npm start
```

The service listens on port 8787. Set `PORT` to change it, `WORLD_DATA_FILE` to select its JSON save file, and `ALLOWED_ORIGIN` to restrict browser access to one origin. The JSON file stores accounts, characters, guilds, parties, and recent zone events. Passwords are stored as salted scrypt hashes. Session tokens are in memory and expire after 90 seconds without a request; saved characters remain.

## Realm and character loop

1. `GET /api/realms` lists Aelmoor, Dragonfall, and Iron Oath.
2. `POST /api/accounts/register` accepts `{ "email", "password" }`.
3. `POST /api/session/login` accepts `{ "email", "password", "realmId" }`; use the returned bearer token for authenticated requests.
4. `POST /api/characters` creates a realm character from `{ "name", "race", "className" }`.
5. `POST /api/world/enter` with `{ "characterId" }` makes that character visible online.
6. Poll `GET /api/world/state?after=<event-id>` for nearby players and retained zone events. Submit one-tile moves to `POST /api/world/move` and connected zone changes to `POST /api/world/zone`.
7. `POST /api/chat` accepts `say`, `zone`, `party`, `guild`, and `whisper` channels. Party and guild channels are limited to their memberships.

Authenticated social routes create a party (`POST /api/party`), invite a realm character (`POST /api/party/invite` with `{ "name" }`), leave (`DELETE /api/party`), create a guild (`POST /api/guilds` with `{ "name" }`), invite a member (`POST /api/guilds/invite`), and leave (`DELETE /api/guilds`). `GET /api/social` returns the character's current party and guild. Party size is capped at five. Guild leadership transfers to the next member if the leader leaves.

All authenticated routes use `Authorization: Bearer <token>`. Route errors return JSON `{ "error": "..." }`. Public character records exclude account IDs. The server checks realm ownership, character-name uniqueness per realm, zone boundaries, movement bounds/rate, chat rate, and group membership.

## MMORPG service boundaries

The server owns account identity, realm character records, shared zone presence, chat delivery, groups, and the event cursor. Character records include level, XP, gold, and inventory fields for later authoritative gameplay APIs; this starter does not yet expose combat, loot, trade, or progression mutation routes. Clients send requests rather than announcing trusted positions or character identity. Zone state is retained in a bounded event queue so clients can recover after missed polls.

The next production systems should be added as separate services around this authority: database-backed persistence and migrations; persistent quest and dungeon instance state; server-validated combat, drops, equipment, and trading; moderation and reporting; party finder and guild ranks; reconnect-safe sessions; telemetry; and rate limits at the edge. Do not expose this prototype directly to the public internet without those controls. The game ROM's `src/network_protocol.h` remains an unused sketch and is not wired to this HTTP service.

## Current client integration

The API is independent of GBDK, so it can be exercised by a web client or a local bridge. A bridge for the cartridge needs a supported transport and a UI for realm selection, login, remote player sprites, chat, and reconnect status. Local Game Boy link-cable multiplayer is a separate transport and cannot connect directly to this server without such a bridge.
