# Native gameplay ownership boundaries

The native C++ server is the authoritative public-game foundation. The native
client renders and requests actions; it never owns flight, combat, inventory,
economic, or progression outcomes. The TypeScript/browser runtime remains a
reference and prototype implementation and is not merged into this authority
path.

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
systems are implemented by Batch 3.
