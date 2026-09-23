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

The legacy renderer uses double buffering, a resizable 960×600 window, and
fixed-function projection. Its OpenGL 2.1 fallback remains playable. The core
renderer has its own OpenGL 3.3 shader, buffer, and glyph-atlas path.

### Core and legacy renderers

Renderer selection is explicit:

```sh
./build-native/native/helion_client --renderer auto
./build-native/native/helion_client --renderer legacy
SDL_VIDEODRIVER=x11 ./build-native/native/helion_client --renderer core
```

`auto` initializes a hardware OpenGL 3.3 core renderer first. If any stage
fails, it destroys the partial context and creates a clean legacy context.
The reason and selected renderer appear in `GRAPHICS SELECT` startup output.
`legacy` uses the 3.0 compatibility then 2.1 compatibility policy. `core`
requests OpenGL 3.3 core, rejects a
compatibility context, loads only the modern functions it needs through SDL,
and enters the core gameplay renderer. Explicit core failure exits nonzero;
it never falls back. Render-check mode uses a deterministic
ship/station/asteroid/contact fixture; normal mode consumes the live Kepler
snapshot. It never falls back silently to the legacy renderer. An unknown
renderer value is rejected.

The core scene uses a `#version 330 core` position/color shader, a VAO and VBO,
and a small model/view/projection transform. It now also has a compact built-in
5x7 bitmap font, one atlas texture, batched glyph quads, and a bounded cockpit
presentation. It is still an incremental gameplay renderer rather than a
complete visual replacement. Run a captured core frame on the verified
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
and destroyed-state markers. Its cockpit shows commander/system context,
credits, XP, hull, fuel, cargo, weapon readiness, mission issuer and
jurisdiction, organization standings, target affiliation/range/hull, and
docked, mining, combat, and recovery states. A bounded recent-message panel
shows GalNet headlines, mission, mining, combat, reward, docking, and error
feedback from the existing client log. The same server-authoritative controls
and commands remain in use.

The renderer-neutral snapshot copies presentation state from `View` without
owning it, mutating gameplay, sending commands, or retaining OpenGL objects.
World geometry, geometric HUD indicators, text, and recent messages are kept
as separate presentation layers. Static grid geometry is uploaded once;
bounded dynamic world, HUD, and text geometry reuse VAO/VBO resources. The
built-in font supports the current Latin command/status alphabet and maps
unsupported characters or bounded UTF-8 sequences to `?`; it is intentionally
not a Unicode shaping or font-layout system. Batch 10 uses one textured
six-vertex quad per visible glyph from a single 96x48 padded RGBA atlas. On the
verified HD 3000 X11 path, representative 960x600 states take about
0.20–0.38 ms per captured frame, with four draw calls, 726–819 world vertices,
96–192 UI vertices, 248–364 glyphs, one text draw call, and 18,432 atlas bytes.
Text VBO uploads are 1,488–2,184 vertices / 47,616–69,888 bytes; CPU build and
render timings are reported separately in `CORE-PERF`. These are bounded
diagnostic measurements, not performance targets.

For deterministic core presentation checks, select a fixture state and size:

```sh
SDL_VIDEODRIVER=x11 ./build-native/native/helion_client --renderer core \
  --render-check /tmp/helion-core-galnet.bmp --render-state galnet \
  --render-size 960 600
```

Available states include `normal`, `mining`, `target`, `combat`, `docked`,
`destroyed`, `galnet` (flight news feed), `galnet-ui` (docked news panel),
`account`, `station`, `market`, `mission`, `outfit`,
`profile`, `options`, `graphics`, `error`, `help`, and `completion`. The TLS
protocol harness remains a separate authority check. The graphical acceptance
harness drives one real SDL/core client through account creation, onboarding,
three contracts, mining, market, outfitting, combat, completion, restart, and
free play; it captures actual client frames at thirteen checkpoints. Its
guarded mode accepts only loopback TLS and an explicit test flag, and injects
ordinary SDL input events through the production UI handler.

Core mode also provides station and account interfaces through the same text
and geometric pipelines. `F1` opens station services when docked; `F2` opens
profile/reputation, `F3` toggles telemetry and opens options, `F4` opens the
market, `F5` the First Ore mission, `F6` outfitting, `F7` GalNet, and `F8`
graphics diagnostics. `Up`/`Down` selects, `Enter` confirms, and `Escape`
returns to the previous station/flight view. Market uses `B`/`S` and `+`/`-`
for bounded unit quantities. Outfitting uses `B`/`F`/`R` for buy, fit, and
remove. Account input remains in the existing bounded console; passwords are
masked in the core presentation and never copied into logs. Mouse movement
highlights controls and left-click activates one control event at a time; wheel
and PageUp/PageDown scroll bounded GalNet content. Hit regions use the same
letterboxed layout as rendering, so resizing preserves alignment. Disabled or
pending transaction controls cannot submit commands.

The Batch 10 verification audit corrected live input routing: menu Enter no
longer opens the console, flight actions remain reachable in core mode, mouse
actions preserve the selected commodity/module, and input hit testing uses
fresh connection/ship presentation flags. Flight-only bars no longer cover
station text. The atlas UVs map complete texels to complete bitmap pixels;
newlines and spaces never address an atlas cell.

`CORE-PERF` frame times measure CPU build and OpenGL submission with diagnostic
error checks; they do not include swap/vsync or establish GPU completion time.
The bounded live soak reports actual client frame counts, average/worst
renderer time, draw/vertex/upload bounds, and OpenGL errors separately from
process RSS samples.
The cockpit's recent-activity feed filters routine protocol framing and
contact/profile polling; the diagnostic console still retains raw responses.
On the Intel HD 3000, the corrected packaged client completed a 600.015-second
X11/core soak with 35,904 frames and 68 full station-to-flight activity cycles.
Measured renderer time averaged 8.254 ms and peaked at 12.842 ms; this live
path includes driver synchronization not represented by the short render-check
fixture. Bounds were four draw calls, 834 world vertices, 276 UI vertices,
5,364 text vertices, 171,648 uploaded text bytes, and one font atlas. OpenGL,
render, SDL, and disconnect error counts were all zero. RSS sampled after
startup rose from 91,616 to 92,228 KiB and then stayed stable; RSS stability
alone does not prove leak freedom.

Three-frame 960×600 X11 hardware fixtures measured 0.222 ms in normal flight,
0.346 ms mining, 0.242 ms combat, 0.252 ms station, 0.234 ms market, 0.225 ms
mission, 0.315 ms docked GalNet, and 0.286 ms completion. All used four draw
calls and one 96×48, 18,432-byte atlas. Across these states, text used
1,854–2,760 vertices (59,328–88,320 bytes); world geometry used 726–819
vertices and UI geometry 42–138 vertices. These are bounded diagnostics, not
GPU frame-completion or cross-machine performance guarantees.

### Core/legacy vertical-slice parity audit

`PARITY` means the same normal action and feedback are available; `CORE_EQUIVALENT`
means the core presentation differs but supports the same decision; and
`LEGACY_ONLY_NONCRITICAL` means a decorative or supplemental legacy view has no
pixel-for-pixel core counterpart. There are no known
`CORE_MISSING_CRITICAL` actions in the Kepler career path. This is functional
parity, not visual identity.

| Area | Classification | Core-mode evidence/qualification |
| --- | --- | --- |
| Account/login, TLS status | PARITY | Bounded fields, masked password, real TLS graphical acceptance |
| Onboarding, help | CORE_EQUIVALENT | Core help overlay and dismissal/reopen controls |
| Station, contracts, market, cargo | CORE_EQUIVALENT | Mouse/keyboard controls submit existing server commands |
| Outfitting, repair/refuel | CORE_EQUIVALENT | Docked controls retain server-side validation |
| Profile, reputation, GalNet | CORE_EQUIVALENT | Bounded core text views and persisted server records |
| Options, graphics diagnostics | CORE_EQUIVALENT | Core menu plus actual context/renderer report |
| Flight, contacts, targeting | CORE_EQUIVALENT | Live server snapshot and selected-target reticle |
| Legacy decorative cockpit/radar styling | LEGACY_ONLY_NONCRITICAL | Core uses geometric HUD and spatial contact indicators |
| Mining, combat, damage | CORE_EQUIVALENT | Live beam, weapon, hull, and feedback state |
| Destruction, recovery | CORE_EQUIVALENT | Disabled-state overlay and recover action |
| Career completion, post-completion free play | CORE_EQUIVALENT | Persistent completion view and resumed flight |
| Keyboard, mouse, resize | CORE_EQUIVALENT | SDL event path, layout hit regions, bounded fixtures |

The guarded graphical acceptance runs one actual core client process through
career completion and a restarted client through persisted free play. It is
not a claim that the Compaq/EliteBook LAN pair or every legacy cosmetic effect
has been physically tested.
The recent-activity panel is player-facing, but a low-level combat-status
response can still appear in the small bottom status line; that is a cosmetic
diagnostic leak, not a missing action or server-authority bypass.

For two-machine installation and certificate SAN setup, use the
[Compaq 610 + EliteBook 8460p guide](compaq610-server-elitebook-client.md).

The station UI presents existing server data only: station context, credits,
hull, fuel, cargo, shared station prices, First Ore issuer/jurisdiction and
state, loadout ownership/fitting, profile progression, GalNet events, and
graphics diagnostics. It queues the existing `PROFILE`, `MISSION`, `BUY`,
`SELL`, `OUTFIT`, `REPAIR`, `REFUEL`, and `GALNET` requests and displays their
responses. It does not calculate or apply economic outcomes.

Deterministic core fixtures cover `account`, `station`, `market`, `mission`,
`outfit`, `profile`, `galnet`, `options`, `graphics`, and `error` in addition
to the flight/combat states. The TLS career test checks secure account
creation, profile and loadout inspection, market buy/sell, mission identity,
mining, docking, mission reward/reputation, GalNet, combat, recovery, and
reconnect; graphical rendering is checked independently at those state
boundaries.

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
SDL_VIDEODRIVER=x11 ./build-native/native/helion_client --graphics-info --renderer legacy
SDL_VIDEODRIVER=x11 ./build-native/native/helion_client --renderer legacy --render-check /tmp/helion-hardware.bmp
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
SDL_VIDEODRIVER=offscreen \
  ./build-native/native/helion_client --renderer legacy --render-check native-mining.bmp
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

## Kepler career arc and controls

Core mode presents First Ore from Orion Extraction Group, Kepler Supply (buy
two parts at Cinder and deliver them to Kepler), and Red Wake Response (fit and
use the pulse laser) without raw commands. Progress, rewards, standings,
salvage, completion, and GalNet headlines are server-owned and saved
atomically. Completion recognizes an established Kepler pilot and leaves free
play open. The first-run guide is skippable with Enter, dismissible with
Escape, and reopenable with F9; F1–F8 open station, profile, options, market,
contracts, outfitting, GalNet, and graphics. Menus support keyboard focus,
one-shot mouse clicks, and bounded wheel scrolling. Passwords are masked and
never enter snapshots or diagnostics. Native audio remains a later polish item.
