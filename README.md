# Helion

Helion is an in-development standalone space game built around a native C++ server and SDL2/OpenGL client. The earlier browser application remains a gameplay prototype and design reference; current development is focused on the independent native client/server architecture.

## Current native foundation

The current recovery branch includes:

- independent server-only, client-only, and test builds;
- a versioned `Helion/1` line protocol with bounded messages;
- a headless POSIX TCP server;
- an SDL2 client using an OpenGL 2.1-compatible renderer;
- persistent commander profiles and chat;
- salted OpenSSL scrypt password hashes;
- automatic migration of legacy plaintext credentials;
- loopback binding by default;
- connection limits, authentication limits, and socket timeouts;
- protocol, security, and server integration tests.

The native project compiles and its current automated tests pass, but it is still a foundation. The graphical login experience, server-authoritative flight simulation, economy, missions, and combat remain under development.

## Quick start on Linux Mint or Ubuntu

Install all server and client dependencies:

```bash
sudo apt update
sudo apt install \
  build-essential \
  cmake \
  ninja-build \
  libssl-dev \
  libsdl2-dev \
  libgl1-mesa-dev \
  mesa-utils
```

Configure, build, and test everything from the repository root:

```bash
cmake -S . -B build-native -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
```

Start the server in one terminal:

```bash
mkdir -p ~/.local/share/helion-server
chmod 700 ~/.local/share/helion-server
./build-native/native/helion_server \
  4242 \
  "$HOME/.local/share/helion-server/helion-server.db"
```

Connect from another terminal:

```bash
./build-native/native/helion_client 127.0.0.1 4242 --terminal
```

Create and inspect a commander profile:

```text
/create pilot choose-a-long-password Cmdr Pilot
/login pilot choose-a-long-password
/profile
/state
/quit
```

New passwords must be 12–128 bytes.

## Separate builds

Build the headless server without SDL2 or OpenGL:

```bash
cmake -S . -B build-server -G Ninja \
  -DHELION_BUILD_CLIENT=OFF
cmake --build build-server --parallel
ctest --test-dir build-server --output-on-failure
```

Build the client without the server or OpenSSL server dependency:

```bash
cmake -S . -B build-client -G Ninja \
  -DHELION_BUILD_SERVER=OFF
cmake --build build-client --parallel
ctest --test-dir build-client --output-on-failure
```

The three CMake switches are:

| Option | Default | Purpose |
| --- | --- | --- |
| `HELION_BUILD_SERVER` | `ON` | Build the headless server |
| `HELION_BUILD_CLIENT` | `ON` | Build the SDL2/OpenGL client |
| `HELION_BUILD_TESTS` | `ON` | Build and register native tests |

## Network security

The server binds to `127.0.0.1` by default. Passwords are protected at rest with salted scrypt hashes, but the current TCP transport does not yet include native TLS.

For another computer, keep the server on loopback and use an encrypted SSH or Tailscale SSH tunnel. Do not expose port `4242` directly to the public internet.

Example client-side tunnel:

```bash
ssh -N -L 14242:127.0.0.1:4242 USER@TAILSCALE_IP
```

Then connect Helion to the local tunnel endpoint:

```bash
./build-native/native/helion_client 127.0.0.1 14242
```

## Documentation

- [Complete installation guide](docs/installation.md)
- [Native server guide](docs/native-server.md)
- [Native client guide](docs/native-client.md)
- [Native implementation overview](native/README.md)

## Development direction

The next networking milestone is native TLS with server identity verification. Later milestones add a polished native login interface, authoritative movement and physics, docking and travel, trading and missions, followed by combat, factions, scanning, mining, and salvage.

Keep generated builds, server databases, credentials, certificates, and private keys out of Git.
