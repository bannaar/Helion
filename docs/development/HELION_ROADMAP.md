# HELION roadmap

Status: 2026-10-22, after Batch 14 M8 acceptance. This is direction
and sequencing, not authorization to begin another milestone. Implementation
evidence lives in [BATCH14_EXECUTION.md](BATCH14_EXECUTION.md) and the source/tests;
lore authority is [DraftComp8](../lore/HELION_World_Bible_draftcomp8.txt).

| Stage | Status / direction |
| --- | --- |
| Batch 14 M0–M7 | Complete, including M7 Production Operations and Service Lifecycle |
| Batch 14 M8 | Complete: Deployment, Packaging, Migration, Release and Recovery |
| Batch 14 M9 | Not started: Final Hardening, Security Review, Documentation Reconciliation and Batch 14 Completion |
| Alpha Readiness / Launcher bridge | Future planning; see [ALPHA_READINESS.md](ALPHA_READINESS.md) |
| Batch 15 | Future Ships, Systems, Combat, Outfitting and AI Cores |
| Batch 16 | Future Equipment Economy, Manufacturing, Salvage and Industry |
| Batch 17 | Future Missions, NPC Careers and Shared World Activity |
| Batch 18+ | Future Riftspace, Xenotech and Sovereign Intelligence |

Batch 14 is not complete. M9 remains unstarted. Batch 15 is not started and requires
completed Batch 14 plus explicit user authorization. Future entries are directional and may need separate design and
acceptance gates; they are not claims of implemented gameplay.

M8 builds on M7's service lifecycle, packages, persistence loader, verified backups
and non-destructive restore planning. It adds release/build identity, STABLE/TEST
artifacts, installed-package inspection, TEST-first migration/deployment rehearsal,
backup-before-upgrade, failure/recovery checks and bounded restart validation;
see [release/deployment](../release-deployment.md). Destructive Production restore
remains unsupported.
M9 remains the complete hardening, security, regression, sanitizer, package,
binary/legacy graphical and documentation acceptance gate.

The [future Ship AI Core design](../design/SHIP_AI_CORES.md) extends the validated
native server/client, equipment and protocol boundaries. It does not authorize
an equipment rewrite, combat expansion, provider integration or AI implementation
during Batch 14.
