#include "shared/loadout.h"
#include "shared/ships.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
void check(bool condition, const char* label) {
  if (!condition) { std::cerr << "FAILED: " << label << '\n'; std::exit(1); }
}
}

int main() {
  using namespace helion;
  check(loadout::validateRegistry(), "canonical module registry validates");
  check(loadout::kCatalogue.size() == 7, "representative module set remains bounded");
  check(loadout::find("pulse-laser") != nullptr, "valid module lookup");
  check(loadout::find("not-a-module") == nullptr, "invalid module lookup");

  const auto* sidewinder = ships::find("SIDEWINDER");
  const auto* mule = ships::find("TITAN_MULE");
  const auto* laser = loadout::find("pulse-laser");
  const auto* efficient = loadout::find("engine-efficient");
  check(sidewinder && mule && laser && efficient, "test registry definitions exist");
  check(laser->slotType == loadout::SlotType::weaponHardpoint &&
        laser->size == ships::SlotSize::size1 && laser->purchasePrice == 650 &&
        std::string_view(laser->manufacturerId) == "TITAN_FORGE",
        "module definition carries physical slot size price and manufacturer");
  check(efficient->slotType == loadout::SlotType::coreInternal &&
        efficient->coreSystem == loadout::CoreSystem::thrusters &&
        efficient->fuelConsumptionMultiplier < 1.0,
        "core module identifies its canonical core system and gameplay effect");
  check(loadout::effectSummary(*efficient) == "fuel-use-x0.65" &&
        loadout::effectSummary(*laser).find("damage-25") != std::string::npos,
        "module effects have deterministic player-facing summaries");

  check(loadout::compatibility(*laser, *sidewinder).compatible,
        "size-one weapon fits a size-one hardpoint");
  const auto muleLaser = loadout::compatibility(*laser, *mule);
  check(!muleLaser.compatible && muleLaser.issue == loadout::FitIssue::slotTypeUnavailable,
        "weapon is rejected when hull has no weapon hardpoint");

  auto oversizedLaser = *laser;
  oversizedLaser.size = ships::SlotSize::size2;
  const auto sidewinderOversize = loadout::compatibility(oversizedLaser, *sidewinder);
  check(!sidewinderOversize.compatible && sidewinderOversize.issue == loadout::FitIssue::sizeTooLarge,
        "oversized weapon is rejected deterministically");

  auto utility = *laser;
  utility.slotType = loadout::SlotType::utility;
  utility.coreSystem = loadout::CoreSystem::none;
  utility.size = ships::SlotSize::size1;
  check(loadout::compatibility(utility, *sidewinder).compatible,
        "utility mount compatibility uses canonical hull utility slots");
  utility.size = ships::SlotSize::size2;
  check(loadout::compatibility(utility, *sidewinder).issue == loadout::FitIssue::sizeTooLarge,
        "utility size boundary is enforced");

  auto plant = *efficient;
  plant.coreSystem = loadout::CoreSystem::powerPlant;
  plant.size = ships::SlotSize::size1;
  check(loadout::compatibility(plant, *sidewinder).compatible,
        "ordered core-system slot compatibility is explicit");
  plant.size = ships::SlotSize::size2;
  check(loadout::compatibility(plant, *sidewinder).issue == loadout::FitIssue::sizeTooLarge,
        "core-system size boundary is enforced");

  check(std::string_view(loadout::slotTypeName(loadout::SlotType::weaponHardpoint)) == "WEAPON_HARDPOINT" &&
        std::string_view(loadout::slotTypeName(loadout::SlotType::utility)) == "UTILITY" &&
        std::string_view(loadout::slotTypeName(loadout::SlotType::coreInternal)) == "CORE_INTERNAL" &&
        std::string_view(loadout::slotTypeName(loadout::SlotType::optionalInternal)) == "OPTIONAL_INTERNAL",
        "all canonical physical slot concepts have stable names");

  std::cout << "module registry and physical compatibility tests passed\n";
}
