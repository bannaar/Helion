# Helion native server and OpenGL client

This standalone C++17 pair uses TLS over POSIX TCP sockets. The client opens an SDL2
window with an OpenGL 2.1-compatible fixed-function renderer (no shaders,
VAOs, or modern OpenGL requirements). The client now includes a playable
top-down mining sector with server-authoritative flight and saved payouts.
Networking and the command parser remain
usable from a terminal with `--terminal`.

## Build

From the repository root:

```sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --parallel
```

`HELION_BUILD_SERVER`, `HELION_BUILD_CLIENT`, and `HELION_BUILD_TESTS` are
independent CMake options and default to `ON`. For a headless server without
SDL2 or OpenGL, configure with `-DHELION_BUILD_CLIENT=OFF`. To build only the
client, use `-DHELION_BUILD_SERVER=OFF`. Client builds require SDL2 and OpenGL;
server and protocol-test builds do not. Both network executables require
OpenSSL SSL/Crypto; integration tests require Python 3 and the OpenSSL CLI. Run `ctest --test-dir build-native
--output-on-failure` after building.

## Install and run

See [the installation guide](../docs/native-install.md) for Debian packages,
portable archives, desktop launch, and dedicated-server setup. Build packages
with `cmake --build build-native --target package`, or install with
`cmake --install build-native --prefix /your/prefix`. The combined install
provides `helion-play`, which creates a private local TLS certificate, starts
the server, opens the client, and stops the server when the client exits.

For manual development, first generate a local certificate:

```sh
native/scripts/helion-dev-cert /tmp/helion-local-tls
./build-native/native/helion_server 4242 helion-server.db \
  --cert /tmp/helion-local-tls/server.crt --key /tmp/helion-local-tls/server.key
# In a separate terminal:
./build-native/native/helion_client 127.0.0.1 4242 --ca /tmp/helion-local-tls/server.crt
```
This opens the cockpit and commander console. Create an account with
`/create username password Display Name` or use `/login username password`.
Successful authentication opens the flight controls automatically.
For the retained command-line mode, use
`./build-native/native/helion_client 127.0.0.1 4242 --ca /tmp/helion-local-tls/server.crt --terminal`.

The client commands are `/create username password display`, `/login username
password`, `/chat message`, `/profile`, `/state`, `/contacts`, `/buy food 1`,
`/sell parts 1`, `/mission`, `/accept`, `/turnin`, `/upgrade engine|hull`,
`/repair`, `/launch`, `/flight`, `/input thrust turn brake`, `/mine`, `/dock`,
and `/quit`.
The optional server data-file stores commander profiles and GalNet
messages durably and is replaced atomically after each write.

Each data file has a persistent owner-only `.lock` sidecar. Only one server
can own that save at a time, including when using different ports. Ownership
is released on shutdown or process exit; leave the sidecar in place across
restarts and never delete it while a server may be running. Keep the data
directory private to the server user.

## Mining career loop

1. Press **L** to launch from Kepler station.
2. Hold **W** to thrust, **A/D** to turn left/right, and **S** to brake.
   Arrow keys are aliases. The view stays north-up and follows your ship.
3. Approach a teal ore asteroid within **85 m**, slow below **35 m/s**, and
   press **E** to extract one unit. The extractor recharges in **1.25 s**;
   the hold carries **8 units**. Asteroids are renewable in this first sector.
4. Return to the amber base marker on the radar. Within **85 m** at less
   than **35 m/s**, press **F** to dock and sell your cargo for **60 credits
   and 5 XP per unit**. A full trip pays **480 credits and 40 XP**.
5. **Enter** opens the command/chat console; **Escape** closes it. Press **R**
   while docked to repair a damaged hull. Opening
   the console or losing focus brakes the ship. Credentials are masked in
   the graphical input and command log.

The server advances flight at a fixed 60 Hz and validates every mining and
docking action. Clients send only bounded control inputs, never positions or
rewards. Inputs expire after half a second. Credits, XP, upgrades, flight
position, hull damage, and unsold cargo are persisted after authoritative
updates, so a restart resumes the commander's saved in-flight state. Asteroid
impacts damage the hull based on collision speed; a disabled ship is recovered
at its station with cargo lost and must be repaired before launch. Hull
upgrades increase maximum integrity and repairs cost credits per missing point.
Each commander
currently flies a separate instance of the same sector; shared asteroid
depletion and full player ship replication are not yet implemented. Contact
records and deterministic NPC traffic are exposed through multiplayer
snapshots, and GalNet chat remains shared.

## Native assets and render check

The built-in asset set includes a faceted Sidewinder with animated exhaust,
rotating Kepler station, seven ore asteroids, parallax stars, a mining beam
and impact particles, radar, and an original bitmap cockpit font. Assets are
procedural geometry in `client/render.cpp`; there are no downloaded textures,
font packages, shaders, or runtime asset-path dependencies. The 960x600
cockpit letterboxes on differently proportioned windows.

For a headless renderer check on SDL installations with the offscreen driver:

```sh
SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 \
  ./build-native/native/helion_client --render-check native-mining.bmp
```

This writes flight and console BMP captures, using a rendering fixture without
connecting to a server. CTest separately exercises actual server flight,
mining, docking, input validation, and persisted rewards after restart.

## Line protocol

The server sends `WELCOME Helion/2` and `INFO` lines on connect. Protocol version
2 is defined in `shared/protocol.h`; the client rejects a different greeting.
Requests are one
newline-terminated line: `CREATE username password display`, `LOGIN username
password`, `CHAT message`, `PROFILE`, `STATE`, `CONTACTS`, `BUY`, `SELL`,
`MISSION`, `ACCEPT`, `TURNIN`, `UPGRADE`, `REPAIR`, `LAUNCH`, `INPUT`, `FLIGHT`,
`MINE`, `DOCK`, or `QUIT`. Responses are newline-terminated
`OK`, `ERR`, `PROFILE`, `STATE`, `TRANSACTION`, and `CHAT` records. The server stores salted
scrypt password hashes, migrates existing plaintext profile records on startup,
and creates owner-only data files. New passwords must be 12–128 bytes. The connection requires TLS 1.2 or newer. Clients verify the certificate
chain, expiration, and DNS/IP Subject Alternative Name using OS trust roots
or an explicit `--ca` file. There is no plaintext fallback or insecure bypass.

The shared line decoder accepts at most 4096 bytes before LF (including an
optional CR), buffers partial reads, and emits each complete line in order.
Oversized lines are discarded through the next LF and receive one
`ERR line-too-long`; control-byte or malformed requests receive
`ERR malformed-message`. Processing resumes at the next line. The maximum
chat payload is 512 bytes.

`INPUT thrust turn brake` accepts exactly `0|1`, `-1|0|1`, and `0|1`.
Positive turn is left. `LAUNCH`, `FLIGHT`, `MINE`, `DOCK`, and `REPAIR` take no
arguments; all gameplay commands require authentication. Each gameplay command
returns a flight snapshot, except authentication/persistence failures. A
successful `DOCK` also emits a server-calculated transaction record between
the legacy result and the snapshot:

```text
FLIGHT x y vx vy yaw docked cargo credits experience cooldown food parts station hull maxHull

TRANSACTION DOCK_SALE station=0 quantity=1 unit-price=60 credits=60 experience=5
```

The `OK DOCKED earned=...` line remains for protocol compatibility. The
structured transaction is informational: the server derives the quantity from
authoritative cargo and computes credits and XP; clients cannot supply or
override either value. Cargo removal and the reward are committed together,
and a persistence failure rolls the profile back without sending a transaction
record.

Coordinates are metres on an XY plane, +Y north, yaw in radians with zero
pointing north. `docked` is `0|1`; cooldown is seconds. Existing version-1
commands and profile record formats remain supported; new mission, upgrade,
and flight fields are appended so legacy saves remain readable. `INFO
commands=` advertises the added gameplay commands.
