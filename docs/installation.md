# Helion Native Server and Client Installation Guide

This guide installs the Phase 2A standalone Helion C++ server and SDL2/OpenGL client on Linux Mint, Ubuntu, or another Debian-based Linux system. It is based on the verified native security patch that introduced protected accounts, loopback-first networking, connection limits, and server integration tests.

The native project currently provides:

- a headless C++17 server;
- a standalone SDL2/OpenGL 2.1 client;
- a versioned line protocol (`Helion/1`);
- salted scrypt password hashing;
- connection limits, timeouts, and bounded messages;
- automated protocol, security, and server tests.

The current network transport does not yet include native TLS. Run the server on loopback and use an SSH or Tailscale SSH tunnel for connections between computers.

Repository documentation:

- `README.md` — project status and quick start;
- `docs/installation.md` — this complete installation guide;
- `docs/native-server.md` — server configuration and protocol details;
- `docs/native-client.md` — client controls, graphics, and troubleshooting.

## 1. Get the Helion source

If Helion already exists at `~/Helion`, use that copy:

```bash
cd ~/Helion
git branch --show-current
```

The current development branch is:

```text
helion-local-recovery-2026-09-14
```

For a fresh installation, authenticate with GitHub and clone that branch:

```bash
cd ~
git clone --branch helion-local-recovery-2026-09-14 \
  https://github.com/bannaar/Helion.git
cd Helion
```

Because the repository is private, GitHub must authenticate the clone. Never put a GitHub password or token directly in the command.

If this computer cannot reach `github.com`, transfer an existing Helion source folder to it or fix its network access before continuing.

## 2. Install common build tools

Install the compiler, CMake, Ninja, and Git:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build git
```

Check the installed tools:

```bash
g++ --version
cmake --version
ninja --version
```

Helion requires CMake 3.16 or newer and a C++17 compiler. GCC 9 or newer is recommended.

## 3. Install and build the server

The server requires OpenSSL development files for password hashing. It does not require SDL2, OpenGL, or a desktop session.

Install its dependency:

```bash
sudo apt install libssl-dev
```

Configure a server-only build from the repository root:

```bash
cd ~/Helion
cmake -S . -B build-server -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DHELION_BUILD_SERVER=ON \
  -DHELION_BUILD_CLIENT=OFF \
  -DHELION_BUILD_TESTS=ON
```

Build and test it:

```bash
cmake --build build-server --parallel
ctest --test-dir build-server --output-on-failure
```

The server executable will be:

```text
~/Helion/build-server/native/helion_server
```

### Start the server for a local test

Create a private data directory:

```bash
mkdir -p ~/.local/share/helion-server
chmod 700 ~/.local/share/helion-server
```

Start the server:

```bash
cd ~/Helion
./build-server/native/helion_server \
  4242 \
  "$HOME/.local/share/helion-server/helion-server.db"
```

The expected startup message is similar to:

```text
Helion server listening on 127.0.0.1:4242 max-clients=32
```

Keep this terminal open. Stop the server cleanly with `Ctrl+C`.

The server defaults to:

| Setting | Default |
| --- | --- |
| Address | `127.0.0.1` |
| Port | `4242` |
| Maximum clients | 32 |
| Read/idle timeout | 30 seconds |
| Write timeout | 5 seconds |
| Maximum message line | 4096 bytes |
| New-password length | 12–128 bytes |

An alternative client limit can be set explicitly:

```bash
./build-server/native/helion_server \
  4242 \
  "$HOME/.local/share/helion-server/helion-server.db" \
  --max-clients 16
```

Keep the default loopback address until native TLS is implemented.

## 4. Install and build the client

Install SDL2, OpenGL, and a graphics diagnostic utility:

```bash
sudo apt install libsdl2-dev libgl1-mesa-dev mesa-utils
```

Configure a client-only build:

```bash
cd ~/Helion
cmake -S . -B build-client -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DHELION_BUILD_SERVER=OFF \
  -DHELION_BUILD_CLIENT=ON \
  -DHELION_BUILD_TESTS=ON
```

Build and test it:

```bash
cmake --build build-client --parallel
ctest --test-dir build-client --output-on-failure
```

The client executable will be:

```text
~/Helion/build-client/native/helion_client
```

Check the active OpenGL renderer:

```bash
glxinfo -B
```

The native client targets OpenGL 2.1-class hardware, including the Intel graphics in an HP EliteBook 8460p.

## 5. Connect on the same computer

Leave the server running in its first terminal. Open a second terminal and start the diagnostic client:

```bash
cd ~/Helion
./build-client/native/helion_client 127.0.0.1 4242 --terminal
```

Create an account using a password between 12 and 128 characters:

```text
/create pilot choose-a-long-password Cmdr Pilot
```

Then try the available commands:

```text
/login pilot choose-a-long-password
/profile
/state
/chat Hello from the native client
/quit
```

Start the graphical client with:

```bash
./build-client/native/helion_client 127.0.0.1 4242
```

The graphical interface is still an early development shell. Terminal mode is currently the clearest way to test accounts, chat, profiles, and protocol responses.

## 6. Connect from another computer through Tailscale SSH

Keep the Helion server bound to `127.0.0.1`. On the client computer, create an encrypted SSH tunnel to the server computer:

```bash
ssh -N -L 14242:127.0.0.1:4242 \
  USER@TAILSCALE_IP
```

Leave the tunnel terminal open. On the client computer, connect Helion to the local end of the tunnel:

```bash
./build-client/native/helion_client 127.0.0.1 14242 --terminal
```

Or launch its graphical mode:

```bash
./build-client/native/helion_client 127.0.0.1 14242
```

The connection path is:

```text
Helion client -> local port 14242 -> encrypted SSH/Tailscale tunnel -> server port 4242
```

If the server's Tailscale address changes, obtain its current address on the server with:

```bash
tailscale ip -4
```

If Tailscale SSH is not enabled on the server:

```bash
sudo tailscale set --ssh=true
```

Do not expose Helion port `4242` directly to the internet. Passwords are hashed on disk, but native TLS transport is still under development.

## 7. Install the server as a system service

Complete the local foreground test before installing the service.

Create a restricted service account and data directory:

```bash
sudo useradd --system \
  --home-dir /var/lib/helion \
  --shell /usr/sbin/nologin \
  helion
sudo install -d -o helion -g helion -m 750 /var/lib/helion
```

Install the server binary under `/opt/helion`:

```bash
cd ~/Helion
sudo cmake --install build-server --prefix /opt/helion
sudo chown root:root /opt/helion/bin/helion_server
sudo chmod 755 /opt/helion/bin/helion_server
```

Create `/etc/systemd/system/helion-server.service` as an administrator with this content:

```ini
[Unit]
Description=Helion standalone server
After=network.target

[Service]
Type=simple
User=helion
Group=helion
WorkingDirectory=/var/lib/helion
ExecStart=/opt/helion/bin/helion_server 4242 /var/lib/helion/helion-server.db
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

Load and start the service:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now helion-server
sudo systemctl status helion-server
```

Follow its logs:

```bash
journalctl -u helion-server -f
```

Stop or restart it with:

```bash
sudo systemctl stop helion-server
sudo systemctl restart helion-server
```

## 8. Install a desktop launcher for the client

Install the client into your user account:

```bash
cd ~/Helion
cmake --install build-client --prefix "$HOME/.local"
```

The installed executable is normally:

```text
~/.local/bin/helion_client
```

Create `~/.local/share/applications/helion-client.desktop` with the following content. Replace `YOUR_USERNAME` with the Linux username on that computer.

```ini
[Desktop Entry]
Type=Application
Name=Helion Native Client
Comment=Connect to a Helion standalone server
Exec=/home/YOUR_USERNAME/.local/bin/helion_client 127.0.0.1 4242
Terminal=false
Categories=Game;
```

For an SSH tunnel using local port `14242`, change the last argument in `Exec` from `4242` to `14242`.

Make the launcher executable:

```bash
chmod +x ~/.local/share/applications/helion-client.desktop
```

## 9. Updating Helion

Back up the server data file before replacing the server:

```bash
cp ~/.local/share/helion-server/helion-server.db \
  ~/.local/share/helion-server/helion-server.db.backup
chmod 600 ~/.local/share/helion-server/helion-server.db.backup
```

When normal GitHub access is available:

```bash
cd ~/Helion
git status
git pull --ff-only
cmake --build build-server --parallel
cmake --build build-client --parallel
ctest --test-dir build-server --output-on-failure
ctest --test-dir build-client --output-on-failure
```

Do not run `git pull` if `git status` reports uncommitted work you have not backed up or committed.

For a system service, reinstall and restart the server after tests pass:

```bash
sudo systemctl stop helion-server
sudo cmake --install build-server --prefix /opt/helion
sudo systemctl start helion-server
sudo systemctl status helion-server
```

## 10. Troubleshooting

### GitHub cannot connect on port 443

Test name resolution and HTTPS access:

```bash
getent hosts github.com
curl -4 -I --connect-timeout 10 https://github.com
```

If both fail, the problem is network access rather than Git authentication. Use an existing source copy while the network problem is repaired.

### CMake cannot find OpenSSL

Install the development package and reconfigure:

```bash
sudo apt install libssl-dev
cmake -S . -B build-server -G Ninja \
  -DHELION_BUILD_CLIENT=OFF
```

### CMake cannot find SDL2 or OpenGL

Install the client dependencies:

```bash
sudo apt install libsdl2-dev libgl1-mesa-dev
```

Then configure `build-client` again.

### The client reports `unable to connect`

Confirm the server is listening:

```bash
ss -ltn | grep 4242
```

Test the local port:

```bash
nc -vz 127.0.0.1 4242
```

For a tunnel using port `14242`:

```bash
nc -vz 127.0.0.1 14242
```

### SDL initialization fails

Run the graphical client from the XFCE desktop session and inspect:

```bash
echo "$DISPLAY"
echo "$WAYLAND_DISPLAY"
```

Terminal mode does not need a graphical window.

### OpenGL window creation fails

Inspect the driver:

```bash
glxinfo -B
```

Test software rendering to distinguish a driver problem from an application problem:

```bash
LIBGL_ALWAYS_SOFTWARE=1 \
  ./build-client/native/helion_client 127.0.0.1 4242
```

### Account creation returns `ERR password-length`

Use a password between 12 and 128 bytes.

### The server refuses to load its data file

Do not delete the file. Stop the server, preserve a copy, and inspect the service logs:

```bash
sudo systemctl stop helion-server
sudo cp /var/lib/helion/helion-server.db \
  /var/lib/helion/helion-server.db.recovery-copy
sudo chmod 600 /var/lib/helion/helion-server.db.recovery-copy
journalctl -u helion-server -n 100 --no-pager
```

The server aborts startup when it detects malformed persistence data or cannot migrate legacy credentials safely.

## 11. Current development limits

The native foundation builds and its automated tests pass, but it is not yet a complete game. The graphical client still needs a polished login interface and text rendering. The server does not yet run an authoritative ship simulation, trading economy, missions, or combat. Native TLS is the next networking milestone; until it lands, use loopback or an encrypted tunnel.
