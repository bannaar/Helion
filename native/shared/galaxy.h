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
enum class SettlementScale { uninhabited, outpost, colony, established, metropolitan };
enum class StellarType { redDwarf, orangeDwarf, yellowDwarf, whiteStar, binary, neutronStar };
enum class TrafficProfile { dormant, sparse, frontier, regular, dense };
enum class EconomicRole {
  government, commerce, industry, shipbuilding, extraction, refining, agriculture,
  research, military, tourism, salvage, gateLogistics, frontierMixed
};
enum class Infrastructure : std::uint32_t {
  none = 0,
  station = 1u << 0,
  orbitalSettlement = 1u << 1,
  shipyard = 1u << 2,
  refinery = 1u << 3,
  factory = 1u << 4,
  warehouse = 1u << 5,
  researchFacility = 1u << 6,
  militaryFacility = 1u << 7,
  gatewatchFacility = 1u << 8,
  agriculturalHabitat = 1u << 9,
  extractionPlatform = 1u << 10,
  frontierOutpost = 1u << 11
};
enum class ResourceFamily : std::uint32_t {
  none = 0,
  structuralMetals = 1u << 0,
  industrialMinerals = 1u << 1,
  rareMetals = 1u << 2,
  iceVolatiles = 1u << 3,
  fuelIsotopes = 1u << 4,
  industrialGases = 1u << 5,
  organics = 1u << 6,
  salvage = 1u << 7,
  scientificSamples = 1u << 8,
  gateMaterials = 1u << 9,
  xenologicalTraces = 1u << 10
};
enum class CommodityFamily : std::uint32_t {
  none = 0,
  food = 1u << 0,
  ore = 1u << 1,
  parts = 1u << 2,
  fuel = 1u << 3,
  medicine = 1u << 4,
  machinery = 1u << 5,
  electronics = 1u << 6,
  processedMaterials = 1u << 7,
  consumerGoods = 1u << 8,
  scientificEquipment = 1u << 9
};

constexpr Infrastructure operator|(Infrastructure left, Infrastructure right) {
  return static_cast<Infrastructure>(static_cast<std::uint32_t>(left) |
                                     static_cast<std::uint32_t>(right));
}
constexpr ResourceFamily operator|(ResourceFamily left, ResourceFamily right) {
  return static_cast<ResourceFamily>(static_cast<std::uint32_t>(left) |
                                     static_cast<std::uint32_t>(right));
}
constexpr CommodityFamily operator|(CommodityFamily left, CommodityFamily right) {
  return static_cast<CommodityFamily>(static_cast<std::uint32_t>(left) |
                                      static_cast<std::uint32_t>(right));
}
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

struct SystemProfile {
  std::string_view systemId;
  std::uint64_t population = 0;
  SettlementScale settlement = SettlementScale::uninhabited;
  StellarType stellarType = StellarType::yellowDwarf;
  std::uint8_t planetCount = 0;
  std::uint8_t moonCount = 0;
  std::uint8_t asteroidFieldCount = 0;
  TrafficProfile traffic = TrafficProfile::dormant;
  EconomicRole primaryEconomy = EconomicRole::frontierMixed;
  Infrastructure infrastructure = Infrastructure::none;
  ResourceFamily resources = ResourceFamily::none;
  CommodityFamily production = CommodityFamily::none;
  CommodityFamily consumption = CommodityFamily::none;
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

inline constexpr std::size_t kRegionCount = 10;
inline constexpr std::size_t kSystemCount = 90;
inline constexpr std::size_t kFactionTerritoryCount = 5;
inline constexpr std::size_t kGateConnectionCount = 133;
using GalaxyState = std::array<SystemState, kSystemCount>;

const std::array<RegionDefinition, kRegionCount>& regions();
const std::array<SystemDefinition, kSystemCount>& systems();
const std::array<SystemProfile, kSystemCount>& systemProfiles();
const std::array<FactionTerritory, kFactionTerritoryCount>& factionTerritories();
const std::array<GateConnection, kGateConnectionCount>& gateConnections();

const RegionDefinition* findRegion(std::string_view id);
const SystemDefinition* findSystem(std::string_view id);
const SystemProfile* findSystemProfile(std::string_view systemId);
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
const char* settlementScaleName(SettlementScale value);
const char* stellarTypeName(StellarType value);
const char* trafficProfileName(TrafficProfile value);
const char* economicRoleName(EconomicRole value);
bool hasRole(const SystemDefinition& system, SystemRole role);
bool hasInfrastructure(const SystemProfile& profile, Infrastructure infrastructure);
bool hasResource(const SystemProfile& profile, ResourceFamily resource);
bool produces(const SystemProfile& profile, CommodityFamily commodity);
bool consumes(const SystemProfile& profile, CommodityFamily commodity);

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
