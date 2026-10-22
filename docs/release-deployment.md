# Native release, deployment and recovery (Batch 14 M8)

This guide extends the existing authoritative native server/client, M6 environment
policy and M7 operations. No cloud, container or systemd runtime is required.
Production service examples remain optional systemd units. This is not a full
player launcher, remote deployment service, live restore or atomic package/database
rollback system. M9 final acceptance and Batch 15 remain separate and unstarted.

## Identity and channels

The M8 project version is **0.2.1**. Inspect either executable with `--build-info`
or `--version`: both emit JSON without opening a display, listener or state file.
Installed manifests live at `share/helion/<combined|server|client>/release-manifest.json`
under the install prefix. Split manifests occupy different paths so server/client
packages can coexist. They report project version, Git revision, dirty flag,
source/configuration SHA-256, build ID, package kind/components, compiler/build type,
release channel, permitted server environments, TLS minimum, legacy/binary protocol
versions, persistence/backup compatibility and supported migration families.

The source digest fingerprints native/CMake/scripts/assets and documentation inputs
at CMake configuration. Build IDs include Git revision and source/configuration
hashes; branch names alone are not release identity. No build timestamp is included.
Reconfigure/rebuild after changing inputs. A validated working-tree build is labeled
dirty and has a content identity; published release candidates should be built from
an explicitly recorded clean commit. Source archives without Git are labeled
unversioned, never falsely assigned a revision. This does not claim byte-identical
packages across compilers, machines or packaging tools.

`-DHELION_RELEASE_CHANNEL=STABLE|TEST` selects the **artifact** channel (default STABLE).
A STABLE candidate can be rehearsed in PRIVATE TEST before Production promotion.
A TEST server build can run DEVELOPMENT or PRIVATE TEST and refuses Production.
A client channel never grants admission, role, gameplay or administrator authority.
Server environment and invitation/role policy remain authoritative. The existing
PROFILE/status `channel` is the M6 environment-derived audience label; new status
`build_channel` and `build_id` identify the artifact separately. These labels are
not authentication or a replacement for environment isolation.

Transport compatibility remains `helion-tls-legacy2-binary1`: TLS 1.2+, negotiated
bounded binary v1 or explicit legacy text v2. An incompatible binary negotiation
gets the existing explicit update-required error; no silent downgrade is added.
Build equality is not required for compatible server/client versions. M8 adds no
frame type, wire field rename, client state rewrite or protocol-version change.

## Build and package

Use an isolated build directory for each flavor/channel. For example:

```sh
cmake -S . -B /tmp/helion-release-server -DCMAKE_BUILD_TYPE=Release \
  -DHELION_BUILD_CLIENT=OFF -DHELION_PACKAGE_FLAVOR=server \
  -DHELION_RELEASE_CHANNEL=STABLE -DHELION_WARNINGS_AS_ERRORS=ON
cmake --build /tmp/helion-release-server -j2
ctest --test-dir /tmp/helion-release-server --output-on-failure
cpack --config /tmp/helion-release-server/CPackConfig.cmake -G TGZ -B /tmp/helion-artifacts
cpack --config /tmp/helion-release-server/CPackConfig.cmake -G DEB -B /tmp/helion-artifacts
sha256sum /tmp/helion-artifacts/*
```

Use `HELION_BUILD_SERVER=OFF` / `HELION_PACKAGE_FLAVOR=client` for client-only;
use both executables / `combined` for local-play packages. Server/client defaults
infer the corresponding split flavor when configuring a fresh split build.
Tarball names carry the channel; TEST Debian versions use `0.2.1~test`. Combined
and split Debian packages declare conflicts to prevent overlapping executable
ownership; server and client split packages coexist. This is not automatic channel
switching or an update service. Keep artifacts in per-build/channel directories.
An explicit combined flavor requires both executables; mismatched split flavors
and invalid channels fail at configuration. Package and newly created install directories use `0755`
independently of the build user's umask; executables use `0755` and data/docs
use `0644`, without group/world write access. Private runtime state/TLS are provisioned
separately with the stricter permissions described below.

Packages contain binaries, manifests, appropriate helpers, desktop assets and
portable server/maintenance examples. Documentation retains its relative tree,
with older flat guide paths kept for compatibility. No service is enabled, no
persistent data directory is populated, and no certificate, private key, invitation,
credential, state file or test fault library is distributed. Runtime libraries are
dynamically linked; install the package dependencies for the target distribution.
A successful local package test does not prove physical Compaq/LAN deployment.

## Clean installation and inspection

Extract a trusted tarball into an empty prefix, or use the Debian package manager.
Tarballs place executables under `usr/bin`; a source `cmake --install --prefix`
places them under `bin`. No executable depends on the source/build directory.

```sh
/path/to/installed/usr/bin/helion-release-check \
  --prefix /path/to/installed/usr --kind server --channel STABLE
/path/to/installed/usr/bin/helion_server --build-info
```

`helion-release-check` uses Python 3 standard libraries and checks the installed
manifest against executable identities, required helpers/service examples and
unsafe executable/manifest modes. Server/combined packages ship it; an operator
may use the same checker for a client prefix. It inspects **trusted local files**,
not untrusted downloaded executables. Metadata and SHA-256 are not signatures or
publisher authentication: obtain artifacts and expected hashes through a trusted
release channel, verify hashes before execution, and retain the approved manifest.
It changes no authoritative state and installs nothing.

Provision a dedicated service account, `0700` state directory and service-owned
`0600` private key with a valid matching certificate as described in
[Production operations](production-operations.md). PRIVATE TEST uses independent
colocated credentials/roster and state. Never copy Production credentials or saves
into PRIVATE TEST. Local `helion-play` remains a DEVELOPMENT helper.

## Persistence and migration contract

M8 retains `persistence-admin1` and all existing record shapes; no new save schema
or reverse migration is introduced. The existing strict loader validates:

- environment `E` first/unique, legacy untagged state DEVELOPMENT-only;
- plaintext `P` and scrypt `H` accounts, optional appended flight/career/reputation
  fields, deterministic owned `S` ships and per-ship loadout;
- regional market `K` rows and economic telemetry; distinct two-/six-column `T`
  telemetry/companion records;
- exact supported legacy galaxy sets (absent, 28 or 90) expanding to 200, `D`
  knowledge, `V GALAXY_KNOWLEDGE 1` and `Z` completeness terminator;
- bounded versioned `L`/`A` administrative receipts, chat and UniNet events.

Loading validates before disk migration. Migrations hash credentials, tag eligible
DEVELOPMENT legacy state, initialize compatible economy/ships and expand galaxy/
knowledge through existing paths. Unknown record types, unsupported versions,
wrong environments, inconsistent counts and corrupt/truncated required state fail
closed. No unknown required record is silently discarded. Existing optional legacy
extensions retain their established defaults; a future format requires explicit
versioning and tests, not another permissive parser.

With normal startup options, `--validate-state` takes the exclusive stable lock,
validates TLS/environment/storage and existing state, reports JSON validity and
whether migration is needed, then exits without migration or a network listener.
It requires an existing file. It may write ephemeral status/lock sidecars; it does
not replace the authoritative file or grant TEST mutation privileges.

## Backup-before-upgrade and bounded upgrade workflow

Rehearse the exact candidate package in PRIVATE TEST first with synthetic/invited
accounts, independent credentials and representative data. Validate installation,
readiness, binary/legacy TLS, scoped companion reads/denials, gameplay persistence,
maintenance/drain, checkpoint, shutdown, restart and supported migrations.
A TEST artifact is not automatically promotable to Production; repeat acceptance
for the intended STABLE artifact. Record candidate identities and package hashes.

For Production, the operator-controlled workflow is:

1. Verify the candidate package/hash/manifest and retain the known-good binary and
   configuration. Inspect it in a separate prefix before replacing binaries.
2. Request M7 maintenance/drain and stop the old process. Confirm exit/readiness;
   acquire no new gameplay work. The stable `.lock` inode must remain in place.
3. Run the candidate offline against the explicit existing Production target:

   ```sh
   helion_server 4242 /var/lib/helion/commander.db --environment production \
     --cert /etc/helion/server.crt --key /etc/helion/server.key \
     --operations PRODUCTION PREPARE_UPGRADE pre-021 --reason approved-upgrade
   ```

   This requires protected configuration, explicit Production, an immutable unused
   backup ID, reason and exclusive live lock. It validates the source, creates and
   verifies an M7 backup of its exact original bytes **before migration**, performs
   supported migrations, checks durability and strictly revalidates the resulting
   state. Failure is nonzero; never proceed on uncertainty. Reusing an ID fails
   rather than overwriting a recovery point. Success JSON binds the source digest,
   backup ID and acting build ID. The durable operations journal records intent,
   result and acting build ID, independently of TEST audit.
4. Install/activate the approved package/binary. Packages own no player data,
   credentials or preferences. Retain previous artifacts and the verified backup.
5. Start with the protected existing configuration; verify the M7 health probe,
   expected build/environment identity, authorized login and representative saved
   progression. Watch logs and refuse promotion if readiness or validation fails.

Normal Production startup also creates and verifies an immutable `pre-migration-*`
backup before migrating an existing compatible file, so omission of PREPARE_UPGRADE
cannot bypass that safeguard. A fresh explicit missing file may initialize a new
universe; upgrade validation requires existing state. Backup failure prevents
migration. Existing corrupt state never becomes a fresh universe. Migration or
post-migration startup/readiness failure is not deployment success.

Binary/package replacement and state migration are **not one atomic transaction**.
A pre-rename save failure preserves the source; a post-rename durability failure
may have committed state and must not trigger memory-only rollback or a success
report. Keep backups and inspect current state. The existing backup wrapper's
`native-0.2.0` and TEST wrapper's `native-0.2.0-admin1` are compatibility tags,
not the M8 application release version. They remain readable by the tested M7
loader. Backups contain sensitive account/token verifiers, and legacy backups may
contain original credentials; protect them as private recovery material.

## Recovery and restore boundary

If installation fails before state migration, retain/reinstall the known-good
artifact and original configuration; verify it before reopening. If startup fails
after migration, keep the service stopped and retain the verified backup, failed
state, logs, binary identities and configuration. The tested M7 0.2.0 -> M8 0.2.1
path uses the same persistence compatibility; the known M7 binary is rehearsed
loading M8-written state without conversion. This is a tested same-format binary
rollback, not permission to downgrade arbitrary future releases or reverse migrate.
Revalidate compatibility and readiness before reopening; stop on any failure.

Production restore remains **offline verification/planning only** through
VERIFY_BACKUP/PLAN_RESTORE. M8 adds no destructive restore command, live-state
replacement or arbitrary path write. Production credential revocation/receipt
semantics during a historical-state replacement need further reviewed design;
manual destructive replacement is not automated or declared accepted here. Preserve
the current recovery point and use the conservative separate recovery rehearsal
in the M7 guide. TEST recovery remains the existing authenticated, audited,
companion-revoking M6 workflow. Never restore TEST snapshots into Production.

Uninstall removes package-owned files, not external `/var/lib/helion`, TLS,
configuration or client preferences. Reinstall is rehearsed, but dependency
compatibility with every Linux distribution, remote backup replication, retention,
certificate renewal, signed distribution, unattended rollout and zero-downtime
upgrades are not supplied.

## Acceptance tooling

CTest includes offline identity/inspection and installed-prefix release rehearsal.
`native/tests/release_package_acceptance.py` audits actual TGZ/DEB contents before
extraction, binds installed identities, checks dependencies/secrets/modes, exercises
combined helpers and installed split clients, then runs the TEST-first migration,
backup/failure and lifecycle rehearsal. It accepts a trusted previous M7 server
package for a real prior-package upgrade and same-format binary rollback. The
same permission audit applies to that artifact: early M7 packages made with a
`0002` development umask have group-writable directories and are rejected. M8's
rehearsal rebuilds the exact verified M7 source `f02d9e38c32f22678664e55946bef3b700ac5d3c`
with `umask 022`, preserving the original package, and records the rebuilt
artifact hash. No audit exception or prior runtime change is introduced.
Rollback compares every ordered record and the exact multiset of companion token
records (M7 uses unordered token iteration), then runs the current strict loader. Its
required JSON report records artifact hashes/build IDs and actual rehearsal counts.
Test-only fault injection forces a migration commit failure after verified backup;
no injection code or environment hook is linked into or shipped with the service.

Run the artifact gate from the candidate source checkout, supplying the actual
combined/server/client TGZ and DEB artifacts and, optionally, a TEST-server TGZ:

```sh
python3 native/tests/release_package_acceptance.py \
  --artifact /path/to/helion-server-0.2.1-STABLE-Linux-x86_64.tar.gz \
  --artifact /path/to/helion-client-0.2.1-STABLE-Linux-x86_64.tar.gz \
  --artifact /path/to/helion-0.2.1-STABLE-Linux-x86_64.tar.gz \
  --artifact /path/to/helion-server_0.2.1_amd64.deb \
  --artifact /path/to/helion-client_0.2.1_amd64.deb \
  --artifact /path/to/helion-0.2.1-STABLE-Linux-x86_64.deb \
  --previous-server-package /path/to/helion-server-0.2.0-Linux-x86_64.tar.gz \
  --report /path/to/release-acceptance.json
```

This local test harness requires Python 3, OpenSSL, a C++17 compiler for its
temporary fault library, and the host runtime dependencies. DEB checks use
`dpkg-deb`; systemd syntax is checked with `systemd-analyze` when installed.
It uses temporary synthetic state, not an operator-supplied live data directory.
Retain its JSON report with approved artifact hashes and regression evidence.
