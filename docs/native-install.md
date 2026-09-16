# Install and play Helion (Linux)

Helion 0.2 includes a native SDL2/OpenGL client and a standalone TLS server.
The combined package includes both, a desktop entry, local certificate setup,
and a local-play launcher. No browser or Node.js runtime is needed.

## Install a built package

On Debian/Ubuntu, install the `.deb` using your package manager (which also
installs SDL2, OpenGL, OpenSSL and launcher dependencies):

```sh
sudo apt install ./helion-0.2.0-Linux-x86_64.deb
```

Open **Helion** from the Games menu, or run `helion-play`. The launcher creates
a private local certificate on first use, starts a loopback-only server, and
opens the client with that certificate trusted explicitly. Closing the client
stops the local server. It does not install or enable a system service.

For a portable install, extract the `.tar.gz` into a user-owned directory and
run its `usr/bin/helion-play`. This package is dynamically linked: SDL2,
OpenGL, OpenSSL, `openssl`, `flock` and `timeout` must be available. Binaries
must match the target CPU architecture and a compatible Linux libc.

## First flight

In the command console, enter `/create pilot YOUR_PASSWORD Commander Name`
(use a new password of 12–128 bytes). On later visits enter
`/login pilot YOUR_PASSWORD`. Passwords are masked in the graphical console.

- **L** launches; **W** thrusts; **A/D** turn; **S** brakes.
- Approach an ore asteroid within **85 m** below **35 m/s**, then press **E**.
- Mine up to **8 units**, waiting **1.25 seconds** between extractions.
- Follow the amber radar marker back to base; press **F** within **85 m**
  below **35 m/s** to sell. Each unit pays **60 credits and 5 XP**.
- **Enter** opens the console; **Escape** returns to flight.

Local saves and logs live in `$XDG_DATA_HOME/helion`, defaulting to
`~/.local/share/helion`. `HELION_DATA_DIR` overrides this directory and
`HELION_PORT` overrides the default local port 4242. Do not share the private
key. Credits, XP, upgrades, market cargo, mined ore, position, velocity, and
dock state are saved after authoritative changes. Restarting the server resumes
the commander's saved flight. Sign out by closing the client.

`helion-play --check` validates the installed server/client TLS path without
opening a window. `helion-play --terminal` opens the command-line client.

## Remote or dedicated server

Install/build the server on its own host. Obtain a certificate with a Subject
Alternative Name matching the DNS name or IP clients will use, then run:

```sh
helion_server 4242 /path/to/commander.db --bind 0.0.0.0 \
  --cert /path/to/fullchain.pem --key /path/to/private-key.pem
helion_client game.example.org 4242
```

The client uses the operating system's trusted CA roots by default. For a
private CA, append `--ca /path/to/trusted-ca.pem`. Distribute that CA
certificate through a trusted channel; never distribute the server key.
Unknown CAs, expired certificates, and mismatched names are rejected. There
is no insecure or plaintext mode. Server accounts still use passwords;
client certificates are not required.

For local manual development only:

```sh
helion-dev-cert /path/to/local-tls
helion_server 4242 /path/to/commander.db \
  --cert /path/to/local-tls/server.crt --key /path/to/local-tls/server.key
helion_client 127.0.0.1 4242 --ca /path/to/local-tls/server.crt
```

Local helper certificates expire after one year. With the local game stopped,
move the `tls` directory to a private backup and run `helion-play` again to
issue a new local certificate. The separate `commander.db` is preserved.
Remote CA-issued certificates need their normal renewal process and a server
restart to load the renewed files.

## Build and package

Build dependencies on Debian/Ubuntu:

```sh
sudo apt install build-essential cmake libssl-dev libsdl2-dev libgl1-mesa-dev python3 openssl dpkg-dev
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
cmake --build build-native --target package
```

For a hardened development build, enable the native sanitizers and run the
headless server tests:

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DHELION_BUILD_CLIENT=OFF -DHELION_SANITIZERS=ON
cmake --build build-sanitize --parallel
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-sanitize --output-on-failure
```

The project keeps its renderer on fixed-function OpenGL 2.1 calls and SDL2
windowing so HD 3000-era Intel drivers remain supported. GLAD, GLM, Dear ImGui,
and stb_ttf are intentionally not required dependencies: the client uses a
small original bitmap font and procedural geometry, which keeps the installed
binary self-contained and avoids pulling a core-profile loader into the legacy
renderer. A future UI expansion can add Dear ImGui's OpenGL2 backend behind a
separate optional target without changing the gameplay protocol.

CMake also supports `cmake --install build-native --prefix /your/prefix`.
For a server-only build use `-DHELION_BUILD_CLIENT=OFF`; for a client-only
build use `-DHELION_BUILD_SERVER=OFF`. Both now require OpenSSL. The combined
build provides the local-play launcher; separate builds provide their native
executable for manual remote connections. Headless tests require Python 3 and
the OpenSSL command-line tool, and generate disposable test certificates.
