#include "shared/organizations.h"

#include <iostream>
#include <map>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main() {
  using namespace helion::organizations;
  check(find("authority.kepler") && find("authority.kepler")->displayName == "Kepler Authority", "Kepler registry entry");
  check(find("corp.orion") && find("corp.orion")->displayName == "Orion Extraction Group", "Orion registry entry");
  check(find("criminal.vanta") && find("criminal.vanta")->displayName == "Vanta Syndicate", "Vanta registry entry");
  check(find("criminal.red_wake") && find("criminal.red_wake")->displayName == "Red Wake", "Red Wake registry entry");
  check(find("faction.commonwealth") && find("faction.commonwealth")->displayName == "Helion Commonwealth", "Commonwealth registry entry");
  check(find("unknown.organization") == nullptr, "unknown organization rejected");
  check(std::string(standingLabel(-100)) == "Hostile" && std::string(standingLabel(-75)) == "Hostile", "hostile threshold");
  check(std::string(standingLabel(-74)) == "Unfriendly" && std::string(standingLabel(-25)) == "Unfriendly", "unfriendly threshold");
  check(std::string(standingLabel(-24)) == "Neutral" && std::string(standingLabel(24)) == "Neutral", "neutral threshold");
  check(std::string(standingLabel(25)) == "Friendly" && std::string(standingLabel(74)) == "Friendly", "friendly threshold");
  check(std::string(standingLabel(75)) == "Allied" && std::string(standingLabel(100)) == "Allied", "allied threshold");
  std::map<std::string, int> neutral;
  for (const auto& definition : kRegistry) neutral.emplace(definition.id, 0);
  const auto summary = reputationSummary(neutral);
  std::map<std::string, int> restored;
  check(parseReputationSummary(summary, restored) && restored == neutral, "reputation summary round trip");
  check(!parseReputationSummary("unknown.organization=0:Neutral", restored), "unknown reputation id rejected");
  check(!parseReputationSummary("authority.kepler=101:Allied", restored), "out-of-range reputation rejected");
  return failures == 0 ? 0 : 1;
}
