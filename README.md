# Helion

Helion is a low-resource space career game prototype with two runtimes:

- **Browser reference game** — a playable Three.js/TanStack application used for
  rapid gameplay and UI development.
- **Standalone native foundation** — a C++17 TCP server and SDL2/OpenGL client
  designed for Linux systems with OpenGL 2.1-era hardware, including older
  Intel integrated graphics.

The native runtime now provides a playable Kepler career arc, procedural
ship/station/asteroid assets, account creation/login, durable commander
profiles, shared chat, and mandatory verified TLS transport. Linux packages
and a desktop local-play launcher are available. Separate server/client packages
support a [Compaq 610 + EliteBook 8460p setup](docs/compaq610-server-elitebook-client.md).
See [installation](docs/native-install.md)
and [project state](docs/PROJECT_STATE.md). It is not yet a complete port of
the browser simulation.

## Gameplay concept renders

These presentation renders visualize native gameplay systems represented in
the current development build. They are concept renders, not pixel-perfect
screenshots of the SDL/OpenGL client.

| Ore extraction | Station return and trading | Live multiplayer contacts |
| --- | --- | --- |
| ![A Helion commander extracting ore from an asteroid](docs/renders/mining-extraction.png) | ![A Helion ship returning to a station to trade and refit](docs/renders/station-return.png) | ![Helion commander and NPC ships sharing a live sector](docs/renders/multiplayer-contacts.png) |

## Repository layout

| Path | Purpose |
| --- | --- |
| `src/` | Browser game, simulation, UI, audio, and persistence adapters |
| `native/server/` | Standalone POSIX TCP server |
| `native/client/` | Standalone SDL2/OpenGL client |
| `native/README.md` | Native quick start and protocol reference |
| `docs/installation.md` | Complete source, package, server, and client installation guide |
| `docs/native-server.md` | Server installation and production configuration |
| `docs/native-client.md` | Client installation, launcher usage, and graphics troubleshooting |
| `migrations/` | Browser database migrations |
| `CMakeLists.txt` | Native build entry point |
| `startup.sh` | Browser preview startup contract |

## Quick start: browser game

Requirements: Node.js 22 or newer and npm.

```sh
npm install
npm run dev
```

Open the preview served by the development environment. Useful checks:

```sh
npm run typecheck
npm run test
npm run build
```

The browser version uses local save storage by default. Authenticated profile,
market, news, and GalNet persistence use the existing database adapters when
configured.

## Quick start: standalone native runtime

On Debian/Ubuntu, install the native build dependencies:

```sh
sudo apt install build-essential cmake libsdl2-dev libgl1-mesa-dev libssl-dev python3 openssl
```

Build from the repository root:

```sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --parallel
```

Start the server:

```sh
native/scripts/helion-dev-cert /tmp/helion-local-tls
./build-native/native/helion_server 4242 helion-server.db \
  --cert /tmp/helion-local-tls/server.crt --key /tmp/helion-local-tls/server.key
```

Connect with the graphical client:

```sh
./build-native/native/helion_client 127.0.0.1 4242 --ca /tmp/helion-local-tls/server.crt
```

For protocol diagnostics without a graphical session:

```sh
./build-native/native/helion_client 127.0.0.1 4242 --ca /tmp/helion-local-tls/server.crt --terminal
```

See the [native server guide](docs/native-server.md) and
[native client guide](docs/native-client.md) for service setup, firewall
configuration, data-file handling, and troubleshooting.

## Native protocol snapshot

Requests are newline-terminated:

```text
CREATE username password display
LOGIN username password
CHAT message
PROFILE
STATE
LAUNCH
INPUT 1 0 0
FLIGHT
MINE
DOCK
QUIT
```

The native server stores salted scrypt password hashes and requires TLS 1.2+
for all connections. Clients verify certificate trust and server identity;
there is no plaintext fallback. Live player and NPC contacts are replicated,
while shared asteroid depletion, player-to-player collision, and
cross-connection/IP rate limiting remain future work.

## Graphics compatibility

The client supports both an OpenGL 3.3 core shader renderer and a fixed-function
compatibility renderer with an OpenGL 2.1 fallback. A 960×600 window is created
by default and can be resized. Startup diagnostics identify hardware or
software rendering and the selected context.

## Current gameplay scope

The browser reference currently includes low-poly ships and stations, multiple
ship classes and factions, outfitting, trading, cargo, missions, mining,
salvage, scanning, combat, police/pirate/alien contacts, GalNet news/chat,
procedural audio, TTS, and local commander saves. Native gameplay is being
implemented incrementally against the same concepts.

## License and contribution

This repository is an evolving prototype. Keep changes focused, preserve the
OpenGL 2.1 compatibility target for native client work, and run the relevant
browser type checks or native CMake build before submitting changes.
