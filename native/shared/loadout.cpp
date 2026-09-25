#include "shared/loadout.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace helion::loadout {
namespace {
constexpr std::size_t coreIndex(CoreSystem system) {
  switch (system) {
    case CoreSystem::powerPlant: return 0;
    case CoreSystem::thrusters: return 1;
    case CoreSystem::frameShiftDrive: return 2;
    case CoreSystem::lifeSupport: return 3;
    case CoreSystem::powerDistributor: return 4;
    case CoreSystem::sensors: return 5;
    case CoreSystem::fuelSystem: return 6;
    case CoreSystem::none: break;
  }
  return 7;
}

template <typename Range, typename SizeAccessor>
FitCompatibility mountCompatibility(const Range& mounts, ships::SlotSize required, SizeAccessor size) {
  if (mounts.empty()) return {false, FitIssue::slotTypeUnavailable};
  const bool fits = std::any_of(mounts.begin(), mounts.end(), [&](const auto& mount) {
    return static_cast<int>(size(mount)) >= static_cast<int>(required);
  });
  return fits ? FitCompatibility{true, FitIssue::none} :
    FitCompatibility{false, FitIssue::sizeTooLarge};
}
}

const std::array<ModuleDefinition, kCatalogueSize> kCatalogue{{
  {"mining-basic", "Basic Extractor", "ORION_EXTRACTION_GROUP", Category::miningTool,
   Slot::mining, SlotType::optionalInternal, ships::SlotSize::size1, CoreSystem::none,
   'E', 0, 2.0, 1.0, 50, LegalStatus::legal, "", 0, 0, "", true,
   1.0, 1.0, 0, 0, 0, 0},
  {"mining-mk2", "Prospector Extractor", "ORION_EXTRACTION_GROUP", Category::miningTool,
   Slot::mining, SlotType::optionalInternal, ships::SlotSize::size1, CoreSystem::none,
   'C', 800, 3.0, 2.0, 70, LegalStatus::legal, "", 0, 0, "", true,
   0.65, 1.0, 0, 0, 0, 0},
  {"engine-basic", "Standard Drive", "INDEPENDENT_YARDS", Category::thrusters,
   Slot::engine, SlotType::coreInternal, ships::SlotSize::size1, CoreSystem::thrusters,
   'E', 0, 4.0, 2.0, 70, LegalStatus::legal, "", 0, 0, "", true,
   1.0, 1.0, 0, 0, 0, 0},
  {"engine-efficient", "Efficient Drive", "ASTER_DYNAMICS", Category::thrusters,
   Slot::engine, SlotType::coreInternal, ships::SlotSize::size1, CoreSystem::thrusters,
   'C', 700, 3.5, 1.5, 65, LegalStatus::legal, "", 0, 0, "", true,
   1.0, 0.65, 0, 0, 0, 0},
  {"hull-standard", "Standard Plating", "INDEPENDENT_YARDS", Category::hullReinforcement,
   Slot::defense, SlotType::optionalInternal, ships::SlotSize::size1, CoreSystem::none,
   'E', 0, 3.0, 0.0, 100, LegalStatus::legal, "", 0, 0, "", true,
   1.0, 1.0, 0, 0, 0, 0},
  {"hull-plating", "Reinforced Plating", "TITAN_FORGE", Category::hullReinforcement,
   Slot::defense, SlotType::optionalInternal, ships::SlotSize::size1, CoreSystem::none,
   'C', 900, 8.0, 0.0, 160, LegalStatus::legal, "", 0, 0, "", true,
   1.0, 1.0, 25, 0, 0, 0},
  {"pulse-laser", "Pulse Laser", "TITAN_FORGE", Category::weapon,
   Slot::weapon, SlotType::weaponHardpoint, ships::SlotSize::size1, CoreSystem::none,
   'C', 650, 4.0, 3.0, 80, LegalStatus::legal, "", 0, 0, "", true,
   1.0, 1.0, 0, 240, 25, 1.0}
}};

const ModuleDefinition* find(std::string_view id) {
  for (const auto& module : kCatalogue) if (id == module.id) return &module;
  return nullptr;
}

const char* slotName(Slot slot) {
  switch (slot) {
    case Slot::mining: return "mining";
    case Slot::engine: return "engine";
    case Slot::defense: return "defense";
    case Slot::weapon: return "weapon";
  }
  return "unknown";
}

const char* slotTypeName(SlotType type) {
  switch (type) {
    case SlotType::weaponHardpoint: return "WEAPON_HARDPOINT";
    case SlotType::utility: return "UTILITY";
    case SlotType::coreInternal: return "CORE_INTERNAL";
    case SlotType::optionalInternal: return "OPTIONAL_INTERNAL";
  }
  return "UNKNOWN";
}

const char* categoryName(Category category) {
  switch (category) {
    case Category::miningTool: return "MINING_TOOL";
    case Category::thrusters: return "THRUSTERS";
    case Category::hullReinforcement: return "HULL_REINFORCEMENT";
    case Category::weapon: return "WEAPON";
  }
  return "UNKNOWN";
}

const char* coreSystemName(CoreSystem system) {
  switch (system) {
    case CoreSystem::none: return "NONE";
    case CoreSystem::powerPlant: return "POWER_PLANT";
    case CoreSystem::thrusters: return "THRUSTERS";
    case CoreSystem::frameShiftDrive: return "FRAME_SHIFT_DRIVE";
    case CoreSystem::lifeSupport: return "LIFE_SUPPORT";
    case CoreSystem::powerDistributor: return "POWER_DISTRIBUTOR";
    case CoreSystem::sensors: return "SENSORS";
    case CoreSystem::fuelSystem: return "FUEL_SYSTEM";
  }
  return "UNKNOWN";
}

const char* legalStatusName(LegalStatus status) {
  switch (status) {
    case LegalStatus::legal: return "LEGAL";
    case LegalStatus::restricted: return "RESTRICTED";
    case LegalStatus::illegal: return "ILLEGAL";
  }
  return "UNKNOWN";
}

const char* fitIssueName(FitIssue issue) {
  switch (issue) {
    case FitIssue::none: return "none";
    case FitIssue::slotTypeUnavailable: return "slot-type-unavailable";
    case FitIssue::sizeTooLarge: return "module-size-too-large";
    case FitIssue::coreSystemUnavailable: return "core-system-unavailable";
  }
  return "unknown";
}

std::string effectSummary(const ModuleDefinition& module) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(2);
  if (module.miningCooldownMultiplier < 1.0)
    out << "mining-cooldown-x" << module.miningCooldownMultiplier;
  else if (module.fuelConsumptionMultiplier < 1.0)
    out << "fuel-use-x" << module.fuelConsumptionMultiplier;
  else if (module.hullBonus > 0)
    out << "maximum-hull-plus-" << module.hullBonus;
  else if (module.weaponDamage > 0)
    out << "range-" << module.weaponRange << "-damage-" << module.weaponDamage <<
      "-cooldown-" << module.weaponCooldown;
  else out << "baseline";
  return out.str();
}

std::size_t slotIndex(Slot slot) { return static_cast<std::size_t>(slot); }

FitCompatibility compatibility(const ModuleDefinition& module, const ships::Definition& hull) {
  switch (module.slotType) {
    case SlotType::weaponHardpoint:
      return mountCompatibility(hull.hardpoints, module.size,
        [](const ships::Hardpoint& hardpoint) { return hardpoint.size; });
    case SlotType::utility:
      return mountCompatibility(hull.utilityMounts, module.size,
        [](ships::SlotSize size) { return size; });
    case SlotType::optionalInternal:
      return mountCompatibility(hull.optionalSlots, module.size,
        [](ships::SlotSize size) { return size; });
    case SlotType::coreInternal: {
      const std::size_t index = coreIndex(module.coreSystem);
      if (index >= hull.coreSlots.size()) return {false, FitIssue::coreSystemUnavailable};
      return static_cast<int>(hull.coreSlots[index]) >= static_cast<int>(module.size) ?
        FitCompatibility{true, FitIssue::none} : FitCompatibility{false, FitIssue::sizeTooLarge};
    }
  }
  return {false, FitIssue::slotTypeUnavailable};
}

bool validateRegistry() {
  std::unordered_set<std::string_view> ids;
  for (const auto& module : kCatalogue) {
    if (!module.id || std::string_view(module.id).empty() || !ids.emplace(module.id).second ||
        !module.displayName || std::string_view(module.displayName).empty() ||
        !module.manufacturerId || !ships::findManufacturer(module.manufacturerId) ||
        module.grade < 'A' || module.grade > 'E' || module.purchasePrice < 0 ||
        !std::isfinite(module.mass) || module.mass < 0 || !std::isfinite(module.powerDraw) ||
        module.powerDraw < 0 || module.integrity <= 0 || module.minimumReputation < 0 ||
        module.minimumRank < 0 || !std::isfinite(module.miningCooldownMultiplier) ||
        module.miningCooldownMultiplier <= 0 || !std::isfinite(module.fuelConsumptionMultiplier) ||
        module.fuelConsumptionMultiplier <= 0 || module.hullBonus < 0 || module.weaponRange < 0 ||
        module.weaponDamage < 0 || module.weaponCooldown < 0) return false;
    if ((module.slot == Slot::weapon) != (module.slotType == SlotType::weaponHardpoint) ||
        (module.slot == Slot::engine) != (module.slotType == SlotType::coreInternal) ||
        (module.slotType == SlotType::coreInternal) != (module.coreSystem != CoreSystem::none)) return false;
    if (module.weaponDamage > 0 &&
        (module.slot != Slot::weapon || module.weaponRange <= 0 || module.weaponCooldown <= 0)) return false;
  }
  return true;
}

} // namespace helion::loadout
