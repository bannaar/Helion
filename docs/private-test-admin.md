# Private TEST administration (Batch 14 M6)

HELION's canonical currency is the Galactic Standard Credit (GSC). Existing
`credits` protocol/save fields remain compatible. The server owns environment,
account roles, admission, gameplay truth, transactions and persistence. A UI,
companion token, client role string or packet cannot select these privileges.

## Environments and storage

| Environment | Admission | Economy | Privileged TEST operations | Channel |
| --- | --- | --- | --- | --- |
| `development` | Existing account/create rules | Canonical | Denied | DEVELOPMENT |
| `test` | Server invitation roster and bound password | Convenience or production-like | DEV_ADMIN player sessions only | TEST |
| `production` | Existing account/create rules | Canonical | Denied, including DEV_ADMIN | STABLE |

Omitted `--environment` remains development for old launchers. TEST privileges
require explicit selection. Unknown/repeated environment options fail startup.
`--test-economy` and recovery options are rejected outside TEST.

TEST and Production require separate owner-only (`0700`) state directories.
The server claims the directory with an owner-only `.helion-environment` marker
containing environment, instance and economy policy. Every environment honors an
existing marker, including when opening a fresh data filename. Saves retain the
strict `E` environment record. A mismatch aborts before migration/save. Legacy
untagged saves remain development-only. Keep one universe per claimed directory;
changing instance or economy means preparing a separate directory/universe.

TEST additionally requires its access file, certificate and private key to be
regular owner-only (`0600`) files directly in its state directory. Symlinks,
hardlinks and foreign-owned private files are rejected. Use independent TEST
certificate/key material, passwords, invitations and backups. Operators with OS
write access can deliberately copy secrets or edit saves; these controls protect
against client privilege escalation and accidental shared paths, not a hostile
service-account owner. Retain the existing stable `.lock` sidecar.

Production loads persistent state before accepting connections, runs without
connected players, checkpoints flight and market changes independently of logout,
and flushes dirty state on clean shutdown. Restart restores legitimate saved
progression. M6 adds no artificial background simulation.

## Provision invitations

Create a dedicated TEST directory and independent TLS identity. Before startup,
prepare its access file using the service account. The header selects the trusted
server environment and a stable operator-chosen instance ID. Each row binds an
account, role and exact scrypt verifier:

```text
HELION_ACCESS_V1 test internal-qa
administrator DEV_ADMIN <scrypt-verifier>
invitedpilot INVITED_TESTER <different-scrypt-verifier>
explicitguest NORMAL_PLAYER <different-scrypt-verifier>
```

Generate a verifier with `helion_server --hash-password`; supply a 12–128 byte
password on standard input, never in command arguments. The command outputs only
the verifier. Distribute each invitation password through a separate secure
channel. Restrict the completed access file to `0600`. Never commit it.

A new invitee uses the established CREATE/first-commander flow with that password.
A different password cannot claim an invited name or administrator role. CREATE
stores the roster verifier. Existing TEST accounts must have that exact verifier
in the roster. LOGIN and companion authentication require the current binding.
Changing/removing invitations takes effect after server restart; live policy
reload and account recovery UI are deferred. Production account behavior is
unchanged. A normal account is admitted to TEST only if explicitly rostered.

```sh
helion_server 4242 /srv/helion-test/universe.db \
  --environment test --access-file /srv/helion-test/access \
  --cert /srv/helion-test/server.crt --key /srv/helion-test/server.key
```

Roles are loaded only from this protected server file. DEV_ADMIN can administer
TEST; INVITED_TESTER can play TEST with conveniences and cannot mutate developer
state. Explicitly admitted NORMAL_PLAYER has ordinary ship requirements.
Companion authentication stays read-only even for an administrator's token.
PROFILE adds server-supplied `environment`, `role` and `channel` metadata for
presentation; they are never authorization input.

## Economy and ship policy

Default TEST policy is `--test-economy convenience`: eligible hulls/modules and
normal market purchases cost 100 GSC, with the established abundant TEST stocks,
demand and budgets. Selling uses the established TEST economy policy. Fuel and
services retain their existing validated semantics; this is not a blanket rewrite
of every amount to 100. `--test-economy production-like` selects canonical prices,
scarcity and restock rules in an isolated TEST universe. An explicitly invited
NORMAL_PLAYER can also exercise ordinary ship requirements for progression QA. Production/development
always use canonical prices; canonical definitions are unchanged.

DEV_ADMIN and INVITED_TESTER bypass the shipyard's implemented reputation
requirement predicate in TEST. Station offers, docking, pad compatibility,
ownership, credits and physical storage constraints still apply. Current
canonical offers have no nonzero reputation gate; deterministic tests exercise a
synthetic gated offer through the same predicate. Rank, permit, career and loyalty
ship gates are metadata/future gameplay, not invented M6 systems.

## Typed administrative requests

Password-authenticated DEV_ADMIN player sessions in TEST may send bounded text
requests, either through explicit legacy TLS or the existing negotiated binary
v1 text-request envelope:

```text
TESTADMIN TEST <numeric-request-id> <verb> <account-or-universe> <field> <value>
```

The explicit `TEST` token is mandatory. IDs contain 1–18 decimal digits. Fields,
verbs, targets, values and domain constraints are validated server-side. There
is no raw database, arbitrary file, memory-offset or script interface. Each
connection has a 64-request administrative budget; the next attempt is audited,
denied and closes that connection. Cross-connection/IP throttling remains future
work, as with existing authentication limits.

| Verb | Field/target | Behavior |
| --- | --- | --- |
| GET | gsc, experience, salvage, ore, food, parts, hull, fuel | Read implemented scalar; value must be `-` |
| SET, ADD, REMOVE | Same scalar fields | Nonnegative bounded integer changes; hull/fuel/cargo limits enforced |
| GRANT | ship / registered hull ID | Add owned stored ship; dock/pad/duplicate/capacity constraints apply |
| GRANT, REVOKE | module / registered module ID | Change active ship's module ownership while docked; fitted modules cannot be revoked |
| TELEPORT | station / station index | Move active ship to an implemented station, clear motion/input and reset local combat encounter; destroyed ships require recovery first |
| RESET | player / starter | Reset target's implemented progression/ships/knowledge; retain identity/password/display; revoke target's companion tokens |
| RESET | universe markets / seed | Reseed only this TEST universe's markets under its selected economy policy |

Examples:

```text
TESTADMIN TEST 100 ADD invitedpilot gsc 10000
TESTADMIN TEST 101 GRANT invitedpilot ship TITAN_MULE
TESTADMIN TEST 102 ADD invitedpilot food 2
TESTADMIN TEST 103 TELEPORT invitedpilot station 1
TESTADMIN TEST 104 RESET universe markets seed
```

Ship/cargo/fuel/module edits require appropriate docked state, known registries,
nonnegative quantities and capacities; ship grants cannot duplicate an owned
hull. GSC changes are recorded as economy faucets/sinks. Experience and salvage
are implemented progression scalars. Mission/career graph edits, faction/rank,
permits/licenses/unlocks and arbitrary assets are not exposed: they need separate
domain transition services rather than unsafe integer/string mutation.

Successful mutations atomically persist both state and a receipt in the existing
save. The account/request-ID key and canonical-operation fingerprint detect
retries and conflicting reuse across TLS sessions and restart. Identical retries
return the original result with `replay=1`; conflicting operations fail. GET is
read-only and not memoized. The bounded ledger holds 4096 mutations/recovery
receipts and fails closed when full; automated compaction/rotation is deferred.
Never retry an uncertain mutation with a new ID.

## Audit

`<data-file>.audit.jsonl` is owner-only, append-only through the server API and
fsynced per record. Records identify timestamp, environment, instance, account,
server-derived role, process/connection or local session, action, target, field,
request ID, normalized requested/before/after values, result and reason.
Successful mutation writes an intent before changing live state and an outcome
after durable state/receipt save. Denials, validation failures, reads and retries
are also logged. Audit failure before commit denies the mutation. A crash between
state commit and outcome can leave an intent plus a committed receipt; correlate
and retry the same ID. State and the separate journal are not a cross-file atomic
transaction. A failed outcome reports uncertainty and keeps the committed receipt.

The `account_id_hex` field preserves exact opaque account bytes alongside the
readable account field, including non-ASCII legacy identifiers.
Passwords, invitation verifiers, bearer tokens, token hashes and TLS material are
never passed to audit. Raw requests and invalid values are not logged. Unknown
targets are normalized, malformed requests are redacted, and records escape log
injection characters. This local journal is not tamper-proof against its OS owner;
remote collection, retention and integrity chaining are deferred.

## Offline recovery and snapshots

Stop the TEST server first. Recovery acquires the same exclusive persistence lock
and authenticates a rostered DEV_ADMIN account with its existing password on
standard input. It never binds a network socket. All three commands require an
explicit TEST environment and target:

```sh
helion_server 4242 /srv/helion-test/universe.db --environment test \
  --access-file /srv/helion-test/access --cert /srv/helion-test/server.crt \
  --key /srv/helion-test/server.key --operator administrator \
  --test-recovery TEST SNAPSHOT before-experiment
# Replace SNAPSHOT with RESTORE to restore that snapshot ID.
# RESET with a new operation ID resets the implemented TEST universe.
```

Snapshots are immutable owner-only files in the owner-only
`.helion-test-snapshots` directory beside the save. The wrapper records TEST,
instance, economy policy, format/schema identity, timestamp, snapshot ID, build
compatibility tag and SHA-256 content digest. Snapshots include sensitive account
verifiers; protect them as saves. Restore rejects incompatible, symlinked,
corrupt, cross-environment and invalid normal persistence payloads before replacing
live state. The normal strict loader validates galaxy, ownership, economy,
knowledge and credentials. Restore also requires the authenticated operator's
account/verifier to survive unchanged.

Restore preserves/merges current administration receipts, so rollback cannot make
old grants executable again. Replaying a grant rolled back by recovery returns its
historical receipt without reapplying it. Recovery IDs are one-use: restoring the
same snapshot ID again is rejected. RESET preserves account identities/passwords
and the roster, resets implemented world/progression/ships/markets, and retains
receipt history. Restore and RESET revoke all companion tokens rather than
resurrecting credentials. Audit and invitation files are never restored/reset.
Production cannot invoke these paths or open the TEST storage directory.

This is local offline recovery on a cooperating local filesystem, not disaster
recovery orchestration. It does not implement reverse migrations, live restores,
cloud backups, binary deployment rollback, remote operator access, a public admin
UI or a production administration facility. Schema changes should preserve a
known-good TEST snapshot and compatible server binary for rehearsal.

## Experimental/release foundation and deferred work

EnvironmentPolicy computes TEST/STABLE/DEVELOPMENT release identity on the server;
Production cannot select TEST economy/recovery options or reuse credential paths
inside a claimed TEST directory. This small policy seam
supports future experimental capability checks. M6 adds no experimental gameplay
feature, arbitrary feature-name activation, launcher CDN or deployment service.

M7+ work remains subject to the Batch 14 plan: richer mission/rank/permit domain
services and their admin adapters, cross-sector operations where implemented,
credential rotation/recovery and invitation reload, ledger lifecycle, remote audit
collection, durable operational monitoring, production operations with separately
designed privileges, snapshot lifecycle/migration tooling and release distribution.
No M7 or Batch 15 implementation is included here.

M7 follow-up: [Production operations](production-operations.md) now supplies the
separate OS-service-account lifecycle/health/backup/verification foundation. It
adds no TEST cheat capability or Production player-state mutation. TEST recovery
continues to use the rostered DEV_ADMIN path described above.
