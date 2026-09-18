# Native gameplay ownership boundaries

The native C++ server is the authoritative public-game foundation. The native
client renders and requests actions; it never owns flight, combat, inventory,
economic, or progression outcomes. The TypeScript/browser runtime remains a
reference and prototype implementation and is not merged into this authority
path.

Graphics capability negotiation is isolated in the native client boundary. The
fixed-function renderer requests OpenGL 3.0 compatibility first and falls back
to OpenGL 2.1 compatibility. OpenGL 3.3 core is deliberately not selected by
normal gameplay; the experimental core diagnostic renderer requests OpenGL
3.3 core explicitly and never passes a core context to fixed-function code.
Runtime reports distinguish actual vendor, renderer, profile, and
software/hardware classification from the requested context. Context request,
modern function loading, shader/program ownership, buffer ownership, transform
math, and the diagnostic scene are separate from the legacy gameplay renderer.
Core gameplay now consumes a renderer-neutral presentation snapshot assembled
from the existing client `View`; it contains copied player, station, asteroid,
contact, target, feedback, and HUD state but has no networking, authority, or
OpenGL ownership. Static and bounded dynamic geometry are separate GPU paths.
The core path can be made the automatic renderer only after gameplay-rendering
parity, text, cockpit, and stability are reached.

## Current compatibility boundary

`Profile` still physically stores the commander record, active ship state,
credits, experience, upgrades, and owned/fitted modules. This is intentional:
moving those fields into a new persistence schema would add migration risk
without helping the current slice. Conceptually, the fields are separated as:

- **COMMANDER:** identity, credentials, credits, XP, progression, and future
  guild membership.
- **SHIP:** the active `flight::State` (hull, fuel, cargo, position and
  docking), ship upgrades, and fitted modules. The current profile has one
  active ship; its state is server-authoritative.
- **PERSONAL INVENTORY:** future stored commander-owned modules, salvage and
  other assets. Batch 3 keeps one small persisted salvage counter as a
  compatibility bridge rather than introducing a second inventory system.
- **FUTURE GUILD ASSETS:** guild treasury, shared inventory, hangars, stations,
  structures and projects. These must not be placed in the commander or active
  ship state when implemented.

Runtime hostile NPCs are ephemeral server entities. Their deterministic
generation and reward-claim state are persisted so a restart cannot replay a
destroyed target's reward.

## Canonical organizations and reputation

The canonical organization registry lives in `native/shared/organizations.*`.
Stable lowercase identifiers are persistence/protocol keys and are separate
from display text:

| Identifier | Display name |
| --- | --- |
| `authority.kepler` | Kepler Authority |
| `corp.orion` | Orion Extraction Group |
| `criminal.vanta` | Vanta Syndicate |
| `criminal.red_wake` | Red Wake |
| `faction.commonwealth` | Helion Commonwealth |

`Profile::reputation` is commander-owned, supports all organizations at once,
and stores bounded values from -100 to 100. Labels are deterministic:
`-100..-75 Hostile`, `-74..-25 Unfriendly`, `-24..24 Neutral`,
`25..74 Friendly`, and `75..100 Allied`. Older records receive neutral
defaults; the Batch 4 reputation summary is an appended persistence field.
Mission and combat handlers apply standing changes server-side and roll them
back with rewards when an atomic save fails.

The First Ore contract is issued by Orion under Kepler Authority jurisdiction.
The current hostile is a Red Wake raider. Vanta is present only as a
registered organization and future-career hook. Deterministic GalNet events
are persisted only when these real state transitions occur. The canonical
setting source is [`docs/lore/world-bible-v0.2.txt`](lore/world-bible-v0.2.txt);
it is a design source, not a claim that its later guild, alien, industry,
station, or territorial systems are implemented.

## Extension path

The intended long-term progression is:

`PLAYER → SPACE GUILD → EXPEDITION → UNKNOWN-SPACE CLAIM → OUTPOST → STATION → INDUSTRIAL NETWORK → CAPITAL SHIPYARD → TERRITORIAL POWER`

Guild membership should later define ranks, permissions, treasury, shared
inventory, ship storage, diplomacy, alliances and territorial claims. Player-
built station facilities can eventually include markets, black markets,
shipyards, refineries, research labs, factories, hangars, personal/guild
storage, fuel and repair depots, command centers, and sensor/communications
facilities. Deep-space structures may specialize more strongly than ordinary
stations.

Industry should use physical assets and jobs:

`RAW RESOURCE → REFINERY → REFINED MATERIAL → FACTORY → COMPONENT → SHIP / MODULE / STATION CONSTRUCTION`

Research can unlock blueprints, technology, material/manufacturing efficiency,
advanced modules, hulls, station technologies and capital technologies.
Capital shipyards are guild/player-built infrastructure required for XL and
capital hulls; regular shipyards must not construct those highest classes.
Territory should require active infrastructure and upkeep, not free permanent
claims. None of these guild, station, industry, research, capital or territory
systems are implemented by Batch 4.
