# Helion standalone server

This guide installs and configures the standalone Linux server. The server is a
small C++17 POSIX TCP process with no database service dependency.

## 1. Requirements

Supported toolchain:

- Linux with POSIX sockets and pthreads
- C++17 compiler (GCC 9+ or Clang 10+ recommended)
- CMake 3.16+
- `make` or Ninja

Debian/Ubuntu:

```sh
sudo apt update
sudo apt install build-essential cmake
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
server-only configure path does not search for SDL2 or OpenGL.

The executable is written to:

```text
build-native/native/helion_server
```

To build both server and client together, use `cmake --build build-native
--parallel`.

## 3. Start and configure

The command accepts an optional TCP port and persistence path:

```sh
./build-native/native/helion_server [port] [data-file]
```

Defaults:

| Setting | Default |
| --- | --- |
| Listen port | `4242` |
| Data file | `helion-server.db` in the current directory |
| Bind address | all local interfaces |
| Maximum request line | 4096 bytes |

Examples:

```sh
# Local development
./build-native/native/helion_server 4242 ./var/helion-server.db

# Dedicated host
./build-native/native/helion_server 4242 /var/lib/helion/helion-server.db
```

Create the data directory before startup and restrict it to the service user:

```sh
sudo install -d -o helion -g helion -m 750 /var/lib/helion
sudo chown helion:helion ./build-native/native/helion_server
```

The server writes updates to `<data-file>.tmp` and atomically replaces the
main file. Keep both files on the same filesystem and include the data file in
your backup plan.

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

Only expose the configured TCP port to trusted clients:

```sh
sudo ufw allow from 203.0.113.0/24 to any port 4242 proto tcp
```

Do not open the port globally while the reference authentication protocol is
still plaintext.

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

The line-oriented data file contains profile records and chat messages. The
current reference server stores passwords directly in that file. Treat the file
as a secret, use filesystem permissions such as `0600`, and never commit it to
version control.

Before public deployment, add:

1. Salted password hashing (Argon2id, scrypt, or bcrypt).
2. TLS or a private tunnel.
3. Login rate limiting and account lockout.
4. Input normalization and stronger identity rules.
5. Versioned migrations for profile records.
6. A server-authoritative gameplay tick and validated state updates.

## 8. Logs and shutdown

The reference server logs persistence failures to stderr. Under systemd:

```sh
journalctl -u helion-server -f
```

Stop cleanly with `Ctrl-C` in the foreground or:

```sh
sudo systemctl stop helion-server
```
