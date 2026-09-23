# Helion native server and OpenGL client

This standalone C++17 pair uses TLS over POSIX TCP sockets. The client opens an SDL2
window with an OpenGL compatibility-profile fixed-function gameplay renderer
(OpenGL 3.0 compatibility first, OpenGL 2.1 fallback). The OpenGL 3.3 core
renderer presents live Kepler flight state with procedural VAO/VBO geometry,
batched bitmap text, and a compact cockpit/HUD; legacy remains the default
gameplay path. The client includes a playable top-down mining sector with
server-authoritative flight and saved payouts.
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
`/repair`, `/refuel`, `/outfit LIST|BUY|FIT|REMOVE`, `/launch`, `/flight`,
`/input thrust turn brake`, `/mine`, `/dock`, and `/quit`.
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

Flight consumes fuel only while thrusting. The starter Sidewinder carries
100 fuel, and engine upgrades increase capacity by 20 per level. Return to a
station and press **T** or enter `REFUEL` to fill the tank; the station charges
2 credits per fuel unit at Kepler and 3 at Cinder. Fuel is server-owned and
saved with the ship.

The initial outfitting catalogue has mining, engine, defense, and weapon slots.
Every commander starts with basic mining, standard engine, and standard hull
modules. `OUTFIT LIST` inspects the loadout; `OUTFIT BUY module-id`, `OUTFIT
FIT module-id`, and `OUTFIT REMOVE slot` are docked, server-priced operations.
`mining-mk2` shortens the extractor cooldown, `engine-efficient` reduces fuel
consumption, and `hull-plating` adds 25 maximum hull. `pulse-laser` is an
owned/fittable weapon with a 240 m range, 25 damage, and a one-second server
cooldown.

The server advances flight at a fixed 60 Hz and validates every mining and
docking action. Clients send only bounded control inputs, never positions or
rewards. Inputs expire after half a second. Credits, XP, upgrades, flight
position, hull damage, and unsold cargo are persisted after authoritative
updates, so a restart resumes the commander's saved in-flight state. Asteroid
impacts damage the hull based on collision speed. Hostile `RAIDER-N` NPCs
pursue nearby launched ships and fire within 185 m. A disabled ship is
recovered at its station with cargo lost, half hull, and a full safe fuel tank;
repair remains available before the next launch. Hull
upgrades increase maximum integrity and repairs cost credits per missing point.
Each commander
currently flies a separate instance of the same sector; shared asteroid
depletion and full player ship replication are not yet implemented. Contact
records and deterministic NPC traffic are exposed through multiplayer
snapshots, and GalNet chat remains shared.

## Graphics diagnostics

`helion_client --graphics-info` reports the actual SDL context, GL vendor,
renderer, version, GLSL version when available, fallback state, and a
conservative hardware/software classification. The current target is Intel HD
Graphics 3000 (`8086:0116`) with kernel driver `i915`, Mesa 25.2.8, userspace
driver `crocus`, direct rendering, and OpenGL 3.3 compatibility. Use
`--renderer auto`, `--renderer legacy`, or `--renderer core` to select a path.
`auto` fully initializes hardware OpenGL 3.3 core first and recreates a clean
legacy context if context, functions, shaders, atlas, or GPU resources fail.
`core` requires a true OpenGL 3.3 core profile and fails clearly if
the context or a required SDL-loaded function is unavailable; it never falls
back to fixed-function rendering. It enters the real server/client loop and
draws the player, stations, asteroids, haulers, commanders, Red Wake contacts,
target reticles, mining/fire beams, and geometric hull/fuel/cargo/cooldown/
docking/recovery indicators with `#version 330 core` shaders. Its built-in 5x7
font and cockpit show commander, mission, standings, target, combat, mining,
docking, recovery, and recent GalNet/action messages.

Validate the core hardware path with:

```sh
SDL_VIDEODRIVER=x11 ./build-native/native/helion_client --renderer core \
  --render-check /tmp/helion-core-hardware.bmp
```

For live core gameplay, supply the normal host, port, and CA arguments:

```sh
SDL_VIDEODRIVER=x11 ./build-native/native/helion_client 127.0.0.1 4242 \
  --ca /path/to/server.crt --renderer core
```

For deterministic core presentation states, use `--render-state` with one of
`normal`, `mining`, `target`, `combat`, `docked`, `destroyed`, or `galnet`, and
optionally set a bounded window size with `--render-size WIDTH HEIGHT`:

```sh
SDL_VIDEODRIVER=x11 ./build-native/native/helion_client --renderer core \
  --render-check /tmp/helion-core-galnet.bmp --render-state galnet \
  --render-size 960 600
```

Do not use llvmpipe or a forced software/offscreen run as evidence of HD 3000
hardware support. The fixed-function offscreen check remains a separate
diagnostic and the Intel X11 check must report the Mesa Intel renderer. A
Batch 9 representative 960x600 Intel HD 3000 states measured about 0.87–2.25
ms, four draw calls, 726–819 world vertices, 72–144 UI vertices, one text draw
call, and one atlas texture. The exact counts vary with screen and message;
the diagnostics also separate glyph count, glyph vertices, float components,
VBO bytes, atlas dimensions/bytes, and CPU/render timing. These are bounded
diagnostics rather than a fixed performance target.

For an accelerated X11 validation, omit software-forcing and offscreen
variables:

```sh
SDL_VIDEODRIVER=x11 ./build-native/native/helion_client --graphics-info --renderer legacy
SDL_VIDEODRIVER=x11 ./build-native/native/helion_client --renderer legacy --render-check /tmp/helion-hardware.bmp
```

The offscreen render check remains a diagnostic only and does not prove GPU
acceleration.

Core station and account interfaces are selected with F1–F8 after secure
authentication: station, profile, options/telemetry, market, contracts,
outfitting, GalNet, and graphics diagnostics. F9 reopens the onboarding guide.
Up/Down selects, Enter confirms,
and Escape backs out. Market quantity uses `+`/`-` and buy/sell uses `B`/`S`;
outfitting uses `B`/`F`/`R`. Core account fields are bounded and mask
passwords. Mouse movement highlights controls and left-click activates one
control event at a time; wheel/PageUp/PageDown scroll bounded GalNet content.
These controls submit the existing server commands and do not make
prices, rewards, repairs, fuel, mission state, or module ownership client
authoritative.

The release acceptance harness runs one guarded loopback-only SDL/core client
through all three career stages and reconnects after a server restart. It
captures onboarding, mining, supply, combat, completion, and restored-profile
frames. The dedicated [Compaq 610 + EliteBook 8460p deployment guide](../docs/compaq610-server-elitebook-client.md)
covers split packages, TLS identities, service setup, and LAN/Tailscale.
Local package and verified-TLS tests do not substitute for installing and
testing the service on the actual Compaq and connecting over the intended LAN.
The corrected packaged Intel HD 3000 core client passed a 600.015-second
graphical activity soak (35,904 frames, 68 cycles, no OpenGL/SDL/render errors
or disconnects) after completing and reconnecting the full Kepler career.

The core render check accepts these UI fixtures in addition to flight states:
`account`, `station`, `market`, `mission`, `outfit`, `profile`, `galnet-ui`,
`options`, `graphics`, and `error`.

`CORE-PERF` reports units explicitly: `glyphs`, `text-vertices`,
`text-indices`, `text-glyph-vertices` (six per glyph), `text-components` (eight
floats per uploaded vertex), `text-bytes`, `text-draw-calls`, `textures`,
`atlas=WIDTHxHEIGHT`, `atlas-bytes`, `cpu-build-ms`, `cpu-ui-build-ms`,
`cpu-text-build-ms`, and `render-ms`. The non-indexed text invariant is
`text-vertices == glyphs * 6`; indices remain zero. Each glyph samples one
cell of the single nearest-filtered atlas, which is uploaded once per context
and not rebuilt per frame.

## Native assets and render check

The built-in asset set includes a faceted Sidewinder with animated exhaust,
rotating Kepler station, seven ore asteroids, parallax stars, a mining beam
and impact particles, radar, and an original bitmap cockpit font. Assets are
procedural geometry in `client/render.cpp`; there are no downloaded textures,
font packages, shaders, or runtime asset-path dependencies. The 960x600
cockpit letterboxes on differently proportioned windows.

For a headless renderer check on SDL installations with the offscreen driver:

```sh
SDL_VIDEODRIVER=offscreen \
  ./build-native/native/helion_client --renderer legacy --render-check native-mining.bmp
```

This writes flight and console BMP captures, using a rendering fixture without
connecting to a server. It is a diagnostic only; do not combine
`LIBGL_ALWAYS_SOFTWARE=1` with an explicitly selected hardware device on Mesa,
and use the X11 check above to prove Intel hardware acceleration. CTest
separately exercises actual server flight, mining, docking, input validation,
and persisted rewards after restart.

## Line protocol

The server sends `WELCOME Helion/2` and `INFO` lines on connect. Protocol version
2 is defined in `shared/protocol.h`; the client rejects a different greeting.
Requests are one
newline-terminated line: `CREATE username password display`, `LOGIN username
password`, `CHAT message`, `PROFILE`, `STATE`, `CONTACTS`, `BUY`, `SELL`,
`MISSION`, `ACCEPT`, `TURNIN`, `UPGRADE`, `REPAIR`, `REFUEL`, `OUTFIT`, `LAUNCH`,
`INPUT`, `FLIGHT`, `MINE`, `DOCK`, `FIRE target-id`, `RECOVER`, `GALNET`, or
`QUIT`. Responses are newline-terminated `OK`, `ERR`, `PROFILE`, `STATE`,
`GALNET`, `FUEL`,
`LOADOUT`, `COMBAT`, `TRANSACTION`, and `CHAT` records. The server stores salted
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
Positive turn is left. `LAUNCH`, `FLIGHT`, `MINE`, `DOCK`, `RECOVER`, and
`REPAIR` take no
arguments; all gameplay commands require authentication. Flight commands return
a flight snapshot and an additive fuel frame, except authentication/persistence
failures. A
successful `DOCK` also emits a server-calculated transaction record between
the legacy result and the snapshot:

```text
FLIGHT x y vx vy yaw docked cargo credits experience cooldown food parts station hull maxHull

TRANSACTION DOCK_SALE station=0 quantity=1 unit-price=60 credits=60 experience=5

FUEL 86.40 100.00
TRANSACTION REFUEL station=0 amount=13.60 unit-price=2 credits=28 fuel=100.00 max-fuel=100.00
LOADOUT owned=mining-basic,engine-basic,hull-standard fitted-mining=mining-basic fitted-engine=engine-basic fitted-defense=hull-standard fitted-weapon=none
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
commands=` advertises the added gameplay commands. Profile records from before
the fuel/loadout extension load with full starter fuel and the basic starter
modules; newer fuel, loadout, salvage, combat-generation, and destruction
fields are appended to the existing record. The active weapon cooldown is
also checkpointed so reconnecting cannot bypass a shot delay. Combat target entities are
runtime-only; the generation/defeat marker prevents replaying a reward after a
restart. A successful `FIRE` emits server-calculated `COMBAT HIT`, `COMBAT
DESTROYED`, and `TRANSACTION COMBAT_REWARD` records. `COMBAT STATUS` reports
destruction and weapon cooldown. Hostile `CONTACT` records include current and
maximum hull plus an optional canonical affiliation ID/display token. `PROFILE`
also includes the organization registry and bounded reputation summary. First
Ore uses `corp.orion` as issuer and `authority.kepler` as jurisdiction;
reputation changes are server-calculated. `GALNET` returns persisted events
generated by actual mission, launch, and combat transitions. A destroyed player
must use `RECOVER`; ordinary flight, mining,
docking, and firing are rejected until then.

The core UI includes the complete tested Kepler career path: dismiss the
first-run help, accept and turn in First Ore, buy two parts at Cinder and
deliver them to Kepler, then accept Red Wake Response and defeat the
authoritative `RAIDER-N` with a fitted pulse laser. The client displays the
issuer, jurisdiction, objective, progress, rewards, standings, combat
feedback, GalNet events, and persistent established-pilot completion state.
The journey is available through keyboard or layout-derived mouse controls;
normal play never requires raw commands. The append-only career record remains
compatible with older profiles, and failed saves roll back the whole stage
transaction.
