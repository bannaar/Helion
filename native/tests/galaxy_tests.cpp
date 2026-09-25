#include "shared/galaxy.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
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
        classifySecurity(0) == SecurityClass::nullSecurity &&
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
        "security response hooks distinguish high low bands and nullsec without fake fleet behavior");

  auto state = seedState();
  for (const auto& system : systems())
    check(!system.id.empty() && findRegion(system.regionId), "every system has stable identity and a known region");
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
  check(topologyTrunks == 4, "canonical graph has four major inter-faction trunks");
  std::set<std::string> topologyVisited;
  std::vector<std::string> topologyPending{"CONCORDIA"};
  while (!topologyPending.empty()) {
    auto current = topologyPending.back(); topologyPending.pop_back();
    if (!topologyVisited.emplace(current).second) continue;
    for (auto& next : adjacentSystems(current)) topologyPending.push_back(std::move(next));
  }
  check(topologyVisited.size() == systems().size(), "canonical topology is connected before state validation");
  check(validateGalaxy(state), "canonical galaxy graph and seeded authoritative state validate");
  check(systems().size() == kSystemCount && regions().size() == kRegionCount &&
        gateConnections().size() == kGateConnectionCount, "canonical topology sizes are stable");

  std::size_t high = 0, low = 0, nullSecurity = 0;
  for (const auto& system : state) {
    const auto classification = classifySecurity(system.securitySteps);
    high += classification == SecurityClass::high;
    low += classification == SecurityClass::low;
    nullSecurity += classification == SecurityClass::nullSecurity;
    check(validateSovereignty(system.securitySteps, system.sovereignty),
          "every canonical sovereignty/security pair is legal");
  }
  check(high > 0 && low > 0 && nullSecurity == 4,
        "NPC high/low territory and explicit nullsec all exist");

  auto illegalNpcNull = state;
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
        "unclaimed Guild and Alliance nullsec claims preserve 0.0 and reject NPC/non-null claims");
  check(!setNullSovereignty(claims, "DARKWARD", SovereignType::playerGuild, "guild.test") &&
        setDiscovery(claims, "DARKWARD", DiscoveryState::charted) &&
        setNullSovereignty(claims, "DARKWARD", SovereignType::playerGuild, "guild.test") &&
        !setDiscovery(claims, "DARKWARD", DiscoveryState::unknown),
        "unknown nullsec must be charted before a player sovereignty claim");
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
  check(trunks == 4 && core > trunks && secondary > trunks && frontier >= trunks && backwater >= trunks,
        "major trunks remain a small subset beside core secondary frontier and backwater routes");
  for (const auto& system : systems())
    if (hasRole(system, SystemRole::deadEnd))
      check(adjacentSystems(system.id).size() == 1, "declared backwater dead ends have one permanent Gate");

  const auto borders = deriveBorders(state);
  check(borders.size() == 8 && borders.front().connectionId == "TRUNK.KEPLER.FARPOINT",
        "faction/nullsec borders derive deterministically from authoritative adjacency and sovereignty");
  auto changedBorders = state;
  check(setNullSovereignty(changedBorders, "VERGE_ANCHOR", SovereignType::playerGuild, "guild.pathfinders") &&
        deriveBorders(changedBorders).size() == 9 &&
        setNullSovereignty(changedBorders, "PATHFINDER_REACH", SovereignType::playerGuild,
                           "guild.pathfinders") &&
        deriveBorders(changedBorders).size() == 9,
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

  std::cout << "galactic topology territory security border route and nullsec tests passed\n";
}
