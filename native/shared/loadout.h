#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace helion::loadout {

enum class Slot { mining = 0, engine = 1, defense = 2, weapon = 3 };
inline constexpr std::size_t kSlotCount = 4;

struct ModuleDefinition {
  const char* id;
  const char* name;
  Slot slot;
  int price;
  double miningCooldownMultiplier;
  double fuelConsumptionMultiplier;
  int hullBonus;
  double weaponRange;
  int weaponDamage;
  double weaponCooldown;
};

inline constexpr std::array<ModuleDefinition, 7> kCatalogue{{
  {"mining-basic", "Basic Extractor", Slot::mining, 0, 1.0, 1.0, 0, 0, 0, 0},
  {"mining-mk2", "Prospector Extractor", Slot::mining, 800, 0.65, 1.0, 0, 0, 0, 0},
  {"engine-basic", "Standard Drive", Slot::engine, 0, 1.0, 1.0, 0, 0, 0, 0},
  {"engine-efficient", "Efficient Drive", Slot::engine, 700, 1.0, 0.65, 0, 0, 0, 0},
  {"hull-standard", "Standard Plating", Slot::defense, 0, 1.0, 1.0, 0, 0, 0, 0},
  {"hull-plating", "Reinforced Plating", Slot::defense, 900, 1.0, 1.0, 25, 0, 0, 0},
  {"pulse-laser", "Pulse Laser", Slot::weapon, 650, 1.0, 1.0, 0, 240, 25, 1.0}
}};

inline const ModuleDefinition* find(std::string_view id) {
  for (const auto& module : kCatalogue)
    if (id == module.id) return &module;
  return nullptr;
}

inline constexpr const char* slotName(Slot slot) {
  switch (slot) {
    case Slot::mining: return "mining";
    case Slot::engine: return "engine";
    case Slot::defense: return "defense";
    case Slot::weapon: return "weapon";
  }
  return "unknown";
}

inline constexpr std::size_t slotIndex(Slot slot) {
  return static_cast<std::size_t>(slot);
}

} // namespace helion::loadout
