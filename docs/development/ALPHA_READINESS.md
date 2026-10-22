# Alpha readiness and launcher bridge

Status: planning only, 2026-10-22. Batch 14 is complete through M7; M8 and M9 are
not started. Batch 14 is not complete. No launcher or alpha deployment is added
or authorized by this document. See the [roadmap](HELION_ROADMAP.md).

The combined native package already includes `helion-play`, a local-play helper:
it prepares a private loopback certificate, starts a DEVELOPMENT server and client,
and stops the server when the client closes. `--check` performs an installation
smoke check. The split client has a configured connection helper/desktop shortcut.
The native client already has account creation/login. These tools are distinct
from the planned full player launcher and do not implement its account/update UX.

The future full launcher may provide:

- account/login flow and a separately designed secure session/pairing experience;
- authorized server/environment selection with clear Production versus PRIVATE
  TEST presentation; a client selection never grants invitation or role access;
- server status and maintenance messages;
- build/version checks, STABLE/TEST release channels and compatible client launch;
- updates, repair/verify and patch management with recoverable failure behavior;
- bounded diagnostic collection, launch configuration and news/status.

M6 already supplies server-controlled environment/channel identity, not a finished
release/update service or client-side channel admission policy. M7 supplies local
health/maintenance hooks, not a public launcher status API. M8 release/recovery
rehearsal and M9 acceptance must establish evidence for the next bridge; their
existing gates are not replaced by this plan. Future authentication, distribution,
update integrity and diagnostics need explicit security/privacy design. Keep
credentials out of arguments, news and diagnostic bundles.

Recommended alpha progression, subject to separate approval:

1. Internal TEST: deterministic regression, operational/recovery rehearsal and
   developer-led sessions on isolated noncanonical state.
2. Trusted closed alpha on PRIVATE TEST: invited users, resettable progression,
   clear maintenance/reset expectations and no implicit DEV_ADMIN access.
3. Small persistent Production alpha: canonical progress only after Batch 14
   acceptance and successful backup/upgrade/recovery rehearsal with explicit
   operational ownership and player-facing limitations.
4. Expanded closed alpha: grow population after observing stability, authority,
   persistence and support/recovery behavior.
5. Later wider alpha: expand reach only on measured readiness and separate approval.

Companions and cognitive providers are optional future extensions; players must
not need them. A local native session can run without internet; playing on remote
Production requires network access. The browser under `src/` remains legacy
reference and does not become an alternate alpha backend.
