# Native gameplay ownership boundaries

The native C++ server is the authoritative public-game foundation. The native
client renders and requests actions; it never owns flight, combat, inventory,
economic, or progression outcomes. The TypeScript/browser runtime remains a
reference and prototype implementation and is not merged into this authority
path.

Graphics capability negotiation is isolated in the native client boundary. The
fixed-function renderer requests OpenGL 3.0 compatibility first and falls back
to OpenGL 2.1 compatibility. Automatic selection attempts hardware OpenGL 3.3
core initialization and uses core after context, functions, shaders, font
atlas, and GPU resources succeed. Failure destroys partial core resources and
creates a clean legacy context. Explicit core never silently falls back, and
a core context is never passed to fixed-function code.
Runtime reports distinguish actual vendor, renderer, profile, and
software/hardware classification from the requested context. Context request,
modern function loading, shader/program ownership, buffer ownership, transform
math, and the diagnostic scene are separate from the legacy gameplay renderer.
Core gameplay now consumes a renderer-neutral presentation snapshot assembled
from the existing client `View`; it contains copied player, station, asteroid,
contact, target, feedback, and HUD state but has no networking, authority, or
OpenGL ownership. Static and bounded dynamic geometry are separate GPU paths.
The core path separates world geometry, geometric HUD indicators, textured
bitmap text, and recent-message/GalNet presentation. `TextRenderer` owns one
bounded font atlas and one batched six-vertex quad per visible glyph; the GLSL
text shader samples the glyph mask and multiplies it by the requested color.
It consumes snapshot/log data but never sends commands or creates authoritative
events. Batch 12's `auto` path attempts hardware core initialization first,
including shaders, atlas, and GPU resources, and destroys partial resources
before recreating a legacy context on failure. Explicit core never falls back.
Batch 12 drives the complete career with a guarded
loopback-only client driver whose ordinary SDL events pass through `UiInput`.
The driver observes presentation copies and cannot change server outcomes.
The TLS server/persistence path remains authoritative. Source builds separate
the headless server and SDL/OpenGL client; split packages preserve player data
outside package-owned paths.

Core text is deliberately small: a checked-in 5x7 bitmap alphabet is packed
once into a 96x48 RGBA atlas with padded six-by-eight cells. Unsupported
characters become `?`, UTF-8 sequences are bounded to one fallback glyph, and
generated strings are clipped to fixed character/vertex bounds. This is
sufficient for the current cockpit, mission, reputation, GalNet, and combat
feedback, but is not a general Unicode, font-download, or text-shaping
subsystem.

The Batch 8 core gameplay test keeps two concerns explicit. A TLS socket
harness drives authoritative account, mission, mining, docking, combat,
recovery, and reconnect predicates. Deterministic render fixtures then verify
normal, mining, target, combat, docked, destroyed, and GalNet presentation
states through the real 3.3-core client path. This avoids making graphical
automation depend on uncontrolled gameplay timing while still testing the
authoritative loop and the renderer separately.

Batch 9 extends the same boundary with `UiState`. The client owns only bounded
navigation/focus state and presentation-ready copies of server responses:
connection status, profile fields, market rows derived from the shared station
catalogue, mission identity/progress, owned/fitted module identifiers, GalNet
events, options, graphics diagnostics, and recent status messages. The core
renderer receives that copy through `PresentationSnapshot`; it does not read
network sockets, send commands, validate prices, or mutate persistence.
Stable organization identifiers remain separate from their display names.
Keyboard actions map to the existing authenticated commands, so server
validation remains authoritative for every transaction.

Core telemetry uses explicit units. `glyphs` is the visible glyph-quad count;
`text-vertices` and `text-glyph-vertices` are both six vertices per glyph for
the current non-indexed path; `text-indices` is zero; `text-components` is
eight floats per uploaded vertex; and `text-bytes` is the uploaded VBO byte
count. Atlas dimensions/bytes, text draw calls, UI/world vertices, CPU build
times, and render time are reported separately. Text and atlas resources are
created once per context and reused across frames.

Batch 10 adds renderer-neutral hit regions derived from the same logical
960x600 layout used by core rendering. SDL mouse coordinates are converted
through the letterbox scale before hit testing. Hover, focus, disabled state,
bounded wheel/PageUp/PageDown scrolling, and single-click command mapping stay
in the client presentation layer; existing server commands still validate
every transaction.

Batch 11 adds a deliberately small `career::State`, not a general quest
engine. Stable IDs `career.first_ore`, `career.kepler_supply`, and
`career.red_wake_response` encode prerequisites, bounded progress, idempotent
completion, and the persisted vertical-slice completion marker. First Ore
continues using its original mission fields for compatibility; the appended
career record owns the later stages and onboarding dismissal. The server alone
changes stages, delivery cargo, rewards, reputation, combat completion, and
GalNet state. The client displays copies through `UiState` and the core
cockpit.

Career persistence is `career-v1 supply response purchased intro complete`,
appended after existing profile extensions. Missing records default safely;
invalid ranges, prerequisites, and extra fields are rejected. Supply delivery
consumes exactly two server-purchased parts at Kepler, and the Red Wake stage
reuses the deterministic hostile and reward transaction. Completion unlocks
continued free play rather than changing simulation mode.

## Current compatibility boundary

`Profile` stores commander identity, progression, economy, reputation, and a
fleet of persistent `OwnedShip` instances. The active instance is selected by
stable ID; hull definitions remain immutable shared data. The legacy commander
record projects the active vessel for backward compatibility, while `S` records
persist the authoritative fleet. The ownership boundaries are:

- **COMMANDER:** identity, credentials, credits, XP, progression, and future
  guild membership.
- **SHIP:** each owned instance has its own `flight::State` (hull, fuel, cargo,
  position and docking), upgrades, modules, visual variant, livery, wear, and storage state.
  Exactly one is active; all state and switching are server-authoritative.
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
