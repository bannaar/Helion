#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace helion::galaxy {

enum class SecurityClass { high, nullSecurity, low };
enum class LowSecurityBand { none, supported, deep, extreme };
enum class SovereignType { npcFaction, playerGuild, playerAlliance, unclaimed };
enum class DiscoveryState { unknown, charted };
enum class RouteClass { majorInterfactionTrunk, factionCore, secondary, frontier, backwater };
enum class SystemRole : std::uint32_t {
  none = 0,
  capital = 1u << 0,
  commerceHub = 1u << 1,
  developedInterior = 1u << 2,
  industrialResource = 1u << 3,
  frontier = 1u << 4,
  lowSecurityBackwater = 1u << 5,
  borderGateway = 1u << 6,
  deadEnd = 1u << 7
};

constexpr SystemRole operator|(SystemRole left, SystemRole right) {
  return static_cast<SystemRole>(static_cast<std::uint32_t>(left) |
                                 static_cast<std::uint32_t>(right));
}

struct Sovereignty {
  SovereignType type = SovereignType::unclaimed;
  std::string id;
};

struct RegionDefinition {
  std::string_view id;
  std::string_view displayName;
};

struct SystemDefinition {
  std::string_view id;
  std::string_view displayName;
  std::string_view regionId;
  int initialSecuritySteps = 0; // Exact 0.2 increments: -25..+25.
  SovereignType initialSovereignType = SovereignType::unclaimed;
  std::string_view initialSovereignId;
  DiscoveryState initialDiscovery = DiscoveryState::charted;
  SystemRole roles = SystemRole::none;
  bool futurePlayerInfrastructureEligible = false;
};

struct SystemState {
  std::string systemId;
  int securitySteps = 0;
  Sovereignty sovereignty;
  DiscoveryState discovery = DiscoveryState::charted;
};

struct GateConnection {
  std::string_view id;
  std::string_view firstSystemId;
  std::string_view secondSystemId;
  RouteClass routeClass = RouteClass::secondary;
};

struct FactionTerritory {
  std::string_view factionId;
  std::string_view capitalSystemId;
  std::string_view commerceHubSystemId;
};

struct Border {
  std::string connectionId;
  std::string firstSystemId;
  std::string secondSystemId;
  Sovereignty firstSovereignty;
  Sovereignty secondSovereignty;
};

struct SecurityResponsePolicy {
  bool automaticFactionCrime = false;
  bool localFactionResponse = false;
  bool conditionalGdfSupport = false;
  bool occasionalGdfExpedition = false;
};

inline constexpr std::size_t kRegionCount = 6;
inline constexpr std::size_t kSystemCount = 28;
inline constexpr std::size_t kFactionTerritoryCount = 4;
inline constexpr std::size_t kGateConnectionCount = 35;
using GalaxyState = std::array<SystemState, kSystemCount>;

const std::array<RegionDefinition, kRegionCount>& regions();
const std::array<SystemDefinition, kSystemCount>& systems();
const std::array<FactionTerritory, kFactionTerritoryCount>& factionTerritories();
const std::array<GateConnection, kGateConnectionCount>& gateConnections();

const RegionDefinition* findRegion(std::string_view id);
const SystemDefinition* findSystem(std::string_view id);
const FactionTerritory* findTerritory(std::string_view factionId);
const SystemState* findState(const GalaxyState& state, std::string_view systemId);
SystemState* findState(GalaxyState& state, std::string_view systemId);

bool validSecuritySteps(int steps);
std::optional<int> parseSecuritySteps(std::string_view value);
std::string securityValue(int steps);
std::optional<SecurityClass> classifySecurity(int steps);
LowSecurityBand lowSecurityBand(int steps);
SecurityResponsePolicy responsePolicy(int steps);

const char* securityClassName(SecurityClass value);
const char* sovereignTypeName(SovereignType value);
std::optional<SovereignType> parseSovereignType(std::string_view value);
const char* discoveryStateName(DiscoveryState value);
std::optional<DiscoveryState> parseDiscoveryState(std::string_view value);
const char* routeClassName(RouteClass value);
bool hasRole(const SystemDefinition& system, SystemRole role);

GalaxyState seedState();
bool validateSovereignty(int securitySteps, const Sovereignty& sovereignty);
bool validateGalaxy(const GalaxyState& state);
bool setSecurity(GalaxyState& state, std::string_view systemId, int securitySteps);
bool setNullSovereignty(GalaxyState& state, std::string_view systemId,
                        SovereignType type, std::string_view sovereignId);
bool setDiscovery(GalaxyState& state, std::string_view systemId, DiscoveryState discovery);

bool adjacent(std::string_view firstSystemId, std::string_view secondSystemId);
std::vector<std::string> adjacentSystems(std::string_view systemId);
std::vector<Border> deriveBorders(const GalaxyState& state);
bool canHostFuturePlayerInfrastructure(const SystemDefinition& definition,
                                       const SystemState& state);

std::string systemLine(const SystemDefinition& definition, const SystemState& state);
std::string territoryLine(const FactionTerritory& territory);
std::string gateLine(const GateConnection& connection);
std::string borderLine(const Border& border);

} // namespace helion::galaxy
