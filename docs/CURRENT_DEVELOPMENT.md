# HELION current development

Status snapshot: **22 October 2026**, using development checkpoint
[`52255923de2ad26edeb74018a8acce656383b92f`](https://github.com/bannaar/Helion/commit/52255923de2ad26edeb74018a8acce656383b92f).
Return to the [public README](../README.md).

## Branches and implementation truth

`main` is the previous accepted code baseline, based on
[`23cf90b`](https://github.com/bannaar/Helion/commit/23cf90b0b7bdd0996c513f610fab21dc9a4af2e0).
This public-docs refresh adds no Batch 14 implementation. Main contains the earlier
native mining/trading/First Ore slice, persistent flight/commander data, live
contacts, mandatory TLS and scoped companion authentication.

Active engineering takes place on
[**batch14/implementation-v0.11**](https://github.com/bannaar/Helion/tree/batch14/implementation-v0.11).
**M0–M7 are complete there. M8 and M9 are not started. Batch 14 is not complete;
Batch 15 is not started.** Do not read the development branch's feature list as a
claim about main's binary. Development branch links may advance; the commit above
pins the evidence used for this status snapshot.

The authoritative architecture is a standalone native C++17 persistent server and
native SDL2/OpenGL client. The server owns gameplay, persistent world state,
environment and authorization. Clients request permitted actions. `src/` remains
legacy browser reference, not another Production runtime or persistent universe.

## Development branch through M7

The development branch extends the earlier slice with the Kepler career arc,
owned ships/shipyards, basic combat/recovery, reputation/UniNet foundations and
persistent gameplay. Its galaxy foundation has exactly 200 Known Space systems:
120 faction-controlled and 80 Zero Space. Data-model coverage does not mean every
system has complete travel, missions or simulated activity.

It also contains negotiated bounded binary protocol v1 over mandatory TLS 1.2+,
explicit legacy compatibility, companion scope boundaries, isolated environments
and TEST-only administrative controls. M7 established service lifecycle,
fail-closed Production startup, health/readiness, structured logs, lightweight
metrics, maintenance warning, bounded draining, durable shutdown checkpoints and
verified Production backups. Production restore is verification/planning only;
TEST snapshots are a separate mechanism. Combined/server/client packages exist,
while M8 release/migration/recovery rehearsal remains unfinished.

| Development reference | Location on active branch |
| --- | --- |
| Implementation status | [PROJECT_STATE.md](https://github.com/bannaar/Helion/blob/batch14/implementation-v0.11/docs/PROJECT_STATE.md) |
| Actual milestone validation and limitations | [BATCH14_EXECUTION.md](https://github.com/bannaar/Helion/blob/batch14/implementation-v0.11/docs/development/BATCH14_EXECUTION.md) |
| Protocol contract | [BATCH14_PROTOCOL.md](https://github.com/bannaar/Helion/blob/batch14/implementation-v0.11/docs/development/BATCH14_PROTOCOL.md) |
| Production lifecycle/backup foundation | [production-operations.md](https://github.com/bannaar/Helion/blob/batch14/implementation-v0.11/docs/production-operations.md) |
| Isolated TEST admission/admin/recovery | [private-test-admin.md](https://github.com/bannaar/Helion/blob/batch14/implementation-v0.11/docs/private-test-admin.md) |
| Native installation | [native-install.md](https://github.com/bannaar/Helion/blob/batch14/implementation-v0.11/docs/native-install.md) |

## Canon authority

Current canon is **HELION World Bible DraftComp8**, with synchronization/checksum
**v0.21**. The full Bible stays on the development branch rather than being copied
into main. These links pin the approved canon/docs checkpoint:

- [World Bible DraftComp8](https://github.com/bannaar/Helion/blob/52255923de2ad26edeb74018a8acce656383b92f/docs/lore/HELION_World_Bible_draftcomp8.txt)
- [Canon synchronization v0.21](https://github.com/bannaar/Helion/blob/52255923de2ad26edeb74018a8acce656383b92f/docs/lore/HELION_CANON_CHANGELOG_AI_SYNC_v0.21.txt)
- [Canon/document reconciliation v0.21](https://github.com/bannaar/Helion/blob/52255923de2ad26edeb74018a8acce656383b92f/docs/development/HELION_CANON_AND_DOC_RECONCILIATION_v0.21.md)

Core checksum: Severance 187 AS; present 1087 AS, exactly 900 years after the
Severance; 200 Known Space systems, comprising 120 faction-controlled and 80 Zero
Space. Zero Space is the 0.0 frontier, distinct from The Null. Riftspace is outside
that count. IGG (Inter-Galactic Governance) is non-sovereign; UniNet, Galactic
Standard Credit (GSC), Crownspire, Ironstar Forge and Verdance are current names.

World Bible controls lore. Source/tests and the execution record establish
implemented behavior; active engineering instructions control authorized work.
Historical canon, old guides and compatibility identifiers do not override newer
approved direction. Canon content is not proof of working gameplay.

## Launcher, alpha and future direction

The existing `helion-play` helper starts a local native server/client with local
TLS; it is not the future full player launcher. That planned launcher may add
account sign-in, authorized environment presentation, status/maintenance messages,
version checks, STABLE/TEST channels, updates, repair/verify, diagnostics,
news/status and launch configuration. Native account login already exists;
the full launcher experience does not.

The DEVELOPMENT / PRIVATE TEST / PRODUCTION architecture is on Batch 14.
DEVELOPMENT is disposable/diagnostic; PRIVATE TEST is isolated, noncanonical and
resettable; PRODUCTION is canonical persistent state. TEST developer privileges
never imply Production gameplay mutation authority. Planned alpha progression is
internal testing → trusted PRIVATE TEST closed alpha → small persistent Production
alpha → expanded closed alpha → later wider testing. This does not announce a
public open alpha or a currently available Production service.

Optional companions may include HELION Commander, mobile/tablet apps, cockpit
hardware and Linux cyberdeck-style clients. They consume permitted authoritative
data and submit permitted requests, host no independent game backend, inherit no
Production administrator authority and receive no hidden truth simply because a
UI could conceal it. The full companion ecosystem is future work.

Future physical Ship AI Core equipment should use deterministic native automation
and typed server-validated intents, work without LLMs, and retain manual fallback.
Optional asynchronous cognitive providers and optional companion interfaces may
extend it. Persistent identity/personality/memory remain future possibilities;
AI Cores are not implemented, and the design requires no particular provider/vendor.

M8 remains Deployment, Packaging, Migration, Release and Recovery; M9 remains
final hardening/security/documentation and completion acceptance. After Batch 14,
planning points toward the alpha/full-launcher bridge, Batch 15 ships/systems/
combat/outfitting/AI Cores, Batch 16 equipment economy/manufacturing/salvage/industry,
Batch 17 missions/NPC careers/shared-world activity, and Batch 18+ Riftspace/
xenotech/advanced or Sovereign intelligence. These are directional, not completion
claims or authorization to begin them.

Further planning on the active development branch:

- [Roadmap](https://github.com/bannaar/Helion/blob/batch14/implementation-v0.11/docs/development/HELION_ROADMAP.md)
- [Alpha readiness / launcher bridge](https://github.com/bannaar/Helion/blob/batch14/implementation-v0.11/docs/development/ALPHA_READINESS.md)
- [Future Ship AI Core design](https://github.com/bannaar/Helion/blob/batch14/implementation-v0.11/docs/design/SHIP_AI_CORES.md)
