#pragma once

#include "shared/flight.h"
#include "shared/loadout.h"
#include "shared/ships.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace helion::ships {

inline constexpr std::string_view kWearFactory = "FACTORY";
inline constexpr std::string_view kWearUsed = "USED";
inline constexpr std::string_view kWearWeathered = "WEATHERED";
inline constexpr std::string_view kWearFrontierWorn = "FRONTIER_WORN";
inline constexpr std::string_view kWearDamaged = "DAMAGED";
inline constexpr std::string_view kWearFieldRepaired = "FIELD_REPAIRED";

struct OwnedShip {
  std::string instanceId;
  std::string hullId;
  std::string customName;
  flight::State flight;
  int hullLevel = 1;
  int engineLevel = 1;
  std::vector<std::string> ownedModules{"mining-basic", "engine-basic", "hull-standard"};
  std::array<std::string, loadout::kSlotCount> fittedModules{{
    "mining-basic", "engine-basic", "hull-standard", ""
  }};
  std::string visualVariantId;
  std::string livery;
  std::string wearState = std::string(kWearFactory);
  bool stored = false;
};

std::string deterministicInstanceId(std::string_view commander, unsigned sequence);
OwnedShip makeStarterShip(std::string_view commander, const flight::State& legacyFlight = {},
  int hullLevel = 1, int engineLevel = 1);
OwnedShip makePurchasedShip(std::string instanceId, const Definition& definition, int station);
const Definition& definition(const OwnedShip& ship);
int cargoCapacity(const OwnedShip& ship);
int hullCapacity(const OwnedShip& ship, int moduleHullBonus = 0);
double fuelCapacity(const OwnedShip& ship);
void refreshDerivedState(OwnedShip& ship, int moduleHullBonus = 0);
bool validWearState(std::string_view value);
bool validate(const OwnedShip& ship);

} // namespace helion::ships
