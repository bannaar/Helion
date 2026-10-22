# HELION canon and document reconciliation v0.21

Date: 2026-10-22. Documentation/canon checkpoint after M7 and before M8.
No gameplay implementation, runtime string edit, migration, build configuration
or engineering milestone is introduced here.

## Verified baseline and scope

Read-only preflight found branch `batch14/implementation-v0.11`, HEAD and freshly
fetched origin branch at `f02d9e38c32f22678664e55946bef3b700ac5d3c` (M7).
`origin/main` was `23cf90b0b7bdd0996c513f610fab21dc9a4af2e0`, an ancestor (exit 0).
Tracked tree/index were clean; only `.agents/` and `plugins/` were untracked.
No merge, rebase, cherry-pick or revert was active. The post-M7 backup branch
matched M7, and `HELION_POST_M7_2026-10-22_012034.bundle` verified with complete
history. No unexpected work was discarded.

M0–M7 are complete. **M8 not started; M9 not started; Batch 14 not complete;
Batch 15 not started.** This reconciliation is not M9 final acceptance and does
not authorize M8, AI Cores or a full launcher. No push, merge or main-branch
change belongs to this checkpoint.

## Source-of-truth hierarchy

| Order | Source | Authority domain |
| --- | --- | --- |
| 1 | [DraftComp8](../lore/HELION_World_Bible_draftcomp8.txt) | Current working master canon |
| 2 | [Canon sync v0.21](../lore/HELION_CANON_CHANGELOG_AI_SYNC_v0.21.txt) | Current checksum and terminology/change rules |
| 3 | This reconciliation | Document audit, provenance and retained identifiers |
| 4 | Active engineering goal and latest authorized milestone instructions | Intended and authorized engineering work |
| 5 | Actual source/tests and [BATCH14_EXECUTION.md](BATCH14_EXECUTION.md) | Implemented behavior and actual validation evidence |
| 6 | Historical canon, prompts and handoffs | Provenance only |

These are separate authority domains: World Bible controls lore; repository/tests
control what is implemented; engineering instructions control current work.
Canon does not make future gameplay implemented. Old prompts do not override
newer approved direction. Historical milestone tables are read at their dates,
not as today's status.

## Documents and evidence audited

| Material | Disposition |
| --- | --- |
| Root README, AGENTS.md, docs/PROJECT_STATE.md | Current authority/status routing corrected; generic browser instructions retained with native-specific scope |
| docs/lore/README.md | Rewritten as current canon index with checksum, history and implementation distinction |
| v0.10 Markdown/DOCX, world-bible-v0.2.txt | Historical terminology/chronology reviewed; originals unchanged |
| Approved Desktop DraftComp7 and canon sync v0.20 | Located, hashes matched M3's execution evidence; originals preserved under docs/lore/history/ with documented final-line normalization |
| BATCH14_EXECUTION.md | Current M7 evidence and historical M3 reconciliation reviewed; post-M7/pre-M8 checkpoint added without rewriting history |
| BATCH14_PROTOCOL.md | Wire contract retained; GSC versus `credits` distinction and historical M4 scope clarified |
| HELION_Codex_Goal_v0.11.txt, HELION_Batch14_Prompt_v0.11.txt | Historical bodies retained, explicit superseded-authority notices added |
| native-architecture.md, native-server.md, native-client.md, native-ships.md | Current prose reconciled; actual identifiers, balance and protocol examples retained |
| private-test-admin.md, production-operations.md, companion-auth.md | M6/M7 isolation and narrow operations/token scopes checked; no new privileges implied |
| installation.md, native-install.md, compaq610-server-elitebook-client.md, native/README.md | Current names and helper/launcher distinction corrected; stale service setup instructions aligned with M7 |
| Optional current handoff/GEMINI/AGENTS.project documents | Not present at the inspected repository paths |
| Native ship/galaxy registries, server persistence/response identifiers, protocol/client state and test references | Read only to classify compatibility versus display names and implementation claims |

The initial semantic inventory searched README.md, docs/ and native/ for the
requested obsolete names, era/scale wording, currency, starter hull and old Bible
authority. A bounded text inventory found 660 matching lines across 69 files
before edits; that inventory covered Markdown/text/C++ headers and sources/Python/
shell text, excluding generated package/build copies. A final broad `rg` search
also includes the newly archived sources.
Raw hit totals are not a zero-staleness test: history, replacement tables and
literal protocol fixtures intentionally contain old spellings.

## DraftComp8 derivation and preserved history

DraftComp8 is derived from the exact approved DraftComp7, whose SHA-256 is
`2270345d1c7e566fda1941b06d45472a43a32dfa32607c23d4604f8b045b7c69`.
The archived v0.20 sync SHA-256 is
`cf5add4ab8cf6439302a6430ccd0b0cf12396f0ef4e5907cd8934b46a7c0289a`.
These match the source hashes already recorded during M3. See the
[history index](../lore/history/README.md).

The DraftComp7 repository copy removes only one final empty line for Git whitespace
checks; its SHA-256 is
`992ee5c686f839aafe53b45de8f53d3cadab3fca13e01f88af49642c05cf4d15`.
DraftComp8 also uses one final newline. The Desktop originals remain unchanged.

The full substantive DraftComp7 text remains: setting, factions, nine-century
timeline, survey/exploration, story guardrails, leaders, historical figures,
military/service people, crews, criminals, guilds, cults, fleets, corporate boards,
service brands, media, advertising, ordinary workers and spacer culture.
Besides final-empty-line normalization, changes to the retained base are limited
to six explicit prose edits:

1. Compilation title becomes DraftComp8.
2. September reconciliation heading is identified as historical context.
3. Its mistaken “This DraftComp6” paragraph is corrected to describe DraftComp7
   as the September handoff compilation.
4. One generic obsolete frontier security label becomes Zero Space.
5. One “0.0” obsolete frontier security label becomes Zero Space.
6. The stale 80–90 starting-galaxy survey heading becomes the 200-system Known Space galaxy.

A new October section precedes retained layers. It records current authority,
checksum, native architecture, environments, M7 operational context, optional
companions, future physical AI Core direction, server authority and Batch boundary.
Embedded v0.11 version labels and earlier integration summaries identify source
layers. Their dated implementation notes do not override current status.

The v0.10 Markdown/DOCX and v0.2 files remain byte-identical to M7. Historical
prompt bodies also remain identical after their new context notices. DraftComp7
has only the documented final-empty-line normalization; the v0.20 copy remains
byte-identical to its approved Desktop source.

## Current checksum and terminology corrections

Severance **187 AS**; present **1087 AS**, exactly **900 years after the Severance**.
Known Space is exactly **200 persistent systems**: **120 faction-controlled**
(Commonwealth 27, Directorate 25, Compact 25, Meridian 23, Synod 20) and **80
Zero Space**. Riftspace is outside that count; approximately 48 initial Riftspace
systems is future content direction, not Batch 14 work. Zero Space is the 0.0
Known Space frontier, distinct from The Null and Riftspace. Unknown Space remains
a broader scientific/mystery term. Survey progression remains
`DETECTED -> CLASSIFIED -> LOCATED -> SURVEYED`.

Current prose uses Inter-Galactic Governance (IGG), UniNet, Galactic Standard Credit
(GSC), Crownspire, Ironstar Forge and Verdance. IGG is a non-sovereign independent
treaty/service framework, not a sixth faction, territorial owner or domestic
police. INSA, Gatewatch and GDF remain coequal; Crossroads Charter and
“INSA knows. Gatewatch understands. The GDF acts.” are retained.
Adaptive Systems Architecture (ASA) is the formal discipline; Adaptive Technology
may remain colloquial. Humans have no routine clone immortality or consequence-
free resurrection.

The [v0.21 sync](../lore/HELION_CANON_CHANGELOG_AI_SYNC_v0.21.txt) explicitly lists
the strict old-to-current substitutions, including obsolete era/scale wording.
Maintained guides now use current prose; old spellings remain only in explicit
historical/compatibility/display-debt context. No global rename was applied to code.
Veyra, AsterTech, The Null and broad Unknown Space usages were not automatically changed.

## Retained compatibility and source display debt

| Evidence / identifier | Why retained |
| --- | --- |
| `Profile::credits`, `credits=`, `credits:u32` at binary offset 39, transaction/error keys | Existing state/wire contract; GSC is current currency prose |
| `GALNET`, `/galnet`, `GalNetEvent`, client `GameEngineState::GALNET`, render/acceptance fixture names | Existing commands, records, code and tests; UniNet is current lore |
| `CONCORDIA`, `GATE.CONCORDIA.VANTAGE` and related topology references | Stable system/route IDs; galaxy already displays Crownspire |
| `TITAN_FORGE`, `TITAN_MULE` and registry references | Stable manufacturer/hull identifiers; current manufacturer canon is Ironstar Forge |
| `SIDEWINDER`, legacy `sidewinder`, deterministic owned-ship migration | Existing starter identity in saves/wire/fixtures; final HELION-native display name is unresolved |
| `nullSecurity`, `setNullSovereignty` and older galaxy migration fixtures | Existing compatibility aliases/history, not current frontier names |
| `UNDETECTED` | Initial absence of earned survey knowledge; does not replace the four approved earned stages |
| Old lore/prompt/handoff language and legacy browser `src/`/migrations | Historical provenance/reference; no second production runtime or current canon authority |

Read-only source evidence includes `native/shared/ships.cpp` (`TITAN_FORGE`,
`SIDEWINDER`, “Titan Forge”, “Titan Forge Mule”, “Sidewinder”),
`native/shared/galaxy.cpp` (`CONCORDIA` -> Crownspire and stable Gate IDs),
`native/server/main.cpp` (`credits` and `GalNetEvent`/responses), the client
renderer/UI labels and the galaxy/persistence/TLS test fixtures. Current executable
labels still include “GalNet”, “Credits” and legacy hull/manufacturer text. These
are identified for a separately authorized presentation cleanup; they have not
been renamed or misrepresented as already corrected. Generic starter shuttle
wording is not approval of a final hull name. No actual gameplay defect requiring
implementation was identified in this scoped documentation review.

## Implementation truth and operations reconciliation

The native C++ persistent server and native SDL2/OpenGL client remain authoritative.
The browser is legacy/reference. The server controls state and environment/roles;
DEV_ADMIN TEST access and INVITED_TESTER admission confer no Production mutation
authority. Companion tokens remain narrow `profile.read`, scoped, expiring and
revocable; future device interfaces do not exist merely because tokens do.

M7 supplies the service lifecycle, fail-closed startup, local health/readiness and
metrics, structured logs, maintenance warning, bounded drain, durable checkpoint,
verified immutable Production backup and non-destructive restore planning.
TEST snapshots remain separate. [Production operations](../production-operations.md)
remains the detailed implementation guide; it needed no canon correction.

Installation documentation had stale examples: group-readable state/key modes and
root-owned keys were incompatible with M7 Production validation; a DEVELOPMENT
manual check reused the directory of a Production unit. Guides now specify `0700`
state, service-owned `0600` keys and consistent explicit Production identity for
that service, with disposable DEVELOPMENT examples using a separate path. No
startup rule was relaxed. Old single-copy backup/upgrade instructions now point
to verified M7 backups and distinguish filesystem copies from HELION envelopes.
Package file preservation is not arbitrary migration/rollback compatibility.
Obsolete build-branch and renderer-default prose was also corrected.

## Future direction and unresolved questions

[HELION_ROADMAP.md](HELION_ROADMAP.md) records M8/M9 and directional Batches 15–18+.
[SHIP_AI_CORES.md](../design/SHIP_AI_CORES.md) separates future engineering from
setting canon: physical equipment, persistent identity, deterministic no-LLM
automation, typed server-validated intents, optional asynchronous providers with
mandatory no-provider/Null-provider behavior, optional companions, manual fallback,
authorization/capability separation and no hidden-information leakage. It adds
no system or external provider dependency.

[ALPHA_READINESS.md](ALPHA_READINESS.md) distinguishes the existing DEVELOPMENT
local-play helper and native login from the future full launcher. It records
future authorized environment selection, account flow, status/maintenance,
STABLE/TEST updates, repair, diagnostics and patch/news management. Recommended
progression is internal TEST, trusted PRIVATE TEST closed alpha, small persistent
Production alpha, expanded closed alpha, then later wider alpha. This is planning.

Unresolved canon/design questions remain: final starter hull name; Veyra's identity;
AsterTech as marque/legacy term; exact future AI engineering/balance and Sovereign
AI population; approval of implementation-only seeded labels where not covered by
the Bible. Protected cosmic mysteries remain protected. No new final answers,
AI mass/power/heat/autonomy numbers or vendor requirements are introduced.

## Validation and stop boundary

Final broad terminology search: **689 matching lines in 74 files**. Classification:

| Retained context | Matching lines |
| --- | ---: |
| Unchanged native source, identifiers and deferred display cleanup | 265 |
| Unchanged test/migration fixtures | 153 |
| Maintained-guide literal interfaces, compatibility notes and historical design | 45 |
| Historical canon sources and annotated prompts | 177 |
| Explicit replacement rules, audit and retained-identifier explanations | 22 |
| Dated execution history and technical identifiers | 26 |
| DraftComp8's explicitly historical interim galaxy target | 1 |

No unqualified obsolete current-canon claim remains in maintained guides. The
search deliberately does not require old spellings to disappear from history,
literal interfaces or the synchronization replacement list. Generated package/build
copies are excluded from maintained-source acceptance and were not regenerated.

Validation checks the canon checksum and allocation, authority links, maintained
prose terminology, historical/source preservation with the documented final-line normalization, unchanged historical
prompt bodies, documentation-only file scope, and whitespace. The execution record
records final counts/results. Native build, gameplay, graphical, package and
sanitizer suites are not rerun for this docs-only checkpoint; M7's actual evidence
remains recorded as M7 evidence, not a new test run.

After this local checkpoint, stop for review. Recommended sequence only after
approval: back up the checkpoint, push the Batch 14 branch, verify synchronization,
update public main-branch documentation separately without merging unfinished
Batch 14 code, then obtain/observe explicit M8 authorization. Nothing here pushes,
modifies main or begins M8, M9 or Batch 15.
