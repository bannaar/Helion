#include "shared/galaxy.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>

namespace {
void check(bool condition, const char* label) {
  if (!condition) { std::cerr << "FAILED: " << label << '\n'; std::exit(1); }
}
}

int main() {
  using namespace helion::galaxy;

  check(parseSecuritySteps("+5.0") == 25 && parseSecuritySteps("+0.2") == 1 &&
        parseSecuritySteps("0.0") == 0 && parseSecuritySteps("-0.2") == -1 &&
        parseSecuritySteps("-5.0") == -25,
        "all canonical security boundaries parse to exact normalized steps");
  check(!parseSecuritySteps("+5.2") && !parseSecuritySteps("+0.1") &&
        !parseSecuritySteps("-0.1") && !parseSecuritySteps("-5.2") &&
        !parseSecuritySteps("0") && !parseSecuritySteps("nan"),
        "out-of-range non-step and malformed security values reject");
  for (int steps = -25; steps <= 25; ++steps) {
    const auto encoded = securityValue(steps);
    check(parseSecuritySteps(encoded) == steps && classifySecurity(steps).has_value(),
          "every exact -5.0..+5.0 0.2-step security value round-trips");
  }
  check(classifySecurity(25) == SecurityClass::high && classifySecurity(1) == SecurityClass::high &&
        classifySecurity(0) == SecurityClass::zeroSpace &&
        classifySecurity(-1) == SecurityClass::low && classifySecurity(-25) == SecurityClass::low &&
        !classifySecurity(26), "security classification is exact at every outer and inner boundary");
  check(securityValue(25) == "+5.0" && securityValue(1) == "+0.2" && securityValue(0) == "0.0" &&
        securityValue(-1) == "-0.2" && securityValue(-25) == "-5.0",
        "normalized security formats deterministically");
  check(lowSecurityBand(-1) == LowSecurityBand::supported && lowSecurityBand(-11) == LowSecurityBand::supported &&
        lowSecurityBand(-12) == LowSecurityBand::deep && lowSecurityBand(-20) == LowSecurityBand::deep &&
        lowSecurityBand(-21) == LowSecurityBand::extreme && lowSecurityBand(-25) == LowSecurityBand::extreme,
        "secondary low-security response bands preserve all clarified boundaries");
  check(responsePolicy(1).automaticFactionCrime && responsePolicy(1).localFactionResponse &&
        responsePolicy(-1).conditionalGdfSupport && responsePolicy(-12).occasionalGdfExpedition &&
        !responsePolicy(-21).conditionalGdfSupport && !responsePolicy(0).automaticFactionCrime,
        "security response hooks distinguish high low bands and Zero Space without fake fleet behavior");

  auto state = seedState();
  std::map<std::string, std::size_t> sovereignCounts;
  std::size_t inhabited = 0, uninhabited = 0, gateMaterialSystems = 0, xenologicalSystems = 0;
  std::set<std::string_view> displayNames;
  for (const auto& system : systems()) {
    const auto* profile = findSystemProfile(system.id);
    check(!system.id.empty() && findRegion(system.regionId) && profile && profile->systemId == system.id &&
          system.domain == SpaceDomain::knownSpace && displayNames.emplace(system.displayName).second,
          "every system has stable identity unique name Known Space domain region and profile");
    check(profile && profile->planetCount >= 2 && profile->asteroidFieldCount >= 1 &&
          profile->resources != ResourceFamily::none,
          "every system has deterministic celestial and resource geography");
    if (profile && profile->population == 0) {
      ++uninhabited;
      check(profile->settlement == SettlementScale::uninhabited &&
            profile->infrastructure == Infrastructure::none &&
            profile->production == CommodityFamily::none &&
            profile->consumption == CommodityFamily::none,
            "zero-population systems do not fabricate settlements or active economy");
    } else if (profile) {
      ++inhabited;
      check(profile->settlement != SettlementScale::uninhabited &&
            profile->infrastructure != Infrastructure::none &&
            profile->production != CommodityFamily::none &&
            profile->consumption != CommodityFamily::none,
            "inhabited systems expose settlement infrastructure and economic identity");
    }
    gateMaterialSystems += profile && hasResource(*profile, ResourceFamily::gateMaterials);
    xenologicalSystems += profile && hasResource(*profile, ResourceFamily::xenologicalTraces);
    if (system.initialSovereignType == SovereignType::npcFaction)
      ++sovereignCounts[std::string(system.initialSovereignId)];
    else
      ++sovereignCounts["ZERO_SPACE"];
  }
  check(systems().size() == 200 && inhabited > 120 && uninhabited > 50 &&
        gateMaterialSystems > 0 && gateMaterialSystems < systems().size() / 2 &&
        xenologicalSystems == 3,
        "v0.20 galaxy scale population and rare restricted resource distribution validate");
  check(sovereignCounts["faction.commonwealth"] == 27 &&
        sovereignCounts["faction.directorate"] == 25 &&
        sovereignCounts["faction.compact"] == 25 &&
        sovereignCounts["faction.meridian"] == 23 &&
        sovereignCounts["faction.aurelian"] == 20 && sovereignCounts["ZERO_SPACE"] == 80,
        "Known Space has the exact five-faction and Zero Space distribution");
  check(spaceDomainName(SpaceDomain::knownSpace) == std::string("KNOWN_SPACE") &&
        securityClassName(SecurityClass::zeroSpace) == std::string("ZERO_SPACE") &&
        spaceDomainName(SpaceDomain::riftspace) == std::string("RIFTSPACE"),
        "space domain is represented independently from security classification");
  check(findSystem("CONCORDIA")->displayName == "Crownspire",
        "current capital name retains its legacy persistent system identifier");
  check(findSystemProfile("ORCHARD")->primaryEconomy == EconomicRole::agriculture &&
        produces(*findSystemProfile("ORCHARD"), CommodityFamily::food) &&
        findSystemProfile("HAMMERFALL")->primaryEconomy == EconomicRole::shipbuilding &&
        consumes(*findSystemProfile("HAMMERFALL"), CommodityFamily::ore) &&
        findSystemProfile("RESONANCE")->primaryEconomy == EconomicRole::research,
        "new faction geography distinguishes food supply manufacturing demand and research");
  const auto* keplerProfile = findSystemProfile("KEPLER");
  check(keplerProfile && produces(*keplerProfile, CommodityFamily::food) &&
        produces(*keplerProfile, CommodityFamily::parts) &&
        consumes(*keplerProfile, CommodityFamily::ore) &&
        hasInfrastructure(*keplerProfile, Infrastructure::warehouse),
        "Kepler geography exposes the existing M2 food parts and ore market relationship");
  const auto* aureliaProfile = findSystemProfile("AURELIA");
  const auto* forgeProfile = findSystemProfile("FORGE_END");
  const auto* riftwardProfile = findSystemProfile("RIFTWARD");
  check(aureliaProfile && forgeProfile && riftwardProfile &&
        hasResource(*aureliaProfile, ResourceFamily::gateMaterials) &&
        hasResource(*forgeProfile, ResourceFamily::structuralMetals) &&
        hasResource(*riftwardProfile, ResourceFamily::xenologicalTraces) &&
        !hasResource(*forgeProfile, ResourceFamily::gateMaterials),
        "resource geography is regionally differentiated rather than globally uniform");
  std::set<std::string> gateIds;
  std::set<std::pair<std::string, std::string>> gatePairs;
  for (const auto& gate : gateConnections()) {
    const std::string first(gate.firstSystemId), second(gate.secondSystemId);
    const auto pair = std::minmax(first, second);
    check(findSystem(gate.firstSystemId) && findSystem(gate.secondSystemId) &&
          gateIds.emplace(gate.id).second && gatePairs.emplace(pair).second,
          "every permanent Gate has unique identity endpoints and adjacency");
  }
  for (const auto& territory : factionTerritories())
    check(findSystem(territory.capitalSystemId) && findSystem(territory.commerceHubSystemId) &&
          adjacent(territory.capitalSystemId, territory.commerceHubSystemId),
          "territory capital and commerce hub exist and are near in topology");
  for (const auto& current : state)
    check(validateSovereignty(current.securitySteps, current.sovereignty),
          "each seeded security and sovereignty pair validates independently");
  for (const auto& system : systems())
    if (hasRole(system, SystemRole::deadEnd))
      check(adjacentSystems(system.id).size() == 1, "canonical declared dead ends have degree one");
  for (const auto& territory : factionTerritories()) {
    bool interior = false, resources = false, frontier = false, backwater = false, gateway = false;
    for (const auto& system : systems()) {
      if (system.initialSovereignId != territory.factionId) continue;
      interior |= hasRole(system, SystemRole::developedInterior);
      resources |= hasRole(system, SystemRole::industrialResource);
      frontier |= hasRole(system, SystemRole::frontier);
      backwater |= hasRole(system, SystemRole::lowSecurityBackwater) && system.initialSecuritySteps < 0;
      gateway |= hasRole(system, SystemRole::borderGateway);
    }
    check(interior && resources && frontier && backwater && gateway,
          "each NPC territory has interior resource frontier backwater and gateway roles");
  }
  std::size_t topologyTrunks = 0;
  for (const auto& gate : gateConnections()) {
    const auto* first = findSystem(gate.firstSystemId);
    const auto* second = findSystem(gate.secondSystemId);
    if (gate.routeClass == RouteClass::majorInterfactionTrunk) {
      ++topologyTrunks;
      check(hasRole(*first, SystemRole::borderGateway) && hasRole(*second, SystemRole::borderGateway) &&
            first->initialSovereignId != second->initialSovereignId,
            "trunk endpoints are gateways belonging to different NPC factions");
    }
  }
  check(topologyTrunks == 6, "canonical graph has six protected major inter-faction trunks");
  std::set<std::string> topologyVisited;
  std::vector<std::string> topologyPending{"CONCORDIA"};
  while (!topologyPending.empty()) {
    auto current = topologyPending.back(); topologyPending.pop_back();
    if (!topologyVisited.emplace(current).second) continue;
    for (auto& next : adjacentSystems(current)) topologyPending.push_back(std::move(next));
  }
  check(topologyVisited.size() == systems().size(), "canonical topology is connected before state validation");
  check(validateGalaxy(state), "canonical galaxy graph and seeded authoritative state validate");
  check(systems().size() == kSystemCount && systemProfiles().size() == kSystemCount &&
        regions().size() == kRegionCount && factionTerritories().size() == kFactionTerritoryCount &&
        gateConnections().size() == kGateConnectionCount, "canonical topology and profile sizes are stable");

  std::size_t high = 0, low = 0, zeroSpace = 0;
  for (const auto& system : state) {
    const auto classification = classifySecurity(system.securitySteps);
    high += classification == SecurityClass::high;
    low += classification == SecurityClass::low;
    zeroSpace += classification == SecurityClass::zeroSpace;
    check(validateSovereignty(system.securitySteps, system.sovereignty),
          "every canonical sovereignty/security pair is legal");
  }
  check(high > 0 && low > 0 && high + low == 120 && zeroSpace == 80,
        "NPC High/Low Security territory and explicit 0.0 Zero Space have exact totals");

  auto illegalNpcNull = state;
  auto erasedFaction = state;
  auto* erasedKepler = findState(erasedFaction, "KEPLER");
  erasedKepler->securitySteps = 0;
  erasedKepler->sovereignty = {SovereignType::unclaimed, ""};
  check(!validateGalaxy(erasedFaction), "persistent faction territory cannot be silently converted into Zero Space");
  auto* verge = findState(illegalNpcNull, "VERGE_ANCHOR");
  verge->sovereignty = {SovereignType::npcFaction, "faction.commonwealth"};
  check(!validateGalaxy(illegalNpcNull), "NPC ownership of 0.0 rejects");
  auto illegalNullTerritory = state;
  auto* kepler = findState(illegalNullTerritory, "KEPLER");
  kepler->securitySteps = 0;
  check(!validateGalaxy(illegalNullTerritory) && !setSecurity(state, "KEPLER", 0),
        "0.0 inside NPC territory rejects through validation and controlled mutation");

  auto claims = state;
  check(setNullSovereignty(claims, "VERGE_ANCHOR", SovereignType::playerGuild, "guild.test") &&
        findState(claims, "VERGE_ANCHOR")->securitySteps == 0 &&
        setNullSovereignty(claims, "VERGE_ANCHOR", SovereignType::playerAlliance, "alliance.test") &&
        findState(claims, "VERGE_ANCHOR")->securitySteps == 0 &&
        setNullSovereignty(claims, "VERGE_ANCHOR", SovereignType::unclaimed, "") &&
        !setNullSovereignty(claims, "KEPLER", SovereignType::playerGuild, "guild.test") &&
        !setNullSovereignty(claims, "VERGE_ANCHOR", SovereignType::npcFaction, "faction.compact"),
        "unclaimed Guild and Alliance Zero Space claims preserve 0.0 and reject NPC/non-zero claims");
  check(!setNullSovereignty(claims, "DARKWARD", SovereignType::playerGuild, "guild.test") &&
        setDiscovery(claims, "DARKWARD", DiscoveryState::charted) &&
        setNullSovereignty(claims, "DARKWARD", SovereignType::playerGuild, "guild.test") &&
        !setDiscovery(claims, "DARKWARD", DiscoveryState::unknown),
        "unknown Zero Space must be charted before a player sovereignty claim");
  check(!validateSovereignty(0, {SovereignType::playerGuild, "alliance.wrong"}) &&
        !validateSovereignty(0, {SovereignType::playerAlliance, "alliance.bad\nwire"}) &&
        validateSovereignty(0, {SovereignType::playerGuild, "guild.valid-id_1"}),
        "player sovereignty identifiers are typed and safe bounded wire tokens");

  for (const auto& territory : factionTerritories()) {
    const auto* capital = findSystem(territory.capitalSystemId);
    const auto* hub = findSystem(territory.commerceHubSystemId);
    check(capital && hub && capital != hub && hasRole(*capital, SystemRole::capital) &&
          hasRole(*hub, SystemRole::commerceHub) && adjacent(capital->id, hub->id),
          "each major faction has a distinct adjacent capital and commerce hub");
  }

  std::set<std::string> visited;
  std::vector<std::string> pending{"CONCORDIA"};
  while (!pending.empty()) {
    auto current = pending.back(); pending.pop_back();
    if (!visited.emplace(current).second) continue;
    for (auto& next : adjacentSystems(current)) pending.push_back(std::move(next));
  }
  check(visited.size() == systems().size(), "permanent Gate graph is fully connected");

  std::size_t trunks = 0, core = 0, secondary = 0, frontier = 0, backwater = 0;
  for (const auto& gate : gateConnections()) {
    trunks += gate.routeClass == RouteClass::majorInterfactionTrunk;
    core += gate.routeClass == RouteClass::factionCore;
    secondary += gate.routeClass == RouteClass::secondary;
    frontier += gate.routeClass == RouteClass::frontier;
    backwater += gate.routeClass == RouteClass::backwater;
  }
  check(trunks == 6 && core > trunks && secondary > trunks && frontier >= trunks && backwater >= trunks,
        "major trunks remain a small subset beside core secondary frontier and backwater routes");
  for (const auto& system : systems())
    if (hasRole(system, SystemRole::deadEnd))
      check(adjacentSystems(system.id).size() == 1, "declared backwater dead ends have one permanent Gate");

  const auto borders = deriveBorders(state);
  check(borders.size() == 21 && borders.front().connectionId == "TRUNK.KEPLER.FARPOINT" &&
        std::any_of(borders.begin(), borders.end(), [](const auto& border) {
          return border.connectionId == "ZERO.OPEN_WOUND.WATCHPOINT";
        }), "derived borders include the new Blackwater-to-Directorate exit");
  auto changedBorders = state;
  check(setNullSovereignty(changedBorders, "VERGE_ANCHOR", SovereignType::playerGuild, "guild.pathfinders") &&
        deriveBorders(changedBorders).size() == 23 &&
        setNullSovereignty(changedBorders, "PATHFINDER_REACH", SovereignType::playerGuild,
                           "guild.pathfinders") &&
        deriveBorders(changedBorders).size() == 24,
        "changing nullsec sovereignty changes derived borders without changing topology or security");

  auto infrastructureState = state;
  check(setNullSovereignty(infrastructureState, "PATHFINDER_REACH", SovereignType::playerGuild,
                           "guild.pathfinders"),
        "charted eligible nullsec accepts a future Guild infrastructure owner");
  const auto* pathfinderDefinition = findSystem("PATHFINDER_REACH");
  const auto* pathfinderState = findState(infrastructureState, "PATHFINDER_REACH");
  check(pathfinderDefinition && pathfinderState &&
        canHostFuturePlayerInfrastructure(*pathfinderDefinition, *pathfinderState) &&
        !canHostFuturePlayerInfrastructure(*findSystem("VERGE_ANCHOR"),
                                           *findState(infrastructureState, "VERGE_ANCHOR")) &&
        !canHostFuturePlayerInfrastructure(*findSystem("KEPLER"), *findState(state, "KEPLER")),
        "future strategic travel hook requires player sovereignty in eligible 0.0");
  check(setDiscovery(state, "DARKWARD", DiscoveryState::charted) &&
        findState(state, "DARKWARD")->sovereignty.type == SovereignType::unclaimed &&
        !setDiscovery(state, "DARKWARD", static_cast<DiscoveryState>(99)),
        "discovery changes independently from sovereignty");

  const auto physicalSeed = seedState();
  auto knowledge = seedKnowledge(physicalSeed);
  const auto* keplerKnowledge = findKnowledge(knowledge, "KEPLER");
  auto* darkwardKnowledge = findKnowledge(knowledge, "DARKWARD");
  check(validateKnowledge(knowledge) && keplerKnowledge && darkwardKnowledge &&
        keplerKnowledge->level == KnowledgeLevel::surveyed &&
        darkwardKnowledge->level == KnowledgeLevel::undetected &&
        !authorizedSystemLine(*findSystem("DARKWARD"), *findState(physicalSeed, "DARKWARD"),
                              *darkwardKnowledge),
        "per-commander knowledge seeds separately from physical system truth");
  check(!advanceKnowledge(knowledge, "DARKWARD", KnowledgeLevel::classified) &&
        advanceKnowledge(knowledge, "DARKWARD", KnowledgeLevel::detected),
        "knowledge cannot skip the DETECTED stage");
  auto detectedLine = authorizedSystemLine(*findSystem("DARKWARD"), *findState(physicalSeed, "DARKWARD"),
                                           *findKnowledge(knowledge, "DARKWARD"));
  check(detectedLine && detectedLine->find("knowledge=DETECTED") != std::string::npos &&
        detectedLine->find("DARKWARD") == std::string::npos &&
        detectedLine->find("name=") == std::string::npos && detectedLine->find("resources=") == std::string::npos &&
        advanceKnowledge(knowledge, "DARKWARD", KnowledgeLevel::classified),
        "DETECTED read models omit classified located and surveyed truth");
  auto classifiedLine = authorizedSystemLine(*findSystem("DARKWARD"), *findState(physicalSeed, "DARKWARD"),
                                             *findKnowledge(knowledge, "DARKWARD"));
  check(classifiedLine && classifiedLine->find("knowledge=CLASSIFIED") != std::string::npos &&
        classifiedLine->find("DARKWARD") == std::string::npos &&
        classifiedLine->find("domain=KNOWN_SPACE") != std::string::npos &&
        classifiedLine->find("region=") == std::string::npos &&
        advanceKnowledge(knowledge, "DARKWARD", KnowledgeLevel::located),
        "CLASSIFIED read models expose classification without location");
  auto locatedLine = authorizedSystemLine(*findSystem("DARKWARD"), *findState(physicalSeed, "DARKWARD"),
                                          *findKnowledge(knowledge, "DARKWARD"));
  check(locatedLine && locatedLine->find("knowledge=LOCATED") != std::string::npos &&
        locatedLine->find("region=OUTER_REACH") != std::string::npos &&
        locatedLine->find("resources=") == std::string::npos &&
        advanceKnowledge(knowledge, "DARKWARD", KnowledgeLevel::surveyed),
        "LOCATED read models expose navigation truth without survey metadata");
  auto surveyedLine = authorizedSystemLine(*findSystem("DARKWARD"), *findState(physicalSeed, "DARKWARD"),
                                           *findKnowledge(knowledge, "DARKWARD"));
  check(surveyedLine && surveyedLine->find("knowledge=SURVEYED") != std::string::npos &&
        surveyedLine->find("resources=") != std::string::npos &&
        !advanceKnowledge(knowledge, "DARKWARD", KnowledgeLevel::located),
        "SURVEYED read models expose full profile and knowledge never regresses");
  auto invalidKnowledge = knowledge;
  invalidKnowledge[1].systemId = invalidKnowledge[0].systemId;
  check(!validateKnowledge(invalidKnowledge) && !parseKnowledgeLevel("CHARTED"),
        "duplicate unknown and legacy knowledge encodings fail closed");

  const std::map<std::string_view, std::size_t> expectedZeroRegions{
    {"LANTERN_CORRIDOR", 9}, {"ASHEN_BASIN", 9}, {"NIGHT_POCKETS", 9},
    {"BLACKWATER_CHOKE", 9}, {"FAR_WILDS", 8}, {"CINDER_CLUSTER", 8}};
  auto expectedRegions = expectedZeroRegions;
  expectedRegions.insert({{"COMMONWEALTH_CORE", 12}, {"KEPLER_REACH", 15},
    {"COMPACT_MARCHES", 25}, {"DIRECTORATE_CORE", 25}, {"MERIDIAN_CORE", 23},
    {"AURELIAN_CORE", 9}, {"SYNOD_MARCHES", 11}, {"OUTER_REACH", 12},
    {"SHROUD_EXPANSE", 8}, {"VERGE", 8}});
  std::map<std::string_view, std::size_t> actualRegions;
  for (const auto& system : systems()) ++actualRegions[system.regionId];
  check(actualRegions == expectedRegions, "all sixteen regions have their exact intended system totals");
  for (const auto& [regionId, expected] : expectedZeroRegions) {
    std::size_t count = 0;
    std::size_t internallyConnected = 0;
    for (const auto& system : systems()) {
      if (system.regionId != regionId) continue;
      ++count;
      for (const auto& adjacentId : adjacentSystems(system.id)) {
        const auto* neighbor = findSystem(adjacentId);
        if (neighbor && neighbor->regionId == regionId) ++internallyConnected;
      }
    }
    check(count == expected && internallyConnected >= expected * 2,
          "each new Zero Space region has its intended size and internal routes");
  }
  check(adjacentSystems("NIGHT_LOCK").size() == 1 &&
        adjacentSystems("NO_RETURN").size() == 1 &&
        adjacentSystems("ENDLESS_BLUE").size() == 1 &&
        adjacentSystems("NIGHT_PEARL").size() == 4,
        "pocket and deep frontier branches include intentional blind ends and a shared access hub");
  check(adjacent("SPLIT_LIGHT", "BLACKWATER") && adjacent("OPEN_WOUND", "WATCHPOINT"),
        "Lantern and Blackwater form a dangerous through-route to faction space");
  std::size_t cinderBoundary = 0;
  for (const auto& connection : gateConnections()) {
    const bool firstCinder = findSystem(connection.firstSystemId)->regionId == "CINDER_CLUSTER";
    const bool secondCinder = findSystem(connection.secondSystemId)->regionId == "CINDER_CLUSTER";
    cinderBoundary += firstCinder != secondCinder;
  }
  check(cinderBoundary == 1 && adjacent("COLD_CINDER", "CINDER_CLUSTER_ONE"),
        "Cinder is internally cyclic but reaches the wider galaxy through one chokepoint");

  std::cout << "200-system Known Space topology population economy security and Zero Space tests passed\n";
}
