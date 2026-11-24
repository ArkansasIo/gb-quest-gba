import { createServer } from 'node:http';
import { randomBytes, scrypt as scryptCallback, timingSafeEqual } from 'node:crypto';
import { promisify } from 'node:util';
import { existsSync, readFileSync, renameSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

// Small authoritative REST/polling server for the GBC game's future online
// companion client. Clients submit intent; this process owns persisted state.
const scrypt = promisify(scryptCallback);
const here = dirname(fileURLToPath(import.meta.url));
const dataFile = process.env.WORLD_DATA_FILE || join(here, 'world-data.json');
const port = Number(process.env.PORT || 8787);
const realmSeeds = [
  { id: 'aelmoor', name: 'Aelmoor', kind: 'Normal', population: 'Low' },
  { id: 'dragonfall', name: 'Dragonfall', kind: 'Roleplay', population: 'Low' },
  { id: 'iron-oath', name: 'Iron Oath', kind: 'Seasonal', population: 'Low' },
];
const sessions = new Map();
const rateBuckets = new Map();
const now = () => Date.now();
const id = (bytes = 12) => randomBytes(bytes).toString('hex');
const cleanName = (value, max = 16) => String(value || '').trim().replace(/[^\p{L}\p{N}_ -]/gu, '').slice(0, max);
const defaultData = () => ({ version: 1, accounts: {}, characters: {}, parties: {}, guilds: {}, events: {}, nextEvent: 1 });
let data = defaultData();

if (existsSync(dataFile)) {
  try {
    const loaded = JSON.parse(readFileSync(dataFile, 'utf8'));
    if (loaded.version === 1) data = { ...defaultData(), ...loaded };
  } catch (error) {
    console.error(`Could not load ${dataFile}: ${error.message}`);
    process.exit(1);
  }
}
// Connections do not survive a process restart; saved characters do.
for (const character of Object.values(data.characters)) character.online = false;

function persist() {
  const temp = `${dataFile}.tmp`;
  writeFileSync(temp, JSON.stringify(data), { mode: 0o600 });
  renameSync(temp, dataFile);
}

function publicCharacter(character) {
  const { accountId, ...visible } = character;
  return visible;
}

function emit(realmId, zoneId, type, payload) {
  const event = { id: data.nextEvent++, at: now(), type, payload };
  const key = `${realmId}:${zoneId}`;
  const queue = data.events[key] || (data.events[key] = []);
  queue.push(event);
  if (queue.length > 200) queue.splice(0, queue.length - 200);
  persist();
}

function reply(res, status, body) {
  res.writeHead(status, { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store' });
  res.end(JSON.stringify(body));
}

async function bodyJson(req) {
  let raw = '';
  for await (const chunk of req) {
    raw += chunk;
    if (raw.length > 64 * 1024) throw Object.assign(new Error('Request body is too large.'), { status: 413 });
  }
  if (!raw) return {};
  try { return JSON.parse(raw); }
  catch { throw Object.assign(new Error('Request body must be valid JSON.'), { status: 400 }); }
}

function getSession(req) {
  const token = (req.headers.authorization || '').replace(/^Bearer\s+/i, '');
  const session = sessions.get(token);
  if (!session) return null;
  session.touchedAt = now();
  return session;
}

function getCharacter(session, characterId) {
  const character = data.characters[characterId || session.characterId];
  if (!character || character.accountId !== session.accountId || character.realmId !== session.realmId)
    return null;
  return character;
}

function zoneKey(character) { return `${character.realmId}:${character.zoneId}`; }

function zonesConnected(a, b) {
  const ac = a % 9, ar = Math.floor(a / 9), bc = b % 9, br = Math.floor(b / 9);
  const adjacent = Math.abs(ac - bc) + Math.abs(ar - br) === 1;
  const trails = [[1,19],[3,22],[5,41],[7,48],[8,53],[9,27],[11,35],[12,39],
    [14,42],[15,44],[17,26],[18,36],[20,45],[21,33],[24,50],[56,69]];
  return adjacent || trails.some(([from, to]) => (a === from && b === to) || (a === to && b === from));
}

function throttle(key, limit = 12, windowMs = 5000) {
  const time = now();
  const bucket = rateBuckets.get(key) || { start: time, count: 0 };
  if (time - bucket.start > windowMs) { bucket.start = time; bucket.count = 0; }
  bucket.count++;
  rateBuckets.set(key, bucket);
  return bucket.count <= limit;
}

async function route(req, res) {
  const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  const path = url.pathname.replace(/\/$/, '') || '/';
  if (req.method === 'OPTIONS') return reply(res, 204, {});
  if (req.method === 'GET' && path === '/health') return reply(res, 200, { ok: true, service: 'labyrinth-world', version: 1 });
  if (req.method === 'GET' && path === '/api/realms') return reply(res, 200, { realms: realmSeeds });

  const body = ['POST', 'PUT', 'PATCH'].includes(req.method) ? await bodyJson(req) : {};

  if (req.method === 'POST' && path === '/api/accounts/register') {
    const email = String(body.email || '').trim().toLowerCase();
    const password = String(body.password || '');
    if (!/^\S+@\S+\.\S+$/.test(email) || password.length < 10 || password.length > 128)
      return reply(res, 400, { error: 'Use a valid email and a password of 10–128 characters.' });
    if (data.accounts[email]) return reply(res, 409, { error: 'An account with that email already exists.' });
    const salt = randomBytes(16);
    const hash = await scrypt(password, salt, 64);
    data.accounts[email] = { id: id(), salt: salt.toString('hex'), passwordHash: hash.toString('hex'), createdAt: now() };
    persist();
    return reply(res, 201, { created: true });
  }

  if (req.method === 'POST' && path === '/api/session/login') {
    const email = String(body.email || '').trim().toLowerCase();
    if (String(body.password || '').length > 128) return reply(res, 400, { error: 'Password is too long.' });
    if (!throttle(`login:${email}`, 8, 60000)) return reply(res, 429, { error: 'Too many login attempts.' });
    const account = data.accounts[email];
    if (!account) return reply(res, 401, { error: 'Email or password is incorrect.' });
    const candidate = await scrypt(String(body.password || ''), Buffer.from(account.salt, 'hex'), 64);
    const expected = Buffer.from(account.passwordHash, 'hex');
    if (candidate.length !== expected.length || !timingSafeEqual(candidate, expected))
      return reply(res, 401, { error: 'Email or password is incorrect.' });
    const realmId = realmSeeds.some(realm => realm.id === body.realmId) ? body.realmId : realmSeeds[0].id;
    const characters = Object.values(data.characters)
      .filter(character => character.accountId === account.id && character.realmId === realmId)
      .map(publicCharacter);
    const token = randomBytes(32).toString('base64url');
    sessions.set(token, { accountId: account.id, realmId, touchedAt: now() });
    return reply(res, 200, { token, realmId, characters });
  }

  if (req.method === 'POST' && path === '/api/session/logout') {
    const token = (req.headers.authorization || '').replace(/^Bearer\s+/i, '');
    const session = sessions.get(token);
    if (session?.characterId) leaveWorld(session.characterId);
    sessions.delete(token);
    return reply(res, 200, { loggedOut: true });
  }

  const session = getSession(req);
  if (!session) return reply(res, 401, { error: 'Sign in to a realm first.' });

  if (req.method === 'POST' && path === '/api/characters') {
    const name = cleanName(body.name);
    if (name.length < 3) return reply(res, 400, { error: 'Character names need at least 3 letters or numbers.' });
    const realmId = session.realmId;
    const duplicate = Object.values(data.characters).some(c => c.realmId === realmId && c.name.toLowerCase() === name.toLowerCase());
    if (duplicate) return reply(res, 409, { error: 'That name is already used on this realm.' });
    const character = {
      id: id(8), accountId: session.accountId, realmId, name,
      race: cleanName(body.race, 16) || 'Human', className: cleanName(body.className, 16) || 'Adventurer',
      level: 1, xp: 0, zoneId: 10, x: 7, y: 7, hp: 20, maxHp: 20,
      gold: 0, inventory: [], quests: [], guildId: null, partyId: null,
      createdAt: now(), lastSeenAt: now(), online: false,
    };
    data.characters[character.id] = character;
    persist();
    return reply(res, 201, { character: publicCharacter(character) });
  }

  if (req.method === 'POST' && path === '/api/world/enter') {
    const character = getCharacter(session, body.characterId);
    if (!character) return reply(res, 404, { error: 'Character not found on this realm.' });
    if (session.characterId && session.characterId !== character.id) leaveWorld(session.characterId);
    session.characterId = character.id;
    character.online = true;
    character.lastSeenAt = now();
    const list = Object.values(data.characters).filter(c => c.online && zoneKey(c) === zoneKey(character)).map(publicCharacter);
    emit(character.realmId, character.zoneId, 'player-entered', { character: publicCharacter(character) });
    persist();
    return reply(res, 200, { character: publicCharacter(character), players: list });
  }

  const character = getCharacter(session);
  if (!character) return reply(res, 409, { error: 'Choose a character and enter the world first.' });

  if (req.method === 'GET' && path === '/api/world/state') {
    const players = Object.values(data.characters)
      .filter(c => c.online && c.id !== character.id && zoneKey(c) === zoneKey(character))
      .map(publicCharacter);
    const after = Number(url.searchParams.get('after') || 0);
    const events = (data.events[zoneKey(character)] || []).filter(event => event.id > after);
    return reply(res, 200, { serverTime: now(), character: publicCharacter(character), players, events, cursor: data.nextEvent - 1 });
  }

  if (req.method === 'POST' && path === '/api/world/move') {
    if (!throttle(`move:${character.id}`, 10, 1000)) return reply(res, 429, { error: 'Movement updates are arriving too quickly.' });
    const x = Number(body.x), y = Number(body.y), moved = Math.abs(x - character.x) + Math.abs(y - character.y);
    if (!Number.isInteger(x) || !Number.isInteger(y) || x < 1 || x > 14 || y < 1 || y > 12 || moved > 1)
      return reply(res, 400, { error: 'Move one tile at a time inside the zone.' });
    character.x = x; character.y = y; character.lastSeenAt = now();
    emit(character.realmId, character.zoneId, 'player-moved', { id: character.id, x, y });
    return reply(res, 200, { x, y });
  }

  if (req.method === 'POST' && path === '/api/world/zone') {
    const zoneId = Number(body.zoneId);
    if (!Number.isInteger(zoneId) || zoneId < 0 || zoneId > 71) return reply(res, 400, { error: 'Unknown zone.' });
    if (!zonesConnected(character.zoneId, zoneId)) return reply(res, 403, { error: 'That zone is not connected by a road or trail.' });
    const oldZone = character.zoneId;
    emit(character.realmId, oldZone, 'player-left-zone', { id: character.id });
    character.zoneId = zoneId; character.x = 7; character.y = 7; character.lastSeenAt = now();
    emit(character.realmId, zoneId, 'player-entered', { character: publicCharacter(character) });
    return reply(res, 200, { zoneId, x: character.x, y: character.y });
  }

  if (req.method === 'POST' && path === '/api/chat') {
    const channel = ['say', 'zone', 'party', 'guild', 'whisper'].includes(body.channel) ? body.channel : 'say';
    const text = String(body.text || '').trim().slice(0, 120);
    if (!text || !throttle(`chat:${character.id}`, 5, 10000)) return reply(res, 429, { error: 'Message empty or chat rate limit reached.' });
    let recipients = [];
    if (channel === 'say' || channel === 'zone') recipients = Object.values(data.characters).filter(c => c.online && zoneKey(c) === zoneKey(character));
    if (channel === 'party' && character.partyId) recipients = partyMembers(character.partyId);
    if (channel === 'guild' && character.guildId) recipients = Object.values(data.characters).filter(c => c.guildId === character.guildId && c.online);
    if (channel === 'whisper') {
      const target = Object.values(data.characters).find(c => c.name.toLowerCase() === cleanName(body.to).toLowerCase() && c.realmId === character.realmId);
      if (target) recipients = [character, target];
    }
    if (!recipients.some(c => c.id === character.id)) recipients.push(character);
    const message = { id: data.nextEvent++, at: now(), type: 'chat', payload: { channel, from: character.name, text } };
    for (const target of recipients) {
      const key = zoneKey(target), queue = data.events[key] || (data.events[key] = []);
      queue.push(message);
      if (queue.length > 200) queue.splice(0, queue.length - 200);
    }
    persist();
    return reply(res, 200, { sent: true, message });
  }

  if (req.method === 'POST' && path === '/api/party') {
    if (character.partyId) return reply(res, 409, { error: 'Leave your current party first.' });
    const party = { id: id(6), leaderId: character.id, members: [character.id], createdAt: now() };
    data.parties[party.id] = party; character.partyId = party.id; persist();
    return reply(res, 201, { party: partyView(party) });
  }

  if (req.method === 'POST' && path === '/api/party/invite') {
    const target = findCharacter(body.name);
    const party = data.parties[character.partyId];
    if (!party || party.leaderId !== character.id) return reply(res, 403, { error: 'Only the party leader can invite.' });
    if (!target || !target.online || target.realmId !== character.realmId) return reply(res, 404, { error: 'That player is not online on this realm.' });
    if (party.members.length >= 5 || target.partyId) return reply(res, 409, { error: 'Party full or player already grouped.' });
    party.members.push(target.id); target.partyId = party.id;
    emit(target.realmId, target.zoneId, 'party-joined', { party: partyView(party) }); persist();
    return reply(res, 200, { party: partyView(party) });
  }

  if (req.method === 'DELETE' && path === '/api/party') {
    leaveParty(character); persist(); return reply(res, 200, { left: true });
  }

  if (req.method === 'POST' && path === '/api/guilds') {
    if (character.guildId) return reply(res, 409, { error: 'You are already in a guild.' });
    const name = cleanName(body.name, 24);
    if (name.length < 3) return reply(res, 400, { error: 'Guild name must be at least 3 characters.' });
    if (Object.values(data.guilds).some(g => g.realmId === character.realmId && g.name.toLowerCase() === name.toLowerCase()))
      return reply(res, 409, { error: 'That guild name is taken.' });
    const guild = { id: id(6), name, realmId: character.realmId, leaderId: character.id, members: [character.id], createdAt: now() };
    data.guilds[guild.id] = guild; character.guildId = guild.id; persist();
    return reply(res, 201, { guild: guildView(guild) });
  }

  if (req.method === 'POST' && path === '/api/guilds/invite') {
    const guild = data.guilds[character.guildId], target = findCharacter(body.name);
    if (!guild || guild.leaderId !== character.id) return reply(res, 403, { error: 'Only the guild leader can invite in this starter server.' });
    if (!target || target.realmId !== character.realmId || target.guildId) return reply(res, 404, { error: 'Player unavailable or already in a guild.' });
    guild.members.push(target.id); target.guildId = guild.id; persist();
    return reply(res, 200, { guild: guildView(guild) });
  }

  if (req.method === 'DELETE' && path === '/api/guilds') {
    leaveGuild(character); persist(); return reply(res, 200, { left: true });
  }

  if (req.method === 'POST' && path === '/api/quests/accept') {
    const questId = String(body.questId || '').slice(0, 32);
    if (!questId) return reply(res, 400, { error: 'questId is required.' });
    if (character.quests.includes(questId)) return reply(res, 409, { error: 'Quest already accepted.' });
    if (character.quests.length >= 10) return reply(res, 409, { error: 'Quest log is full.' });
    character.quests.push(questId); persist(); return reply(res, 200, { quests: character.quests });
  }

  if (req.method === 'GET' && path === '/api/character') return reply(res, 200, { character: publicCharacter(character) });
  if (req.method === 'GET' && path === '/api/social') {
    const party = character.partyId ? data.parties[character.partyId] : null;
    const guild = character.guildId ? data.guilds[character.guildId] : null;
    return reply(res, 200, { party: party ? partyView(party) : null, guild: guild ? guildView(guild) : null });
  }
  return reply(res, 404, { error: 'Route not found.' });
}

function partyMembers(partyId) { return (data.parties[partyId]?.members || []).map(memberId => data.characters[memberId]).filter(Boolean); }
function partyView(party) { return { ...party, members: party.members.map(memberId => publicCharacter(data.characters[memberId])).filter(Boolean) }; }
function guildView(guild) { return { ...guild, members: guild.members.map(memberId => publicCharacter(data.characters[memberId])).filter(Boolean) }; }
function findCharacter(name) { return Object.values(data.characters).find(c => c.name.toLowerCase() === cleanName(name).toLowerCase()); }

function leaveParty(character) {
  const party = data.parties[character.partyId];
  if (!party) { character.partyId = null; return; }
  party.members = party.members.filter(memberId => memberId !== character.id);
  character.partyId = null;
  if (party.leaderId === character.id && party.members.length) party.leaderId = party.members[0];
  if (!party.members.length) delete data.parties[party.id];
}

function leaveGuild(character) {
  const guild = data.guilds[character.guildId];
  if (!guild) { character.guildId = null; return; }
  guild.members = guild.members.filter(memberId => memberId !== character.id);
  character.guildId = null;
  if (guild.leaderId === character.id && guild.members.length) guild.leaderId = guild.members[0];
  if (!guild.members.length) delete data.guilds[guild.id];
}

function leaveWorld(characterId) {
  const character = data.characters[characterId];
  if (!character) return;
  emit(character.realmId, character.zoneId, 'player-left', { id: character.id });
  character.online = false;
  character.lastSeenAt = now();
  character.partyId = character.partyId || null;
  persist();
}

const server = createServer(async (req, res) => {
  res.setHeader('access-control-allow-origin', process.env.ALLOWED_ORIGIN || '*');
  res.setHeader('access-control-allow-methods', 'GET,POST,DELETE,OPTIONS');
  res.setHeader('access-control-allow-headers', 'authorization,content-type');
  try { await route(req, res); }
  catch (error) {
    console.error(error);
    if (!res.headersSent) reply(res, error.status || 500, { error: error.status ? error.message : 'Server error.' });
    else res.destroy();
  }
});

server.listen(port, '0.0.0.0', () => console.log(`Labyrinth world server listening on :${port}`));
server.on('close', () => { for (const session of sessions.values()) if (session.characterId) leaveWorld(session.characterId); });

setInterval(() => {
  const cutoff = now() - 90_000;
  for (const [token, session] of sessions) {
    if (session.touchedAt < cutoff) {
      if (session.characterId) leaveWorld(session.characterId);
      sessions.delete(token);
    }
  }
}, 30_000).unref();
