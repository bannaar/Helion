# Helion project state

Helion has two runtimes. The browser prototype remains in `src/` and is not
part of the native release work. The native runtime is a C++17 SDL2/OpenGL
client plus a POSIX server under `native/`.

The native client is playable today. A commander can create an account or log
in through the graphical command console, launch from Kepler, fly with W/S/A/D
or arrows, mine ore from asteroid fields, return to station, dock, and receive
saved credits and experience. The sector contains a second station, moving NPC
hauler traffic, other logged-in commanders as live contacts, a radar, a
rotating station, a procedural Sidewinder, faceted ore asteroids, engine
exhaust, mining beam, and cockpit font. Contact positions are server-owned and
clients request a refreshed contact stream once per second. A client can use
`/contacts` to inspect the stream.

Trading is server validated while docked. `/buy food 1` and `/buy parts 1`
purchase supplies against station prices and hold capacity; `/sell food 1` and
`/sell parts 1` sell inventory. Station prices differ between Kepler and
Cinder. The graphical **B** shortcut opens a one-unit food purchase; the
console is used for other quantities and sales. `F2` opens the account/profile
view. `F3` toggles telemetry/radar as a game setting. Authentication, chat,
profile queries, flight, contacts, mining, docking, and trading require login.

Progression is now playable: `/mission` shows the First Ore contract,
`/accept` starts it, and `/turnin` pays the reward after returning with ore.
`/upgrade engine` and `/upgrade hull` spend credits at the station and advance
levels; engine levels increase authoritative acceleration and hull levels
increase maximum integrity. Asteroid impacts damage hulls based on impact
speed, disable ships at zero integrity, recover them at their station, and
require the docked `/repair` command before launch. Flight position, velocity,
dock state, hull damage, mined ore, and market cargo are written into the
owner-only save after control and economy changes, so a server restart resumes
the commander in flight.
Other clients receive live positions through `/contacts`, and the client
renders commander ships alongside NPC traffic.

Transport is mandatory TLS 1.2 or newer. The server requires a certificate and
private key and never emits a plaintext greeting. The client validates trust,
expiration, and DNS/IP Subject Alternative Name before sending commands. Local
play generates a private loopback certificate automatically. The server uses
scrypt password hashes, owner-only save files, a stable save lock, bounded
line decoding, connection limits, timeouts, and atomic persistence.

The combined install includes `helion_server`, `helion_client`,
`helion-dev-cert`, `helion-play`, a desktop entry, icon, and documentation.
The launcher creates a private data directory, starts a TLS loopback server,
opens the client, and shuts the server down when the client exits. `--check`
validates the install without opening a window. CMake also supports server-only
and client-only configurations and generates `.deb` and `.tar.gz` packages.

Native automated coverage includes flight/control physics, asteroid collision,
mining cooldown and capacity, docking rewards, market quantity/price/capacity
rules, contact serialization, protocol bounds, password hashing and legacy
migration, save locking, TLS certificate/hostname/IP validation, plaintext
rejection, graceful shutdown, installed launcher startup, saved login, file
permissions, and desktop asset installation.

The remaining major gameplay work is combat, a broader mission chain, and
richer economy simulation. The current
mining, trading, progression, outfitting, persistence, and contact loops form
a playable native career slice rather than a static client shell.

The repository now has GitHub Actions for release builds, package creation,
full TLS/installer tests, and an AddressSanitizer/UndefinedBehaviorSanitizer
server build. The legacy OpenGL renderer intentionally remains dependency-light
and uses SDL2 plus procedural assets; GLAD, GLM, ImGui, and stb_ttf are not
required for the current HD 3000-compatible client.
