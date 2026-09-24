#include "shared/shipyard.h"

namespace helion::shipyard {
namespace {
const std::vector<Offer> kInventory{
  {0, "TITAN_MULE", "", "TITAN_FORGE", "", 0, "", 0, false, ""},
  {0, "COMPACT_MILITIA", "faction.compact", "COMPACT_YARDS", "", 0, "", 0, false, ""},
  {1, "ASTER_RAPTOR", "", "ASTER_DYNAMICS", "", 0, "", 0, false, ""},
  {1, "ASTER_CLIPPER", "", "ASTER_DYNAMICS", "", 0, "", 0, false, ""},
  {1, "COMPACT_RANGER", "faction.compact", "COMPACT_YARDS", "", 0, "", 0, false, ""},
  {1, "COMMONWEALTH_VALIANT", "faction.commonwealth", "TITAN_FORGE", "", 0, "", 0, false, ""},
  {1, "COMMONWEALTH_INTREPID", "faction.commonwealth", "TITAN_FORGE", "", 0, "", 0, false, ""}
};
}

const std::vector<Offer>& inventory() { return kInventory; }

std::vector<const ships::Definition*> availableAt(int station, ships::PadSize maximumPad,
  const std::map<std::string, int>& reputation) {
  std::vector<const ships::Definition*> result;
  for (const auto& offer : kInventory) {
    if (offer.station != station) continue;
    const auto* definition = ships::find(offer.hullId);
    if (!definition || !ships::supportsPad(maximumPad, definition->padSize)) continue;
    if (!offer.reputationId.empty()) {
      const auto standing = reputation.find(std::string(offer.reputationId));
      if (standing == reputation.end() || standing->second < offer.minimumReputation) continue;
    }
    result.push_back(definition);
  }
  return result;
}

bool available(int station, ships::PadSize maximumPad, std::string_view hullId,
  const std::map<std::string, int>& reputation) {
  for (const auto* definition : availableAt(station, maximumPad, reputation))
    if (definition->id == hullId) return true;
  return false;
}

} // namespace helion::shipyard
