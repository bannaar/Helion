# Production operations (Batch 14 M7)

HELION remains a standalone authoritative native C++ server and SDL/OpenGL
client. These local POSIX interfaces require no cloud vendor, web server,
companion service or network administration endpoint. They extend the existing
M6 environment policy, file lock, strict loader and atomic save path.

## Service account and configuration

Use a dedicated unprivileged `helion` account. Own the state directory and
private key with that account; keep directories `0700` and private files `0600`.
Use a separate account/directory/TLS identity for PRIVATE TEST. Never grant
players or companions OS access to either service account. Configuration is
explicit command-line options in the operator-managed systemd unit, with TLS
files under `/etc/helion` and authoritative data under `/var/lib/helion`.
Do not put passwords or bearer credentials in command arguments.

```sh
helion_server 4242 /var/lib/helion/commander.db --environment production \
  --bind 127.0.0.1 --cert /etc/helion/server.crt --key /etc/helion/server.key \
  --drain-seconds 5 --maintenance-warning-seconds 60
```

Production requires an explicitly supplied absolute data path and environment.
An absent file at that explicit path initializes a universe; an existing empty,
corrupt, incompatible, unreadable, symlinked or hardlinked file is rejected.
Untagged saves remain development-only. Persisted password verifiers must match
the supported complete scrypt encoding; a dollar prefix alone is insufficient. Existing directory markers bind the
environment/instance/economy even when a new filename is selected. TEST options
are rejected in Production. Repeated startup options and invalid numeric bounds
are errors. The default omitted environment remains DEVELOPMENT for old local
launchers; omission never grants Production or TEST operations.

TLS certificate/key are mandatory. Production additionally requires an owner-only
regular, singly linked service-owned private key and a currently valid leaf
certificate; the chain/key must match. TLS 1.2+ and normal client certificate/name
verification remain. Public certificate chains may be readable; private keys may
not. Renewal requires restart. This is not certificate issuance/renewal automation.
Use independent certificates and keys; marker/path checks cannot detect a
deliberate byte-for-byte secret copy by an OS owner.

Startup validates environment/storage isolation before reading credentials or
persistence, acquires the existing exclusive stable `.lock`, loads and strictly
validates authoritative state, performs supported existing migrations, initializes
the listener and publishes readiness. It runs with zero players. Failure never
falls back to TEST or replaces corrupt state with a fresh universe. A bind failure
does not leave a ready service. No package installs/enables a service automatically.

## Lifecycle and maintenance

The operational lifecycle is separate from the M5 client state controller and
has no gameplay authority. Startup emits VALIDATING/LOADING events and a local
LOADING status. ONLINE means validated state/TLS/listener ready. Transitions are:

```text
STARTING -> VALIDATING -> LOADING -> ONLINE
ONLINE -> MAINTENANCE_SOON -> DRAINING -> SHUTTING_DOWN -> OFFLINE
ONLINE -> DRAINING -> SHUTTING_DOWN -> OFFLINE
save failure -> DEGRADED (unready); final checkpoint/backup failure -> FATAL
```

`SIGUSR1` schedules maintenance after `--maintenance-warning-seconds` (1–86400,
default 60). MAINTENANCE_SOON remains ready. Repeated signals cannot postpone the
first deadline. `SIGTERM`/`SIGINT` skip warnings and initiate drain immediately.
`--drain-seconds` is 1–5 (default 1). At drain start, readiness becomes false,
new sockets are closed before TLS (no plaintext response), and requests from
existing sessions receive the bounded existing text-envelope error
`ERR service-draining`, then close. An already executing authoritative transaction
may finish under the existing state lock. No new gameplay work is admitted after
the request guard sees draining. Idle sessions/partial handshakes are interrupted
at the drain deadline. If no connections remain, shutdown proceeds immediately.
Repeated stop signals cannot extend the deadline.

After drain the listener closes, outstanding sockets are shut down, workers join,
and an unconditional authoritative checkpoint is saved. Production then creates
and verifies an immutable local shutdown backup. Only successful checkpoint,
backup and status publication produce a successful shutdown log/exit zero.
Failure yields FATAL and exit one. Connection drain is bounded; filesystem I/O
still depends on a functioning local filesystem. systemd's stop timeout is the
outer process bound, not a guarantee of disk progress under hardware failure.

There is no maintenance gameplay UI or new protocol schema. The status file is
the future integration hook. Maintenance ends with a stopped process; reopening
requires an operator restart and fresh startup validation. `Restart=on-failure`
does not automatically reopen after successful scheduled maintenance.

```sh
systemctl kill --kill-whom=main --signal=SIGUSR1 helion-server.service
systemctl stop helion-server.service
systemctl start helion-server.service
systemctl restart helion-server.service
```

Optional `helion-maintenance.service`/`.timer` examples request a daily window
(example 04:00 UTC). Edit the schedule and explicitly enable it. Timer activation
sends a signal to the main service; it does not deploy, migrate, restore, reopen,
or promote a release. Follow systemd's
[service semantics](https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html),
[stop semantics](https://www.freedesktop.org/software/systemd/man/latest/systemd.kill.html)
and [timer semantics](https://www.freedesktop.org/software/systemd/man/latest/systemd.timer.html).

## Health and metrics

`<data-file>.status.json` is an owner-only atomic JSON snapshot, refreshed every
half second and at lifecycle boundaries. It contains no credentials or per-player
gameplay state. `helion-service-status /var/lib/helion/commander.db` uses Python 3
standard libraries to validate the private file, check PID liveness and reject
snapshots older than five seconds. Exit 0 means ready, 1 valid but unready/dead/
stale, 2 unavailable/invalid. A PID alone is not readiness. OFFLINE/FATAL cannot
become ready even if their PID remains alive briefly. A stopped server's old
snapshot is diagnostic, not an active monitor. Probe as the service account;
`systemctl is-active` alone does not check HELION readiness. No TCP health port
is exposed. `Type=simple` systemd startup does not imply application readiness.

Fields include schema, timestamp, PID, environment, instance, server-selected
channel, phase, readiness, durability, uptime, active connections (including
handshakes), player sessions, accepted/rejected connections, complete request
count, malformed count, failed authentications, committed saves/failures/cumulative
microseconds, last commit timestamp, backup successes/failures/last
timestamp and load microseconds. Players means password-authenticated player
connections, not distinct commanders or companions. Counts are process-local,
reset on restart, and not a historical billing/analytics system. Backup age can
be derived from its timestamp; filesystem backup envelopes are the durable
inventory. Routine incomplete frames are not counted as complete requests.
Metrics do not claim CPU/RSS, per-IP abuse protection or per-command latency.

A failed save makes readiness false and new sockets are rejected. Transient
health-file publication failures also refuse new sockets and retry publication;
they do not terminate existing rollback/retry transactions. Health publication
uses atomic replacement without fsync because it is ephemeral observation,
separate from durable authoritative saves. A later
successful checkpoint can restore durability/readiness. A directory-fsync failure
after a committed rename is treated as uncertain durability: no memory-only
transaction rollback is attempted, readiness fails, further requests are rejected
and service shutdown fails. Success replies are suppressed once durability is
uncertain. A committed rename increments the commit counter even when its
directory sync fails, so commit and failure counters can overlap; the durability
flag must be checked alongside timestamps. This avoids claiming power-loss safety that the OS
did not confirm. SIGKILL/power loss or an unwritable health destination can prevent a final
status update; freshness, phase and process exit must be checked together.

## Persistence and logs

Gameplay transactions, flight checkpoints, economy, galaxy, missions, ship
ownership, knowledge, receipts and companion verifiers retain their existing save
formats and rollback rules. Save files are fsynced before atomic rename; the
parent directory is now fsynced after rename. Supported migrations remain the
existing loader/migration path. There is no M7 schema migration or reverse migration.

Structured operations JSON lines go to stderr (journald under systemd): timestamp,
subsystem, environment, instance, PID, static event and result. They cover startup,
TLS, load, sampled save successes, every save failure, readiness, maintenance,
drain, shutdown, backup, accepted/closed connections, authentication outcomes,
authorization denials and malformed protocol/grammar. Metrics count every save;
successful save logs are sampled (first and every 60th). Legacy diagnostic error
lines and the human-readable listening banner remain. Consumers should select
JSON lines; this is not a claim that all output is JSON. Logs never receive raw
requests/responses, passwords, keys, bearer values or verifiers.

Offline privileged operations also write durable owner-only
`<data-file>.operations.jsonl` intent/result records with OS UID, action, backup
ID and operator reason code. Reason codes use 1–96 safe identifier characters,
not arbitrary free text. This journal is separate from the existing TEST admin
audit and never restored. Local journals can be altered by the OS owner; remote
tamper-resistant collection and retention are deferred.

```sh
journalctl -u helion-server.service -f
```

## Production backups and restore verification

M6 TEST snapshots remain in `.helion-test-snapshots`, authenticated using rostered
DEV_ADMIN credentials. M7 Production backups use `.helion-backups` and the
service-account/exclusive-lock boundary. TEST gameplay roles and companion tokens
cannot invoke these operations. No network production operations endpoint exists.

Every graceful Production shutdown checkpoints and creates an immutable backup
named `shutdown-<epoch>-<pid>.snapshot`. Offline explicit backups require the
same protected configuration, explicit Production environment/target, reason and
exclusive data lock. Stop the service first. For example:

```sh
helion_server 4242 /var/lib/helion/commander.db --environment production \
  --cert /etc/helion/server.crt --key /etc/helion/server.key \
  --operations PRODUCTION BACKUP pre-maintenance-20261021 --reason planned-maintenance
# Repeat with VERIFY_BACKUP or PLAN_RESTORE and the same backup ID.
```

The envelope is `HELION_BACKUP_V1`, environment, instance, economy, immutable ID,
timestamp, `persistence-admin1` compatibility identity, `native-0.2.0` server
version, SHA-256 and exact persistence payload. It includes account/token
verifiers required for recovery, so it is sensitive. A backup of a supported
pre-migration legacy record may also contain its original credential; treat all
backups as private authentication material. It includes no TLS keys,
raw bearer tokens or separate operator access policy. Protect and separately
back up TLS/configuration/audit with appropriate OS controls. Snapshot wrapper
version and payload format are independently checked. Payload verification is
bounded to 64 MiB; larger universes require an explicitly reviewed extension.

Creation validates the live payload using the normal strict loader, stages a
`0600` file, fsyncs it, publishes by no-overwrite hardlink then removes the staging
name, syncs directories and rereads/verifies identity and digest before reporting
success. Final shutdown already has a freshly serialized validated state and
performs the same envelope/digest reread. IDs are never overwritten or deleted
automatically. A failed attempt may leave an unusable final file; verification
must pass and the next attempt must use a fresh ID. Monitor capacity; implement
retention externally while preserving multiple verified recovery points.

VERIFY_BACKUP/PLAN_RESTORE require the exclusive live-state lock, validate
metadata/environment/instance/economy/schema/version/checksum, stage a temporary
payload and invoke the strict existing loader for semantic compatibility. They
also work when live state is corrupt, without loading or changing it. TEST
snapshots, cross-environment payloads, malformed/truncated records and correct-
checksum but invalid persistence are rejected. Verification makes no migration,
revocation, player-state or live-file change. PLAN_RESTORE emits the source ID,
digest, compatibility and `replaces_live_state:false`.

**M7 deliberately supplies no destructive restore command.** For an approved
recovery: keep the service stopped, retain the exact current state (even corrupt)
and all verified backups, verify/plan against the intended backup, and rehearse
its verified payload with the matching binary in a separate protected disposable
directory. Check login, persistent progression, environment and health, then stop
that rehearsal. An operator-reviewed offline file replacement must separately
preserve the current recovery point, account for credential revocation and retain
the stable `.lock` inode. Never copy Production secrets into the real PRIVATE
TEST service; the M7 rehearsal is an isolated Production-identity disposable
recovery target. Automated TEST-first migrations, reviewed restore replacement,
companion revocation during Production restore and release rollback belong to
M8. Do not manually transplant an envelope as the live persistence file.

## Security review and limits

Production operations have no player-state editing, raw filesystem/SQL/command
execution or role-elevation API. TLS, negotiated binary v1 bounds/replay rules,
explicit legacy transport, scope enforcement and M6 TEST/Production boundaries
remain. The M7 process tests include a test-only filesystem injector that forces a
post-rename directory-sync error and proves no success reply, failed shutdown
and preservation of the committed state. The injector is compiled only by the
test harness and never linked into or installed with HELION. Tests also cover
Production admin rejection, companion scope,
no secret logs, corrupt/empty state, key protection and wrong environment; full
M4/M5/M6 suites remain required gates. These interfaces trust a cooperating local
filesystem/service account, not an attacker with OS ownership. No public operator
login/MFA system, live restore, remote backup replication, retention daemon,
automatic certificate renewal, cross-IP throttle or zero-downtime deploy exists.

The packaged unit uses least-privilege service identity, `UMask=0077`, private
temporary files, read-only system/home protection and state-only write access.
Copy examples to `/etc/systemd/system`, provision protected state/TLS first,
review the bind address/firewall and run the health probe after startup. The
automated gate checks headless packaged processes and unit syntax where available;
it does not enable a system service or claim physical-host deployment acceptance.
M8 deployment/release/migration/recovery tooling, M9 final Batch 14 acceptance,
and all Ship AI Core/Batch 15 work remain unstarted.
