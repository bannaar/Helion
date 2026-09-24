#pragma once

#include "shared/ships.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace helion::shipyard {

struct Offer {
  int station = 0;
  std::string_view hullId;
  std::string_view factionId;
  std::string_view manufacturerId;
  std::string_view reputationId;
  int minimumReputation = 0;
  std::string_view permitId;
  int minimumMilitaryRank = 0;
  bool criminalContactRequired = false;
  std::string_view worldEventId;
};

const std::vector<Offer>& inventory();
std::vector<const ships::Definition*> availableAt(int station, ships::PadSize maximumPad,
  const std::map<std::string, int>& reputation);
bool available(int station, ships::PadSize maximumPad, std::string_view hullId,
  const std::map<std::string, int>& reputation);

} // namespace helion::shipyard
