#include "shared/flight.h"
#include "shared/owned_ship.h"
#include "shared/ships.h"
#include "shared/shipyard.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <string>

namespace {
void check(bool condition, const char* label) {
  if (!condition) { std::cerr << "FAILED: " << label << '\n'; std::exit(1); }
}
}

int main() {
  using namespace helion;
  check(ships::validateRegistry(), "canonical ship registry validates");
  check(ships::registry().size() == 8, "representative playable fleet is exact");
  check(ships::find("ASTER_RAPTOR") != nullptr, "valid ship definition lookup");
  check(ships::find("NOT_A_HULL") == nullptr, "invalid ship definition lookup");

  const auto* raptor = ships::find("ASTER_RAPTOR");
  const auto* mule = ships::find("TITAN_MULE");
  const auto* intrepid = ships::find("COMMONWEALTH_INTREPID");
  const auto* ranger = ships::find("COMPACT_RANGER");
  const auto* clipper = ships::find("ASTER_CLIPPER");
  check(raptor && raptor->manufacturerId == "ASTER_DYNAMICS" && raptor->maneuverability == 10,
        "manufacturer metadata and interceptor identity");
  check(intrepid && intrepid->manufacturerId == "TITAN_FORGE" && intrepid->operatorId == "COMMONWEALTH_NAVY",
        "manufacturer remains separate from operator");
  check(clipper && clipper->manufacturerId == "ASTER_DYNAMICS" && clipper->operatorId == "CIVILIAN",
        "Clipper reconciliation is encoded explicitly");
  check(ranger && ranger->scienceCapability && ranger->jumpRangeUnladen > intrepid->jumpRangeUnladen,
        "Ranger is a long-range science platform");
  check(ships::findManufacturer("ORION_EXTRACTION_GROUP") != nullptr &&
        ships::findManufacturer("HORIZON_SYSTEMS") != nullptr &&
        ships::findManufacturer("MERIDIAN_SHIPYARDS") != nullptr &&
        ships::findManufacturer("HELIX_INTERSTELLAR") != nullptr,
        "future manufacturer visual profiles exist");
  check(ships::findTechnologyProfile(ships::TechnologyFamily::khepri_grown)->silhouetteLanguage.find("carapace") != std::string_view::npos &&
        ships::findTechnologyProfile(ships::TechnologyFamily::vael_constructed)->constructionLanguage.find("reconfiguration") != std::string_view::npos,
        "alien visual families remain distinct and non-player registries");
  check(raptor->silhouetteFamily != mule->silhouetteFamily && !raptor->wearProfile.empty() && !mule->wearProfile.empty(),
        "visual profile metadata supports silhouette and manufacturer aging");
  check(ships::findVisualVariant("SMUGGLER_CONVERSION") &&
        ships::findVisualVariant("SMUGGLER_CONVERSION")->operatorId == "VANTA_CONVERSION" &&
        ships::findVisualVariant("PROTOTYPE") &&
        ships::defaultVisualVariant(*intrepid) == "COMMONWEALTH_NAVY" &&
        ships::defaultVisualVariant(*clipper) == "CIVILIAN",
        "visual variants are distinct from hulls, manufacturers and operators");

  struct CanonTarget {
    const char* id;
    int mass, speed, boost, handling, hull, shields, cargo;
    double laden, unladen;
    std::size_t hardpoints, utilities;
  };
  constexpr std::array canonTargets{
    CanonTarget{"TITAN_MULE", 45, 180, 260, 4, 120, 60, 14, 9.2, 11.5, 0, 2},
    CanonTarget{"COMPACT_MILITIA", 50, 260, 380, 7, 150, 120, 2, 6.8, 8.4, 4, 2},
    CanonTarget{"ASTER_RAPTOR", 30, 420, 620, 10, 80, 150, 0, 6.2, 7.8, 4, 2},
    CanonTarget{"COMMONWEALTH_VALIANT", 180, 220, 340, 6, 500, 400, 40, 8.5, 10.2, 4, 3},
    CanonTarget{"COMMONWEALTH_INTREPID", 450, 200, 310, 5, 1200, 800, 120, 10.5, 12.8, 6, 4},
    CanonTarget{"COMPACT_RANGER", 420, 220, 340, 5, 900, 600, 150, 25.5, 32.8, 2, 4},
    CanonTarget{"ASTER_CLIPPER", 600, 300, 450, 6, 500, 600, 150, 22.5, 28.8, 2, 4},
  };
  for (const auto& target : canonTargets) {
    const auto* hull = ships::find(target.id);
    check(hull && hull->mass == target.mass && hull->topSpeed == target.speed &&
          hull->boostSpeed == target.boost && hull->maneuverability == target.handling &&
          hull->baseHull == target.hull && hull->baseShields == target.shields &&
          hull->cargoCapacity == target.cargo &&
          std::abs(hull->jumpRangeLaden - target.laden) < 0.001 &&
          std::abs(hull->jumpRangeUnladen - target.unladen) < 0.001 &&
          hull->hardpoints.size() == target.hardpoints && hull->utilityMounts.size() == target.utilities,
          "World Bible v0.10 ship specification target");
    flight::State straightFlight;
    straightFlight.docked = false;
    straightFlight.x = 80;
    straightFlight.y = -1000;
    for (int frame = 0; frame < 360; ++frame) {
      straightFlight.input = {1, 0, 0};
      straightFlight.inputAge = 0;
      flight::step(straightFlight, 1.0 / 60, *hull);
    }
    check(flight::speed(straightFlight) >= hull->topSpeed * 0.99 &&
          flight::speed(straightFlight) <= hull->topSpeed + 0.001,
          "rated top speed is attainable without boost or upgrades");
  }

  const auto legacyId = ships::deterministicInstanceId("pilot", 1);
  check(legacyId == ships::deterministicInstanceId("pilot", 1) &&
        legacyId != ships::deterministicInstanceId("pilot", 2), "deterministic unique migration instance IDs");
  auto starter = ships::makeStarterShip("pilot");
  check(starter.hullId == "SIDEWINDER" && ships::validate(starter) && !starter.stored,
        "owned starter instance is valid");
  check(starter.visualVariantId == "CIVILIAN", "starter instance has a persistent visual variant");
  auto purchased = ships::makePurchasedShip(ships::deterministicInstanceId("pilot", 2), *mule, 0);
  check(purchased.stored && purchased.flight.docked && purchased.flight.hull == mule->baseHull &&
        purchased.flight.maxFuel == mule->fuelCapacity, "purchased ship instance starts stored at the shipyard");
  purchased.visualVariantId = "SMUGGLER_CONVERSION";
  check(ships::validate(purchased) && purchased.hullId == "TITAN_MULE" &&
        ships::findVisualVariant(purchased.visualVariantId)->operatorId == "VANTA_CONVERSION",
        "individual visual conversion leaves the manufactured hull unchanged");
  auto invalidCondition = purchased;
  invalidCondition.flight.hull = invalidCondition.flight.maxHull + 1;
  check(!ships::validate(invalidCondition), "owned ship rejects hull condition above derived capacity");
  invalidCondition = purchased;
  invalidCondition.flight.fuel = invalidCondition.flight.maxFuel + 1;
  check(!ships::validate(invalidCondition), "owned ship rejects fuel above derived capacity");
  invalidCondition = purchased;
  invalidCondition.flight.cargo = std::numeric_limits<int>::max();
  invalidCondition.flight.food = std::numeric_limits<int>::max();
  check(!ships::validate(invalidCondition), "owned ship rejects oversized cargo without integer overflow");

  std::map<std::string, int> neutral;
  const auto kepler = shipyard::availableAt(0, flight::kStations[0].maximumPad, neutral);
  const auto cinder = shipyard::availableAt(1, flight::kStations[1].maximumPad, neutral);
  check(kepler.size() == 2 && shipyard::available(0, flight::kStations[0].maximumPad, "TITAN_MULE", neutral),
        "Kepler exposes bounded early inventory");
  check(!shipyard::available(0, flight::kStations[0].maximumPad, "ASTER_RAPTOR", neutral) && cinder.size() == 5,
        "inventory is station-specific");

  flight::State muleFlight, raptorFlight;
  muleFlight.docked = false; raptorFlight.docked = false;
  muleFlight.input = {1, 1, 0}; raptorFlight.input = {1, 1, 0};
  muleFlight.inputAge = 0; raptorFlight.inputAge = 0;
  for (int i = 0; i < 60; ++i) {
    flight::step(muleFlight, 1.0 / 60, *mule);
    flight::step(raptorFlight, 1.0 / 60, *raptor);
  }
  check(flight::speed(raptorFlight) > flight::speed(muleFlight) &&
        std::abs(raptorFlight.yaw) > std::abs(muleFlight.yaw), "hull-specific acceleration and handling affect flight");
  check(flight::speed(raptorFlight) <= raptor->topSpeed + 0.001, "hull top speed is enforced");

  int credits = 100000;
  flight::State cargoState;
  check(flight::trade(cargoState, credits, true, "food", mule->cargoCapacity, mule->cargoCapacity).rfind("OK", 0) == 0 &&
        flight::trade(cargoState, credits, true, "food", 1, mule->cargoCapacity) == "ERR cargo-full",
        "hull cargo capacity is enforced");
  check(intrepid->mass > raptor->mass && intrepid->mass > 0, "ship mass is queryable for future Rift limits");

  std::cout << "ship registry, ownership, shipyard, movement and cargo tests passed\n";
}
