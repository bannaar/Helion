# Compaq 610 Server + EliteBook 8460p Client

This is the intended two-machine setup for Helion 0.2. The Compaq runs only
the authoritative TLS server; it needs no desktop, SDL, OpenGL, or GPU. The
EliteBook runs the graphical client and verifies the server certificate before
it sends account credentials.

Examples use port `4242`, server name `compaq610.local`, data directory
`/var/lib/helion`, and TLS directory `/etc/helion`. Replace names and addresses
with values that actually resolve on your network. Never copy `server.key` to
the client.

## 1. Check both machines

Run on each machine:

```sh
uname -m
```

The supplied packages require `x86_64`. On the Compaq, find stable identities:

```sh
hostname
hostname -I
```

Prefer a DHCP reservation and a working hostname. If using Tailscale, also
record `tailscale ip -4` and the MagicDNS name. The certificate must contain
every exact DNS name or IP that the client will use.

Release checks exercise the split packages in isolated local installs, a
headless server process, verified TLS clients, restart, and persistence. The
physical Compaq, its installed distribution, systemd service, LAN link, and
optional Tailscale path have not been tested here; verify those steps on the
actual two-machine setup before relying on it for public play.

These release artifacts are dynamically linked on the build distribution.
Before choosing the Debian package on an older Compaq installation, inspect
its dependencies with `dpkg-deb -f helion-server_0.2.0_amd64.deb Depends` and
confirm that distribution provides them. In particular, the supplied Mint 22
build needs a recent glibc and `libssl3t64`; it is not a universal binary for
older Debian releases. Build the server-only flavor on a compatible older
distribution if those requirements are unavailable. Do not replace system
libraries merely to satisfy this package.

## 2A. Install Debian packages

On the Compaq:

```sh
sudo apt install ./helion-server_0.2.0_amd64.deb
```

On the EliteBook:

```sh
sudo apt install ./helion-client_0.2.0_amd64.deb
```

The server package has no SDL, OpenGL, X11, desktop-launcher, or client-binary
dependency. The client package does not contain the server, a database, test
certificate, or private key.

## 2B. Install tarballs instead

On the Compaq:

```sh
sudo install -d -m 0755 /opt/helion-server
sudo tar -xzf helion-server-0.2.0-Linux-x86_64.tar.gz -C /opt/helion-server
```

On the EliteBook:

```sh
mkdir -p "$HOME/helion-client"
tar -xzf helion-client-0.2.0-Linux-x86_64.tar.gz -C "$HOME/helion-client"
```

Tarballs are dynamically linked. Install the normal OpenSSL runtime on the
server; install OpenSSL, SDL2, and Mesa/OpenGL runtimes on the client.
Unlike the Debian packages, these archives are not installed into `/usr`:
their executables are under `/opt/helion-server/usr/bin/` and
`~/helion-client/usr/bin/`. Replace `/usr/bin/helion_server` below with
`/opt/helion-server/usr/bin/helion_server` for the manual server check.
Run `"$HOME/helion-client/usr/bin/helion-client-launch"` for the manual client.
The sample system service assumes the Debian path; change `ExecStart` to the
absolute `/opt/helion-server/usr/bin/helion_server` path before enabling it.
The service's `ProtectHome=true` intentionally blocks executables under
`/home`; keep its binary in `/opt` and the database in `/var/lib/helion`.

## 3. Create the server account and directories

For a system-wide Debian install:

```sh
sudo useradd --system --home /var/lib/helion --shell /usr/sbin/nologin helion
sudo install -d -o helion -g helion -m 0750 /var/lib/helion
sudo install -d -o root -g helion -m 0750 /etc/helion
```

The database and lock stay in `/var/lib/helion`; certificate material stays in
`/etc/helion`. Server-created persistence files are owner-only.

## 4. Create a certificate with the right SANs

Create a private OpenSSL request file on the Compaq. Substitute real values;
do not paste the example IP unchanged:

```sh
sudo sh -c 'cat > /etc/helion/cert.conf' <<'EOF'
[req]
distinguished_name=dn
x509_extensions=ext
prompt=no
[dn]
CN=compaq610.local
[ext]
subjectAltName=DNS:compaq610,DNS:compaq610.local,IP:192.168.1.50
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
EOF
```

Add the Tailscale DNS name or IP to `subjectAltName` only when it will be used.
Then generate the key and certificate:

```sh
sudo openssl req -x509 -newkey rsa:3072 -sha256 -nodes -days 365 \
  -config /etc/helion/cert.conf \
  -keyout /etc/helion/server.key -out /etc/helion/server.crt
sudo chown root:helion /etc/helion/server.key /etc/helion/server.crt
sudo chmod 0640 /etc/helion/server.key
sudo chmod 0644 /etc/helion/server.crt
```

Copy only `server.crt` to the EliteBook over a trusted channel, for example to
`~/.config/helion/compaq610-ca.crt`. A changing DHCP address will invalidate an
IP-based connection; use a reservation, stable local DNS/mDNS, or stable
Tailscale identity.

## 5. Verify manually before enabling a service

LAN-only bind (replace the address):

```sh
sudo -u helion /usr/bin/helion_server 4242 /var/lib/helion/commander.db \
  --bind 192.168.1.50 --cert /etc/helion/server.crt --key /etc/helion/server.key
```

Safe bind patterns:

```text
127.0.0.1       loopback diagnostic only
192.168.1.50    LAN interface only
100.x.y.z       Tailscale interface only
0.0.0.0         all interfaces; use only with restrictive firewall rules
```

Stop the foreground check with `Ctrl-C`.

## 6. Install and enable the service

The package ships an example at
`/usr/share/doc/helion-server/examples/helion-server.service`. Copy it and edit
the `--bind` value before enabling it:

```sh
sudo cp /usr/share/doc/helion-server/examples/helion-server.service \
  /etc/systemd/system/helion-server.service
sudoedit /etc/systemd/system/helion-server.service
sudo systemctl daemon-reload
sudo systemctl enable --now helion-server
sudo systemctl status helion-server
```

The unit uses a dedicated account, restrictive umask, read-only system paths,
one writable data directory, `NoNewPrivileges`, restart-on-failure, bounded
shutdown, and a reasonable descriptor limit.
The shipped unit binds loopback by default. Set `--bind` to the actual LAN or
Tailscale address in your local copy before starting the two-machine server.

Follow logs with:

```sh
journalctl -u helion-server -f
```

## 7. Restrict the firewall

For a typical LAN subnet (replace it):

```sh
sudo ufw allow from 192.168.1.0/24 to any port 4242 proto tcp
sudo ufw status
```

For Tailscale-only access, bind to the Tailscale address and allow the port on
that interface according to your host firewall policy. Do not expose `4242`
to the public internet by default. Tailscale narrows network reachability but
does not replace Helion TLS.

## 8. Configure the EliteBook client

Create a non-secret client configuration:

```sh
mkdir -p "$HOME/.config/helion"
chmod 0700 "$HOME/.config/helion"
cat > "$HOME/.config/helion/client.conf" <<'EOF'
host=compaq610.local
port=4242
ca=/home/YOUR_USER/.config/helion/compaq610-ca.crt
renderer=auto
EOF
```

Replace `YOUR_USER` and ensure the host exactly matches a certificate SAN.
Launch **Helion** from the Games menu or run:

```sh
helion-client-launch
```

For diagnostics or a one-off connection:

```sh
helion_client compaq610.local 4242 \
  --ca "$HOME/.config/helion/compaq610-ca.crt" --renderer auto
```

`auto` tries the qualified OpenGL 3.3 core client first and recreates a clean
legacy context if core initialization fails. `--renderer core` requires core
and exits on failure; `--renderer legacy` requests OpenGL 3.0 compatibility
then falls back to 2.1. Startup output and the in-game Graphics screen show the
selection and fallback reason.

Verify hardware rendering:

```sh
glxinfo -B
```

Expected text includes Intel HD Graphics 3000, direct rendering `Yes`, and
accelerated `Yes`. `llvmpipe`, `softpipe`, or `swrast` means software rendering;
check the Mesa Crocus driver, display session, and package setup.

## 9. Network and TLS diagnostics

On the Compaq:

```sh
ss -ltnp | grep ':4242'
sudo ufw status
journalctl -u helion-server -n 50 --no-pager
```

On the EliteBook:

```sh
nc -vz compaq610.local 4242
openssl s_client -connect compaq610.local:4242 \
  -servername compaq610.local \
  -CAfile "$HOME/.config/helion/compaq610-ca.crt" -verify_return_error
```

A wrong CA or a host/IP absent from the SAN is intentionally rejected. Fix the
certificate or connection identity; never disable verification.

## 10. Tailscale option

Install and authenticate Tailscale using its official instructions on both
machines. Add the actual Tailscale DNS name or IP to the certificate SAN,
reissue it, copy the public certificate to the client, bind the server to its
Tailscale IPv4 address, and use the same identity in `client.conf`. Keep
application TLS enabled and do not add a public firewall rule.

## 11. Backup, upgrade, and uninstall

Stop the service for a consistent backup:

```sh
sudo systemctl stop helion-server
sudo tar -czf "$HOME/helion-backup-$(date +%F).tar.gz" \
  /var/lib/helion /etc/helion
sudo systemctl start helion-server
```

Upgrade by installing the new package over the old one. Packages do not own or
delete `/var/lib/helion`, `/etc/helion`, or client preferences, so the database,
TLS material, and `~/.config/helion/client.conf` survive upgrades.

Remove binaries without silently deleting player data:

```sh
sudo apt remove helion-server
sudo apt remove helion-client
```

Only after a backup and deliberately retiring the server should an
administrator remove `/var/lib/helion` or `/etc/helion` manually.
