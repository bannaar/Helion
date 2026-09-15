# Helion standalone server

This guide installs and configures the standalone Linux server. The server is a
small C++17 POSIX TCP process with no database service dependency.

## 1. Requirements

Supported toolchain:

- Linux with POSIX sockets and pthreads
- C++17 compiler (GCC 9+ or Clang 10+ recommended)
- CMake 3.16+
- `make` or Ninja
- OpenSSL development headers and `libcrypto` (1.1.1 or newer) for scrypt

Debian/Ubuntu:

```sh
sudo apt update
sudo apt install build-essential cmake libssl-dev
```

The server does not require SDL2 or an X11/Wayland session. Those are client
dependencies only.

## 2. Build

From the repository root:

```sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --target helion_server --parallel
```

For a server-only host without SDL2 or OpenGL, configure an isolated build:

```sh
cmake -S . -B build-native-server -DHELION_BUILD_CLIENT=OFF
cmake --build build-native-server --parallel
ctest --test-dir build-native-server --output-on-failure
```

`HELION_BUILD_SERVER` and `HELION_BUILD_TESTS` default to `ON`. Use
`-DHELION_BUILD_TESTS=OFF` if the protocol test binary is not wanted. The
server-only configure path does not search for SDL2 or OpenGL. Configuration
fails if OpenSSL Crypto is unavailable; there is no plaintext password fallback.

The executable is written to:

```text
build-native/native/helion_server
```

To build both server and client together, use `cmake --build build-native
--parallel`.

## 3. Start and configure

The command accepts an optional TCP port and persistence path, plus explicit
bind and connection-limit options:

```sh
./build-native/native/helion_server [port] [data-file] [--bind IPv4-address] [--max-clients 1..1024]
```

Defaults:

| Setting | Default |
| --- | --- |
| Listen port | `4242` |
| Data file | `helion-server.db` in the current directory |
| Bind address | `127.0.0.1` only |
| Concurrent clients | 32 (configurable, maximum 1024) |
| Socket idle/read timeout | 30 seconds; incomplete lines also have a 30-second deadline |
| Socket write timeout | 5 seconds |
| Authentication requests | At most 10 per connection; disconnect after 5 failed credentials |
| Maximum request line | 4096 bytes |

Examples:

```sh
# Local development
./build-native/native/helion_server 4242 ./var/helion-server.db

# Dedicated host, still listening only on loopback
./build-native/native/helion_server 4242 /var/lib/helion/helion-server.db
```

`--bind` accepts a numeric IPv4 address. Any non-loopback address, including
`0.0.0.0`, requires this explicit option and prints a warning. The protocol
does not yet have native TLS. For remote access, keep the server on loopback
and carry TCP through an encrypted tunnel such as SSH or WireGuard. Do not
expose its port directly to the public internet, even with a firewall rule.

Create the data directory before startup and restrict it to the service user:

```sh
sudo install -d -o helion -g helion -m 750 /var/lib/helion
sudo chown helion:helion ./build-native/native/helion_server
```

The server writes updates to a uniquely named temporary file with mode `0600`
and atomically replaces the main file. Existing regular data files are
restricted to owner-only mode before loading. Keep backups protected as well.
On startup, legacy plaintext profile records are hashed before the server
accepts clients. If migration or saving fails, startup aborts rather than
serving plaintext credentials; preserve the original data file for recovery.
Legacy `P` records become hashed `H` records, so even a legacy password that
begins with `$` is migrated unambiguously.

## 4. systemd example

Create `/etc/systemd/system/helion-server.service`:

```ini
[Unit]
Description=Helion standalone space server
After=network.target

[Service]
Type=simple
User=helion
Group=helion
WorkingDirectory=/opt/helion
ExecStart=/opt/helion/build-native/native/helion_server 4242 /var/lib/helion/helion-server.db
Restart=on-failure
RestartSec=3
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/helion

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

The default loopback bind needs no inbound firewall opening. If using an
encrypted tunnel that requires an explicit non-loopback bind, restrict its
source addresses:

```sh
sudo ufw allow from 203.0.113.0/24 to any port 4242 proto tcp
```

Do not open the Helion TCP port globally while transport encryption is absent.

## 6. Protocol smoke test

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

Expected profile output includes the initial `sidewinder`, faction, credits,
and experience fields.

## 7. Persistence and security limitations

The line-oriented data file contains profile records and chat messages. New
passwords must be 12–128 bytes; migrated legacy passwords may be shorter and
continue to work. Passwords use OpenSSL scrypt with unique random salts and
constant-time comparison. The server does not log password values. Treat data
files and any pre-migration backups as sensitive and never commit them.

Before public deployment, add:

1. Native TLS and server identity verification; use an encrypted tunnel until then.
2. Cross-connection/IP rate limiting and durable account lockout; current limits are per connection.
3. Input normalization and stronger identity rules.
4. Versioned migrations for non-credential profile records.
5. A server-authoritative gameplay tick and validated state updates.

## 8. Logs and shutdown

The reference server logs persistence failures to stderr. Under systemd:

```sh
journalctl -u helion-server -f
```

Stop cleanly with `Ctrl-C` in the foreground or:

```sh
sudo systemctl stop helion-server
```
