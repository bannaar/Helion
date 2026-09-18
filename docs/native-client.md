# Helion standalone client

The standalone client is an SDL2 application with a fixed-function OpenGL
renderer. It is designed to run on Linux machines with OpenGL 2.1-class
hardware, including Intel HD 3000-era integrated graphics.

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
/state
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
capacity is eight units. Use the radar's amber base marker to return to
Kepler station, then press **F** within 85 m at low speed to dock and sell.
Each ore unit earns 60 credits and 5 XP, saved by the server.

Flight is top-down with a north-up camera. Opening the console or losing
window focus applies the brake. The server also stops stale controls after
half a second. The client smooths received positions for display; the server
owns movement, collision, cargo, and rewards.

Credits, XP, upgrades, market cargo, mined ore, position, velocity, and dock
state survive server restarts. Each commander still has separate asteroid
depletion, while GalNet chat and live contact positions are shared.

## 5. Graphics configuration

The client requests:

- OpenGL major version 2
- OpenGL minor version 1
- Double buffering
- Resizable 960×600 window
- Fixed-function projection and colored primitives

It does not require GLSL, vertex buffer objects, VAOs, or OpenGL 3+ features.
On a Linux desktop, inspect the active renderer with:

```sh
glxinfo -B
```

If `glxinfo` is missing:

```sh
sudo apt install mesa-utils
```

The renderer line should show the Intel driver or another hardware renderer.
Forcing software rendering is useful only for troubleshooting:

```sh
LIBGL_ALWAYS_SOFTWARE=1 ./build-native/native/helion_client 127.0.0.1 4242 --ca /path/to/local-tls/server.crt
```

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
