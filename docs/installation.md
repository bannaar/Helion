# Helion Native Installation and Operations Guide

This guide installs Helion 0.2's standalone C++ server and SDL2/OpenGL client on Linux Mint, Ubuntu, or another Debian-based Linux system. It covers local play, separate server and client installations, verified TLS, packages, updates, backups, and troubleshooting.

The current native release includes:

- mandatory TLS 1.2 or newer with certificate and hostname/IP verification;
- salted scrypt password hashes and protected commander saves;
- a 60 Hz server-authoritative flight simulation;
- mining, docking, station trading, missions, ship upgrades, and progression;
- persistent position, velocity, dock state, cargo, credits, XP, missions, and upgrades;
- live player and NPC contacts;
- an SDL2/OpenGL 2.1 client for older Linux graphics hardware;
- `.deb` and portable `.tar.gz` packaging;
- protocol, security, TLS, gameplay, save-lock, installer, and rendering tests.

## 1. Choose an installation method

Use one of these routes:

| Route | Best for |
| --- | --- |
| Install a `.deb` package | Normal Linux Mint or Ubuntu installation |
| Build and install locally | Development or testing the newest source |
| Server-only build | A headless dedicated host |
| Client-only build | A player computer connecting to another server |

The native code is currently developed on branch:

```text
helion-local-recovery-2026-09-14
```

## 2. Install a built package

Install a downloaded Helion `.deb` package with APT:

```bash
cd ~/Downloads
sudo apt install ./helion-0.2.0-Linux-x86_64.deb
```

Launch **Helion** from the XFCE Games menu or run:

```bash
helion-play
```

The launcher automatically:

1. creates a private user data directory;
2. generates a loopback-only development certificate when needed;
3. starts the local TLS server;
4. starts the graphical client and trusts that certificate;
5. stops the local server when the client exits.

Validate an installation without opening a graphical window:

```bash
helion-play --check
```

Start the command-line client instead of the graphical client:

```bash
helion-play --terminal
```

User data is stored in:

```text
~/.local/share/helion
```

The directory contains the commander database, TLS key and certificate, and logs. Keep it private and never commit or share its private key.

## 3. Obtain the source

If the repository already exists locally:

```bash
cd ~/Helion
git branch --show-current
git status --short
```

For a fresh clone, authenticate with GitHub and clone the current branch:

```bash
cd ~
git clone --branch helion-local-recovery-2026-09-14 \
  https://github.com/bannaar/Helion.git
cd Helion
```

The repository is private, so GitHub authentication is required. Do not put a password or token directly in the command.

## 4. Install build dependencies

Install the full native toolchain:

```bash
sudo apt update
sudo apt install \
  build-essential \
  cmake \
  ninja-build \
  libssl-dev \
  libsdl2-dev \
  libgl1-mesa-dev \
  mesa-utils \
  python3 \
  openssl \
  dpkg-dev \
  util-linux \
  coreutils
```

The client requires SDL2 and OpenGL. The server does not require a graphical session, but both server and client require OpenSSL because all connections use TLS.

## 5. Build and test everything

Configure a release build from the repository root:

```bash
cd ~/Helion
cmake -S . -B build-native -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
```

Build and run the complete test suite:

```bash
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
```

The main executables are:

```text
build-native/native/helion_server
build-native/native/helion_client
```

Run the headless rendering check when an SDL installation is available:

```bash
SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 \
  ./build-native/native/helion_client \
  --render-check native-mining.bmp
```

## 6. Install a source build for local play

Install the compiled binaries and launcher into your user account:

```bash
cmake --install build-native --prefix "$HOME/.local"
```

Ensure the user binary directory is available in the current shell:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

Test and launch it:

```bash
helion-play --check
helion-play
```

If the desktop entry does not appear immediately, log out and back in or run:

```bash
update-desktop-database ~/.local/share/applications 2>/dev/null || true
```

## 7. Run directly from the build tree

Create a private local certificate for `localhost`, `127.0.0.1`, and `::1`:

```bash
cd ~/Helion
native/scripts/helion-dev-cert /tmp/helion-local-tls
```

The helper refuses to overwrite existing certificate files. Use a new empty directory when generating another certificate.

Start the server in the first terminal:

```bash
./build-native/native/helion_server \
  4242 \
  "$HOME/.local/share/helion-server/commander.db" \
  --cert /tmp/helion-local-tls/server.crt \
  --key /tmp/helion-local-tls/server.key
```

Start the graphical client in a second terminal:

```bash
./build-native/native/helion_client \
  127.0.0.1 \
  4242 \
  --ca /tmp/helion-local-tls/server.crt
```

For protocol diagnostics:

```bash
./build-native/native/helion_client \
  127.0.0.1 \
  4242 \
  --ca /tmp/helion-local-tls/server.crt \
  --terminal
```

There is no plaintext mode or insecure certificate bypass.

## 8. Create a commander and play

Open the graphical console with **Enter** and create an account:

```text
/create pilot YOUR_NEW_PASSWORD Commander Name
```

Passwords must be 12–128 bytes. On later visits:

```text
/login pilot YOUR_NEW_PASSWORD
```

Useful commands include:

```text
/profile
/state
/contacts
/mission
/accept
/turnin
/buy food 1
/buy parts 1
/sell food 1
/sell parts 1
/upgrade engine
/upgrade hull
/chat Hello pilots
/quit
```

Flight controls:

| Key | Action |
| --- | --- |
| `L` | Launch from the station |
| `W` or Up | Thrust |
| `S` or Down | Brake |
| `A/D` or Left/Right | Turn |
| `E` | Mine a nearby ore asteroid |
| `F` | Dock when close and slow enough |
| `B` | Buy one unit of food while docked |
| `F2` | Account/profile view |
| `F3` | Toggle telemetry/radar |
| Enter | Open the command console |
| Escape | Return to flight |

Mine within 85 m of an ore asteroid while travelling below 35 m/s. The extractor has a 1.25-second cooldown and the starter hold carries eight units. Return to the amber station marker and dock within 85 m below 35 m/s to sell ore.

The server persists credits, XP, upgrades, missions, market cargo, mined ore, position, velocity, yaw, dock state, and station. Restarting the server resumes the saved commander state.

## 9. Build only the server

Install server dependencies:

```bash
sudo apt install build-essential cmake ninja-build libssl-dev python3 openssl
```

Configure, build, and test without SDL2 or OpenGL:

```bash
cd ~/Helion
cmake -S . -B build-server -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DHELION_BUILD_SERVER=ON \
  -DHELION_BUILD_CLIENT=OFF \
  -DHELION_BUILD_TESTS=ON
cmake --build build-server --parallel
ctest --test-dir build-server --output-on-failure
```

## 10. Build only the client

Install client dependencies:

```bash
sudo apt install \
  build-essential cmake ninja-build libssl-dev \
  libsdl2-dev libgl1-mesa-dev mesa-utils
```

Configure, build, and test:

```bash
cd ~/Helion
cmake -S . -B build-client -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DHELION_BUILD_SERVER=OFF \
  -DHELION_BUILD_CLIENT=ON \
  -DHELION_BUILD_TESTS=ON
cmake --build build-client --parallel
ctest --test-dir build-client --output-on-failure
```

## 11. Configure a remote or dedicated server

Use a certificate whose Subject Alternative Name matches the exact DNS name or IP address clients use. A loopback development certificate is not valid for a remote hostname or Tailscale address.

Start a remotely reachable server:

```bash
./helion_server \
  4242 \
  /var/lib/helion/commander.db \
  --bind 0.0.0.0 \
  --cert /etc/helion/fullchain.pem \
  --key /etc/helion/private-key.pem \
  --max-clients 32
```

With a publicly trusted certificate, connect using the matching hostname:

```bash
helion_client game.example.org 4242
```

With a private certificate authority, distribute only its CA certificate through a trusted channel:

```bash
helion_client game.example.org 4242 \
  --ca /path/to/trusted-ca.pem
```

Never distribute the server private key. Unknown certificate authorities, expired certificates, and mismatched DNS/IP identities are rejected before credentials are sent.

For Tailscale, bind the server to its Tailscale address or an appropriately firewalled interface and issue a certificate containing the Tailscale IP or MagicDNS hostname the client uses. Then connect with that same identity. TLS verification still applies inside the Tailscale network.

## 12. Install the dedicated server as a system service

Create a service account and protected directories:

```bash
sudo useradd --system \
  --home-dir /var/lib/helion \
  --shell /usr/sbin/nologin \
  helion
sudo install -d -o helion -g helion -m 750 /var/lib/helion
sudo install -d -o root -g helion -m 750 /etc/helion
```

Install the server build:

```bash
cd ~/Helion
sudo cmake --install build-server --prefix /opt/helion
```

Copy the certificate and key to `/etc/helion`, set ownership deliberately, and keep the key unreadable by other users:

```bash
sudo chown root:helion /etc/helion/fullchain.pem /etc/helion/private-key.pem
sudo chmod 640 /etc/helion/fullchain.pem /etc/helion/private-key.pem
```

Create `/etc/systemd/system/helion-server.service`:

```ini
[Unit]
Description=Helion standalone TLS server
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=helion
Group=helion
WorkingDirectory=/var/lib/helion
ExecStart=/opt/helion/bin/helion_server 4242 /var/lib/helion/commander.db --bind 0.0.0.0 --cert /etc/helion/fullchain.pem --key /etc/helion/private-key.pem --max-clients 32
Restart=on-failure
RestartSec=3
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/helion

[Install]
WantedBy=multi-user.target
```

Enable and inspect it:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now helion-server
sudo systemctl status helion-server
journalctl -u helion-server -f
```

Open the port only on the network that should reach it. Example for a private subnet:

```bash
sudo ufw allow from 192.0.2.0/24 to any port 4242 proto tcp
```

Replace the example subnet with the real authorized network; do not copy it literally.

## 13. Build distributable packages

Build the configured package targets:

```bash
cd ~/Helion
cmake --build build-native --target package
```

CMake produces versioned `.deb` and `.tar.gz` packages. Test a package before distributing it:

```bash
sudo apt install ./helion-0.2.0-Linux-x86_64.deb
helion-play --check
```

## 14. Update and back up Helion

Stop the server before a manual backup. For a local user installation:

```bash
cp ~/.local/share/helion/commander.db \
  ~/.local/share/helion/commander.db.backup
chmod 600 ~/.local/share/helion/commander.db.backup
```

For a system service:

```bash
sudo systemctl stop helion-server
sudo cp /var/lib/helion/commander.db \
  /var/lib/helion/commander.db.backup
sudo chmod 600 /var/lib/helion/commander.db.backup
```

Update a clean Git checkout:

```bash
cd ~/Helion
git status
git pull --ff-only
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
```

Do not pull, reset, or switch branches while valuable uncommitted work is present.

Reinstall after successful tests:

```bash
cmake --install build-native --prefix "$HOME/.local"
```

For a system service, reinstall the server and restart it:

```bash
sudo cmake --install build-server --prefix /opt/helion
sudo systemctl start helion-server
sudo systemctl status helion-server
```

## 15. Troubleshooting

### GitHub cannot connect

```bash
getent hosts github.com
curl -4 -I --connect-timeout 10 https://github.com
```

If HTTPS is unreachable, use the existing local source copy while repairing network access.

### CMake cannot find OpenSSL

```bash
sudo apt install libssl-dev openssl
```

Delete only the affected disposable build directory or re-run CMake after installing the dependency.

### CMake cannot find SDL2 or OpenGL

```bash
sudo apt install libsdl2-dev libgl1-mesa-dev
```

### Certificate verification fails

Confirm the certificate is current and contains the identity used by the client:

```bash
openssl x509 -in /path/to/server.crt \
  -noout -dates -subject -issuer -ext subjectAltName
```

Use `--ca` only with the intended trusted CA or local certificate. Never disable certificate verification.

### The local launcher reports an expired certificate

Stop Helion, back up the private `tls` directory, then move it aside and let the launcher generate a new loopback certificate. Do not remove `commander.db`.

### The server says the data file is already in use

Another Helion server process is using the same commander database. Check before stopping anything:

```bash
pgrep -af helion_server
```

Each running server needs its own data file. The `.lock` sidecar prevents two servers from corrupting one save.

### The client cannot connect

Check the service, listening socket, firewall, address, and TLS identity:

```bash
systemctl status helion-server
ss -ltn | grep 4242
```

For a local certificate:

```bash
openssl s_client \
  -connect 127.0.0.1:4242 \
  -CAfile /path/to/server.crt \
  -verify_return_error \
  -verify_ip 127.0.0.1
```

### SDL or OpenGL fails

Run the graphical client inside the XFCE desktop session and inspect the renderer:

```bash
glxinfo -B
```

Test software rendering for diagnosis:

```bash
LIBGL_ALWAYS_SOFTWARE=1 \
  helion_client 127.0.0.1 4242 \
  --ca /path/to/server.crt
```

## 16. Current development limits

The native client/server now forms a playable career slice with secure transport, mining, trading, missions, upgrades, persistent flight, and live contacts. Remaining major systems include combat, broader mission chains, functional hull damage and repair, deeper economy simulation, shared asteroid depletion, and richer exploration.
