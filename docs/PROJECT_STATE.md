# Helion project state

Status: 2026-10-22, after M7 and before M8. **M0–M7 complete; M8 not started;
M9 not started; Batch 14 not complete; Batch 15 not started.** The current
implementation checkpoint is `f02d9e38c32f22678664e55946bef3b700ac5d3c`.
This canon/docs reconciliation is not an engineering milestone or final acceptance.

Current lore authority is [DraftComp8](lore/HELION_World_Bible_draftcomp8.txt) and
[canon sync v0.21](lore/HELION_CANON_CHANGELOG_AI_SYNC_v0.21.txt). Lore describes
the setting and approved future direction; source/tests and the
[execution record](development/BATCH14_EXECUTION.md) control implementation truth.
The present is 1087 AS, exactly 900 years after the 187 AS Severance. The M3
Known Space foundation contains exactly 200 persistent systems: 120
faction-controlled and 80 Zero Space (Commonwealth 27, Directorate 25, Compact 25,
Meridian 23, Synod 20). Riftspace content remains outside this count and future.
IGG is non-sovereign; UniNet, GSC, Crownspire, Ironstar Forge and Verdance are
current canon terminology. See the
[reconciliation](development/HELION_CANON_AND_DOC_RECONCILIATION_v0.21.md) for
retained technical IDs and current executable display terminology debt.

Helion's authoritative implementation and release path is the native C++17
SDL2/OpenGL client plus POSIX TLS server under `native/`. The browser prototype
remains in `src/` as legacy reference material and is not a peer authority
path, production universe, or part of the native release.

The native client is playable today. A commander can create an account or log
in through bounded graphical fields, launch from Kepler, fly with W/S/A/D
or arrows, mine ore from asteroid fields, return to station, dock, and receive
saved GSC and experience. The sector contains a second station, moving NPC
hauler traffic, other logged-in commanders as live contacts, a radar, a
rotating station, a procedural starter shuttle (`SIDEWINDER` compatibility ID), faceted ore
asteroids, engine exhaust, mining beam, and cockpit font. Contact positions are server-owned and
clients request a refreshed contact stream once per second. A client can use
`/contacts` to inspect the stream.

Trading is server validated while docked. `/buy food 1` and `/buy parts 1`
purchase supplies against station prices and hold capacity; `/sell food 1` and
`/sell parts 1` sell inventory. Station prices differ between Kepler and
Cinder. The graphical market supports quantity, buy, and sell controls;
the diagnostic console remains available. `F2` opens the profile view and
`F3` opens options. Authentication, chat,
profile queries, flight, contacts, mining, docking, and trading require login.

Progression is now playable: `/mission` shows the First Ore contract from the
Orion Extraction Group under Kepler Authority jurisdiction,
`/accept` starts it, and `/turnin` pays the reward after returning with ore.
`/upgrade engine` and `/upgrade hull` spend GSC at the station and advance
levels; engine levels increase authoritative acceleration and hull levels
increase maximum integrity. Asteroid impacts damage hulls based on impact
speed, disable ships at zero integrity, recover them at their station, and
require the docked `/repair` command before launch. Flight position, velocity,
dock state, hull damage, mined ore, and market cargo are written into the
owner-only save after control and economy changes, so a server restart resumes
the commander in flight.
Other clients receive live positions through `/contacts`, and the client
renders commander ships alongside NPC traffic.

Batch 13 adds an implemented eight-hull native registry, manufacturer/operator and
visual profiles, persistent owned ship instances, deterministic migration of
legacy `SIDEWINDER` records, active-ship selection, Small/Medium pad declarations, and
authoritative Kepler/Cinder shipyards. Hull cargo, acceleration, top speed,
handling, durability, fuel, mining compatibility, and price now affect the
native game. The cockpit shipyard browses specs and owned vessels and switches
only between ships physically stored at the current station. Exact definitions,
balance, persistence, and canon reconciliation are in `docs/native-ships.md`.

Transport is mandatory TLS 1.2 or newer. The server requires a certificate and
private key and never emits a plaintext greeting. The client validates trust,
expiration, and DNS/IP Subject Alternative Name before sending commands. Local
play generates a private loopback certificate automatically. The server uses
scrypt password hashes, owner-only save files, a stable save lock, bounded
line decoding, connection limits, timeouts, and atomic persistence.
Persistence files now carry a server-validated `development`, `test`, or
`production` environment identity. A mismatched process refuses the file;
untagged legacy state is accepted and atomically migrated only by the
backward-compatible development environment. This establishes state isolation,
with the M6 private TEST administration boundary and the M7 local Production
service lifecycle/health/backup/verification foundation. M7 now provides fail-closed
Production startup, owner-only local status/readiness and process metrics,
structured operational events, maintenance warning and bounded drain, durable
shutdown checkpoints, immutable verified Production backups, non-destructive
restore planning and portable headless systemd examples. Production backups are
separate from M6 TEST snapshots; no destructive Production restore exists. See
[Production operations](production-operations.md). Deployment/release/migration
automation and final Batch 14 acceptance remain M8/M9 work.

The combined install includes `helion_server`, `helion_client`,
`helion-dev-cert`, `helion-play`, a desktop entry, icon, and documentation.
The local-play launcher/helper creates a private data directory, starts a TLS loopback server,
opens the client, and shuts the server down when the client exits. `--check`
validates the install without opening a window. Separate server-only and
client-only `.deb` and `.tar.gz` artifacts provide a two-machine installation.
This is not the planned full launcher with account flow, authorized environment
selection, update/repair, release channels and news/patch management; see
[alpha readiness](development/ALPHA_READINESS.md).
The server package has no SDL/OpenGL/X11 dependency; the client package has no
server private material. The [two-machine guide](compaq610-server-elitebook-client.md)
covers SAN identities, LAN and Tailscale binding, service setup, and backups.
Split-package installation, TLS rejection, and server restart have been tested
in isolated local environments; the actual Compaq operating system and
two-machine network path remain to be verified on site.

Native automated coverage includes flight/control physics, asteroid collision,
mining cooldown and capacity, docking rewards, market quantity/price/capacity
rules, contact serialization, protocol bounds, password hashing and legacy
migration, save locking, TLS certificate/hostname/IP validation, plaintext
rejection, graceful shutdown, installed launcher startup, saved login, file
permissions, and desktop asset installation.

The native slice also includes a server-authoritative Red Wake encounter,
pulse-laser combat, recovery, salvage rewards, multi-organization reputation,
and deterministic UniNet events. The current mining, trading, progression,
outfitting, combat, reputation, persistence, and contact loops form a
playable native career slice rather than a static client shell. Guilds,
stations, industry, capital construction, alien progression, and the later
World Bible phases remain design work.

The repository now has GitHub Actions for release builds, package creation,
full TLS/installer tests, and an AddressSanitizer/UndefinedBehaviorSanitizer
server build. The legacy OpenGL renderer intentionally remains dependency-light
and uses SDL2 plus procedural assets; GLAD, GLM, ImGui, and stb_ttf are not
required. The client requests OpenGL 3.0 compatibility first and safely falls
back to OpenGL 2.1 compatibility. On the verified target, Mesa 25.2.8 with
Intel HD Graphics 3000 and `crocus` provides accelerated OpenGL 3.3
compatibility and core contexts. The graphics foundation supports explicit
`--renderer auto`, `--renderer legacy`, and `--renderer core` selection. Auto
initializes hardware OpenGL 3.3 core first and recreates a clean legacy
context if initialization fails. Explicit core exits on failure. Core mode
enters the real client loop and renders copied Kepler presentation state with
GLSL 3.30: stations, asteroids, traffic, commanders, Red Wake contacts,
targeting, mining/fire feedback, geometric status indicators, a built-in
bitmap font, and a compact cockpit/HUD. The cockpit presents mission,
reputation, UniNet/recent-message, combat, mining, docking, and recovery
feedback without adding a second event system. Text is intentionally limited
to a bounded Latin bitmap alphabet; textures beyond the font atlas and model
loading remain future work. Batch 10 Intel HD 3000 X11 states use the optimized
glyph atlas path with bounded world/HUD/text geometry. Core mode now
also exposes bounded account/connection, station, market, mission, outfitting,
profile, UniNet, options, and graphics-diagnostic screens. These screens are
renderer-neutral copies of existing client data and queue existing server
commands; they do not introduce new gameplay rules or client-side authority.
Keyboard navigation is available with F1–F9, Up/Down, Enter, and Escape.

The native core client now presents a short replayable Kepler career arc:
First Ore → Kepler Supply → Red Wake Response. Orion Extraction Group issues
First Ore under Kepler Authority; the supply stage teaches Cinder purchasing
and delivery; the response stage uses the existing Red Wake pulse-laser
encounter. Rewards, standings, parts consumption, salvage, UniNet headlines,
onboarding dismissal, and the established-pilot completion marker survive
disconnect and restart through the compatible career extension. After
completion the commander remains free to mine, trade, refit, patrol, recover,
and read UniNet. Batch 12's guarded client acceptance path exercises the whole
career through one real SDL/OpenGL window, TLS server, and persistence file;
it restarts both processes and verifies free play and reward idempotence.
The corrected packaged client passed a 600.015-second Intel HD 3000 X11/core
soak: 35,904 frames, 68 activity cycles, 8.254 ms average and 12.842 ms worst
measured renderer time, no OpenGL/SDL/render errors or disconnects, and
post-startup RSS remaining near 92 MiB. The same packaged-client acceptance
captured thirteen actual career checkpoints. This establishes a tested local
graphical vertical slice; the physical two-machine deployment is still an
on-site acceptance step.

Batch 10 replaces the earlier lit-pixel text expansion with one textured
six-vertex quad per visible glyph from a single 96x48 RGBA atlas. Diagnostics
now report glyphs, text vertices, indices, float components, VBO bytes, atlas
dimensions/bytes, text draw calls, CPU UI/text build time, and render time in
explicit units. On the Intel HD 3000, representative 960x600 states measured
about 0.20–0.38 ms per captured frame, with 248–364 glyphs, 1,488–2,184 text
vertices, 47,616–69,888 text bytes, and one atlas texture. This is a bounded
diagnostic comparison rather than a performance target. Core UI hit regions now
share the render layout and support mouse hover, click, and bounded scrolling.
