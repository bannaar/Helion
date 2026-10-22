# HELION

HELION is an actively developed **native, server-authoritative, persistent online
science-fiction game**. Its authoritative architecture is a **C++17 server** and
**native SDL2/OpenGL client**. The browser application under `src/` is retained as
legacy/reference material, not the authoritative Production game runtime.

**Development status — 22 October 2026:** `main` is the previous accepted code
baseline. Active Batch 14 development lives on
[**batch14/implementation-v0.11**](https://github.com/bannaar/Helion/tree/batch14/implementation-v0.11),
where **M0–M7 are complete**. **M8 and M9 are not started; Batch 14 is not complete.**
The development reference is checkpoint
[`5225592`](https://github.com/bannaar/Helion/commit/52255923de2ad26edeb74018a8acce656383b92f).
This public documentation refresh does not bring Batch 14 implementation into main.
See [current development](docs/CURRENT_DEVELOPMENT.md) for branch-specific references.

## Playable alpha-development slice

HELION is a playable vertical slice in active alpha development, with substantial
work still ahead. The branches contain different stages of that slice:

| Branch | Implemented scope at this documentation checkpoint |
| --- | --- |
| `main` | Native account creation/login, flight, launch/docking, mining, station trading/markets, First Ore mission and upgrades, persistent commander/flight/cargo state, chat and live player/NPC contacts; mandatory TLS and scoped companion credentials |
| Active Batch 14 development | Extends that foundation with the Kepler career arc, persistent owned ships/shipyards, basic combat/recovery, reputation and UniNet event foundations, plus the galaxy/protocol/environment/operations foundations below |

Deep combat, full outfitting, industry, large-scale faction warfare, player
sovereignty, full dynamic-universe simulation and Riftspace gameplay remain future
work. Neither AI Cores nor a full player launcher or finished companion ecosystem
is implemented. A galaxy data foundation does not imply complete multi-system gameplay.

## Architecture and Batch 14 foundations

The standalone native server owns authoritative gameplay and persistent world
state. Clients request actions and render permitted data; client presentation
never grants movement, assets, rewards, roles or administrative authority.
The Linux client retains an OpenGL 2.1 compatibility target; the development
branch also has its separate OpenGL 3.3 core renderer.

**The active development branch, not main, contains the completed M0–M7 work:**

- mandatory TLS 1.2+, negotiated bounded binary protocol v1, explicit legacy
  protocol compatibility, and companion authentication/scope boundaries;
- persistent commander/world state and an authoritative **200-system Known Space**
  foundation: **120 faction-controlled systems and 80 Zero Space systems**;
- DEVELOPMENT / PRIVATE TEST / PRODUCTION separation and TEST-only administration;
- Production service lifecycle, fail-closed startup, health/readiness, lightweight
  metrics, maintenance preparation, bounded draining and graceful durable shutdown;
- Production backup/integrity verification and non-destructive restore-planning
  foundations, plus packaged combined/server/client builds.

These are development foundations, not a completed Batch 14 release or a claim
that a public Production service is available. M7 supplies no destructive
Production restore command. Detailed evidence and limitations are linked from
[current development](docs/CURRENT_DEVELOPMENT.md).

## Production, PRIVATE TEST and alpha direction

Batch 14 establishes server-controlled environments: **DEVELOPMENT** for local and
diagnostic work, **PRIVATE TEST** for isolated, noncanonical, resettable developer
and invited-tester sessions, and **PRODUCTION** for the canonical persistent universe.
TEST administrator privileges do not grant Production gameplay mutation authority;
invited testers are not automatically developers or administrators.

Planned testing progression is internal testing → trusted PRIVATE TEST closed
alpha → small persistent Production alpha → expanded closed alpha → later wider
testing. This is a readiness plan, not an open-alpha invitation or announcement.

## Current canon

The current working canon master is
[**HELION World Bible DraftComp8**](https://github.com/bannaar/Helion/blob/52255923de2ad26edeb74018a8acce656383b92f/docs/lore/HELION_World_Bible_draftcomp8.txt),
with [**canon synchronization/checksum v0.21**](https://github.com/bannaar/Helion/blob/52255923de2ad26edeb74018a8acce656383b92f/docs/lore/HELION_CANON_CHANGELOG_AI_SYNC_v0.21.txt).
These documents live on the development branch; they are linked here, not copied
into this older main baseline. Lore describes the setting; source and tests
establish what is implemented.

- The Severance occurred in **187 AS**. The current/game-start era is **1087 AS**,
  exactly **900 years after the Severance**.
- Known Space comprises **200 persistent systems**: **120 faction-controlled** and
  **80 Zero Space**. Zero Space is the 0.0 frontier, distinct from **The Null**.
  **Riftspace** lies outside that count and remains future gameplay direction.
- **Inter-Galactic Governance (IGG)** is an independent, non-sovereign treaty/service
  framework. **UniNet** is the information network; **Galactic Standard Credit
  (GSC)** is the currency. **Crownspire**, **Ironstar Forge** and **Verdance** are
  current canon names.

## Installation and development links

Use documentation matching the branch you build:

| Previous accepted `main` baseline | Current Batch 14 development |
| --- | --- |
| [Native installation](docs/native-install.md), [complete installation guide](docs/installation.md) | [Development installation](https://github.com/bannaar/Helion/blob/batch14/implementation-v0.11/docs/native-install.md) |
| [Server guide](docs/native-server.md), [client guide](docs/native-client.md), [native quick start](native/README.md) | [Production operations](https://github.com/bannaar/Helion/blob/batch14/implementation-v0.11/docs/production-operations.md), [PRIVATE TEST administration](https://github.com/bannaar/Helion/blob/batch14/implementation-v0.11/docs/private-test-admin.md) |
| [Baseline project state](docs/PROJECT_STATE.md), [companion credentials](docs/companion-auth.md) | [Development project state](https://github.com/bannaar/Helion/blob/batch14/implementation-v0.11/docs/PROJECT_STATE.md), [canon index](https://github.com/bannaar/Helion/blob/batch14/implementation-v0.11/docs/lore/README.md) |

Baseline guides may retain historical terminology and earlier setup instructions;
they do not define current canon or imply that newer development features exist
on main. Do not apply Batch 14-only flags or save assumptions to the older binary.

## Launcher and future roadmap

The existing **local-play launcher/helper**, `helion-play`, prepares local TLS,
starts a local native server and client, and stops the server when the client
closes. It is distinct from the **future full player launcher**, which may add
account sign-in, authorized Production/PRIVATE TEST presentation, server status,
maintenance messages, version checks, updates, repair/verify, release channels,
diagnostics, news/status and launch configuration. That full launcher is not implemented.

| Stage | Status / direction |
| --- | --- |
| Batch 14 M0–M7 | Complete on the development branch |
| Batch 14 M8 | Not started: Deployment, Packaging, Migration, Release and Recovery |
| Batch 14 M9 | Not started: Final Hardening, Security Review, Documentation Reconciliation and Completion |
| Alpha Readiness / full launcher bridge | Future planning after Batch 14 acceptance |
| Batch 15 | Future ships, systems, combat, outfitting and AI Cores |
| Batch 16 | Provisional equipment economy, manufacturing, salvage and industry |
| Batch 17 | Provisional missions, NPC careers and shared-world activity |
| Batch 18+ | Provisional Riftspace, xenotech and advanced/Sovereign intelligence |

Future entries are direction, not completed features. Batch 15 is not started and
requires completed Batch 14 plus explicit authorization.

## Optional companions and future Ship AI Cores

Potential companion surfaces include HELION Commander, mobile/tablet applications,
cockpit devices, Linux cyberdeck-style interfaces and other cockpit clients.
They are optional and non-authoritative: permitted data and requests go through
the authoritative server. Scoped credential foundations exist; the complete
companion ecosystem and interoperability are not implemented. No device is required to play.

Future Ship AI Cores are intended as physical ship equipment with deterministic
native automation, validated by the authoritative server and usable without LLMs.
Optional asynchronous cognitive providers and companion interfaces may extend them;
persistent identity/personality/memory are possible future capabilities. AI Cores
are not implemented and require no particular provider/vendor. See the
[development overview](docs/CURRENT_DEVELOPMENT.md) for design and planning links.

## Historical browser reference and contribution

The Three.js/TanStack application under `src/`, its browser migrations and preview
helpers remain available for legacy/reference work. Their state is not a second
canonical universe or a peer Production backend. Browser feature coverage is not
evidence of native implementation.

For native contributions, consult the active development branch and its current
instructions, preserve server authority, TLS, compatibility and persistent state,
and validate changes against the relevant branch. This README refresh changes
public documentation only; it does not merge implementation or advance a milestone.
