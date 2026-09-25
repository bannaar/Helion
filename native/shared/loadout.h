#pragma once

#include "shared/ships.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace helion::loadout {

// Stable fitted-instance addresses used by the current persistence and command
// protocol. Physical slot type and size live on ModuleDefinition below.
enum class Slot { mining = 0, engine = 1, defense = 2, weapon = 3 };
inline constexpr std::size_t kSlotCount = 4;

enum class SlotType {
  weaponHardpoint,
  utility,
  coreInternal,
  optionalInternal
};

enum class Category {
  miningTool,
  thrusters,
  hullReinforcement,
  weapon
};

enum class CoreSystem {
  none,
  powerPlant,
  thrusters,
  frameShiftDrive,
  lifeSupport,
  powerDistributor,
  sensors,
  fuelSystem
};

enum class LegalStatus { legal, restricted, illegal };

struct ModuleDefinition {
  const char* id;
  const char* displayName;
  const char* manufacturerId;
  Category category;
  Slot slot;
  SlotType slotType;
  ships::SlotSize size;
  CoreSystem coreSystem;
  char grade;
  int purchasePrice;
  double mass;
  double powerDraw;
  int integrity;
  LegalStatus legalStatus;
  const char* requiredFactionId;
  int minimumReputation;
  int minimumRank;
  const char* requiredPermitId;
  bool testAvailable;
  double miningCooldownMultiplier;
  double fuelConsumptionMultiplier;
  int hullBonus;
  double weaponRange;
  int weaponDamage;
  double weaponCooldown;
};

inline constexpr std::size_t kCatalogueSize = 7;
extern const std::array<ModuleDefinition, kCatalogueSize> kCatalogue;

enum class FitIssue {
  none,
  slotTypeUnavailable,
  sizeTooLarge,
  coreSystemUnavailable
};

struct FitCompatibility {
  bool compatible = false;
  FitIssue issue = FitIssue::slotTypeUnavailable;
};

const ModuleDefinition* find(std::string_view id);
const char* slotName(Slot slot);
const char* slotTypeName(SlotType type);
const char* categoryName(Category category);
const char* coreSystemName(CoreSystem system);
const char* legalStatusName(LegalStatus status);
const char* fitIssueName(FitIssue issue);
std::string effectSummary(const ModuleDefinition& module);
std::size_t slotIndex(Slot slot);
FitCompatibility compatibility(const ModuleDefinition& module, const ships::Definition& hull);
bool validateRegistry();

} // namespace helion::loadout
