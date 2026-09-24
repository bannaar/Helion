# Native ship architecture (Batch 13)

The native TLS server owns ship definitions, commander fleets, active-ship
selection, movement, cargo limits, damage, purchases, and persistence. The
client receives presentation frames and never prices or commits a hull.

## Data boundaries

- `shared/ships.*` is the canonical hull registry. A `Definition` is immutable
  model data: identity, performance, pad, mass, hardpoints/slots, capabilities,
  and art-direction metadata.
- `shared/owned_ship.*` defines a persistent individual vessel. Every instance
  has a stable unique ID, hull ID, name, flight/cargo/damage state, per-ship
  upgrades and fitted modules, livery, wear state, and stored/deployed state.
- Manufacturer and operator are separate registry references. In particular,
  Valiant and Intrepid are Titan Forge constructions operated by the
  Commonwealth Navy.
- A visual-variant profile is a third, separate presentation identity. Owned
  ships persist their variant (`CIVILIAN`, `CORPORATE`, `COMMONWEALTH_NAVY`,
  `DIRECTORATE`, `MILITIA`, `FRONTIER_WEATHERED`, `PIRATE_CONVERSION`,
  `SMUGGLER_CONVERSION`, or `PROTOTYPE`). It can supply a different operator
  treatment without changing the manufactured hull. Livery and wear remain
  additional per-instance presentation, never new definitions.
- `shared/shipyard.*` owns station inventory and already carries fields for
  faction, manufacturer, reputation, permits, military rank, criminal contacts,
  and world-event gates. Batch 13 uses only station and pad availability.

The seven core-slot entries are ordered as Power Plant, Thrusters, FSD, Life
Support, Power Distributor, Sensors, and Fuel Tank. Mount sizes are canonical
`SIZE_1` through `SIZE_4`. Full outfitting remains a later batch.

## Playable registry and native-scale balance

| Hull ID | Manufacturer | Operator | Pad | Price | Mass | Speed | Accel | Handling | Hull | Shields* | Cargo | Laden / unladen range* |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `SIDEWINDER` | Independent Yards | Civilian | Small | starter | 25 | 200 | 130 | 6 | 100 | 0 | 8 | 6.2 / 7.0 |
| `TITAN_MULE` | Titan Forge | Civilian | Small | 2,800 | 45 | 180 | 120 | 4 | 120 | 60 | 14 | 9.2 / 11.5 |
| `COMPACT_MILITIA` | Compact Yards | Free Systems Compact | Small | 4,200 | 50 | 260 | 175 | 7 | 150 | 120 | 2 | 6.8 / 8.4 |
| `ASTER_RAPTOR` | Aster Dynamics | Civilian | Small | 8,000 | 30 | 420 | 280 | 10 | 80 | 150 | 0 | 6.2 / 7.8 |
| `COMMONWEALTH_VALIANT` | Titan Forge | Commonwealth Navy | Medium | 14,000 | 180 | 220 | 150 | 6 | 500 | 400 | 40 | 8.5 / 10.2 |
| `COMMONWEALTH_INTREPID` | Titan Forge | Commonwealth Navy | Medium | 24,000 | 450 | 200 | 135 | 5 | 1,200 | 800 | 120 | 10.5 / 12.8 |
| `COMPACT_RANGER` | Compact Yards | Civilian | Medium | 17,500 | 420 | 220 | 150 | 5 | 900 | 600 | 150 | 25.5 / 32.8 |
| `ASTER_CLIPPER` | Aster Dynamics | Civilian | Medium | 12,500 | 600 | 300 | 200 | 6 | 500 | 600 | 150 | 22.5 / 28.8 |

`*` Shields and jump ranges are canonical metadata prepared for later systems;
the current native game does not simulate shields, boost, or FSD travel.

The native simulation previously hard-coded Sidewinder acceleration 130,
turn rate 2.2 rad/s, hull 100, fuel 100, and cargo 8. Those values remain the
starter baseline. For the seven hulls specified by World Bible v0.10, top
speed, boost, handling, hull, shields, mass, cargo, jump range, hardpoints, and
utility counts match the Bible. Purchase price, acceleration, turn rate, fuel,
sensors, and capability flags remain native gameplay tuning where the Bible
supplies no directly usable value; acceleration is calibrated so each rated top
speed is attainable under existing flight drag. Maintenance is not simulated.
The v0.10 core/optional slots above
Size 4 are capped to `SIZE_4` in this Batch 13 foundation; expanding the size
taxonomy belongs with outfitting rather than silently inventing working Size
5-8 mechanics.

Top speed, acceleration, turn rate, base hull, fuel, mass lookup, purchase cost,
mining compatibility, and cargo capacity now come from the active definition.
Engine/hull upgrades and fitted modules remain per owned ship.

## Shipyards and pads

Kepler supports Small and Medium pads but carries only the early Titan Mule and
Compact Militia. Cinder supports Small and Medium pads and carries the Raptor,
Clipper, Ranger, Valiant, and Intrepid. Sidewinder is a starter and is not sold.
No Large-pad hull is in the Batch 13 fleet.

`SHIPYARD LIST`, `SHIPYARD OWNED`, `SHIPYARD BUY <hull-id>`, and
`SHIPYARD SWITCH <instance-id>` are server-validated. A purchase requires a
docked, operational active ship, local availability, pad support, a known hull,
and sufficient credits. Saves are atomic and failures restore credits and fleet
state. Batch 13 permits one instance of each hull, which makes repeated purchase
requests and reconnect replay idempotent while the ownership schema itself can
represent multiple arbitrary instances. Switching requires both vessels to be
docked at the same station.

## Persistence and migration

The existing `H` commander record remains a legacy projection of the active
ship so established fields and clients remain readable. New `S` records are the
authoritative owned instances, including the visual-variant ID. Earlier Batch
13 `S` records lacking that trailing field load with the definition's default
variant. Old records without `S` lines migrate to exactly
one deterministic `SIDEWINDER` instance derived from commander identity and
sequence 1. The migration writes atomically, and loading the migrated file does
not create another ship.

Credits, XP, missions, reputation, career state, GalNet, cargo, location,
velocity, hull condition, fuel, upgrades, modules, and credentials retain their
existing ownership and migration behavior. New organization standings missing
from an older reputation summary are added neutrally.

## Visual foundation

Manufacturer profiles encode silhouette, construction, materials, and aging for
Aster Dynamics, Titan Forge, Orion Extraction Group, Horizon Systems, Compact
Yards, Meridian Shipyards, and Helix Interstellar. Operator profiles encode
Commonwealth, Directorate, Compact, civilian, and Vanta treatments. Vanta is an
operator/conversion identity, not a standardized manufacturer.

Human-engineered, Khepri-grown, and Vael-mathematically-constructed technology
profiles are separate. No Khepri or Vael vessel is placed in a player shipyard,
and no Architect, Choir, or Null ship is defined. The core renderer selects
distinct current-fleet silhouettes for Aster, Titan, Compact, and the starter,
with separate medium-hull shapes rather than scaled-up small craft;
unimplemented manufacturer families remain metadata for future authored assets,
not a procedural ship generator.

The metadata supports a future path of hull definition → manufacturer profile
→ instance visual variant/operator treatment → livery → wear. The current
renderer uses the active hull's family and silhouette only; variant paint,
livery, visible equipment, and full damage/wear layers remain deferred.

## Canon reconciliation

The v0.10 World Bible was added to the working tree during final validation and
its ship sections were reconciled directly. It confirms the Clipper as an Aster
Dynamics-built medium fast freighter; the native registry records
`ASTER_CLIPPER` as Aster-built and civilian-operated. It also confirms Valiant
and Intrepid as Titan Forge licensed construction under Commonwealth identity.
The legacy browser registry still contains Elite-like placeholder hulls and a
different balance scale and is not used as native fleet canon. Sidewinder does
not appear in v0.10's ship specification tables, so its existing native starter
identity and balance are intentionally preserved for compatibility.
