#include "shared/owned_ship.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace helion::ships {

std::string deterministicInstanceId(std::string_view commander, unsigned sequence) {
  // Stable FNV-1a makes legacy migration deterministic without relying on an
  // implementation-defined std::hash or storing a transient migration nonce.
  std::uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char ch : commander) { hash ^= ch; hash *= 1099511628211ULL; }
  std::ostringstream out;
  out << "ship-" << std::hex << std::setfill('0') << std::setw(16) << hash
      << '-' << std::dec << std::setw(4) << sequence;
  return out.str();
}

OwnedShip makeStarterShip(std::string_view commander, const flight::State& legacyFlight,
  int hullLevel, int engineLevel) {
  OwnedShip ship;
  ship.instanceId = deterministicInstanceId(commander, 1);
  ship.hullId = "SIDEWINDER";
  ship.customName = "Sidewinder";
  ship.flight = legacyFlight;
  ship.hullLevel = std::clamp(hullLevel, 1, 5);
  ship.engineLevel = std::clamp(engineLevel, 1, 5);
  ship.visualVariantId = std::string(defaultVisualVariant(sidewinder()));
  ship.livery = std::string(sidewinder().defaultLiveryFamily);
  ship.wearState = std::string(kWearUsed);
  refreshDerivedState(ship);
  return ship;
}

OwnedShip makePurchasedShip(std::string instanceId, const Definition& hull, int station) {
  OwnedShip ship;
  ship.instanceId = std::move(instanceId);
  ship.hullId = std::string(hull.id);
  ship.customName = std::string(hull.model);
  ship.flight = {};
  ship.flight.station = station;
  ship.flight.x = flight::kStations[static_cast<std::size_t>(station)].x;
  ship.flight.y = flight::kStations[static_cast<std::size_t>(station)].y;
  ship.visualVariantId = std::string(defaultVisualVariant(hull));
  ship.livery = std::string(hull.defaultLiveryFamily);
  ship.wearState = std::string(kWearFactory);
  ship.stored = true;
  refreshDerivedState(ship);
  ship.flight.hull = ship.flight.maxHull;
  ship.flight.fuel = ship.flight.maxFuel;
  return ship;
}

const Definition& definition(const OwnedShip& ship) {
  const auto* found = find(ship.hullId);
  if (!found) throw std::logic_error("owned ship references unknown hull");
  return *found;
}

int cargoCapacity(const OwnedShip& ship) { return definition(ship).cargoCapacity; }
int hullCapacity(const OwnedShip& ship, int moduleHullBonus) {
  return definition(ship).baseHull + (std::clamp(ship.hullLevel, 1, 5) - 1) * 25 + std::max(0, moduleHullBonus);
}
double fuelCapacity(const OwnedShip& ship) {
  return definition(ship).fuelCapacity * (1.0 + 0.20 * (std::clamp(ship.engineLevel, 1, 5) - 1));
}
void refreshDerivedState(OwnedShip& ship, int moduleHullBonus) {
  ship.flight.maxHull = hullCapacity(ship, moduleHullBonus);
  ship.flight.hull = std::clamp(ship.flight.hull, 0, ship.flight.maxHull);
  ship.flight.maxFuel = fuelCapacity(ship);
  ship.flight.fuel = std::clamp(ship.flight.fuel, 0.0, ship.flight.maxFuel);
}
bool validWearState(std::string_view value) {
  return value == kWearFactory || value == kWearUsed || value == kWearWeathered ||
    value == kWearFrontierWorn || value == kWearDamaged || value == kWearFieldRepaired;
}
bool validate(const OwnedShip& ship) {
  const auto* hull = find(ship.hullId);
  if (!hull || ship.instanceId.empty() || ship.instanceId.size() > 64 || ship.customName.size() > 64 ||
      ship.hullLevel < 1 || ship.hullLevel > 5 || ship.engineLevel < 1 || ship.engineLevel > 5 ||
      ship.flight.station < 0 || ship.flight.station >= static_cast<int>(flight::kStations.size()) ||
      ship.flight.cargo < 0 || ship.flight.food < 0 || ship.flight.parts < 0 ||
      static_cast<long long>(ship.flight.cargo) + ship.flight.food + ship.flight.parts > hull->cargoCapacity ||
      !std::isfinite(ship.flight.fuel) || ship.flight.fuel < 0 || ship.flight.fuel > ship.flight.maxFuel ||
      !findVisualVariant(ship.visualVariantId) || !validWearState(ship.wearState) ||
      ship.livery.empty() || ship.livery.size() > 64) return false;
  std::unordered_set<std::string> modules;
  for (const auto& id : ship.ownedModules) if (!loadout::find(id) || !modules.emplace(id).second) return false;
  for (std::size_t slot = 0; slot < ship.fittedModules.size(); ++slot) {
    const auto& id = ship.fittedModules[slot];
    if (id.empty()) continue;
    const auto* module = loadout::find(id);
    if (!module || loadout::slotIndex(module->slot) != slot || modules.count(id) == 0) return false;
  }
  const auto& defenseId = ship.fittedModules[loadout::slotIndex(loadout::Slot::defense)];
  const auto* defense = defenseId.empty() ? nullptr : loadout::find(defenseId);
  const int hullBonus = defense ? defense->hullBonus : 0;
  return ship.flight.maxHull == hullCapacity(ship, hullBonus) &&
    std::abs(ship.flight.maxFuel - fuelCapacity(ship)) <= 0.001 && ship.flight.hull <= ship.flight.maxHull;
}

} // namespace helion::ships
