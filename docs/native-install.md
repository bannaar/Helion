# Install and play Helion (Linux)

Helion 0.2 includes a native SDL2/OpenGL client and a standalone TLS server.
The combined package provides a local-play launcher. Separate server and client
packages support two machines. No browser or Node.js runtime is needed.

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

Enter an account name and a 12–128 byte password in the graphical account
screen, then select **Create Commander**. On later visits select **Log In**.
Passwords are masked in the graphical fields. The first-run guide introduces
the Kepler career: First Ore, Cinder supply delivery, and Red Wake response.

- **L** launches; **W** thrusts; **A/D** turn; **S** brakes.
- Approach an ore asteroid within **85 m** below **35 m/s**, then press **E**.
- Mine up to **8 units**, waiting **1.25 seconds** between extractions.
- Follow the amber radar marker back to base; press **F** within **85 m**
  below **35 m/s** to sell. Each unit pays **60 credits and 5 XP**.
- **F5** opens contracts, **F4** market, **F6** outfitting, **F7** GalNet,
  and **F9** help. Mouse and keyboard both cover normal progression.
- Select **Shipyard / Hulls and Owned Fleet** while docked to inspect local
  inventory, purchase an affordable hull, or activate a stored ship at the
  same station. Up/Down and Enter work alongside mouse controls.

Local saves and logs live in `$XDG_DATA_HOME/helion`, defaulting to
`~/.local/share/helion`. `HELION_DATA_DIR` overrides this directory and
`HELION_PORT` overrides the default local port 4242. Do not share the private
key. Credits, XP, upgrades, market cargo, mined ore, position, velocity, and
dock state are saved after authoritative changes. Restarting the server resumes
the commander's saved flight. Sign out by closing the client.

`helion-play --check` validates the installed server/client TLS path without
opening a window. `helion-play --terminal` opens the command-line client.

## Split packages and remote server

For the Compaq 610 server and EliteBook 8460p client, follow the dedicated
[two-machine guide](compaq610-server-elitebook-client.md). Split packages are:

```text
helion-server-0.2.0-Linux-x86_64.tar.gz
helion-client-0.2.0-Linux-x86_64.tar.gz
helion-server_0.2.0_amd64.deb
helion-client_0.2.0_amd64.deb
```

The server package has no SDL/OpenGL/X11 dependency. The client package has a
desktop launcher, icon, and CA placement guidance, and contains no server key
or database. Packages built on Linux Mint 22.3 require that generation of
glibc/libstdc++/OpenSSL; older Linux installations may need a build on their
own target or a compatible older build environment.
The split-package checks are local, isolated install and TLS tests; an actual
Compaq-to-EliteBook LAN or Tailscale deployment still needs on-site verification.

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
cmake --build build-native -j2
ctest --test-dir build-native --output-on-failure
cmake --build build-native --target package -j2

# Separate release builds:
cmake -S . -B build-server -DHELION_BUILD_CLIENT=OFF -DHELION_BUILD_TESTS=OFF -DHELION_PACKAGE_FLAVOR=server
cmake --build build-server -j2
cpack --config build-server/CPackConfig.cmake -G TGZ
cpack --config build-server/CPackConfig.cmake -G DEB
cmake -S . -B build-client -DHELION_BUILD_SERVER=OFF -DHELION_BUILD_TESTS=OFF -DHELION_PACKAGE_FLAVOR=client
cmake --build build-client -j2
cpack --config build-client/CPackConfig.cmake -G TGZ
cpack --config build-client/CPackConfig.cmake -G DEB
```

For a hardened development build, enable the native sanitizers and run the
headless server tests:

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DHELION_BUILD_CLIENT=OFF -DHELION_SANITIZERS=ON
cmake --build build-sanitize -j2
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-sanitize --output-on-failure
```

The client first attempts hardware OpenGL 3.3 core in `auto` mode and recreates
a clean fixed-function legacy context if core initialization fails. Legacy
requests OpenGL 3.0 compatibility and falls back to 2.1. The client uses a
small built-in glyph atlas and procedural geometry; no GLAD, GLM, ImGui, or
external font runtime is required. Use `--renderer core` to require core or
`--renderer legacy` to force compatibility rendering.

CMake also supports `cmake --install build-native --prefix /your/prefix`.
For a server-only build use `-DHELION_BUILD_CLIENT=OFF`; for a client-only
build use `-DHELION_BUILD_SERVER=OFF`. Both now require OpenSSL. The combined
build provides the local-play launcher; separate builds provide their native
executable for manual remote connections. Headless tests require Python 3 and
the OpenSSL command-line tool, and generate disposable test certificates.
