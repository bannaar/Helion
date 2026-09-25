# Batch 14 v0.12 execution record

This file is the live implementation record for Batch 14. The repository-local
`HELION_Batch14_Prompt_v0.11.txt` and `HELION_Codex_Goal_v0.11.txt` remain the
baseline contract, amended by the user-approved v0.12 continuation supplied on
2026-09-25. That continuation preserves completed M0-M2 work and makes the
territorial/security clarification below authoritative for M3.

## Baseline

- Branch: `batch14/implementation-v0.11`
- Required and observed starting commit: `7d9aa976348c67ce948be80740088c10606714f8`
- Batch 13 implementation commit: `62fc8155a9ada5ea75c99f6bb748b26d0e1d72ad`
- Pre-existing untracked paths: `.agents/`, `plugins/` (unrelated; excluded
  from Batch 14 changes and commits)
- Canon: `docs/lore/HELION_World_Bible_v0.10.docx` and its repository-local
  Markdown rendering. The retained `docs/lore/world-bible-v0.2.txt` is an
  older reference and must not override v0.10.

## Repository audit

### Batch 13 preserved implementation

- `shared/ships.*` owns an immutable eight-hull registry with manufacturer,
  operator, visual variant, pad, mass, flight, cargo, hardpoint, utility,
  seven ordered core-slot, and optional-slot data.
- `shared/owned_ship.*` owns individually persistent ship instances. Instance
  flight state, upgrades, module ownership/fits, visual variant, livery, wear,
  and stored/deployed state are separate from canonical hull definitions.
- The server persists the active ship ID and all owned ships. Legacy profiles
  migrate deterministically to one Sidewinder; an `H` record remains a legacy
  active-ship projection while `S` records are authoritative ship instances.
- Kepler and Cinder shipyards are station- and pad-aware. Browse, purchase,
  switching, failure rollback, reconnect, and one-instance-per-hull replay
  resistance are server-authoritative and tested.
- Hull definitions drive cargo capacity, acceleration, top speed, handling,
  hull capacity, fuel capacity, mining compatibility, mass queries, and price.
  Existing per-ship engine/hull upgrades and fitted-module effects remain live.

### Existing integration points

- Accounts are server-owned profiles with scrypt password hashes, bounded
  authentication attempts, owner-only atomic save files, and legacy credential
  migration. There is no external account/launcher service yet.
- TLS 1.2+ is mandatory. Protocol v2 is a bounded 4096-byte newline command
  stream with an explicit welcome-version equality check. The client sends
  `INPUT` near frame cadence and polls `CONTACTS` once per second; flight and
  contact snapshots are text. There is no binary framing path yet.
- `native/client/main.cpp` currently couples connection, authentication,
  presentation navigation, input scheduling, response parsing, reconnect, and
  rendering in one loop. `UiScreen` is typed, but it is not an engine/session
  state controller and cannot by itself establish authoritative gameplay state.
- `shared/loadout.h` is the current canonical seven-entry module catalogue.
  Four legacy functional slots (`mining`, `engine`, `defense`, `weapon`) are
  persisted per owned ship. Purchase, fit, remove, rollback, reconnect, mining,
  fuel-efficiency, hull, and pulse-laser effects already exist, but definitions
  lack structured physical slot type/size and fit does not consult hull mounts.
- Commodities and service prices are station-specific in `shared/flight.*`,
  and shipyard offers are station-specific in `shared/shipyard.*`. Commodity
  stock, replenishment, orders, economic telemetry, and a persisted regional
  market service do not yet exist. Current faucets include ore sales, mission
  and career rewards, and combat rewards; current sinks include commodity,
  ship, module, upgrade, repair, and fuel purchases. These are not yet
  classified or aggregated.
- Organizations and bounded reputation exist. Rank, loyalty, warrants,
  bounties, system security ratings, and general crime evaluation do not.
  Existing Red Wake combat is a scripted server-authoritative encounter, not a
  general police or Concordat response simulation.
- Runtime environment identity is already server-controlled as development,
  test, or production and persisted files fail closed on mismatch. Untagged
  migration is development-only. TEST roles/admin operations/audit and
  production operations controls are not implemented.
- The authoritative server simulates and checkpoints profiles independently of
  connected clients, starts with zero players, accepts later connections,
  flushes dirty state on disconnect/shutdown, and uses a stable data lock.
- Native CMake supports combined, server-only, and client-only builds, split
  packages, install smoke tests, TLS integration, sanitizer builds, and
  graphical acceptance. No Dockerfile or provisioned production service stack
  exists; a systemd service asset and deployment/backup documentation do.
- GalNet events are persistent and deduplicated, but no economic telemetry
  adapter exists. The browser code under `src/` is legacy reference material
  and is not a second native-game authority.

## Smallest coherent milestone order

| Milestone | Scope | Status |
| --- | --- | --- |
| 0 | Audit baseline, record seams/deferrals, run relevant baseline validation | Complete |
| 1 | Extend the existing module registry with structured physical slot/size metadata; centralize compatibility; enforce it in existing server-authoritative fit paths; preserve saves/UI/effects | Complete |
| 2 | Add the regional seeded-market model, production-like versus TEST pricing policy, transaction safety, persistence, and economic telemetry for implemented flows | Complete |
| 3 | Add one authoritative galaxy graph with faction territories, capitals/hubs, derived borders, route classes, exact High/Low/Null security, nullsec sovereignty, persistence, and future travel-infrastructure hooks | Complete |
| 4 | Add explicit bounded/versioned binary framing over TLS for measured high-frequency traffic, compatibility negotiation, decoder safety tests, and benchmarks | In progress |
| 5 | Introduce a typed client engine-state controller and incrementally route connection/auth/navigation/reconnect without granting client authority | Not started |
| 6 | Complete TEST roles, typed admin operations/audit, and TEST eligibility bypass on top of the isolated pricing policy, with production isolation tests | Not started |
| 7 | Add the safe production operations foundation: status/lifecycle, least-privilege authorization boundaries, structured logs/metrics, backup/recovery and release documentation | Not started |
| 8 | Full migration/regression/security review, all build/package/sanitizer gates, completion report, and Batch 15 recommendations | Not started |

The order keeps persistence and authority in the existing server, establishes
data contracts before market/security/protocol consumers, and delays client
control-flow changes until the new authoritative responses are stable.

## v0.12 canonical territorial and security clarification

This section records the user-approved change for the next World Bible
revision. It supersedes conflicting older wording for Batch 14 implementation
without silently editing World Bible v0.10.

- NPC faction territory may use only exact 0.2-step ratings from `+5.0` through
  `+0.2` (High Security) or `-0.2` through `-5.0` (Low Security). NPC faction
  territory must never contain `0.0`.
- `0.0` is Null Security. Nullsec may be unclaimed or claimed by a player Guild
  or player Alliance. A claim changes sovereignty only; security remains
  exactly `0.0`.
- Security rating, sovereignty type, sovereign identifier, and
  discovery/charting state are separate authoritative fields. Unknown space is
  not an ownership type, and a charted system is not necessarily claimed.
- The galaxy requires one shared graph of stable systems, regions, permanent
  Gate connections, and route classes. Adjacency comes from that graph;
  faction and nullsec borders derive from adjacent sovereignty rather than
  from security value.
- Each implemented major NPC faction requires one capital, one distinct
  dominant commerce hub near it, developed interior and resource systems,
  frontier systems, low-security backwaters, border gateways, and
  secondary/dead-end routes.
- Inter-faction civilian traffic concentrates through a deliberately small
  set of protected major trunk starlanes joining border gateways and feeding
  toward faction commerce hubs. Trunks do not replace secondary routes,
  loops, frontier branches, resource routes, dangerous low-security
  alternatives, backwaters, or dead ends.
- M3 adds typed eligibility and endpoint seams for future player-built
  strategic travel infrastructure in claimed nullsec. It does not implement
  relays, bridges, tolls, fuel, access control, vulnerability, capital-jump,
  sovereignty-war, or Rift mechanics.

M3 executes as the following coherent submilestones:

| Submilestone | Scope | Status |
| --- | --- | --- |
| M3A | Shared topology/system/region/Gate data model and graph validation | Complete |
| M3B | NPC faction territories, capitals, commerce hubs, roles, and derived borders | Complete |
| M3C | Exact High/Low/Null rating model, ownership invariants, and response hooks | Complete |
| M3D | Protected trunks plus core, secondary, frontier, and backwater routes | Complete |
| M3E | Persisted unclaimed/Guild/Alliance nullsec sovereignty foundation | Complete |
| M3F | Future player travel-infrastructure eligibility hooks and shared query seam | Complete |

### M3 repository audit and shared-model decisions

- Before M3, no galaxy graph, region registry, Gate adjacency, territorial
  ownership, border representation, discovery state, or system security model
  existed. The only spatial data was the two-station Kepler flight map.
  `flight::State::station` and the `kStations` array drive current docking,
  markets, shipyards, recovery, rendering, and saves; they must remain stable.
- `organizations.*` already owns stable IDs for the Commonwealth, Compact,
  Directorate, and Meridian major factions. M3 reuses those IDs rather than
  adding a second faction catalogue. Kepler Authority remains a local
  organization and market owner inside Commonwealth-sovereign Kepler; market
  ownership and system sovereignty are intentionally distinct.
- `shared/galaxy.*` is the single topology model. It owns stable systems,
  regions, permanent Gate edges, route classes, major-faction capital/hub
  metadata, canonical initial security/sovereignty/discovery state, border
  derivation, response-policy hooks, and future infrastructure eligibility.
  UI, markets, future routing, crime, missions, and companion read models must
  query this model rather than hard-code independent maps.
- Security is stored as integer steps `-25..+25`; one step is exactly `0.2`.
  Parsing rejects other increments and out-of-range values. Positive values
  classify High, negative values Low, and only zero classifies Null. The older
  supported/deep/extreme Low Security bands remain secondary response hooks.
- Mutable `SystemState` keeps `securitySteps`, `SovereignType`/ID, and
  `DiscoveryState` as separate fields. Controlled helpers reject NPC-owned
  nullsec, null security inside NPC territory, player claims outside nullsec,
  claims of unknown space before charting, and regression from claimed space
  to unknown. Guild/Alliance IDs are typed, bounded safe tokens; claims never
  modify the security field.
- The initial graph has 28 systems in six regions and 35 permanent Gates. Four
  major protected trunks connect faction border gateways in a ring; faction
  core, secondary loops, resource/frontier links, low-security branches, and
  one-Gate backwaters remain independently represented. Each faction territory
  is connected, its commerce hub is adjacent to its distinct capital, and its
  trunk gateway is no more than two faction-local hops from the hub.
- Commonwealth `Concordia`, Directorate `Solaris`, and Meridian
  `Meridian Prime` retain World Bible identities. The new Compact capital
  `Freestead`, Compact hub `Crossroads`, Directorate hub `Helios Exchange`, and
  Meridian political capital `Meridian Forum` are implementation names to
  confirm in the next World Bible revision; they are not silent edits to v0.10.
- Existing Kepler and Cinder station indices are unchanged. Both now explicitly
  map to system `KEPLER`; market validation proves its `KEPLER_REACH` region
  comes from the authoritative galaxy registry.
- Canonical nullsec seeds unclaimed; M3 does not fabricate player organizations
  or production claims. The typed state accepts charted Guild/Alliance claims,
  and live restart coverage persists a representative Alliance claim while
  preserving security `0.0`. A future organization service can bind real IDs
  without changing the geography or persistence shape.
- One `Y` record per canonical system extends the existing atomic,
  environment-bound save with exact security steps, sovereignty type/ID, and
  discovery state. Legacy saves migrate to deterministic seed state. Loading
  rejects unknown, duplicate, incomplete, malformed, or invariant-breaking
  galaxy state before the server accepts clients.
- Authenticated `GALAXY` is an additive read-only query over the shared model.
  It exposes current system, territory capital/hub metadata, systems, permanent
  Gate route classes, derived borders, discovery, sovereignty, and future
  infrastructure eligibility. It grants no claim, security, movement, or
  infrastructure mutation authority and is a reusable server read-model seam
  for the native client and future Companion Service.

## Milestone 1 decisions

- Keep module assets on each `OwnedShip`; this is the existing persistent asset
  path. Do not add a parallel commander inventory.
- Keep the four legacy functional slot identities and wire commands for save and
  client compatibility. Add physical `WEAPON_HARDPOINT`, `UTILITY`,
  `CORE_INTERNAL`, and `OPTIONAL_INTERNAL` metadata to definitions, plus
  canonical `SlotSize` requirements.
- Treat the existing functional slot as the stable fitted-instance address for
  this milestone. A module definition maps that address to a physical slot type
  and required size. Later multi-mount instance indexing can migrate from this
  representation without mutating hull definitions.
- Centralize registry validation and hull compatibility in `shared/loadout.*`.
  The server must call the shared compatibility service before committing a
  fit; the client may display it but is never authoritative.
- Preserve all seven existing module IDs and effects. Add only representative
  definitions needed to test structured slot/size behavior; do not invent new
  active mechanics.
- Preserve `H` and `S` persistence compatibility. No persistence migration is
  required merely to add immutable definition metadata.

## Milestone 2 decisions

- Evolve `flight::State` cargo, `Profile::credits`, the existing atomic server
  save, and existing station prices. The market does not own a second copy of
  player inventory or currency. Shared `tradeAtPrice` and `dockAtPrice` entry
  points let the regional service supply an authoritative price while retaining
  legacy `trade` and `dock` behavior.
- Model six initial NPC orders: food, parts, and ore at Kepler and Cinder.
  Every definition has a station, system, region, organization owner, buy/sell
  capability, canonical price, target stock/demand/budget, and deterministic
  replenishment step. Kepler Authority seeds Kepler essentials; Orion owns the
  initial ore demand and Cinder market surface. Player order matching remains
  deferred rather than being simulated by a second inventory system.
- Treat development and production as production-like. They retain the exact
  existing commodity, ore, ship, and module prices for save/gameplay
  compatibility. Only private TEST selects abundant seeded inventory and the
  100-credit purchase policy for eligible commodities, ships, and modules.
  TEST NPC purchases from players use 80 credits, preventing an immediate
  same-station buy/sell inversion. Canonical prices remain independently
  queryable and immutable.
- Use finite production-like targets and a 60-second deterministic restock.
  NPC purchases are bounded by both demand and a per-order currency budget.
  Restock runs with zero connected players and atomically persists its result;
  save failure restores both order state and restock telemetry.
- Add `K` market records and one `T` telemetry record to the existing
  environment-bound save. Loading rejects unknown, duplicate, incomplete,
  negative, oversized, or malformed economic state. A legacy save receives
  deterministic seeded markets and zero historical counters through an
  additive migration; current credits in circulation are calculated from
  profiles so migration does not invent historical flows.
- Classify starter grants, mission/career payouts, and combat rewards as
  faucets; ship/module purchases and fuel/repair/upgrade services as sinks;
  and bounded NPC commodity/ore trades as transfers. Persist cumulative
  source/sink/transfer, purchase/service, market volume/value, mining, and
  restock counters. Fees and taxes report zero because neither exists yet; no
  arbitrary charge was added for telemetry completeness.
- Add the authenticated, dock-only `ECONOMY` query as an additive protocol
  surface. It reports the current station's location/owner/order state,
  effective policy, cumulative implemented-flow telemetry, and live player
  credits in circulation. All mutations remain in existing server handlers
  and save boundaries.
- For every implemented credit/cargo/market mutation, snapshot the affected
  profile, regional orders, GalNet state where relevant, dirty flag, and
  telemetry before persistence. A failed save restores prior state and emits
  no transaction result. Existing duplicate ship-purchase rejection and
  module ownership checks remain replay protection for those asset purchases.

Milestone decomposition changed only at Milestone 6: the isolated TEST
100-credit price policy is complete in Milestone 2 as required. Milestone 6
retains TEST roles, typed admin/audit operations, eligibility bypass, and their
production-isolation checks; it must reuse this policy rather than add a second
environment switch.

## Validation log

### Baseline audit

- `cmake -S . -B /tmp/helion-b14-debug -DCMAKE_BUILD_TYPE=Debug -DHELION_BUILD_CLIENT=ON -DHELION_BUILD_SERVER=ON -DHELION_BUILD_TESTS=ON -DHELION_WARNINGS_AS_ERRORS=ON`
  followed by `cmake --build /tmp/helion-b14-debug --parallel`: passed.
- `ctest --test-dir /tmp/helion-b14-debug -R 'helion_(flight|combat|career|ships|organizations|protocol|environment|data_lock|security)_tests' --output-on-failure`:
  9/9 passed.

### Milestone 1

- Warnings-as-errors Debug rebuild: passed.
- `ctest --test-dir /tmp/helion-b14-debug -R 'helion_(loadout|ships|protocol|flight|ui|presentation|career|combat)_tests|^helion_tls_integration$' --output-on-failure`:
  9/9 passed, including the 31.55-second live TLS/server persistence suite.
- `git diff --check`: passed.

Implemented evidence:

- Canonical seven-entry module definitions now include manufacturer, category,
  physical slot type, size, core-system identity, grade, canonical purchase
  price, mass, power draw, integrity, legal status, future requirements, TEST
  availability, and implemented modifiers.
- Shared compatibility covers hardpoint, utility, ordered core, and optional
  mounts and distinguishes unavailable slot types from undersized mounts.
- Server startup validates both ship and module registries. `OUTFIT LIST`
  reports definition/ownership/fitted/compatibility state. Purchases and fits
  repeat compatibility validation server-side and retain existing atomic
  persistence rollback.
- Focused tests cover lookup, invalid module, all physical slot names, valid
  fits, invalid slot type, invalid size, ordered core slots, deterministic
  effect summaries, and a live server rejection on a weaponless Mule.
- Existing live tests continue to cover unowned fit rejection, purchase
  rollback, fit visibility, implemented fuel/mining/hull/weapon effects,
  per-ship persistence, switching, and reconnect.

Milestone review: no save schema changed; no hull definition is mutated; text
protocol compatibility is additive; the client preview cannot commit a fit;
TEST/PRODUCTION policy is untouched; no parallel inventory, market, protocol,
or browser authority was introduced.

### Milestone 2

- Warnings-as-errors Debug rebuild in `/tmp/helion-b14-debug`: passed.
- `ctest --test-dir /tmp/helion-b14-debug -R
  'helion_(economy|environment|flight|protocol)_tests' --output-on-failure`:
  4/4 passed.
- `ctest --test-dir /tmp/helion-b14-debug -R '^helion_tls_integration$'
  --output-on-failure`: 1/1 passed in 32.34 seconds with live development and
  TEST economy assertions. The all-suite run also passed the live suite in
  33.59 seconds before the final TEST assertions were added; the focused rerun
  above validates those additions.
- `ctest --test-dir /tmp/helion-b14-debug --output-on-failure`: 23/23 passed
  in 152.68 seconds, including TLS/server persistence, install smoke, core
  gameplay, graphical acceptance, render-state, and renderer-fallback gates.
- `git diff --check`: passed.

Implemented evidence:

- Shared tests cover regional ownership, canonical Kepler/Cinder pricing,
  TEST-only 100-credit purchases, non-inverted TEST resale, finite stock,
  demand/budget rejection without mutation, deterministic restock, bounded ore
  sales, telemetry serialization, and malformed telemetry rejection.
- Environment tests prove development and production retain canonical prices,
  only private TEST selects convenience policy, non-TEST-eligible items retain
  canonical price, and ambiguous environments fail closed. Live TLS coverage
  additionally verifies a TEST commodity purchase and advertised ship/module
  prices are 100 while their canonical prices remain unchanged.
- Live TLS tests exercise production-like starter progression and existing
  commodity/ship/module/service flows. They verify exact regional order and
  telemetry deltas, induce a persistence failure during a purchase and observe
  unchanged credits/market/telemetry, then restart the server and verify the
  committed order book and counters were neither reset nor duplicated.
- Existing ship replay rejection, cargo capacity, insufficient credits,
  mining, ore sale, mission, combat, service rollback, reconnect, legacy save,
  and environment mismatch coverage remains green.

Milestone review: there is one shared market service and one existing player
cargo/credits path; no client can set price, stock, demand, budget, telemetry,
or transaction outcome. The save migration is additive and environment-bound.
Development/production never select TEST prices or supply. Protocol changes
are additive text responses, and no player matching, manufacturing, economic
AI, arbitrary fee, admin surface, or unrelated world feature entered scope.

### Milestone 3A-F

- Warnings-as-errors Debug rebuild in `/tmp/helion-b14-debug`: passed.
- `ctest --test-dir /tmp/helion-b14-debug -R
  'helion_(galaxy|protocol)_tests|^helion_tls_integration$'
  --output-on-failure`: 3/3 passed in 33.86 seconds after final invariant
  hardening. The preceding broader M3 regression selection passed 8/8.
- `ctest --test-dir /tmp/helion-b14-debug --output-on-failure`: 24/24
  passed in 173.66 seconds after the final repair, including live TLS/server
  persistence, security, install smoke, core gameplay, graphical acceptance,
  render-state, and renderer-fallback gates.
- `git diff --check`: passed.

Implemented evidence:

- All 51 exact security values from `-5.0` through `+5.0` round-trip in `0.2`
  steps. Outer and inner High/Low/Null boundaries, malformed values, and the
  supported/deep/extreme Low Security hook boundaries are deterministic.
- Tests reject NPC ownership of `0.0`, reject `0.0` inside NPC territory,
  accept charted Guild and Alliance nullsec claims, preserve security at
  `0.0`, require discovery before claims, and keep discovery independent from
  sovereignty.
- Registry validation covers stable IDs, known regions, unique undirected Gate
  edges, overall and per-faction connectivity, distinct adjacent capitals and
  commerce hubs, territory role completeness, gateway-to-hub distance,
  one-Gate dead ends, all route classes, and exactly four major trunks.
- Border tests derive eight initial ownership boundaries from Gate adjacency
  and prove a nullsec ownership change updates border results without modifying
  topology or security. All initial nullsec is unclaimed; tests exercise both
  Guild and Alliance ownership without adding phantom organizations.
- Future strategic-travel eligibility is true only for Guild/Alliance-owned,
  eligible `0.0`; no relay, toll, fuel, bridge, access-control, warfare, or
  Rift mechanic exists.
- Existing flight, career, ship, and economy tests remain green after stations
  and markets were linked to authoritative Kepler system/region identity.
- Live TLS coverage verifies `GALAXY` requires authentication, legacy saves
  gain exactly 28 system records, a valid charted Alliance claim survives
  shutdown/restart/reconnect, its rating remains `0.0`, infrastructure
  eligibility is derived rather than persisted separately, and borders are
  re-derived from the restored sovereignty state.

Milestone review: `shared/galaxy.*` is the only topology/territory/security
model. Persistence extends the existing save and lock path, and the query is
read-only and additive. No client declares ownership, discovery, security, or
route truth. Environment selection and prior outfitting/economy behavior are
unchanged. No player-organization backend, claim warfare, travel mechanic,
parallel map, companion backend, or browser authority was introduced.

## Milestone 4 protocol audit and execution map

- TLS 1.2+ remains the transport. The current application protocol is version
  2, bounded to 4096 bytes per newline frame, and incrementally decodes partial
  or coalesced reads. Client and server reject malformed lines; the native
  client requires an exact `WELCOME Helion/2` before sending commands.
- The native client submits `INPUT` at 10 Hz while authenticated and polls
  `CONTACTS` at 1 Hz. Every accepted `INPUT` currently causes text `FLIGHT`,
  `FUEL`, and `ACTIVE SHIP` responses, so flight replication—not login or
  station commands—is the first useful binary target.
- A binary `INPUT` frame with a full safe header is not automatically smaller
  than the compact 12–13 byte text command. It will be benchmarked rather than
  assumed to be a bandwidth win. The flight snapshot can combine fixed-width
  flight, fuel, destruction, and cooldown fields and is expected to produce a
  material byte reduction, subject to measurement.
- The existing line protocol remains an explicit compatibility mode during
  migration. Binary mode will require an explicit version negotiation; the
  server will not permanently guess protocol mode from arbitrary bytes. Low
  frequency commands may use a bounded typed text-command envelope so gameplay
  continues through one server handler rather than a second authority.

M4 executes as the following coherent submilestones:

| Submilestone | Scope | Status |
| --- | --- | --- |
| M4A | ABI-independent binary header, incremental bounded decoder, explicit field codecs, malformed/fuzz tests | Complete |
| M4B | Explicit binary-mode negotiation and deterministic update-required path while retaining bounded legacy mode | Not started |
| M4C | Migrate representative authoritative input/flight traffic without duplicating gameplay handlers | Not started |
| M4D | Measure bytes/encode/decode cost on realistic intervals; retain text where binary has no material benefit | Not started |

### M4A decisions and validation

- `shared/binary_protocol.*` defines a 14-byte wire header: two-byte magic,
  binary schema version, stable one-byte message type, two-byte flags,
  four-byte payload length, and four-byte request/correlation ID. Integers are
  big-endian and no C++ object layout, enum width, padding, pointer, or native
  boolean representation is sent.
- Payloads are capped at 4096 bytes. The decoder buffers at most one bounded
  frame, supports partial and multiple frames per read, and fails closed on
  bad magic, incompatible version, unknown type, nonzero unknown flags,
  oversized length, or truncated end-of-stream input.
- Representative codecs include explicit three-byte control input and a
  57-byte flight snapshot containing fixed-width IEEE-754 fields plus bounded
  cargo, credits, experience, station, hull, fuel, destruction, and cooldown
  state. Semantic validation occurs before encode and after decode.
- Warnings-as-errors build of `helion_binary_protocol_tests`,
  `helion_protocol_tests`, and `helion_flight_tests`: passed.
- `ctest --test-dir /tmp/helion-b14-debug -R
  'helion_(binary_protocol|protocol|flight)_tests' --output-on-failure`: 3/3
  passed. Coverage includes explicit header bytes, partial/coalesced frames,
  maximum/oversized payloads, malformed input and snapshot fields, deterministic
  update-required classification, and 2,000 deterministic random byte streams.
- `git diff --check`: passed.

## Deferred work

- Multiple independently addressed hardpoints/optional bays and module wear or
  damage need an explicit fitted-instance persistence migration; Batch 14 will
  not fake them by mutating hull definitions.
- Full player order matching, manufacturing, autonomous market optimization,
  police/GDF fleet AI, INSA/Gatewatch campaigns, character creation, launcher,
  CDN, public admin UI, and cloud provisioning remain explicitly out of scope.
- Production-like balance is exercised and observable but is not declared
  ready for ordinary-player handoff. Longer-duration earning-rate, stockout,
  replenishment, wealth-distribution, and replacement-cost trials remain a
  later validation gate; the 100-credit TEST configuration is not evidence for
  that gate.
- Historical economic counters cannot be reconstructed from legacy saves.
  Migration intentionally begins cumulative counters at zero while reporting
  current credits in circulation from authoritative profiles.
