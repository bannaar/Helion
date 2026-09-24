# Helion

Helion is a native, server-authoritative, persistent online science-fiction
game. Its implementation and release baseline is the C++17 TLS server and
SDL2/OpenGL client under `native/`, designed for Linux systems with OpenGL
2.1-era hardware, including older Intel integrated graphics. The Three.js /
TanStack application under `src/` is retained as a legacy design reference; it
is not a peer authority path or the production game architecture.

The native runtime now provides a playable Kepler career arc, procedural
ship/station/asteroid assets, account creation/login, durable commander
profiles, shared chat, and mandatory verified TLS transport. Linux packages
and a desktop local-play launcher are available. Separate server/client packages
support a [Compaq 610 + EliteBook 8460p setup](docs/compaq610-server-elitebook-client.md).
See [installation](docs/native-install.md) and
[project state](docs/PROJECT_STATE.md). Native development follows World Bible
v0.10 and grows incrementally from the validated server-authoritative slice.

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
| `src/` | Legacy browser reference; not the authoritative release runtime |
| `native/server/` | Standalone POSIX TCP server |
| `native/client/` | Standalone SDL2/OpenGL client |
| `native/README.md` | Native quick start and protocol reference |
| `docs/installation.md` | Complete source, package, server, and client installation guide |
| `docs/native-server.md` | Server installation and production configuration |
| `docs/native-client.md` | Client installation, launcher usage, and graphics troubleshooting |
| `migrations/` | Browser database migrations |
| `CMakeLists.txt` | Native build entry point |
| `startup.sh` | Browser preview startup contract |

## Legacy browser-reference development

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

This path exists for historical reference and isolated UI experimentation.
Its local or database-backed state is not part of the canonical native
universe.

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
SHIPYARD LIST
SHIPYARD OWNED
SHIPYARD BUY TITAN_MULE
LAUNCH
INPUT 1 0 0
FLIGHT
MINE
FIRE
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

The native game currently provides account persistence, the Kepler career
slice, mining, station markets, missions, upgrades and modules, combat and
recovery, reputation, GalNet, live contacts, persistent owned ships, distinct
hull performance, and authoritative Kepler/Cinder shipyards. Later World Bible
systems remain future native work unless the project-state documentation says
otherwise.

## License and contribution

This repository is evolving incrementally. Keep changes focused, preserve the
OpenGL 2.1 compatibility target, and run the native CMake validation relevant
to every native change. Run browser checks when modifying the legacy `src/`
reference, but do not use browser behavior as evidence of native completion.
