# Helion standalone client

The standalone client is an SDL2 application with a fixed-function gameplay
renderer and an OpenGL 3.3 core gameplay renderer. It is
designed to run on Linux machines with OpenGL 2.1-class hardware, including
Intel HD 3000-era integrated graphics.

## 1. Requirements

Install the compiler/build tools and client libraries:

```sh
sudo apt update
sudo apt install build-essential cmake libsdl2-dev libgl1-mesa-dev libssl-dev
```

The client needs a graphical session (X11 or Wayland through SDL2). A server
does not need a display.

## 2. Build

From the repository root:

```sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --target helion_client --parallel
```

For a client-only build, add `-DHELION_BUILD_SERVER=OFF` when configuring.
`HELION_BUILD_CLIENT` defaults to `ON` and is the only option that requires
SDL2 and OpenGL. Both client and server require OpenSSL SSL/Crypto.
`HELION_BUILD_TESTS` defaults to `ON`; run
`ctest --test-dir build-native --output-on-failure` after building all targets.

The executable is:

```text
build-native/native/helion_client
```

## 3. Connect

Graphical mode:

```sh
./build-native/native/helion_client [host] [port] [--ca trusted-ca.pem]
```

Examples:

```sh
# Local server
./build-native/native/helion_client 127.0.0.1 4242 --ca /path/to/local-tls/server.crt

# Remote server
./build-native/native/helion_client helion.example.org 4242
```

The graphical cockpit opens with a commander console. Sign in or create an
account; the flight display opens automatically on success. Press Enter to
reopen the console and Escape to return to flight. Use:

```text
/create username password display
/login username password
/chat hello pilots
/profile
/galnet
/state
/refuel
/outfit LIST
/fire target-id
/recover
/launch
/flight
/mine
/dock
/quit
```

The client expects the versioned `WELCOME Helion/2` greeting. The shared
protocol decoder buffers split network reads, separates multiple lines in one
read, and rejects oversized or malformed lines. The maximum line size is 4096
bytes before LF. Version 1 retains the existing command names.

For headless diagnostics, use terminal mode:

```sh
./build-native/native/helion_client 127.0.0.1 4242 --ca /path/to/local-tls/server.crt --terminal
```

## 4. Flight and mining

Press **L** to launch, **W** to thrust, **A/D** to turn, and **S** to brake.
Arrow keys also work. Approach a teal asteroid to within 85 m at no more than
35 m/s and press **E** to mine. Wait 1.25 seconds between extractions; cargo
capacity is eight units. Thrust consumes fuel from the authoritative 100-unit
starter tank. Use the radar's amber base marker to return to Kepler station,
then press **F** within 85 m at low speed to dock and sell. Each ore unit earns
60 credits and 5 XP, saved by the server. After a
successful dock, the client shows the authoritative sale quantity, credit
reward, and XP reward in the activity line; rejected actions remain visible
with their server-provided reason.

While docked, press **T** or enter `/refuel` to fill the tank at the station's
server-calculated price. Press **U** to inspect the loadout, or use the console
commands `/outfit BUY module-id`, `/outfit FIT module-id`, and
`/outfit REMOVE slot`. The initial modules are basic mining, standard engine,
and standard hull; the outfitting preview includes a faster mining extractor,
fuel-efficient engine, reinforced hull plating, and a combat-preview pulse
laser.

Hostile contacts appear in red on the sector view and are selected
automatically; press **Tab** to cycle hostile contacts and **Space** to fire
the selected target. The cockpit shows target range and hull, laser cooldown,
incoming damage, combat rewards, and server rejection reasons. The pulse laser
requires the fitted weapon module and is limited by server range, cooldown, and
damage rules. Contacts identify the hostile's canonical affiliation, currently
Red Wake. If the ship is destroyed, cargo is lost and **R** requests
safe-station recovery; repair and refit remain docked actions. The header shows
Kepler Authority and Orion standing labels, while mission/combat feedback shows
numeric reputation changes.

The First Ore contract is displayed as an Orion Extraction Group contract in
Kepler Authority jurisdiction. `/galnet` reads the server-owned deterministic
event stream, including contract availability, Red Wake activity, defeated
raiders, and the resulting Kepler security response.

Flight is top-down with a north-up camera. Opening the console or losing
window focus applies the brake. The server also stops stale controls after
half a second. The client smooths received positions for display; the server
owns movement, collision, cargo, and rewards.

Credits, XP, upgrades, market cargo, mined ore, position, velocity, and dock
state survive server restarts. Each commander still has separate asteroid
depletion, while GalNet chat and live contact positions are shared.

## 5. Graphics capability and context selection

The fixed-function renderer first requests an OpenGL 3.0 compatibility
context. If that attempt fails or produces an unusable/core context, the
client destroys it and retries with OpenGL 2.1 compatibility. It never selects
OpenGL 3.3 core for this renderer; a future core-profile renderer would need
shaders and a separate migration.

The target was verified with Intel HD Graphics 3000 / PCI `8086:0116`, kernel
driver `i915`, Mesa 25.2.8, and Mesa userspace driver `crocus`. `glxinfo -B`
reported direct rendering, OpenGL core 3.3, compatibility 3.3, and OpenGL ES
3.0. On the accelerated X11 desktop, the native runtime reported:

```text
GRAPHICS requested=3.0-compatibility actual=3.3-compatibility first-compat=yes fallback-21=no renderer-class=hardware vendor=Intel renderer=Mesa Intel(R) HD Graphics 3000 (SNB GT2) version=3.3 (Compatibility Profile) Mesa 25.2.8-0ubuntu0.24.04.2 glsl=3.30
```

Use `--graphics-info` to inspect runtime capabilities without connecting to a
server:

```sh
SDL_VIDEODRIVER=x11 ./build-native/native/helion_client --graphics-info
```

The report includes the requested and actual SDL context, profile, GL vendor,
renderer, version, GLSL version, fallback state, and conservative renderer
classification. Known software renderers such as llvmpipe and softpipe are
never labeled hardware.

The client still uses double buffering, a resizable 960×600 window, and
fixed-function projection and colored primitives. It does not require shaders,
vertex buffer objects, VAOs, or OpenGL 3+ features. OpenGL 2.1 fallback remains
fully playable.

### Experimental core renderer

Renderer selection is explicit:

```sh
./build-native/native/helion_client --renderer auto
./build-native/native/helion_client --renderer legacy
SDL_VIDEODRIVER=x11 ./build-native/native/helion_client --renderer core
```

`auto` probes for a core context for diagnostics but deliberately keeps normal
gameplay on the proven legacy path. `legacy` always uses the 3.0 compatibility
then 2.1 compatibility policy. `core` requests OpenGL 3.3 core, rejects a
compatibility context, loads only the modern functions it needs through SDL,
and enters the core gameplay renderer. Render-check mode uses a deterministic
ship/station/asteroid/contact fixture; normal mode consumes the live Kepler
snapshot. It never falls back silently to the legacy renderer. An unknown
renderer value is rejected.

The core scene uses a `#version 330 core` position/color shader, a VAO and VBO,
and a small model/view/projection transform. It is a compatibility and driver
validation path, not the gameplay renderer: text, textures, models, and the
full cockpit remain legacy work. Run a captured core frame on the verified
desktop driver with:

```sh
SDL_VIDEODRIVER=x11 ./build-native/native/helion_client \
  --renderer core --render-check /tmp/helion-core-hardware.bmp
```

The command must report the Intel HD 3000 hardware renderer, a 3.3 core
context, successful shader setup, and a nonempty BMP. Software renderers such
as llvmpipe are reported as software and do not count as hardware validation.
The core path now consumes the live client presentation state. Launch it with
the same TLS connection arguments as the legacy client:

```sh
SDL_VIDEODRIVER=x11 ./build-native/native/helion_client 127.0.0.1 4242 \
  --ca /path/to/server.crt --renderer core
```

Core mode renders the player, Kepler and Cinder, asteroids, hauler traffic,
other commanders, Red Wake contacts, target reticles, mining and weapon beams,
and destroyed-state markers. Its geometric HUD shows hull, fuel, cargo,
weapon cooldown, mining/fire state, target direction/range, docking, and
recovery state. The same server-authoritative controls and commands remain in
use; detailed console text and the cockpit font remain legacy-only. `auto`
still selects legacy gameplay until core visual parity and stability are
demonstrated.

The renderer-neutral snapshot copies presentation state from `View` without
owning it, mutating gameplay, sending commands, or retaining OpenGL objects.
Static grid geometry is uploaded once; bounded dynamic world and HUD geometry
reuse VAO/VBO resources. The verified HD 3000 X11 check reports approximately
0.10 ms for the captured 960×600 frame, three draw calls, 100 static vertices,
and 963 dynamic vertices. This is a short diagnostic measurement, not a
performance target.

OpenGL 3.3 core requires shaders because fixed-function calls are unavailable.
On a Linux desktop, inspect the active renderer with:

```sh
glxinfo -B
```

If `glxinfo` is missing:

```sh
sudo apt install mesa-utils
```

The renderer line should show the Intel driver or another hardware renderer.
Forcing software rendering is useful only for troubleshooting and does not
prove HD 3000 compatibility:

```sh
LIBGL_ALWAYS_SOFTWARE=1 ./build-native/native/helion_client 127.0.0.1 4242 --ca /path/to/local-tls/server.crt
```

For the actual desktop hardware check, omit both `LIBGL_ALWAYS_SOFTWARE` and
the offscreen driver:

```sh
SDL_VIDEODRIVER=x11 ./build-native/native/helion_client --graphics-info
SDL_VIDEODRIVER=x11 ./build-native/native/helion_client --render-check /tmp/helion-hardware.bmp
```

The existing offscreen render check remains a useful deterministic diagnostic,
but it is not evidence of GPU acceleration.

## 6. Desktop launcher

Create `~/.local/share/applications/helion-client.desktop` and adjust the
`Exec` path:

```ini
[Desktop Entry]
Type=Application
Name=Helion Native Client
Comment=Connect to a Helion standalone server
Exec=helion-play
Terminal=false
Categories=Game;
```

The installed combined package already supplies this desktop entry. For
a manual remote shortcut, set Exec to `helion_client HOST PORT` plus `--ca`
when using a private CA. The launcher is a desktop shortcut. Account commands are entered in the in-game
console; the graphical console masks passwords while typing and in its log.

## 7. Troubleshooting

### `unable to connect`

Confirm the server is running and the port is reachable:

```sh
./build-native/native/helion_client 127.0.0.1 4242 --ca /path/to/local-tls/server.crt --terminal
nc -vz 127.0.0.1 4242
```

Check the server firewall and systemd status if the server is remote.

### `SDL initialization failed`

Run from a graphical desktop session and inspect:

```sh
echo "$DISPLAY"
echo "$WAYLAND_DISPLAY"
```

SSH users may need X forwarding, but low-latency gameplay is better on a local
desktop session.

### `OpenGL window creation failed`

Check the driver with `glxinfo -B`. The client requires a working OpenGL
context, not just the presence of `libGL.so`. Try the software-rendering
command above to distinguish a driver issue from an application issue.

### Visual verification

The client contains its own bitmap cockpit font and procedural ship, station,
asteroid, and mining-effect geometry. No font or texture download is required.
The 960x600 display letterboxes when the window has a different aspect ratio.
For a headless render check with SDL's offscreen driver:

```sh
SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 \
  ./build-native/native/helion_client --render-check native-mining.bmp
```

This saves the flight display and masked command console as BMP files. It
uses a render fixture and does not modify any commander data.

## 8. Security note

All network traffic uses TLS 1.2 or newer. The client verifies the certificate
chain, expiration, and DNS/IP Subject Alternative Name before sending any
commands. Default trust comes from the operating system; `--ca` selects a
private CA or explicitly trusted local certificate. There is no insecure
bypass or plaintext fallback. Unknown roots, wrong names, and expired
certificates fail the connection with a TLS error.

The server stores salted scrypt password hashes. The graphical console masks
credentials, but terminal input is echoed by the terminal. Use the graphical
client for normal play. See `docs/native-install.md` for installation,
local-play certificate setup, and dedicated-server instructions.
