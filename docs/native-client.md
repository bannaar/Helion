# Helion standalone client

The standalone client is an SDL2 application with a fixed-function OpenGL
renderer. It is designed to run on Linux machines with OpenGL 2.1-class
hardware, including Intel HD 3000-era integrated graphics.

## 1. Requirements

Install the compiler/build tools and client libraries:

```sh
sudo apt update
sudo apt install build-essential cmake libsdl2-dev libgl1-mesa-dev
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
SDL2 and OpenGL. `HELION_BUILD_TESTS` defaults to `ON`; run
`ctest --test-dir build-native --output-on-failure` after building all targets.

The executable is:

```text
build-native/native/helion_client
```

## 3. Connect

Graphical mode:

```sh
./build-native/native/helion_client [host] [port]
```

Examples:

```sh
# Local server
./build-native/native/helion_client 127.0.0.1 4242

# Remote server
./build-native/native/helion_client helion.example.org 4242
```

The current graphical shell accepts protocol commands as typed input. Use:

```text
/create username password display
/login username password
/chat hello pilots
/profile
/state
/quit
```

The client expects the versioned `WELCOME Helion/1` greeting. The shared
protocol decoder buffers split network reads, separates multiple lines in one
read, and rejects oversized or malformed lines. The maximum line size is 4096
bytes before LF. Version 1 retains the existing command names.

For headless diagnostics, use terminal mode:

```sh
./build-native/native/helion_client 127.0.0.1 4242 --terminal
```

## 4. Graphics configuration

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
LIBGL_ALWAYS_SOFTWARE=1 ./build-native/native/helion_client 127.0.0.1 4242
```

## 5. Desktop launcher

Create `~/.local/share/applications/helion-client.desktop` and adjust the
`Exec` path:

```ini
[Desktop Entry]
Type=Application
Name=Helion Native Client
Comment=Connect to a Helion standalone server
Exec=/opt/helion/build-native/native/helion_client 127.0.0.1 4242
Terminal=false
Categories=Game;
```

For a remote server, replace `127.0.0.1` with its hostname. The current
launcher is a desktop shortcut, not an updater or account-management shell;
the graphical client’s login form is the next native UI milestone.

## 6. Troubleshooting

### `unable to connect`

Confirm the server is running and the port is reachable:

```sh
./build-native/native/helion_client 127.0.0.1 4242 --terminal
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

### Text is not shown

The current shell renders the connection/log layout and accepts text input;
font atlas rendering and a polished login form are still being developed. The
terminal mode remains the authoritative protocol diagnostic.

## 7. Security note

The current reference protocol sends credentials over the TCP connection without
TLS and the server stores them plainly. Use it only on localhost, a trusted LAN,
or a private tunnel. Do not use a real password. Production client work must
add encrypted transport, password hashing, server identity verification, and a
proper account/login UI before public use.
