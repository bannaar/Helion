# HELION canon and provenance

[World Bible DraftComp8](HELION_World_Bible_draftcomp8.txt) is the current working
canon master. [Canon synchronization v0.21](HELION_CANON_CHANGELOG_AI_SYNC_v0.21.txt)
provides the current checksum and terminology rules. The
[canon/document reconciliation](../development/HELION_CANON_AND_DOC_RECONCILIATION_v0.21.md)
records the October 2026 audit, retained identifiers and unresolved questions.

World Bible controls lore; actual source/tests and
[BATCH14_EXECUTION.md](../development/BATCH14_EXECUTION.md) control implementation
truth. Active engineering instructions control authorized work. Richer canon is
not evidence that gameplay exists. Batch 14 M0–M7 are complete; M8 and M9 are not
started; Batch 14 is not complete; Batch 15 is not started.

## Current canon checksum

The Severance occurred in **187 AS**. The present/game-start era is **1087 AS**,
exactly **900 years after the Severance**. Known Space contains exactly **200
persistent systems**, comprising **120 faction-controlled** and **80 Zero Space**:

| Allocation | Systems |
| --- | ---: |
| Helion Commonwealth | 27 |
| Solar Directorate | 25 |
| Free Systems Compact | 25 |
| Meridian League | 23 |
| Aurelian Synod | 20 |
| Zero Space | 80 |

Zero Space is the persistent 0.0 Known Space frontier, distinct from The Null and
Riftspace. Riftspace lies outside the count; approximately 48 initial systems is
future direction only. Unknown Space may remain a broader scientific/mystery term.
Survey knowledge progresses `DETECTED -> CLASSIFIED -> LOCATED -> SURVEYED`;
technical `UNDETECTED` is an initial condition, not an earned stage.

| Domain | Current canon |
| --- | --- |
| Treaty/service framework | Inter-Galactic Governance (IGG), non-sovereign, not a sixth faction or owner of territory |
| Coequal IGG arms | INSA, Gatewatch, Galactic Defense Force (GDF) |
| Retained charter and phrase | Crossroads Charter; “INSA knows. Gatewatch understands. The GDF acts.” |
| Information network | UniNet |
| Currency | Galactic Standard Credit (GSC) |
| Commonwealth capital | Crownspire |
| Heavy military/industrial manufacturer | Ironstar Forge |
| Lost agricultural colony | Verdance |
| Advanced/hybrid discipline | Adaptive Systems Architecture (ASA); Adaptive Technology colloquially |

Humans do not have routine clone immortality or consequence-free resurrection.
Protected mysteries remain unresolved. Veyra, the AsterTech marque question and
the final HELION-native starter hull name remain deliberate canon review items.

## Implementation and future design

The authoritative game is a native C++ persistent server and native SDL2/OpenGL
client. `src/` is legacy browser reference, not another production runtime or
persistent universe. The server owns authoritative gameplay, environment and
roles; clients and optional companions consume allowed data and request actions.

DEVELOPMENT is local/disposable; PRIVATE TEST is isolated, noncanonical and
resettable; PRODUCTION is canonical persistent state. TEST DEV_ADMIN authority
never implies Production mutation authority; invited testers are not automatically
administrators. See [TEST administration](../private-test-admin.md) and
[Production operations](../production-operations.md) for implemented boundaries.

The future [AI Core design](../design/SHIP_AI_CORES.md),
[roadmap](../development/HELION_ROADMAP.md) and
[alpha/launcher plan](../development/ALPHA_READINESS.md) are direction only.
Neither AI Cores nor the full player launcher is implemented. The existing
local-play helper remains a DEVELOPMENT server/client startup tool.

Use current canon names in prose. Preserve compatibility-sensitive wire/save IDs,
migration fixtures and stable identifiers until separately authorized migrations.
For example, `credits`, `GALNET`, `CONCORDIA`, `TITAN_FORGE`, `TITAN_MULE` and
`SIDEWINDER` do not define current lore wording. Existing executable display
terminology debt is documented, not silently renamed in this checkpoint.

## Historical sources

[DraftComp7](history/HELION_World_Bible_draftcomp7.txt) and
[canon sync v0.20](history/HELION_CANON_CHANGELOG_AI_SYNC_v0.20.txt) are preserved
historical sources, including the full living-universe expansion. Only DraftComp7’s
final empty line is normalized in the repository copy; the Desktop original is
unchanged. Source/copy hashes and status are in the [history index](history/README.md).

[Historical World Bible v0.10 Markdown](HELION_World_Bible_v0.10.md),
[v0.10 DOCX](HELION_World_Bible_v0.10.docx) and
[v0.2 text](world-bible-v0.2.txt) remain unchanged historical/provenance material.
Old institution, network, manufacturer, chronology and currency wording in these
files is superseded. Their own historical “current” statements do not override
DraftComp8 or the v0.21 synchronization rules. Historical prompts/handoffs likewise
remain evidence of earlier work rather than current development authority.
