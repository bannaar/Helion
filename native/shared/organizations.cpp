#include "shared/organizations.h"

#include <charconv>
#include <set>
#include <utility>

namespace helion::organizations {

const Definition* find(std::string_view id) {
  for (const auto& definition : kRegistry)
    if (definition.id == id) return &definition;
  return nullptr;
}

int clampStanding(int value) {
  return value < kMinStanding ? kMinStanding : value > kMaxStanding ? kMaxStanding : value;
}

const char* standingLabel(int value) {
  if (value <= -75) return "Hostile";
  if (value <= -25) return "Unfriendly";
  if (value < 25) return "Neutral";
  if (value < 75) return "Friendly";
  return "Allied";
}

std::string organizationSummary() {
  std::string result;
  for (const auto& definition : kRegistry) {
    if (!result.empty()) result += ',';
    result += definition.id;
    result += ':';
    result += definition.wireName;
  }
  return result;
}

std::string reputationSummary(const std::map<std::string, int>& standings) {
  std::string result;
  for (const auto& definition : kRegistry) {
    if (!result.empty()) result += ',';
    const auto found = standings.find(std::string(definition.id));
    const int value = found == standings.end() ? 0 : clampStanding(found->second);
    result += definition.id;
    result += '=';
    result += std::to_string(value);
    result += ':';
    result += standingLabel(value);
  }
  return result;
}

bool parseReputationSummary(std::string_view value, std::map<std::string, int>& standings) {
  std::map<std::string, int> parsed;
  std::set<std::string> seen;
  std::size_t start = 0;
  while (start <= value.size()) {
    const auto end = value.find(',', start);
    const auto entry = value.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
    const auto equals = entry.find('=');
    const auto colon = entry.find(':', equals == std::string_view::npos ? 0 : equals + 1);
    if (equals == std::string_view::npos || colon == std::string_view::npos ||
        equals == 0 || colon <= equals + 1 || colon + 1 >= entry.size()) return false;
    const auto id = entry.substr(0, equals);
    const auto number = entry.substr(equals + 1, colon - equals - 1);
    const auto label = entry.substr(colon + 1);
    if (!find(id) || !seen.emplace(id).second) return false;
    int standing = 0;
    const auto result = std::from_chars(number.data(), number.data() + number.size(), standing);
    if (result.ec != std::errc{} || result.ptr != number.data() + number.size() ||
        standing < kMinStanding || standing > kMaxStanding) return false;
    if (label != standingLabel(standing)) return false;
    parsed.emplace(id, standing);
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  if (parsed.empty()) return false;
  // Older saves contain only the organizations known when they were written.
  // New canonical standings are introduced neutrally during load.
  for (const auto& definition : kRegistry) parsed.emplace(definition.id, 0);
  standings = std::move(parsed);
  return true;
}

} // namespace helion::organizations
