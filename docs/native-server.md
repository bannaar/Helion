# Helion standalone server

Current lore uses UniNet, GSC, Ironstar Forge and a generically named starter
shuttle. Literal protocol/save identifiers (`GALNET`, `credits`, `SIDEWINDER` /
`sidewinder`, `TITAN_FORGE`) and existing executable display labels remain
compatible. See the [canon reconciliation](development/HELION_CANON_AND_DOC_RECONCILIATION_v0.21.md).

This guide installs and configures the standalone Linux server. The server is a
small C++17 POSIX TCP process with no database service dependency.

## 1. Requirements

Supported toolchain:

- Linux with POSIX sockets and pthreads
- C++17 compiler (GCC 9+ or Clang 10+ recommended)
- CMake 3.16+
- `make` or Ninja
- OpenSSL development headers, `libssl` and `libcrypto` (1.1.1 or newer) for TLS and scrypt

Debian/Ubuntu:

```sh
sudo apt update
sudo apt install build-essential cmake libssl-dev
```

The server does not require SDL2, OpenGL, or an X11/Wayland session. Those are
client dependencies only. The `helion-server` split package is headless; see
the [Compaq 610 + EliteBook 8460p guide](compaq610-server-elitebook-client.md)
for package and tarball installation.

## 2. Build

From the repository root:

```sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --target helion_server -j2
```

For a server-only host without SDL2 or OpenGL, configure an isolated build:

```sh
cmake -S . -B build-native-server -DHELION_BUILD_CLIENT=OFF
cmake --build build-native-server -j2
ctest --test-dir build-native-server --output-on-failure
```

`HELION_BUILD_SERVER` and `HELION_BUILD_TESTS` default to `ON`. Use
`-DHELION_BUILD_TESTS=OFF` if the protocol test binary is not wanted. The
server-only configure path does not search for SDL2 or OpenGL. Configuration
fails if OpenSSL SSL/Crypto is unavailable; there is no unencrypted transport
or plaintext password fallback. Tests also need Python 3 and the OpenSSL CLI.

The executable is written to:

```text
build-native/native/helion_server
```

To build both server and client together on the older target machines, use
`cmake --build build-native -j2`.

## 3. Start and configure

The command accepts an optional TCP port and persistence path, plus explicit
bind and connection-limit options:

```sh
./build-native/native/helion_server [port] [data-file] [--environment development|test|production] [--bind IPv4-address] [--max-clients 1..1024] --cert certificate.pem --key private-key.pem
```

Defaults:

| Setting | Default |
| --- | --- |
| Listen port | `4242` |
| Data file | `helion-server.db` in the current directory |
| Runtime environment | `development` for backward compatibility |
| Bind address | `127.0.0.1` only |
| Transport | TLS 1.2+; certificate and key required |
| Handshake deadline | 5 seconds; counts against the client limit |
| Concurrent clients | 32 (configurable, maximum 1024) |
| Socket idle/read timeout | 30 seconds; incomplete lines also have a 30-second deadline |
| Socket write timeout | 5 seconds |
| Authentication requests | At most 10 per connection; disconnect after 5 failed credentials |
| Maximum request line | 4096 bytes |

Examples:

```sh
# Local development
./build-native/native/helion_server 4242 ./var/helion-server.db \
  --environment development \
  --cert ./tls/server.crt --key ./tls/server.key

# Operator-managed production identity, still listening only on loopback
./build-native/native/helion_server 4242 /var/lib/helion/helion-server.db \
  --environment production \
  --cert /etc/helion/fullchain.pem --key /etc/helion/private-key.pem
```

### Environment identity and persistence isolation

Every newly written persistence file begins with a stable environment identity:
`development`, `test`, or `production`. The server refuses to open a file whose
identity differs from `--environment`. Existing untagged saves remain compatible
with the default `development` environment and are atomically tagged during
their established migration path; TEST and production deliberately reject
untagged state. This prevents an operator from silently mounting copied writable
state from another environment.

TEST and Production now claim separate owner-only (`0700`) state directories
with an environment/instance/economy marker. A fresh filename cannot bypass a
claimed directory's identity. TEST requires protected colocated TLS/access files
and a server-owned invitation roster. See [private TEST administration](private-test-admin.md)
for provisioning, roles, canonical GSC terminology, commands, audit, offline
snapshot/restore and reset safety. Current local-play and hardware guides retain
`development` explicitly. M7 service lifecycle, maintenance, health, verified backups and conservative restore planning are documented in [Production operations](production-operations.md).

`--bind` accepts a numeric IPv4 address. The default remains loopback-only;
remote servers must opt into a non-loopback bind. All connections require
TLS 1.2 or newer, including loopback. Supply a PEM certificate chain and its
matching PEM private key; missing or mismatched files prevent startup.
Certificates are loaded at startup, so restart after renewal.

Remote certificates must include the DNS name or IP used by clients in their
Subject Alternative Name. Clients use operating-system CA roots or an
explicit `--ca` trust file; invalid identities are rejected before credentials
are sent. The server authenticates players with passwords, not client
certificates. Handshakes are bounded to five seconds, consume client slots,
and are interrupted by server shutdown. At capacity, connections are closed
before the handshake, with no plaintext error response.

For local development, `native/scripts/helion-dev-cert /private/tls-directory`
creates a one-year loopback certificate; pass its `server.crt` to the client
with `--ca`. The private key stays on the server. For an installed local game,
`helion-play` handles this setup automatically. See `docs/native-install.md`.

Create the data directory before startup and restrict it to the service user:

```sh
sudo install -d -o helion -g helion -m 700 /var/lib/helion
```

The server writes updates to a uniquely named temporary file with mode `0600`
and atomically replaces the main file. Existing regular data files are
restricted to owner-only mode before loading. Keep backups protected as well.
On startup, legacy plaintext profile records are hashed before the server
accepts clients. If migration or saving fails, startup aborts rather than
serving plaintext credentials; preserve the original data file for recovery.
Legacy `P` records become hashed `H` records, so even a legacy password that
begins with `$` is migrated unambiguously.

Before loading or migrating, the server takes an exclusive non-blocking lock
on `<data-file>.lock`. A second server using the same save exits with
`persistence file is already in use by another server`, even on a different
port. The sidecar remains owner-only and must stay in place: do not remove it
while any server may be running. The operating system releases ownership on
shutdown or process exit, so restarting needs no lock-file cleanup. Symlink,
hard-linked, foreign-owned, and non-regular lock files are rejected. This is
an advisory lock for cooperating Helion processes on a local filesystem;
keep the directory writable only by the service user and stop the server
before restoring or manually editing a save.

## 4. systemd example

The split server package includes a hardened example at
`/usr/share/doc/helion-server/examples/helion-server.service`. Copy it to
`/etc/systemd/system/helion-server.service`, edit `--bind` to the intended
LAN/Tailscale address, and verify the certificate and key paths before
enabling it. A source-install example is:

The automated release check restarts the packaged server process headlessly
and verifies saved progression. It does not start systemd on the physical
Compaq; inspect and test the unit on that host before enabling it permanently.

```ini
[Unit]
Description=Helion standalone space server
After=network.target

[Service]
Type=simple
User=helion
Group=helion
WorkingDirectory=/opt/helion
UMask=0077
ExecStart=/opt/helion/build-native/native/helion_server 4242 /var/lib/helion/helion-server.db --environment production --bind 127.0.0.1 --cert /etc/helion/fullchain.pem --key /etc/helion/private-key.pem
Restart=on-failure
RestartSec=3
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/helion
LimitNOFILE=4096

[Install]
WantedBy=multi-user.target
```

Enable it:

```sh
sudo systemctl daemon-reload
sudo systemctl enable --now helion-server
sudo systemctl status helion-server
```

## 5. Firewall

The default loopback bind needs no inbound firewall opening. For a remote server, bind explicitly and restrict inbound access to the
intended clients or network:

```sh
sudo ufw allow from 203.0.113.0/24 to any port 4242 proto tcp
```

TLS protects transport; it does not replace admission controls or per-IP
rate limits. Keep the server restricted while those limits are developed.

## 6. Protocol smoke test

The graphical client negotiates bounded binary version 1 over TLS by default.
Use `--legacy-protocol` explicitly for an older text-only server; failed binary
negotiation never silently downgrades. Terminal mode remains bounded text over
the same authenticated TLS connection. See the [M4 transport contract](development/BATCH14_PROTOCOL.md)
for framing, versioning, request IDs, bounds and compatibility behavior.

From a machine with the client binary:

```sh
./build-native/native/helion_client server.example 4242 --terminal
```

Then enter:

```text
/create pilot choose-a-local-password Cmdr Pilot
/login pilot choose-a-local-password
/profile
/state
/quit
```

Expected profile output includes the legacy `sidewinder`, faction, `credits` (GSC),
and experience fields.

Docked clients may use `SHIPYARD LIST`, `SHIPYARD OWNED`, `SHIPYARD BUY
<hull-id>`, and `SHIPYARD SWITCH <instance-id>`. The server validates local
inventory, pad support, ownership, GSC, and physical ship location before
atomically persisting a transaction. New `S` records store owned ship
instances; legacy profiles without them migrate deterministically to one
starter shuttle (`SIDEWINDER`) and retain the existing `H` record as an active-ship projection.

## 7. Companion credentials

A normal password-authenticated player session can issue a separate companion
credential with `COMPANION ISSUE`. The plaintext bearer token is returned once;
the persistence file stores only its SHA-256 verifier, owner, public ID, expiry,
and fixed initial `profile.read` scope.

Companion tokens expire after 30 days. Each account may have at most eight
active tokens. Use `COMPANION LIST` to inspect active token IDs/expiry and
`COMPANION REVOKE id` from a normal player session to revoke one.

A new TLS connection may use `COMPANION AUTH token` instead of a password.
A companion-authenticated connection may read `PROFILE` but receives
`ERR scope-denied` for account creation, password login, token management,
chat, trading, flight, missions, upgrades, repairs, and other gameplay
mutations. The server never upgrades a companion token into full player
authority.

The same scope checks apply to negotiated binary text requests and typed input.
Companion sessions receive no player chat broadcasts. Profile reads revalidate
the token on every request, so expiry and durable revocation affect existing
connections as well as new authentication attempts. Public `STATE` diagnostics
and `QUIT` remain available. Negotiate binary transport before authentication.

This is the server-side foundation for HELION Commander pairing. The eventual
launcher/QR/device-code experience can wrap it without storing the game
password on the companion device. See `docs/companion-auth.md`.

## 8. Persistence and security limitations

The line-oriented data file begins with an `E` environment record, a versioned
bounded administrative receipt ledger (`L`/`A`), and then
contains galaxy/knowledge, economy, profile, owned-ship, companion-token,
UniNet, and chat records. Economic telemetry and companion tokens retain their
legacy `T` tags with distinct, strictly validated two- and six-column shapes. New passwords must be
12–128 bytes; migrated legacy passwords may be shorter and continue to work.
Passwords use OpenSSL scrypt with unique random salts and constant-time
comparison. The server does not log password values. Treat data
files and any pre-migration backups as sensitive and never commit them.

Before public deployment, add:

1. Certificate renewal automation and operational monitoring. TLS and server
   identity verification are implemented.
2. Cross-connection/IP rate limiting and durable account lockout; current limits are per connection.
3. Input normalization and stronger identity rules.
4. Versioned migrations for non-credential profile records.
5. Cross-sector simulation and richer world persistence. The current mining
   sector uses a 60 Hz authoritative flight tick, persists in-flight position
   and unsold cargo, and validates extraction and docking rewards. Multiplayer
   contact records and deterministic NPC traffic are available to clients, but
   full shared ship replication remains future work.

## 9. Logs and shutdown

The server emits structured operational events plus legacy diagnostic errors to stderr. Production shutdown now drains, checkpoints and creates a verified backup, with nonzero exit on failure. See [Production operations](production-operations.md) for health, metrics, maintenance signals and offline backup/restore verification. Under systemd:

```sh
journalctl -u helion-server -f
```

Stop cleanly with `Ctrl-C` in the foreground or:

```sh
sudo systemctl stop helion-server
```

## 10. Mining gameplay

The native client now flies the starter shuttle in Kepler Reach. Authenticated
`LAUNCH`, `INPUT`, `FLIGHT`, `MINE`, `DOCK`, and `REPAIR` commands implement the
mining and hull-care loop. The server advances positions at 60 Hz, resolves
asteroid collisions into authoritative hull damage, bounds the sector to 1200
m, expires controls after 0.5 seconds, and validates proximity, speed,
extractor cooldown, and cargo capacity.

Docking sells ore at 60 GSC and 5 XP per unit. Rewards are acknowledged
only after the profile snapshot is saved; a failed write restores cargo,
flight state, GSC, and XP so the player can retry. Hull upgrades increase
maximum integrity. A disabled ship is recovered at its station with cargo
lost; `/repair` restores integrity for a GSC cost before launch. Legacy
profile records remain readable, while new mission, upgrade, flight, and hull
fields are appended. Flight positions, hull damage, and unsold cargo persist
across restart. Flight sectors are currently per-commander instances, while
chat remains shared.

See `native/README.md` for controls, command arguments, and snapshot fields.
