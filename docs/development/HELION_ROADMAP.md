# HELION roadmap

Status: 2026-10-22, after the verified Batch 14 M7 checkpoint. This is direction
and sequencing, not authorization to begin another milestone. Implementation
evidence lives in [BATCH14_EXECUTION.md](BATCH14_EXECUTION.md) and the source/tests;
lore authority is [DraftComp8](../lore/HELION_World_Bible_draftcomp8.txt).

| Stage | Status / direction |
| --- | --- |
| Batch 14 M0–M7 | Complete, including M7 Production Operations and Service Lifecycle |
| Batch 14 M8 | Not started: Deployment, Packaging, Migration, Release and Recovery |
| Batch 14 M9 | Not started: Final Hardening, Security Review, Documentation Reconciliation and Batch 14 Completion |
| Alpha Readiness / Launcher bridge | Future planning; see [ALPHA_READINESS.md](ALPHA_READINESS.md) |
| Batch 15 | Future Ships, Systems, Combat, Outfitting and AI Cores |
| Batch 16 | Future Equipment Economy, Manufacturing, Salvage and Industry |
| Batch 17 | Future Missions, NPC Careers and Shared World Activity |
| Batch 18+ | Future Riftspace, Xenotech and Sovereign Intelligence |

Batch 14 is not complete. The post-M7 canon/docs checkpoint starts neither M8 nor
M9. Batch 15 is not started and requires completed Batch 14 plus explicit user
authorization. Future entries are directional and may need separate design and
acceptance gates; they are not claims of implemented gameplay.

M8 must build on M7's existing service lifecycle, packages, persistence loader,
verified backups and non-destructive restore planning. Its remaining work includes
release/build identity, STABLE/TEST artifacts, clean installation and upgrades,
TEST-first migration/deployment rehearsal, backup-before-upgrade, failed migration
and deployment recovery, restart/longer-lived validation and release documentation.
M9 remains the complete hardening, security, regression, sanitizer, package,
binary/legacy graphical and documentation acceptance gate.

The [future Ship AI Core design](../design/SHIP_AI_CORES.md) extends the validated
native server/client, equipment and protocol boundaries. It does not authorize
an equipment rewrite, combat expansion, provider integration or AI implementation
during Batch 14.
