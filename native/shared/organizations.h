#pragma once

#include <array>
#include <map>
#include <string>
#include <string_view>

namespace helion::organizations {

inline constexpr int kMinStanding = -100;
inline constexpr int kMaxStanding = 100;

struct Definition {
  std::string_view id;
  std::string_view displayName;
  std::string_view wireName;
};

inline constexpr std::array<Definition, 5> kRegistry{{
  {"authority.kepler", "Kepler Authority", "Kepler_Authority"},
  {"corp.orion", "Orion Extraction Group", "Orion_Extraction_Group"},
  {"criminal.vanta", "Vanta Syndicate", "Vanta_Syndicate"},
  {"criminal.red_wake", "Red Wake", "Red_Wake"},
  {"faction.commonwealth", "Helion Commonwealth", "Helion_Commonwealth"}
}};

const Definition* find(std::string_view id);
int clampStanding(int value);
const char* standingLabel(int value);
std::string organizationSummary();
std::string reputationSummary(const std::map<std::string, int>& standings);
bool parseReputationSummary(std::string_view value, std::map<std::string, int>& standings);

} // namespace helion::organizations
