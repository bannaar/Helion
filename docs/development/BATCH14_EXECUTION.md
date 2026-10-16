# Batch 14 v0.20 execution record

## Current M4 continuation — 2026-10-16

The user reviewed and approved M3 checkpoint
`f028ebd97250fb16661296f627df9905c3550ec4` and explicitly authorized M4.
This section supersedes the earlier stop-before-M4 instruction below, not the
completed M3 implementation or its validation evidence. Batch 14 as a whole
remains incomplete. Stop for review at the local M4 checkpoint before M5.

### Preserved-work audit

The starting branch was `batch14/implementation-v0.11`, HEAD `f028ebd`, six
commits ahead of the unchanged local origin tracking ref `280c0b0`. Tracked
files/index were clean; only `.agents/` and `plugins/` were untracked. Both
remain tool infrastructure outside the implementation/checkpoint. No fetch,
push, merge, rebase, reset or change to `main` is part of this continuation.

- `5ed7772`: implemented explicit 14-byte binary v1 framing, bounded incremental
  decoding, typed input/flight codecs, fixed-width big-endian encoding and
  focused unit tests. This is the retained transport architecture.
- `b19f08a`: mixed M3 work and M4 negotiation, client/server TLS integration,
  typed INPUT/flight traffic and per-connection request sequencing. M3's content
  is preserved at its approved boundary. M4's initial integration was partial:
  basic tests existed, but negotiation read boundaries, error ordering and
  cross-client broadcast correlation still had gaps.
- `3764db1`: implemented the original 100,000-iteration protocol benchmark.
  Preserved and extended, not replaced. Originally it did not gate semantic
  correctness or include the per-update ACTIVE SHIP envelope in aggregate bytes.
- Recovered protocol unit tests passed before changes. Existing live TLS tests
  covered basic negotiation/typed traffic; their existence was not treated as
  proof of all adversarial/compatibility behavior.

### Implemented M4 completion work

The [transport contract](BATCH14_PROTOCOL.md) records the exact wire format,
upgrade boundary, error behavior, limits and compatibility policy.

- Preserved TLS and the single authoritative command-dispatch path; no new
  gameplay authority, persistence format, M3 galaxy change or M5 engine exists.
- Completed graphical negotiation handling: ACK plus binary data in one TLS
  read, acknowledgement only after the request was sent, strict version checks,
  no automatic downgrade, immediate transport closure on terminal errors and
  binary-framed QUIT. Partial bytes no longer refresh the client reply deadline.
- Added explicit graphical `--legacy-protocol` compatibility. Terminal mode
  stays bounded text and rejects attempts to switch its decoder manually.
- Server upgrade rejects trailing partial text as well as complete pipelined
  commands. Binary errors are dispatched in stream order: valid prefixes finish
  before the terminal error, and trailing requests cannot mutate state.
- Broadcast correlation is zero for all recipients, not the sender's request
  ID; nested response scopes restore the original request ID.
- Retained strict payload bounds, direction checks, monotonic nonzero request
  IDs and sticky decoder failures. Binary text control characters now terminate
  the session. Float narrowing preserves legal snapshot boundary values.
- Added exhaustive input-byte checks, every split/truncated prefix, malformed
  headers/fields, atomic decode checks and retained fuzz-like random input.
  New real-server TLS tests cover negotiation, coalescing, partial frames,
  bounds, replayed purchases, reconnect semantics, broadcasts and authority.
  New real-client/adversarial-server TLS tests cover coalesced ACKs, explicit
  text compatibility, unsolicited/version-invalid ACKs, future correlations,
  malformed snapshots, directions, flags, payloads and lengths.
- The preserved benchmark is now a CTest validation gate with semantic and
  size assertions. Timing remains informational. It reports codec-only and
  full sampled update bytes separately and does not claim zero allocations.

### Final validation and checkpoint boundary

All applicable gates passed on 2026-10-16. Every configuration used
`HELION_WARNINGS_AS_ERRORS=ON`; no warnings-as-errors bypass was added.

| Configuration / gate | Result |
| --- | --- |
| Combined Debug, `/tmp/helion-b14-debug` | Build passed; complete CTest **30/30** passed |
| Release server-only, `/tmp/helion-b14-server` | Build passed; complete CTest **18/18** passed |
| Release client-only, `/tmp/helion-b14-client` | Build passed; complete CTest **20/20** passed |
| Debug ASan/UBSan server-only, `/tmp/helion-b14-sanitize` | Build passed; complete CTest **18/18** passed; no sanitizer diagnostics |
| Packaging | Combined/server/client TGZ and DEB: all six generated successfully |
| Installed launcher smoke | First-run TLS, account creation, saved login, permissions, shutdown and shipped protocol guide passed |
| Graphical integration | Core gameplay, binary journey/reconnect, explicit legacy journey/reconnect, render states and renderer fallback passed |
| Whitespace | `git diff --check` passed |

The full suites include existing environment/security, authoritative server
gameplay, economy, galaxy/persistence/migration, certificate verification and
live TLS tests. New protocol suites add adversarial client/server transport
checks described above. The terminal upgrade-rejection assertion was separately
rerun through `helion_tls_integration` after it was added and passed. Sanitizers
ran without ASAN/LSAN/UBSAN environment overrides; graphical driver paths were
validated in Debug/Release, not claimed to have sanitizer coverage.

Commands: `cmake --build <build> --parallel 3`,
`ctest --test-dir <build> --output-on-failure`, and
`cpack --config <build>/CPackConfig.cmake -B <build>/packages` for the three
non-sanitizer package configurations. CTest logs remain under each build's
`Testing/Temporary/`. The original 29-test combined suite also passed before
the legacy graphical journey became a permanent thirtieth gate.

During focused validation, two new assertions needed correction: the benchmark
must respect the legacy text cooldown's two-decimal precision, and the live BUY
test must expect the existing `OK BOUGHT` reply. Both now validate the actual
protocol contract; neither was removed or weakened to ignore a server failure.
All final gates pass; no unexplained failures or outstanding compiler/sanitizer
warnings remain. This is targeted protocol/security regression evidence, not
an exhaustive independent security audit or production-load certification.

M4A framing/codecs, M4B negotiation/compatibility, M4C typed authoritative
input/flight integration and M4D benchmark validation are implemented and
validated. The local M4 checkpoint extends `f028ebd` and all five earlier
recovery commits unchanged. Only this coherent M4 change is checkpointed;
tool infrastructure remains untracked. No remote publication or `main` change
is authorized. Stop here for review; M5 is the recommended next milestone only
after authorization.

### Preserved benchmark measurements

Release server-only build, 100,000 iterations per operation, sampled on this
development host (nanoseconds/operation; not an end-to-end network benchmark):

| Sample | Text bytes | Binary bytes | Text encode | Binary encode | Text decode | Binary decode |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| INPUT | 13 | 17 | 52.09 | 140.29 | 720.30 | 179.26 |
| Equivalent-field flight state | 143 | 71 | 8648.40 | 403.86 | 6617.93 | 433.19 |

The actual routine text FLIGHT/FUEL pair is 96 bytes; the equivalent-field
143-byte codec sample additionally includes COMBAT STATUS. Ten seconds of
10 Hz input/snapshot traffic gives 10,900 text vs 8,800 binary codec bytes.
Including representative ACTIVE SHIP metadata on every update gives 36,300
vs 35,500 bytes (about 2.2% lower), excluding TLS/IP overhead and unrelated
traffic. INPUT is larger and slower to encode, but faster to parse in this
sample. Flight encoding/decoding is substantially cheaper here; no blanket
CPU or total-network-bandwidth improvement is claimed. Decoder retained bytes
after complete samples are zero; maximum frame is 4110 bytes. Allocation counts
are uninstrumented. Timing varied under concurrent build/test load, so CTest
asserts semantics/bounds/byte relationships rather than timing thresholds.

### Intentional scope limits

Typed INPUT and authoritative flight snapshots are implemented. Lower-volume
commands/responses and ACTIVE SHIP metadata remain bounded text inside binary
envelopes; there is no speculative rewrite of contacts, markets or outfitting.
Connection-scoped duplicate rejection is implemented and tested; durable
cross-reconnection idempotency is not. Allocation counts are not instrumented;
there is no end-to-end latency/production-load claim. Per-environment approved
protocol/build policy is deferred (both currently use the same v1 transport).
M5 client-engine restructuring and all later milestones are not started here.

## Completed M3 recovery record — 2026-10-15/16

This section records the approved M3 boundary. The v0.17 material below is
retained as explicitly superseded historical evidence, not a current milestone
status or canon specification. The controlling scope is the user's v0.20
recovery/resume instruction: finish and checkpoint M3, report, then stop before
further M4 implementation. Batch 14 as a whole is **not complete**.

### Recovery and preservation

- Independently verified branch `batch14/implementation-v0.11`, starting HEAD
  `3764db1e602a1022cbd44ce032076b9c297add36`, five commits ahead of the local
  `origin/batch14/implementation-v0.11` tracking ref at `280c0b0`. No remote
  operation was performed. The initial tracked working tree and index were clean.
- Preserved `e9edfe0` (M3 topology/security), `80454d3` (M3 persistence/query),
  `5ed7772` (M4 bounded binary foundation), `b19f08a`, and `3764db1` unchanged.
- `b19f08a` is mixed recovery work: M3's 90-system/133-gate registry, Aurelian
  faction, profiles, resources/economy metadata and migration; plus M4 binary
  negotiation, native client/server integration and TLS coverage.
- `3764db1` adds the M4 protocol benchmark only. M4 remains preserved WIP,
  including the committed binary foundation and later negotiation/typed
  input/flight/replay protection. Its completion has not been claimed here.
- The full-history recovery bundle
  `~/Desktop/HELION_PRE_RESUME_2026-10-15_171006.bundle` was verified again and
  remains untouched. `.agents/` contains a plugin marketplace manifest;
  `plugins/` contains plugin manifests, skills, hooks and utilities. These are
  development-tool infrastructure, not native runtime dependencies. Their exact
  creating process cannot be proven from Git; contents identify their purpose.
  Both remain untracked, unchanged, and excluded from checkpoint staging. No
  `.gitignore` policy change is necessary to complete M3.
- The recovered infrastructure serialization already used
  `STATION,WAREHOUSE,GATEWATCH,EXTRACTION,OUTPOST`; that historical repair was
  retained rather than repeated.

### Canon and implemented M3 scope

Reconciliation uses the user's v0.20 contract and the Desktop references
`HELION_World_Bible_draftcomp7.txt` (SHA-256
`2270345d1c7e566fda1941b06d45472a43a32dfa32607c23d4604f8b045b7c69`)
and `HELION_CANON_CHANGELOG_AI_SYNC_v0.20.txt` (SHA-256
`cf5add4ab8cf6439302a6430ccd0b0cf12396f0ef4e5907cd8934b46a7c0289a`).
The present is 1087 AS, exactly 900 years after the 187 AS Severance. IGG is a
non-sovereign treaty/service framework with coequal INSA, Gatewatch and GDF
arms. GSC, UniNet, Crownspire, Ironstar Forge, Verdance, Riftspace and ASA /
Adaptive Technology are current terminology. Broader old UI/lore terminology
outside M3 is deferred; compatibility-sensitive IDs are retained.

| Authoritative Known Space allocation | Systems |
| --- | ---: |
| Helion Commonwealth | 27 |
| Solar Directorate | 25 |
| Free Systems Compact | 25 |
| Meridian League | 23 |
| Aurelian Synod | 20 |
| Faction-controlled subtotal | 120 |
| Zero Space | 80 |
| Total | 200 |

The existing `native/shared/galaxy.*` authority is extended in place. All 90
recovered system IDs and 133 gate edges remain. The registry now contains 200
systems, 16 regions and 265 undirected permanent gates, including six protected
inter-faction trunks. Every system and each faction territory is connected.
Remote pockets are isolated by chokepoints, not accidentally unreachable.
Crownspire is the Commonwealth capital display name; `CONCORDIA` remains its
stable save/topology ID.

Six new Zero Space regions add 52 systems: Lantern Corridor (9), Ashen Basin
(9), Night Pockets (9), Blackwater Choke (9), Far Wilds (8), Cinder Cluster (8).
Lantern/Blackwater form a dangerous through-route to the Directorate border;
Ashen is a resource ring with one entrance; Night has two small pockets and a
blind spur; Far Wilds branches into remote dead ends; Cinder is internally
cyclic behind a single entry. Existing frontier routes and faction backwaters
remain. The initial sovereignty graph has 21 derived borders.

All systems retain population, settlement scale, traffic, celestial counts,
infrastructure, resource families and production/consumption metadata. New
food, industrial, research, military, logistics and extraction systems have
distinct roles. Uninhabited systems have resource potential but no invented
active production. M2's existing station markets remain the live economy;
galaxy-wide extraction, manufacturing and transport simulation is deferred.

Security remains integer `-25..25`, exactly 0.2 per step: positive High,
negative Low, zero Zero Space. All 120 major-faction systems are nonzero;
all 80 Zero Space systems are 0.0. `SpaceDomain` is a separate field; Riftspace,
Unknown Space and Deep Unknown are outside this registry. The internal
`nullSecurity` alias and `setNullSovereignty` name remain compatibility seams.
Bounded server-side security/sovereignty mutations and crime/response query
policies remain; full GDF/INSA/Gatewatch fleet AI is deferred. System sovereignty
does not derive from station/market ownership, with a Kepler/Orion regression
test proving that distinction.

Per-commander knowledge is persistent and separate from physical state.
`UNDETECTED -> DETECTED -> CLASSIFIED -> LOCATED -> SURVEYED` advances one stage
at a time through a server-side helper. Early stages expose contact handles,
not descriptive IDs; full profiles require SURVEYED. GALAXY uses the
authenticated profile and omits hidden systems and gates/borders with unknown
endpoints. Territory summaries are also filtered if their capital or commerce
hub is unknown to that commander. Existing `systems`, `gates`, `borders` query keys remain, counting
visible rows; new explicit total/knowledge fields distinguish the public
200-system total. `class=ZERO_SPACE` replaces the former WIP `class=NULL`
metadata label; TLS framing and binary negotiation versions are unchanged.
Legacy public charting seeds initial knowledge only. Gameplay discovery
acquisition, physical anomaly instances, survey trading and Commander/API
transport remain deferred.

Persistence keeps the existing atomic, owner-only, environment-bound store.
Exact historical 28- and 90-ID sets expand to 200 deterministically; old
security, sovereignty, location, credentials, ships, modules, balances and
telemetry survive. New metadata comes from canonical seeds, never invented
historical counters. A `V\tGALAXY_KNOWLEDGE\t1` marker distinguishes complete
current saves from legacy saves; each commander has exactly 200 `D` records.
The final `Z\tGALAXY_END` record checks the system, commander and total record
counts. Truncation or omitted ship/economy records cannot silently invoke a
legacy reconstruction path. This is structural integrity checking, not a
cryptographic signature against an operator who deliberately rewrites a save.
Unknown versions, duplicates, malformed/trailing fields, partial galaxies,
partial/missing knowledge blocks and environment mismatches fail before
replacing the save. Legacy knowledge is seeded after all physical rows load,
independent of row order. Commander serialization is sorted for repeatable
snapshots. Older binaries cannot load the new additive records; restore the
pre-upgrade backup when deliberately rolling back a server binary.

The server integration fixture now waits for two authoritative `COMBAT STATUS`
ready samples instead of assuming a fixed wall-clock sleep expires cooldown.
A Release rerun exposed the old timing assumption. The initial attempted wait
incorrectly inspected `FLIGHT` data, which does not contain weapon cooldown;
the final helper parses the actual combat-status extension from a side-effect-free
rejected target request (ordinary FLIGHT responses do not always include it).
Damage, cooldown, reward and persistence-rollback assertions remain unchanged.

### Current milestone status and validation

| Milestone | Current state |
| --- | --- |
| M0–M2 | Existing implemented foundations preserved and regression-tested |
| M3 | Complete: implemented and validated at the v0.20 200-system foundation scope |
| M4 | Preserved partial implementation/WIP; no new protocol expansion |
| M5–M8 | Not started in this recovery continuation |
| M9 | Batch-wide completion remains pending later milestones |

Final validation after the persistence, visibility and timing repairs:

| Configuration / gate | Result |
| --- | --- |
| Combined Debug, warnings as errors, `/tmp/helion-b14-debug` | Build passed; full CTest **26/26**, 182.94 s |
| Server Debug ASan/UBSan, warnings as errors, `/tmp/helion-b14-sanitize` | Build passed; full CTest **16/16**, 66.97 s; no sanitizer/leak diagnostics |
| Server-only Release, warnings as errors, `/tmp/helion-b14-server` | Build passed; full CTest **16/16**, 55.09 s |
| Client-only Release, warnings as errors, `/tmp/helion-b14-client` | Build passed; full CTest **18/18**, 13.71 s |
| Release live TLS repeat (`--repeat until-fail:2`) | Both additional runs passed, 78.08 s total |
| Combined, server-only and client-only packages | All six TGZ/DEB artifacts generated successfully |
| Whitespace / patch integrity | `git diff --check` passed |

Commands were `cmake --build <build-dir> --parallel 3`,
`ctest --test-dir <build-dir> --output-on-failure`, and
`cpack --config <build-dir>/CPackConfig.cmake -B <build-dir>/packages`.
The full Debug suite includes galaxy, economy, security, environment/data-lock,
text and binary protocol compatibility, live TLS/server integration, the new
28/90-to-200 persistence/migration/isolation/corruption gate, installed-launcher
smoke, core gameplay, graphical acceptance, core render states and renderer
fallback. The sanitizer build includes the actual server integration and live
migration processes, with AddressSanitizer/UndefinedBehaviorSanitizer enabled
and no environment override disabling leak detection. CTest logs are under
each build's `Testing/Temporary/` directory; packages are under `packages/`.

Earlier restricted invocations failed because sockets/X11 were unavailable or
LeakSanitizer encountered ptrace confinement. Unrestricted reruns passed.
Those failures were not concurrency capacity failures, despite an initial
progress-message hypothesis. Later Release and intermediate test-helper
failures are explained above and were repaired before these final green runs.
There are no unexplained final gate failures.

The Codex Security diff workflow completed scan
`67e8e348-aa1d-4465-a4f5-b6793159eddc` with no reportable findings on its selected
earlier patch. It records a changed-working-tree warning and is not evidence
for subsequent edits. Final direct source review covered `saveStateLocked`,
`loadState`, the authenticated GALAXY handler, `validateGalaxy`, knowledge
validation/progression and `authorizedSystemLine`. That review closed missing
record detection and hidden-capital disclosure gaps. The live malformed-save
and viewer-isolation tests exercise those boundaries, including source-file
preservation on failure. No outstanding M3 security issue was identified by
that review; this is not a claim of exhaustive independent assurance. Tool
infrastructure was explicitly excluded. No independent delegated review ran.

### Checkpoint boundary and explicit deferrals

The local M3 checkpoint extends the five preserved commits; it does not rewrite
history or publish remotely. Only M3 source, tests and this execution record
belong in that checkpoint. `.agents/` and `plugins/` remain outside it.

Advanced enforcement fleets, discovery acquisition gameplay/physical anomaly
instances, survey trading, galaxy-wide production/logistics simulation,
Commander/player APIs and TEST administration remain deferred. The new
knowledge persistence and filtered read model are implemented, not the entire
future discovery gameplay loop. Compatibility-sensitive IDs remain; broader
legacy UI/lore renaming is outside this M3 patch. The next milestone is M4,
beginning with review of the preserved binary negotiation/typed-traffic and
benchmark foundation. Stop for the user's M3 review before expanding it.

## Historical v0.17 record (superseded)

The following was the live implementation record at v0.17. The repository-local
`HELION_Batch14_Prompt_v0.11.txt` and `HELION_Codex_Goal_v0.11.txt` remain the
baseline contract. The user-supplied
`HELION_Batch14_Prompt_v0.17_integrated.txt` (SHA-256
`caf73e1302604ac039a652cc08baac392dc736bd7955157ff7c9d3a5f183e7f4`), read
from `/home/bannaar/Downloads` on 2026-09-25, is the controlling continuation.
It preserves completed work, incorporates the approved v0.12-v0.16
clarifications, and adds the v0.17 self-hosting/player-API direction. This is a
continuation from the actual repository state, not a restart.

## Baseline

- Branch: `batch14/implementation-v0.11`
- Required and observed starting commit: `7d9aa976348c67ce948be80740088c10606714f8`
- Current committed checkpoint at the v0.17 audit: `5ed777237e3a7700a72fbc82170e8179a471a950`
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

### v0.17 continuation audit

- Batch 13 already canonicalizes manufacturer, operator, visual-variant, and
  human/Khepri/Vael technology-family profiles in `shared/ships.*`; owned ships
  persist only stable variant/livery/wear references. Registry and persistence
  tests prove that an illicit/operator treatment does not mutate a hull. The
  compact `docs/native-ships.md` summary predates the full v0.17 art-direction
  wording, so a detailed repository-local canon reference and reusable public
  read-model projection are still required.
- The committed `shared/galaxy.*` graph is the correct single-authority seam,
  but its 28 systems do not meet v0.17's hard 80-system minimum. Existing IDs,
  routes, security, sovereignty, persistence records, station mappings, and
  tests must be extended in place. No population, settlement/infrastructure,
  stellar/celestial, resource-distribution, or production/consumption profile
  is present yet.
- No physical anomaly/site registry, per-commander discovery state, survey
  persistence, sensor capability service, or permission-filtered survey read
  model exists. M3G must use the galaxy and existing atomic environment-bound
  save rather than a second universe store.
- No HTTP server, public API endpoint, OAuth/token service, companion backend,
  or admin network service exists. Current native TLS/game services expose
  server-owned read responses only through the gameplay connection. A future
  API transport must consume shared read models and may initially bind only to
  a private interface; it must not serialize complete hidden world objects.
- The server is already self-hostable without external SaaS: bind address,
  port, database path, environment, TLS certificate/key, and connection limit
  are runtime inputs; systemd, LAN/Tailscale, TLS, package, backup, and restore
  guidance exists. There are no Cloudflare, Sentry, AWS, OVHcloud, or other
  provider SDK dependencies. Typed status/health, logging/metrics/error-sink,
  trusted-proxy, rate-limit, maintenance, and private admin-control seams remain
  incomplete.
- The v0.17 audit found no reason to modify the legacy browser tree or create a
  browser game/API authority. Native C++ server persistence and validation
  remain authoritative.

## Smallest coherent continuation order

The v0.17 audit found M0-M2 stable, the original 28-system M3A-F foundation
committed and validated, M4A committed, and M4B-D already implemented but
uncommitted in the working tree. The smallest safe continuation is therefore
to stabilize and commit the in-flight protocol work before expanding the
already-correct graph model. The 28-system graph is retained as architecture
and seed content, but it is no longer considered the complete M3 deliverable
because v0.17 establishes a hard minimum of 80 systems and adds system profiles
and M3G discovery state.

| Milestone | Scope | Status |
| --- | --- | --- |
| 0 | Audit baseline, record seams/deferrals, run relevant baseline validation | Complete |
| 1 | Extend the existing module registry with structured physical slot/size metadata; centralize compatibility; enforce it in existing server-authoritative fit paths; preserve saves/UI/effects | Complete |
| 2 | Add the regional seeded-market model, production-like versus TEST pricing policy, transaction safety, persistence, and economic telemetry for implemented flows | Complete |
| 3 | Preserve the authoritative graph/security/sovereignty foundation, expand it to the v0.17 galaxy scale and system-profile floor, integrate M2 geography, then add M3G physical-anomaly/per-commander-knowledge persistence and filtered read models | Partial: original 28-system M3A-F complete; v0.17 expansion not started |
| 4 | Add explicit bounded/versioned binary framing over TLS for measured high-frequency traffic, compatibility negotiation, decoder safety tests, and benchmarks | In progress: M4A committed; M4B-D working tree passes focused validation |
| 5 | Introduce a typed client engine-state controller and incrementally route connection/auth/navigation/reconnect without granting client authority | Not started |
| 6 | Complete TEST roles, typed admin operations/audit, and TEST eligibility bypass on top of the isolated pricing policy, with production isolation tests | Not started |
| 7 | Establish the shared Companion/Public API read-model boundary, representative versioned read surfaces, server-side knowledge filtering, bounded/scoped authorization seams, and public/admin trust separation without duplicate state | Not started |
| 8 | Add vendor-neutral self-hosting and production-operations foundations: typed runtime configuration, status/lifecycle, least-privilege authorization boundaries, structured logs/metrics/error sinks, trusted-proxy policy, backup/recovery, and release documentation | Not started |
| 9 | Full migration/regression/security review, all build/package/sanitizer gates, v0.17 completion report, and Batch 15 recommendations | Not started |

The order keeps persistence and authority in the existing server, finishes the
already-open protocol transaction before touching geography, extends rather
than replaces the graph and market contracts, establishes permission-filtered
read models before an API transport, and delays client control-flow changes
until authoritative responses are stable.

## v0.17 integrated canonical clarifications

This section records the user-approved v0.12-v0.17 changes for the next World
Bible revision. They supersede conflicting older wording for Batch 14
implementation without silently editing World Bible v0.10.

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
- The first seeded galaxy must contain at least 80 authoritative star systems,
  targeting approximately 90. Systems require stable non-positional IDs and
  meaningful population, settlement/infrastructure, celestial/resource, and
  production/consumption/import/export profiles. Resource distribution must be
  geographically uneven. Future/inactive resource metadata must not be
  represented as playable extraction or industry.
- Server-owned physical anomalies/sites/resources are distinct from persistent
  per-commander knowledge. Knowledge advances only through validated server
  transitions `DETECTED -> CLASSIFIED -> LOCATED -> SURVEYED`; read models must
  omit hidden fields rather than asking a client to hide them.
- Manufacturer, hull, operator/faction treatment, illicit modification,
  per-instance livery/wear, and fitted equipment remain separate. Existing
  Batch 13 registry/profile data is preserved; v0.17 requires detailed canon
  documentation and stable read-model hooks, not premature 3D/audio/procedural
  asset systems.
- HELION remains self-hosted and vendor-neutral. Cloudflare, Sentry, AWS,
  OVHcloud, and similar providers are optional future adapters, never Batch 14
  runtime dependencies. Secrets remain runtime-only.
- One shared Companion/Public API boundary must consume authoritative,
  permission-filtered read models. It must not duplicate gameplay state or
  share the production ADMIN/OPS trust domain. The initial policy is
  read-first/read-mostly; high-risk unattended gameplay automation remains
  deferred.

M3 executes as the following coherent submilestones:

| Submilestone | Scope | Status |
| --- | --- | --- |
| M3A | Shared topology/system/region/Gate data model and graph validation | Complete at original 28-system floor; model retained |
| M3B | NPC faction territories, capitals, commerce hubs, roles, and derived borders | Complete at original 28-system floor; content expansion required |
| M3C | Exact High/Low/Null rating model, ownership invariants, and response hooks | Complete |
| M3D | Protected trunks plus core, secondary, frontier, and backwater routes | Complete at original 28-system floor; content expansion required |
| M3E | Persisted unclaimed/Guild/Alliance nullsec sovereignty foundation | Complete |
| M3F | Future player travel-infrastructure eligibility hooks and shared query seam | Complete |
| M3A2 | Expand the existing graph to at least 80 systems (target approximately 90) without changing stable existing IDs | Not started |
| M3B2 | Add population, settlement/infrastructure, celestial/resource, economic identity, and M2 integration data for every seeded system | Not started |
| M3G | Add physical anomaly instances, per-commander discovery, filtered survey read models, persistence/migration, and event/companion seams | Not started |

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
