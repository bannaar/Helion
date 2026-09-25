#include "shared/galaxy.h"

#include "shared/organizations.h"

#include <algorithm>
#include <cstdlib>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace helion::galaxy {
namespace {
constexpr SystemRole developed = SystemRole::developedInterior;
constexpr SystemRole resource = SystemRole::industrialResource;
constexpr SystemRole frontierGateway = SystemRole::frontier | SystemRole::borderGateway;
constexpr SystemRole backwaterDeadEnd = SystemRole::lowSecurityBackwater | SystemRole::deadEnd;

const std::array<RegionDefinition, kRegionCount> kRegions{{
  {"COMMONWEALTH_CORE", "Commonwealth Core"},
  {"KEPLER_REACH", "Kepler Reach"},
  {"COMPACT_MARCHES", "Compact Marches"},
  {"DIRECTORATE_CORE", "Directorate Core"},
  {"MERIDIAN_CORE", "Meridian Core"},
  {"OUTER_REACH", "Outer Reach"}
}};

const std::array<SystemDefinition, kSystemCount> kSystems{{
  {"CONCORDIA", "Concordia", "COMMONWEALTH_CORE", 25, SovereignType::npcFaction,
   "faction.commonwealth", DiscoveryState::charted, SystemRole::capital | developed, false},
  {"VANTAGE", "Vantage", "COMMONWEALTH_CORE", 21, SovereignType::npcFaction,
   "faction.commonwealth", DiscoveryState::charted, SystemRole::commerceHub | developed | resource, false},
  {"HOLLOWAY", "Holloway", "COMMONWEALTH_CORE", 14, SovereignType::npcFaction,
   "faction.commonwealth", DiscoveryState::charted, developed, false},
  {"HARROW", "Harrow", "COMMONWEALTH_CORE", 9, SovereignType::npcFaction,
   "faction.commonwealth", DiscoveryState::charted, developed | resource, false},
  {"KEPLER", "Kepler", "KEPLER_REACH", 3, SovereignType::npcFaction,
   "faction.commonwealth", DiscoveryState::charted, frontierGateway, false},
  {"GLOAM", "Gloam", "KEPLER_REACH", -5, SovereignType::npcFaction,
   "faction.commonwealth", DiscoveryState::charted, backwaterDeadEnd, false},

  {"FREESTEAD", "Freestead", "COMPACT_MARCHES", 19, SovereignType::npcFaction,
   "faction.compact", DiscoveryState::charted, SystemRole::capital | developed, false},
  {"CROSSROADS", "Crossroads", "COMPACT_MARCHES", 16, SovereignType::npcFaction,
   "faction.compact", DiscoveryState::charted, SystemRole::commerceHub | developed, false},
  {"IRONHOME", "Ironhome", "COMPACT_MARCHES", 9, SovereignType::npcFaction,
   "faction.compact", DiscoveryState::charted, developed | resource, false},
  {"FARPOINT", "Farpoint", "COMPACT_MARCHES", 2, SovereignType::npcFaction,
   "faction.compact", DiscoveryState::charted, frontierGateway, false},
  {"FARSIDE", "Farside", "COMPACT_MARCHES", -4, SovereignType::npcFaction,
   "faction.compact", DiscoveryState::charted, SystemRole::frontier | resource, false},
  {"RUSTHOLLOW", "Rusthollow", "COMPACT_MARCHES", -15, SovereignType::npcFaction,
   "faction.compact", DiscoveryState::charted, backwaterDeadEnd, false},

  {"SOLARIS", "Solaris", "DIRECTORATE_CORE", 25, SovereignType::npcFaction,
   "faction.directorate", DiscoveryState::charted, SystemRole::capital | developed, false},
  {"HELIOS_EXCHANGE", "Helios Exchange", "DIRECTORATE_CORE", 22, SovereignType::npcFaction,
   "faction.directorate", DiscoveryState::charted, SystemRole::commerceHub | developed, false},
  {"AEGIS", "Aegis", "DIRECTORATE_CORE", 15, SovereignType::npcFaction,
   "faction.directorate", DiscoveryState::charted, developed, false},
  {"SENTINEL", "Sentinel", "DIRECTORATE_CORE", 10, SovereignType::npcFaction,
   "faction.directorate", DiscoveryState::charted, developed | resource, false},
  {"WATCHPOINT", "Watchpoint", "DIRECTORATE_CORE", 2, SovereignType::npcFaction,
   "faction.directorate", DiscoveryState::charted, frontierGateway, false},
  {"ASHEN_RELAY", "Ashen Relay", "DIRECTORATE_CORE", -21, SovereignType::npcFaction,
   "faction.directorate", DiscoveryState::charted, backwaterDeadEnd, false},

  {"MERIDIAN_FORUM", "Meridian Forum", "MERIDIAN_CORE", 20, SovereignType::npcFaction,
   "faction.meridian", DiscoveryState::charted, SystemRole::capital | developed, false},
  {"MERIDIAN_PRIME", "Meridian Prime", "MERIDIAN_CORE", 23, SovereignType::npcFaction,
   "faction.meridian", DiscoveryState::charted, SystemRole::commerceHub | developed, false},
  {"LEDGER", "Ledger", "MERIDIAN_CORE", 13, SovereignType::npcFaction,
   "faction.meridian", DiscoveryState::charted, developed, false},
  {"PROSPERITY", "Prosperity", "MERIDIAN_CORE", 8, SovereignType::npcFaction,
   "faction.meridian", DiscoveryState::charted, developed | resource, false},
  {"GOLDEN_GATE", "Golden Gate", "MERIDIAN_CORE", 2, SovereignType::npcFaction,
   "faction.meridian", DiscoveryState::charted, frontierGateway, false},
  {"LAST_LIGHT", "Last Light", "MERIDIAN_CORE", -12, SovereignType::npcFaction,
   "faction.meridian", DiscoveryState::charted, backwaterDeadEnd, false},

  {"VERGE_ANCHOR", "Verge Anchor", "OUTER_REACH", 0, SovereignType::unclaimed,
   "", DiscoveryState::charted, SystemRole::frontier, true},
  {"PATHFINDER_REACH", "Pathfinder Reach", "OUTER_REACH", 0, SovereignType::unclaimed,
   "", DiscoveryState::charted, SystemRole::frontier, true},
  {"COALITION_MARCH", "Coalition March", "OUTER_REACH", 0, SovereignType::unclaimed,
   "", DiscoveryState::charted, SystemRole::frontier, true},
  {"DARKWARD", "Darkward", "OUTER_REACH", 0, SovereignType::unclaimed,
   "", DiscoveryState::unknown, SystemRole::frontier, true}
}};

const std::array<FactionTerritory, kFactionTerritoryCount> kTerritories{{
  {"faction.commonwealth", "CONCORDIA", "VANTAGE"},
  {"faction.compact", "FREESTEAD", "CROSSROADS"},
  {"faction.directorate", "SOLARIS", "HELIOS_EXCHANGE"},
  {"faction.meridian", "MERIDIAN_FORUM", "MERIDIAN_PRIME"}
}};

const std::array<GateConnection, kGateConnectionCount> kConnections{{
  {"GATE.CONCORDIA.VANTAGE", "CONCORDIA", "VANTAGE", RouteClass::factionCore},
  {"GATE.VANTAGE.HOLLOWAY", "VANTAGE", "HOLLOWAY", RouteClass::factionCore},
  {"GATE.VANTAGE.HARROW", "VANTAGE", "HARROW", RouteClass::secondary},
  {"GATE.HOLLOWAY.KEPLER", "HOLLOWAY", "KEPLER", RouteClass::frontier},
  {"GATE.HARROW.KEPLER", "HARROW", "KEPLER", RouteClass::secondary},
  {"GATE.KEPLER.GLOAM", "KEPLER", "GLOAM", RouteClass::backwater},

  {"GATE.FREESTEAD.CROSSROADS", "FREESTEAD", "CROSSROADS", RouteClass::factionCore},
  {"GATE.CROSSROADS.IRONHOME", "CROSSROADS", "IRONHOME", RouteClass::factionCore},
  {"GATE.CROSSROADS.FARPOINT", "CROSSROADS", "FARPOINT", RouteClass::secondary},
  {"GATE.IRONHOME.FARSIDE", "IRONHOME", "FARSIDE", RouteClass::frontier},
  {"GATE.FARSIDE.FARPOINT", "FARSIDE", "FARPOINT", RouteClass::secondary},
  {"GATE.FARSIDE.RUSTHOLLOW", "FARSIDE", "RUSTHOLLOW", RouteClass::backwater},

  {"GATE.SOLARIS.HELIOS_EXCHANGE", "SOLARIS", "HELIOS_EXCHANGE", RouteClass::factionCore},
  {"GATE.HELIOS_EXCHANGE.AEGIS", "HELIOS_EXCHANGE", "AEGIS", RouteClass::factionCore},
  {"GATE.HELIOS_EXCHANGE.SENTINEL", "HELIOS_EXCHANGE", "SENTINEL", RouteClass::secondary},
  {"GATE.AEGIS.WATCHPOINT", "AEGIS", "WATCHPOINT", RouteClass::frontier},
  {"GATE.SENTINEL.WATCHPOINT", "SENTINEL", "WATCHPOINT", RouteClass::secondary},
  {"GATE.WATCHPOINT.ASHEN_RELAY", "WATCHPOINT", "ASHEN_RELAY", RouteClass::backwater},

  {"GATE.MERIDIAN_FORUM.MERIDIAN_PRIME", "MERIDIAN_FORUM", "MERIDIAN_PRIME", RouteClass::factionCore},
  {"GATE.MERIDIAN_PRIME.LEDGER", "MERIDIAN_PRIME", "LEDGER", RouteClass::factionCore},
  {"GATE.MERIDIAN_PRIME.PROSPERITY", "MERIDIAN_PRIME", "PROSPERITY", RouteClass::secondary},
  {"GATE.LEDGER.GOLDEN_GATE", "LEDGER", "GOLDEN_GATE", RouteClass::frontier},
  {"GATE.PROSPERITY.GOLDEN_GATE", "PROSPERITY", "GOLDEN_GATE", RouteClass::secondary},
  {"GATE.GOLDEN_GATE.LAST_LIGHT", "GOLDEN_GATE", "LAST_LIGHT", RouteClass::backwater},

  {"TRUNK.KEPLER.FARPOINT", "KEPLER", "FARPOINT", RouteClass::majorInterfactionTrunk},
  {"TRUNK.FARPOINT.WATCHPOINT", "FARPOINT", "WATCHPOINT", RouteClass::majorInterfactionTrunk},
  {"TRUNK.WATCHPOINT.GOLDEN_GATE", "WATCHPOINT", "GOLDEN_GATE", RouteClass::majorInterfactionTrunk},
  {"TRUNK.GOLDEN_GATE.KEPLER", "GOLDEN_GATE", "KEPLER", RouteClass::majorInterfactionTrunk},

  {"GATE.KEPLER.VERGE_ANCHOR", "KEPLER", "VERGE_ANCHOR", RouteClass::frontier},
  {"GATE.FARSIDE.PATHFINDER_REACH", "FARSIDE", "PATHFINDER_REACH", RouteClass::frontier},
  {"GATE.WATCHPOINT.COALITION_MARCH", "WATCHPOINT", "COALITION_MARCH", RouteClass::frontier},
  {"GATE.PROSPERITY.DARKWARD", "PROSPERITY", "DARKWARD", RouteClass::frontier},
  {"GATE.VERGE_ANCHOR.PATHFINDER_REACH", "VERGE_ANCHOR", "PATHFINDER_REACH", RouteClass::secondary},
  {"GATE.PATHFINDER_REACH.COALITION_MARCH", "PATHFINDER_REACH", "COALITION_MARCH", RouteClass::secondary},
  {"GATE.COALITION_MARCH.DARKWARD", "COALITION_MARCH", "DARKWARD", RouteClass::backwater}
}};

bool sameSovereignty(const Sovereignty& left, const Sovereignty& right) {
  return left.type == right.type && left.id == right.id;
}

bool validPlayerSovereignId(std::string_view id, std::string_view prefix) {
  if (id.size() <= prefix.size() || id.size() > 64 || id.substr(0, prefix.size()) != prefix) return false;
  return std::all_of(id.begin(), id.end(), [](unsigned char character) {
    return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '.' ||
      character == '_' || character == '-';
  });
}

std::string rolesText(const SystemDefinition& definition) {
  struct Entry { SystemRole role; const char* name; };
  constexpr Entry entries[]{{SystemRole::capital, "CAPITAL"}, {SystemRole::commerceHub, "COMMERCE_HUB"},
    {SystemRole::developedInterior, "DEVELOPED_INTERIOR"}, {SystemRole::industrialResource, "INDUSTRIAL_RESOURCE"},
    {SystemRole::frontier, "FRONTIER"}, {SystemRole::lowSecurityBackwater, "LOWSEC_BACKWATER"},
    {SystemRole::borderGateway, "BORDER_GATEWAY"}, {SystemRole::deadEnd, "DEAD_END"}};
  std::string result;
  for (const auto& entry : entries) {
    if (!hasRole(definition, entry.role)) continue;
    if (!result.empty()) result += ',';
    result += entry.name;
  }
  return result.empty() ? "NONE" : result;
}

std::string wireToken(std::string_view value) {
  std::string result(value);
  std::replace(result.begin(), result.end(), ' ', '_');
  return result;
}

bool validateTopology() {
  std::unordered_set<std::string_view> regionIds;
  for (const auto& region : kRegions)
    if (region.id.empty() || region.displayName.empty() || !regionIds.emplace(region.id).second) return false;

  std::unordered_set<std::string_view> systemIds;
  std::unordered_map<std::string_view, int> degree;
  for (const auto& system : kSystems) {
    if (system.id.empty() || system.displayName.empty() || !findRegion(system.regionId) ||
        !systemIds.emplace(system.id).second) return false;
    degree.emplace(system.id, 0);
  }

  std::unordered_set<std::string_view> connectionIds;
  std::set<std::pair<std::string_view, std::string_view>> pairs;
  std::size_t trunkCount = 0;
  std::array<std::size_t, 5> routeCounts{};
  for (const auto& connection : kConnections) {
    const auto* first = findSystem(connection.firstSystemId);
    const auto* second = findSystem(connection.secondSystemId);
    auto pair = std::minmax(connection.firstSystemId, connection.secondSystemId);
    if (connection.id.empty() || !first || !second || first == second ||
        !connectionIds.emplace(connection.id).second || !pairs.emplace(pair).second) return false;
    ++degree[connection.firstSystemId];
    ++degree[connection.secondSystemId];
    ++routeCounts[static_cast<std::size_t>(connection.routeClass)];
    if (connection.routeClass == RouteClass::majorInterfactionTrunk) {
      ++trunkCount;
      if (!hasRole(*first, SystemRole::borderGateway) || !hasRole(*second, SystemRole::borderGateway) ||
          first->initialSovereignType != SovereignType::npcFaction ||
          second->initialSovereignType != SovereignType::npcFaction ||
          first->initialSovereignId == second->initialSovereignId) return false;
    }
    if (connection.routeClass == RouteClass::backwater &&
        !hasRole(*first, SystemRole::lowSecurityBackwater) &&
        !hasRole(*second, SystemRole::lowSecurityBackwater) &&
        first->initialSecuritySteps != 0 && second->initialSecuritySteps != 0) return false;
  }
  if (trunkCount != 4 || std::any_of(routeCounts.begin(), routeCounts.end(),
                                     [](std::size_t count) { return count == 0; })) return false;
  for (const auto& system : kSystems)
    if (hasRole(system, SystemRole::deadEnd) && degree[system.id] != 1) return false;

  std::unordered_set<std::string> visited;
  std::queue<std::string> pending;
  pending.emplace(kSystems.front().id);
  visited.emplace(kSystems.front().id);
  while (!pending.empty()) {
    const auto current = pending.front(); pending.pop();
    for (const auto& next : adjacentSystems(current))
      if (visited.emplace(next).second) pending.push(next);
  }
  if (visited.size() != kSystems.size()) return false;

  std::unordered_set<std::string_view> factionIds;
  for (const auto& territory : kTerritories) {
    const auto* capital = findSystem(territory.capitalSystemId);
    const auto* hub = findSystem(territory.commerceHubSystemId);
    if (!organizations::find(territory.factionId) || !factionIds.emplace(territory.factionId).second ||
        !capital || !hub || capital == hub || !hasRole(*capital, SystemRole::capital) ||
        !hasRole(*hub, SystemRole::commerceHub) || capital->initialSovereignId != territory.factionId ||
        hub->initialSovereignId != territory.factionId || !adjacent(capital->id, hub->id)) return false;
    bool interior = false, resourceSystem = false, frontierSystem = false, lowBackwater = false, gateway = false;
    std::set<std::string> memberIds;
    for (const auto& system : kSystems) {
      if (system.initialSovereignId != territory.factionId) continue;
      memberIds.emplace(system.id);
      interior |= hasRole(system, SystemRole::developedInterior);
      resourceSystem |= hasRole(system, SystemRole::industrialResource);
      frontierSystem |= hasRole(system, SystemRole::frontier);
      lowBackwater |= hasRole(system, SystemRole::lowSecurityBackwater) && system.initialSecuritySteps < 0;
      gateway |= hasRole(system, SystemRole::borderGateway);
    }
    if (!interior || !resourceSystem || !frontierSystem || !lowBackwater || !gateway) return false;

    std::set<std::string> territoryVisited;
    std::queue<std::pair<std::string, int>> territoryPending;
    std::unordered_map<std::string, int> distanceFromHub;
    territoryPending.emplace(std::string(hub->id), 0);
    while (!territoryPending.empty()) {
      auto [current, distance] = territoryPending.front(); territoryPending.pop();
      if (!territoryVisited.emplace(current).second) continue;
      distanceFromHub.emplace(current, distance);
      for (const auto& next : adjacentSystems(current))
        if (memberIds.count(next) != 0) territoryPending.emplace(next, distance + 1);
    }
    if (territoryVisited != memberIds) return false;
    for (const auto& system : kSystems) {
      if (system.initialSovereignId == territory.factionId && hasRole(system, SystemRole::borderGateway)) {
        const auto distance = distanceFromHub.find(std::string(system.id));
        if (distance == distanceFromHub.end() || distance->second > 2) return false;
      }
    }
  }
  return true;
}
}

const std::array<RegionDefinition, kRegionCount>& regions() { return kRegions; }
const std::array<SystemDefinition, kSystemCount>& systems() { return kSystems; }
const std::array<FactionTerritory, kFactionTerritoryCount>& factionTerritories() { return kTerritories; }
const std::array<GateConnection, kGateConnectionCount>& gateConnections() { return kConnections; }

const RegionDefinition* findRegion(std::string_view id) {
  for (const auto& region : kRegions) if (region.id == id) return &region;
  return nullptr;
}
const SystemDefinition* findSystem(std::string_view id) {
  for (const auto& system : kSystems) if (system.id == id) return &system;
  return nullptr;
}
const FactionTerritory* findTerritory(std::string_view factionId) {
  for (const auto& territory : kTerritories) if (territory.factionId == factionId) return &territory;
  return nullptr;
}
const SystemState* findState(const GalaxyState& state, std::string_view systemId) {
  for (const auto& system : state) if (system.systemId == systemId) return &system;
  return nullptr;
}
SystemState* findState(GalaxyState& state, std::string_view systemId) {
  for (auto& system : state) if (system.systemId == systemId) return &system;
  return nullptr;
}

bool validSecuritySteps(int steps) { return steps >= -25 && steps <= 25; }

std::optional<int> parseSecuritySteps(std::string_view value) {
  bool negative = false;
  if (!value.empty() && (value.front() == '+' || value.front() == '-')) {
    negative = value.front() == '-';
    value.remove_prefix(1);
  }
  if (value.size() != 3 || value[1] != '.' || value[0] < '0' || value[0] > '5' ||
      (value[2] != '0' && value[2] != '2' && value[2] != '4' && value[2] != '6' && value[2] != '8') ||
      (value[0] == '5' && value[2] != '0')) return std::nullopt;
  int steps = ((value[0] - '0') * 10 + (value[2] - '0')) / 2;
  if (negative) steps = -steps;
  return validSecuritySteps(steps) ? std::optional<int>(steps) : std::nullopt;
}

std::string securityValue(int steps) {
  if (!validSecuritySteps(steps)) return "INVALID";
  if (steps == 0) return "0.0";
  const int tenths = std::abs(steps) * 2;
  return std::string(steps > 0 ? "+" : "-") + std::to_string(tenths / 10) + "." +
    std::to_string(tenths % 10);
}

std::optional<SecurityClass> classifySecurity(int steps) {
  if (!validSecuritySteps(steps)) return std::nullopt;
  if (steps > 0) return SecurityClass::high;
  if (steps < 0) return SecurityClass::low;
  return SecurityClass::nullSecurity;
}

LowSecurityBand lowSecurityBand(int steps) {
  if (steps >= 0 || !validSecuritySteps(steps)) return LowSecurityBand::none;
  if (steps >= -11) return LowSecurityBand::supported;
  if (steps >= -20) return LowSecurityBand::deep;
  return LowSecurityBand::extreme;
}

SecurityResponsePolicy responsePolicy(int steps) {
  const auto security = classifySecurity(steps);
  if (!security || *security == SecurityClass::nullSecurity) return {};
  if (*security == SecurityClass::high) return {true, true, false, false};
  const auto band = lowSecurityBand(steps);
  return {true, true, band == LowSecurityBand::supported, band == LowSecurityBand::deep};
}

const char* securityClassName(SecurityClass value) {
  switch (value) {
    case SecurityClass::high: return "HIGH";
    case SecurityClass::nullSecurity: return "NULL";
    case SecurityClass::low: return "LOW";
  }
  return "UNKNOWN";
}
const char* sovereignTypeName(SovereignType value) {
  switch (value) {
    case SovereignType::npcFaction: return "NPC_FACTION";
    case SovereignType::playerGuild: return "PLAYER_GUILD";
    case SovereignType::playerAlliance: return "PLAYER_ALLIANCE";
    case SovereignType::unclaimed: return "UNCLAIMED";
  }
  return "UNKNOWN";
}
std::optional<SovereignType> parseSovereignType(std::string_view value) {
  if (value == "NPC_FACTION") return SovereignType::npcFaction;
  if (value == "PLAYER_GUILD") return SovereignType::playerGuild;
  if (value == "PLAYER_ALLIANCE") return SovereignType::playerAlliance;
  if (value == "UNCLAIMED") return SovereignType::unclaimed;
  return std::nullopt;
}
const char* discoveryStateName(DiscoveryState value) {
  switch (value) {
    case DiscoveryState::unknown: return "UNKNOWN";
    case DiscoveryState::charted: return "CHARTED";
  }
  return "INVALID";
}
std::optional<DiscoveryState> parseDiscoveryState(std::string_view value) {
  if (value == "UNKNOWN") return DiscoveryState::unknown;
  if (value == "CHARTED") return DiscoveryState::charted;
  return std::nullopt;
}
const char* routeClassName(RouteClass value) {
  switch (value) {
    case RouteClass::majorInterfactionTrunk: return "MAJOR_INTERFACTION_TRUNK";
    case RouteClass::factionCore: return "FACTION_CORE";
    case RouteClass::secondary: return "SECONDARY";
    case RouteClass::frontier: return "FRONTIER";
    case RouteClass::backwater: return "BACKWATER";
  }
  return "UNKNOWN";
}
bool hasRole(const SystemDefinition& system, SystemRole role) {
  return (static_cast<std::uint32_t>(system.roles) & static_cast<std::uint32_t>(role)) != 0;
}

GalaxyState seedState() {
  GalaxyState state{};
  for (std::size_t i = 0; i < kSystems.size(); ++i) {
    const auto& definition = kSystems[i];
    state[i] = {std::string(definition.id), definition.initialSecuritySteps,
      {definition.initialSovereignType, std::string(definition.initialSovereignId)}, definition.initialDiscovery};
  }
  return state;
}

bool validateSovereignty(int securitySteps, const Sovereignty& sovereignty) {
  if (!validSecuritySteps(securitySteps)) return false;
  if (securitySteps == 0) {
    if (sovereignty.type == SovereignType::npcFaction) return false;
    if (sovereignty.type == SovereignType::unclaimed) return sovereignty.id.empty();
    if (sovereignty.type == SovereignType::playerGuild)
      return validPlayerSovereignId(sovereignty.id, "guild.");
    if (sovereignty.type == SovereignType::playerAlliance)
      return validPlayerSovereignId(sovereignty.id, "alliance.");
    return false;
  }
  return sovereignty.type == SovereignType::npcFaction && !sovereignty.id.empty() &&
    findTerritory(sovereignty.id) != nullptr;
}

bool validateGalaxy(const GalaxyState& state) {
  static const bool topologyValid = validateTopology();
  if (!topologyValid) return false;
  std::unordered_set<std::string> ids;
  for (const auto& current : state) {
    const auto* definition = findSystem(current.systemId);
    if (!definition || !ids.emplace(current.systemId).second ||
        !validateSovereignty(current.securitySteps, current.sovereignty) ||
        (current.discovery != DiscoveryState::unknown &&
         current.discovery != DiscoveryState::charted)) return false;
    if (current.discovery == DiscoveryState::unknown &&
        (current.sovereignty.type == SovereignType::playerGuild ||
         current.sovereignty.type == SovereignType::playerAlliance)) return false;
    if (current.sovereignty.type == SovereignType::npcFaction &&
        current.sovereignty.id != definition->initialSovereignId) return false;
  }
  return ids.size() == kSystems.size();
}

bool setSecurity(GalaxyState& state, std::string_view systemId, int securitySteps) {
  auto* current = findState(state, systemId);
  if (!current || !validateSovereignty(securitySteps, current->sovereignty)) return false;
  current->securitySteps = securitySteps;
  return true;
}

bool setNullSovereignty(GalaxyState& state, std::string_view systemId,
                        SovereignType type, std::string_view sovereignId) {
  auto* current = findState(state, systemId);
  if (!current || current->securitySteps != 0) return false;
  if ((type == SovereignType::playerGuild || type == SovereignType::playerAlliance) &&
      current->discovery != DiscoveryState::charted) return false;
  Sovereignty next{type, std::string(sovereignId)};
  if (!validateSovereignty(current->securitySteps, next)) return false;
  current->sovereignty = std::move(next);
  return true;
}

bool setDiscovery(GalaxyState& state, std::string_view systemId, DiscoveryState discovery) {
  auto* current = findState(state, systemId);
  if (!current || (discovery != DiscoveryState::unknown && discovery != DiscoveryState::charted)) return false;
  if (discovery == DiscoveryState::unknown &&
      (current->sovereignty.type == SovereignType::playerGuild ||
       current->sovereignty.type == SovereignType::playerAlliance)) return false;
  current->discovery = discovery;
  return true;
}

bool adjacent(std::string_view firstSystemId, std::string_view secondSystemId) {
  for (const auto& connection : kConnections)
    if ((connection.firstSystemId == firstSystemId && connection.secondSystemId == secondSystemId) ||
        (connection.firstSystemId == secondSystemId && connection.secondSystemId == firstSystemId)) return true;
  return false;
}

std::vector<std::string> adjacentSystems(std::string_view systemId) {
  std::vector<std::string> result;
  for (const auto& connection : kConnections) {
    if (connection.firstSystemId == systemId) result.emplace_back(connection.secondSystemId);
    else if (connection.secondSystemId == systemId) result.emplace_back(connection.firstSystemId);
  }
  return result;
}

std::vector<Border> deriveBorders(const GalaxyState& state) {
  std::vector<Border> result;
  if (!validateGalaxy(state)) return result;
  for (const auto& connection : kConnections) {
    const auto* first = findState(state, connection.firstSystemId);
    const auto* second = findState(state, connection.secondSystemId);
    if (!sameSovereignty(first->sovereignty, second->sovereignty))
      result.push_back({std::string(connection.id), first->systemId, second->systemId,
                        first->sovereignty, second->sovereignty});
  }
  return result;
}

bool canHostFuturePlayerInfrastructure(const SystemDefinition& definition,
                                       const SystemState& state) {
  return definition.id == state.systemId && definition.futurePlayerInfrastructureEligible &&
    state.securitySteps == 0 && (state.sovereignty.type == SovereignType::playerGuild ||
                                 state.sovereignty.type == SovereignType::playerAlliance);
}

std::string systemLine(const SystemDefinition& definition, const SystemState& state) {
  const auto classification = classifySecurity(state.securitySteps);
  return "SYSTEM id=" + std::string(definition.id) + " name=" + wireToken(definition.displayName) +
    " region=" + std::string(definition.regionId) + " security=" + securityValue(state.securitySteps) +
    " class=" + (classification ? securityClassName(*classification) : std::string("INVALID")) +
    " sovereign-type=" + sovereignTypeName(state.sovereignty.type) +
    " sovereign-id=" + (state.sovereignty.id.empty() ? std::string("none") : state.sovereignty.id) +
    " discovery=" + discoveryStateName(state.discovery) + " roles=" + rolesText(definition) +
    " future-infrastructure=" + std::to_string(canHostFuturePlayerInfrastructure(definition, state));
}

std::string territoryLine(const FactionTerritory& territory) {
  return "TERRITORY faction=" + std::string(territory.factionId) +
    " capital=" + std::string(territory.capitalSystemId) +
    " commerce-hub=" + std::string(territory.commerceHubSystemId);
}

std::string gateLine(const GateConnection& connection) {
  return "GATE id=" + std::string(connection.id) + " from=" + std::string(connection.firstSystemId) +
    " to=" + std::string(connection.secondSystemId) + " route=" + routeClassName(connection.routeClass) +
    " permanent=1";
}

std::string borderLine(const Border& border) {
  return "BORDER gate=" + border.connectionId + " first=" + border.firstSystemId +
    " first-type=" + sovereignTypeName(border.firstSovereignty.type) +
    " first-owner=" + (border.firstSovereignty.id.empty() ? std::string("none") : border.firstSovereignty.id) +
    " second=" + border.secondSystemId + " second-type=" + sovereignTypeName(border.secondSovereignty.type) +
    " second-owner=" + (border.secondSovereignty.id.empty() ? std::string("none") : border.secondSovereignty.id);
}

} // namespace helion::galaxy
