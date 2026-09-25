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
  {"OUTER_REACH", "Outer Reach"},
  {"AURELIAN_CORE", "Aurelian Core"},
  {"SYNOD_MARCHES", "Synod Marches"},
  {"SHROUD_EXPANSE", "Shroud Expanse"},
  {"VERGE", "The Verge"}
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
   "", DiscoveryState::unknown, SystemRole::frontier, true},

  {"NEW_HORIZON", "New Horizon", "COMMONWEALTH_CORE", 12, SovereignType::npcFaction,
   "faction.commonwealth", DiscoveryState::charted, developed, false},
  {"ASTERIA", "Asteria", "COMMONWEALTH_CORE", 10, SovereignType::npcFaction,
   "faction.commonwealth", DiscoveryState::charted, developed, false},
  {"GREENVALE", "Greenvale", "COMMONWEALTH_CORE", 7, SovereignType::npcFaction,
   "faction.commonwealth", DiscoveryState::charted, developed | resource, false},
  {"PORT_PROSPERITY", "Port Prosperity", "KEPLER_REACH", 5, SovereignType::npcFaction,
   "faction.commonwealth", DiscoveryState::charted, developed, false},
  {"DAWNSTAR", "Dawnstar", "KEPLER_REACH", 3, SovereignType::npcFaction,
   "faction.commonwealth", DiscoveryState::charted, SystemRole::frontier, false},
  {"FORT_WARDEN", "Fort Warden", "KEPLER_REACH", 1, SovereignType::npcFaction,
   "faction.commonwealth", DiscoveryState::charted, SystemRole::frontier, false},
  {"ORION_SPUR", "Orion Spur", "KEPLER_REACH", -3, SovereignType::npcFaction,
   "faction.commonwealth", DiscoveryState::charted, SystemRole::frontier | resource, false},
  {"SHORELINE", "Shoreline", "KEPLER_REACH", -9, SovereignType::npcFaction,
   "faction.commonwealth", DiscoveryState::charted, SystemRole::lowSecurityBackwater, false},

  {"FORGE_END", "Forge End", "COMPACT_MARCHES", 12, SovereignType::npcFaction,
   "faction.compact", DiscoveryState::charted, developed | resource, false},
  {"TUMBLE", "Tumble", "COMPACT_MARCHES", 6, SovereignType::npcFaction,
   "faction.compact", DiscoveryState::charted, developed, false},
  {"FREEPORT", "Freeport", "COMPACT_MARCHES", 4, SovereignType::npcFaction,
   "faction.compact", DiscoveryState::charted, developed, false},
  {"ANVIL", "Anvil", "COMPACT_MARCHES", 2, SovereignType::npcFaction,
   "faction.compact", DiscoveryState::charted, SystemRole::frontier | resource, false},
  {"HOMESTEAD", "Homestead", "COMPACT_MARCHES", -2, SovereignType::npcFaction,
   "faction.compact", DiscoveryState::charted, SystemRole::frontier, false},
  {"LONGWATCH", "Longwatch", "COMPACT_MARCHES", -8, SovereignType::npcFaction,
   "faction.compact", DiscoveryState::charted, SystemRole::frontier, false},
  {"SCRAPYARD", "Scrapyard", "COMPACT_MARCHES", -18, SovereignType::npcFaction,
   "faction.compact", DiscoveryState::charted, backwaterDeadEnd | resource, false},

  {"DOMINION", "Dominion", "DIRECTORATE_CORE", 18, SovereignType::npcFaction,
   "faction.directorate", DiscoveryState::charted, developed, false},
  {"CIPHER", "Cipher", "DIRECTORATE_CORE", 14, SovereignType::npcFaction,
   "faction.directorate", DiscoveryState::charted, developed | resource, false},
  {"BLACKGLASS", "Blackglass", "DIRECTORATE_CORE", 8, SovereignType::npcFaction,
   "faction.directorate", DiscoveryState::charted, developed, false},
  {"ORDER", "Order", "DIRECTORATE_CORE", 4, SovereignType::npcFaction,
   "faction.directorate", DiscoveryState::charted, developed, false},
  {"CAELUM", "Caelum", "DIRECTORATE_CORE", 2, SovereignType::npcFaction,
   "faction.directorate", DiscoveryState::charted, SystemRole::frontier | resource, false},
  {"OBSIDIAN", "Obsidian", "DIRECTORATE_CORE", -6, SovereignType::npcFaction,
   "faction.directorate", DiscoveryState::charted, SystemRole::frontier, false},
  {"SILENT_REACH", "Silent Reach", "DIRECTORATE_CORE", -22, SovereignType::npcFaction,
   "faction.directorate", DiscoveryState::charted, backwaterDeadEnd, false},

  {"ARCADIA_EXCHANGE", "Arcadia Exchange", "MERIDIAN_CORE", 17, SovereignType::npcFaction,
   "faction.meridian", DiscoveryState::charted, developed, false},
  {"CARAVAN", "Caravan", "MERIDIAN_CORE", 11, SovereignType::npcFaction,
   "faction.meridian", DiscoveryState::charted, developed, false},
  {"COPPERLINE", "Copperline", "MERIDIAN_CORE", 6, SovereignType::npcFaction,
   "faction.meridian", DiscoveryState::charted, developed | resource, false},
  {"STARLING", "Starling", "MERIDIAN_CORE", 2, SovereignType::npcFaction,
   "faction.meridian", DiscoveryState::charted, SystemRole::frontier, false},
  {"GILDED_REST", "Gilded Rest", "MERIDIAN_CORE", -4, SovereignType::npcFaction,
   "faction.meridian", DiscoveryState::charted, SystemRole::frontier, false},
  {"OPEN_MARKET", "Open Market", "MERIDIAN_CORE", -14, SovereignType::npcFaction,
   "faction.meridian", DiscoveryState::charted, backwaterDeadEnd, false},

  {"AURELIA", "Aurelia", "AURELIAN_CORE", 23, SovereignType::npcFaction,
   "faction.aurelian", DiscoveryState::charted, SystemRole::capital | developed, false},
  {"LUMEN", "Lumen", "AURELIAN_CORE", 20, SovereignType::npcFaction,
   "faction.aurelian", DiscoveryState::charted, SystemRole::commerceHub | developed, false},
  {"OBSERVATORY", "Observatory", "AURELIAN_CORE", 14, SovereignType::npcFaction,
   "faction.aurelian", DiscoveryState::charted, developed | resource, false},
  {"CANTICLE", "Canticle", "AURELIAN_CORE", 10, SovereignType::npcFaction,
   "faction.aurelian", DiscoveryState::charted, developed, false},
  {"EIDOLON", "Eidolon", "SYNOD_MARCHES", 6, SovereignType::npcFaction,
   "faction.aurelian", DiscoveryState::charted, developed | resource, false},
  {"HALO", "Halo", "SYNOD_MARCHES", 3, SovereignType::npcFaction,
   "faction.aurelian", DiscoveryState::charted, frontierGateway, false},
  {"NEW_PROVIDENCE", "New Providence", "SYNOD_MARCHES", 2, SovereignType::npcFaction,
   "faction.aurelian", DiscoveryState::charted, frontierGateway, false},
  {"PARALLAX", "Parallax", "SYNOD_MARCHES", -3, SovereignType::npcFaction,
   "faction.aurelian", DiscoveryState::charted, SystemRole::frontier | resource, false},
  {"QUIETUS", "Quietus", "SYNOD_MARCHES", -10, SovereignType::npcFaction,
   "faction.aurelian", DiscoveryState::charted, SystemRole::lowSecurityBackwater, false},
  {"PILGRIMS_END", "Pilgrims End", "SYNOD_MARCHES", -19, SovereignType::npcFaction,
   "faction.aurelian", DiscoveryState::charted, backwaterDeadEnd, false},

  {"MISTRAL_VOID", "Mistral Void", "OUTER_REACH", 0, SovereignType::unclaimed,
   "", DiscoveryState::charted, SystemRole::frontier, true},
  {"BLACK_REEF", "Black Reef", "OUTER_REACH", 0, SovereignType::unclaimed,
   "", DiscoveryState::unknown, SystemRole::frontier, true},
  {"SHROUD_EDGE", "Shroud Edge", "OUTER_REACH", 0, SovereignType::unclaimed,
   "", DiscoveryState::charted, SystemRole::frontier, true},
  {"PALE_CURRENT", "Pale Current", "OUTER_REACH", 0, SovereignType::unclaimed,
   "", DiscoveryState::unknown, SystemRole::frontier, true},
  {"NIGHTGLASS", "Nightglass", "OUTER_REACH", 0, SovereignType::unclaimed,
   "", DiscoveryState::unknown, SystemRole::frontier, true},
  {"BROKEN_COMPASS", "Broken Compass", "OUTER_REACH", 0, SovereignType::unclaimed,
   "", DiscoveryState::charted, SystemRole::frontier, true},
  {"RELAY_GRAVE", "Relay Grave", "OUTER_REACH", 0, SovereignType::unclaimed,
   "", DiscoveryState::unknown, SystemRole::frontier, true},
  {"CINDER_DEEP", "Cinder Deep", "OUTER_REACH", 0, SovereignType::unclaimed,
   "", DiscoveryState::charted, SystemRole::frontier, true},
  {"SILENT_CARTOGRAPHY", "Silent Cartography", "SHROUD_EXPANSE", 0, SovereignType::unclaimed,
   "", DiscoveryState::unknown, SystemRole::frontier, true},
  {"TWIN_SHADOWS", "Twin Shadows", "SHROUD_EXPANSE", 0, SovereignType::unclaimed,
   "", DiscoveryState::unknown, SystemRole::frontier, true},
  {"RIFTWARD", "Riftward", "SHROUD_EXPANSE", 0, SovereignType::unclaimed,
   "", DiscoveryState::unknown, SystemRole::frontier, true},
  {"COLD_ARCHIVE", "Cold Archive", "SHROUD_EXPANSE", 0, SovereignType::unclaimed,
   "", DiscoveryState::unknown, SystemRole::frontier, true},
  {"EMBER_WELL", "Ember Well", "SHROUD_EXPANSE", 0, SovereignType::unclaimed,
   "", DiscoveryState::charted, SystemRole::frontier, true},
  {"WANDERERS_REST", "Wanderers Rest", "SHROUD_EXPANSE", 0, SovereignType::unclaimed,
   "", DiscoveryState::charted, SystemRole::frontier, true},
  {"DEAD_RECKONING", "Dead Reckoning", "SHROUD_EXPANSE", 0, SovereignType::unclaimed,
   "", DiscoveryState::unknown, SystemRole::frontier, true},
  {"OBSIDIAN_TIDE", "Obsidian Tide", "SHROUD_EXPANSE", 0, SovereignType::unclaimed,
   "", DiscoveryState::unknown, SystemRole::frontier, true},
  {"GHOSTLIGHT", "Ghostlight", "VERGE", 0, SovereignType::unclaimed,
   "", DiscoveryState::charted, SystemRole::frontier, true},
  {"FALLEN_GATE", "Fallen Gate", "VERGE", 0, SovereignType::unclaimed,
   "", DiscoveryState::unknown, SystemRole::frontier, true},
  {"ASH_DRIFT", "Ash Drift", "VERGE", 0, SovereignType::unclaimed,
   "", DiscoveryState::unknown, SystemRole::frontier, true},
  {"UNBOUND", "Unbound", "VERGE", 0, SovereignType::unclaimed,
   "", DiscoveryState::charted, SystemRole::frontier, true},
  {"WAYWARD", "Wayward", "VERGE", 0, SovereignType::unclaimed,
   "", DiscoveryState::unknown, SystemRole::frontier, true},
  {"DUSK_MERIDIAN", "Dusk Meridian", "VERGE", 0, SovereignType::unclaimed,
   "", DiscoveryState::unknown, SystemRole::frontier, true},
  {"FAR_LANTERN", "Far Lantern", "VERGE", 0, SovereignType::unclaimed,
   "", DiscoveryState::charted, SystemRole::frontier, true},
  {"TERMINUS_ECHO", "Terminus Echo", "VERGE", 0, SovereignType::unclaimed,
   "", DiscoveryState::unknown, SystemRole::frontier, true}
}};

std::array<SystemProfile, kSystemCount> makeSystemProfiles() {
  std::array<SystemProfile, kSystemCount> profiles{};
  const auto is = [](std::string_view value, std::string_view expected) { return value == expected; };
  for (std::size_t index = 0; index < kSystems.size(); ++index) {
    const auto& system = kSystems[index];
    auto& profile = profiles[index];
    profile.systemId = system.id;
    profile.planetCount = static_cast<std::uint8_t>(2 + (index * 3) % 9);
    profile.moonCount = static_cast<std::uint8_t>((index * 5 + 2) % 16);
    profile.asteroidFieldCount = static_cast<std::uint8_t>(1 + index % 3);

    if (system.initialSecuritySteps == 0) {
      profile.population = 0;
      profile.settlement = SettlementScale::uninhabited;
      profile.traffic = TrafficProfile::dormant;
      profile.primaryEconomy = EconomicRole::frontierMixed;
      profile.infrastructure = Infrastructure::none;
      profile.production = CommodityFamily::none;
      profile.consumption = CommodityFamily::none;
    } else if (hasRole(system, SystemRole::capital)) {
      profile.population = 3'800'000'000ULL + index * 70'000'000ULL;
      profile.settlement = SettlementScale::metropolitan;
      profile.traffic = TrafficProfile::dense;
      profile.primaryEconomy = EconomicRole::government;
      profile.infrastructure = Infrastructure::station | Infrastructure::orbitalSettlement |
        Infrastructure::shipyard | Infrastructure::warehouse | Infrastructure::researchFacility |
        Infrastructure::militaryFacility;
    } else if (hasRole(system, SystemRole::commerceHub)) {
      profile.population = 900'000'000ULL + index * 35'000'000ULL;
      profile.settlement = SettlementScale::metropolitan;
      profile.traffic = TrafficProfile::dense;
      profile.primaryEconomy = EconomicRole::commerce;
      profile.infrastructure = Infrastructure::station | Infrastructure::orbitalSettlement |
        Infrastructure::shipyard | Infrastructure::factory | Infrastructure::warehouse;
    } else if (hasRole(system, SystemRole::developedInterior)) {
      profile.population = 12'000'000ULL + (index * 17'000'000ULL) % 420'000'000ULL;
      profile.settlement = SettlementScale::established;
      profile.traffic = TrafficProfile::regular;
      profile.primaryEconomy = hasRole(system, SystemRole::industrialResource) ?
        EconomicRole::industry : EconomicRole::frontierMixed;
      profile.infrastructure = Infrastructure::station | Infrastructure::orbitalSettlement |
        Infrastructure::factory | Infrastructure::warehouse;
    } else {
      profile.population = system.initialSecuritySteps < 0 ?
        800ULL + index * 370ULL : 75'000ULL + index * 8'500ULL;
      profile.settlement = profile.population >= 100'000 ? SettlementScale::colony : SettlementScale::outpost;
      profile.traffic = system.initialSecuritySteps < 0 ? TrafficProfile::sparse : TrafficProfile::frontier;
      profile.primaryEconomy = hasRole(system, SystemRole::industrialResource) ?
        EconomicRole::extraction : EconomicRole::frontierMixed;
      profile.infrastructure = Infrastructure::station | Infrastructure::frontierOutpost;
    }

    if (system.regionId == "COMMONWEALTH_CORE") {
      profile.stellarType = StellarType::yellowDwarf;
      profile.resources = ResourceFamily::structuralMetals | ResourceFamily::organics;
    } else if (system.regionId == "KEPLER_REACH") {
      profile.stellarType = StellarType::orangeDwarf;
      profile.resources = ResourceFamily::industrialMinerals | ResourceFamily::iceVolatiles |
        ResourceFamily::organics;
    } else if (system.regionId == "COMPACT_MARCHES") {
      profile.stellarType = StellarType::redDwarf;
      profile.resources = ResourceFamily::structuralMetals | ResourceFamily::industrialMinerals |
        ResourceFamily::salvage;
    } else if (system.regionId == "DIRECTORATE_CORE") {
      profile.stellarType = StellarType::whiteStar;
      profile.resources = ResourceFamily::rareMetals | ResourceFamily::fuelIsotopes |
        ResourceFamily::industrialMinerals;
    } else if (system.regionId == "MERIDIAN_CORE") {
      profile.stellarType = StellarType::yellowDwarf;
      profile.resources = ResourceFamily::industrialGases | ResourceFamily::fuelIsotopes |
        ResourceFamily::organics;
    } else if (system.regionId == "AURELIAN_CORE") {
      profile.stellarType = StellarType::binary;
      profile.resources = ResourceFamily::scientificSamples | ResourceFamily::industrialGases |
        ResourceFamily::gateMaterials;
    } else if (system.regionId == "SYNOD_MARCHES") {
      profile.stellarType = StellarType::binary;
      profile.resources = ResourceFamily::scientificSamples | ResourceFamily::rareMetals |
        ResourceFamily::gateMaterials;
    } else if (system.regionId == "OUTER_REACH") {
      profile.stellarType = StellarType::redDwarf;
      profile.resources = ResourceFamily::structuralMetals | ResourceFamily::iceVolatiles |
        ResourceFamily::salvage;
    } else if (system.regionId == "SHROUD_EXPANSE") {
      profile.stellarType = StellarType::orangeDwarf;
      profile.resources = ResourceFamily::rareMetals | ResourceFamily::industrialGases |
        ResourceFamily::scientificSamples;
    } else {
      profile.stellarType = StellarType::whiteStar;
      profile.resources = ResourceFamily::fuelIsotopes | ResourceFamily::scientificSamples |
        ResourceFamily::gateMaterials;
    }

    if (system.initialSecuritySteps < 0)
      profile.resources = profile.resources | ResourceFamily::rareMetals | ResourceFamily::salvage;
    if (system.initialSecuritySteps == 0)
      profile.resources = profile.resources | ResourceFamily::iceVolatiles;
    if (is(system.id, "RIFTWARD") || is(system.id, "FALLEN_GATE") ||
        is(system.id, "TERMINUS_ECHO"))
      profile.resources = profile.resources | ResourceFamily::xenologicalTraces;
    if (is(system.id, "COLD_ARCHIVE") || is(system.id, "OBSIDIAN_TIDE"))
      profile.stellarType = StellarType::neutronStar;

    if (is(system.id, "GREENVALE") || is(system.id, "HARROW") ||
        is(system.id, "HOMESTEAD")) profile.primaryEconomy = EconomicRole::agriculture;
    else if (is(system.id, "VANTAGE") || is(system.id, "FORGE_END"))
      profile.primaryEconomy = EconomicRole::shipbuilding;
    else if (is(system.id, "IRONHOME") || is(system.id, "ORION_SPUR") ||
             is(system.id, "ANVIL") || is(system.id, "CAELUM"))
      profile.primaryEconomy = EconomicRole::extraction;
    else if (is(system.id, "PROSPERITY") || is(system.id, "COPPERLINE"))
      profile.primaryEconomy = EconomicRole::refining;
    else if (is(system.id, "SENTINEL") || is(system.id, "FORT_WARDEN") ||
             is(system.id, "ORDER")) profile.primaryEconomy = EconomicRole::military;
    else if (is(system.id, "OBSERVATORY") || is(system.id, "EIDOLON") ||
             is(system.id, "PARALLAX")) profile.primaryEconomy = EconomicRole::research;
    else if (is(system.id, "KEPLER") || is(system.id, "GOLDEN_GATE") ||
             is(system.id, "HALO")) profile.primaryEconomy = EconomicRole::gateLogistics;
    else if (is(system.id, "SCRAPYARD") || is(system.id, "RELAY_GRAVE") ||
             is(system.id, "DEAD_RECKONING")) profile.primaryEconomy = EconomicRole::salvage;
    else if (is(system.id, "PORT_PROSPERITY") || is(system.id, "ARCADIA_EXCHANGE"))
      profile.primaryEconomy = EconomicRole::commerce;

    switch (profile.primaryEconomy) {
      case EconomicRole::government:
        profile.production = CommodityFamily::consumerGoods | CommodityFamily::medicine;
        profile.consumption = CommodityFamily::food | CommodityFamily::parts | CommodityFamily::fuel;
        break;
      case EconomicRole::commerce:
        profile.production = CommodityFamily::consumerGoods | CommodityFamily::electronics;
        profile.consumption = CommodityFamily::food | CommodityFamily::fuel | CommodityFamily::processedMaterials;
        profile.infrastructure = profile.infrastructure | Infrastructure::warehouse;
        break;
      case EconomicRole::industry:
      case EconomicRole::shipbuilding:
        profile.production = CommodityFamily::parts | CommodityFamily::machinery;
        profile.consumption = CommodityFamily::ore | CommodityFamily::processedMaterials |
          CommodityFamily::electronics | CommodityFamily::fuel;
        profile.infrastructure = profile.infrastructure | Infrastructure::factory | Infrastructure::shipyard;
        break;
      case EconomicRole::extraction:
        profile.production = CommodityFamily::ore;
        profile.consumption = CommodityFamily::food | CommodityFamily::parts | CommodityFamily::machinery;
        profile.infrastructure = profile.infrastructure | Infrastructure::extractionPlatform;
        break;
      case EconomicRole::refining:
        profile.production = CommodityFamily::processedMaterials | CommodityFamily::fuel;
        profile.consumption = CommodityFamily::ore | CommodityFamily::parts;
        profile.infrastructure = profile.infrastructure | Infrastructure::refinery;
        break;
      case EconomicRole::agriculture:
        profile.production = CommodityFamily::food;
        profile.consumption = CommodityFamily::machinery | CommodityFamily::parts | CommodityFamily::medicine;
        profile.infrastructure = profile.infrastructure | Infrastructure::agriculturalHabitat;
        break;
      case EconomicRole::research:
        profile.production = CommodityFamily::scientificEquipment | CommodityFamily::electronics;
        profile.consumption = CommodityFamily::medicine | CommodityFamily::parts | CommodityFamily::fuel;
        profile.infrastructure = profile.infrastructure | Infrastructure::researchFacility;
        break;
      case EconomicRole::military:
        profile.production = CommodityFamily::parts | CommodityFamily::electronics;
        profile.consumption = CommodityFamily::food | CommodityFamily::fuel | CommodityFamily::medicine;
        profile.infrastructure = profile.infrastructure | Infrastructure::militaryFacility;
        break;
      case EconomicRole::salvage:
        profile.production = CommodityFamily::parts | CommodityFamily::ore;
        profile.consumption = CommodityFamily::food | CommodityFamily::fuel;
        profile.infrastructure = profile.infrastructure | Infrastructure::extractionPlatform;
        break;
      case EconomicRole::gateLogistics:
        profile.production = CommodityFamily::scientificEquipment;
        profile.consumption = CommodityFamily::parts | CommodityFamily::fuel | CommodityFamily::electronics;
        profile.infrastructure = profile.infrastructure | Infrastructure::gatewatchFacility |
          Infrastructure::warehouse;
        break;
      case EconomicRole::tourism:
        profile.production = CommodityFamily::consumerGoods;
        profile.consumption = CommodityFamily::food | CommodityFamily::medicine;
        break;
      case EconomicRole::frontierMixed:
        if (profile.population != 0) {
          profile.production = CommodityFamily::ore;
          profile.consumption = CommodityFamily::food | CommodityFamily::parts | CommodityFamily::fuel;
        }
        break;
    }
    if (profile.population == 0) {
      // Resource/economic potential remains metadata; no settlement means no active production.
      profile.infrastructure = Infrastructure::none;
      profile.production = CommodityFamily::none;
      profile.consumption = CommodityFamily::none;
    }
    if (is(system.id, "KEPLER")) {
      // These are the three currently active M2 commodity families at Kepler/Cinder.
      profile.production = CommodityFamily::food | CommodityFamily::parts;
      profile.consumption = CommodityFamily::ore | CommodityFamily::food | CommodityFamily::parts;
      profile.infrastructure = profile.infrastructure | Infrastructure::warehouse |
        Infrastructure::extractionPlatform;
    }
  }

  struct InhabitedNullSeed { std::string_view id; std::uint64_t population; };
  const InhabitedNullSeed inhabitedNull[]{
    {"VERGE_ANCHOR", 1'200}, {"PATHFINDER_REACH", 380}, {"COALITION_MARCH", 2'400},
    {"MISTRAL_VOID", 90}, {"SHROUD_EDGE", 720}, {"BROKEN_COMPASS", 40},
    {"CINDER_DEEP", 310}, {"EMBER_WELL", 180}, {"WANDERERS_REST", 4'800},
    {"GHOSTLIGHT", 65}, {"UNBOUND", 1'400}, {"FAR_LANTERN", 320}
  };
  for (const auto& seed : inhabitedNull) {
    for (auto& profile : profiles) {
      if (profile.systemId != seed.id) continue;
      profile.population = seed.population;
      profile.settlement = seed.population >= 2'000 ? SettlementScale::colony : SettlementScale::outpost;
      profile.traffic = seed.population >= 1'000 ? TrafficProfile::frontier : TrafficProfile::sparse;
      profile.primaryEconomy = seed.id == "WANDERERS_REST" ? EconomicRole::commerce :
        seed.id == "CINDER_DEEP" ? EconomicRole::extraction : EconomicRole::frontierMixed;
      profile.infrastructure = Infrastructure::frontierOutpost | Infrastructure::warehouse;
      if (profile.primaryEconomy == EconomicRole::extraction) {
        profile.production = CommodityFamily::ore;
        profile.consumption = CommodityFamily::food | CommodityFamily::parts | CommodityFamily::fuel;
        profile.infrastructure = profile.infrastructure | Infrastructure::extractionPlatform;
      } else if (profile.primaryEconomy == EconomicRole::commerce) {
        profile.production = CommodityFamily::consumerGoods;
        profile.consumption = CommodityFamily::food | CommodityFamily::fuel | CommodityFamily::parts;
      } else {
        profile.production = CommodityFamily::ore;
        profile.consumption = CommodityFamily::food | CommodityFamily::parts | CommodityFamily::fuel;
      }
      break;
    }
  }
  return profiles;
}

const std::array<SystemProfile, kSystemCount> kSystemProfiles = makeSystemProfiles();

const std::array<FactionTerritory, kFactionTerritoryCount> kTerritories{{
  {"faction.commonwealth", "CONCORDIA", "VANTAGE"},
  {"faction.compact", "FREESTEAD", "CROSSROADS"},
  {"faction.directorate", "SOLARIS", "HELIOS_EXCHANGE"},
  {"faction.meridian", "MERIDIAN_FORUM", "MERIDIAN_PRIME"},
  {"faction.aurelian", "AURELIA", "LUMEN"}
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
  {"GATE.COALITION_MARCH.DARKWARD", "COALITION_MARCH", "DARKWARD", RouteClass::backwater},

  {"GATE.VANTAGE.NEW_HORIZON", "VANTAGE", "NEW_HORIZON", RouteClass::factionCore},
  {"GATE.NEW_HORIZON.ASTERIA", "NEW_HORIZON", "ASTERIA", RouteClass::factionCore},
  {"GATE.ASTERIA.GREENVALE", "ASTERIA", "GREENVALE", RouteClass::secondary},
  {"GATE.GREENVALE.HARROW", "GREENVALE", "HARROW", RouteClass::secondary},
  {"GATE.NEW_HORIZON.PORT_PROSPERITY", "NEW_HORIZON", "PORT_PROSPERITY", RouteClass::secondary},
  {"GATE.PORT_PROSPERITY.DAWNSTAR", "PORT_PROSPERITY", "DAWNSTAR", RouteClass::frontier},
  {"GATE.DAWNSTAR.KEPLER", "DAWNSTAR", "KEPLER", RouteClass::secondary},
  {"GATE.DAWNSTAR.FORT_WARDEN", "DAWNSTAR", "FORT_WARDEN", RouteClass::frontier},
  {"GATE.FORT_WARDEN.ORION_SPUR", "FORT_WARDEN", "ORION_SPUR", RouteClass::frontier},
  {"GATE.ORION_SPUR.SHORELINE", "ORION_SPUR", "SHORELINE", RouteClass::backwater},
  {"GATE.SHORELINE.KEPLER", "SHORELINE", "KEPLER", RouteClass::secondary},

  {"GATE.FREESTEAD.FORGE_END", "FREESTEAD", "FORGE_END", RouteClass::factionCore},
  {"GATE.FORGE_END.TUMBLE", "FORGE_END", "TUMBLE", RouteClass::secondary},
  {"GATE.TUMBLE.CROSSROADS", "TUMBLE", "CROSSROADS", RouteClass::secondary},
  {"GATE.FORGE_END.FREEPORT", "FORGE_END", "FREEPORT", RouteClass::secondary},
  {"GATE.FREEPORT.ANVIL", "FREEPORT", "ANVIL", RouteClass::frontier},
  {"GATE.ANVIL.FARPOINT", "ANVIL", "FARPOINT", RouteClass::secondary},
  {"GATE.ANVIL.HOMESTEAD", "ANVIL", "HOMESTEAD", RouteClass::frontier},
  {"GATE.HOMESTEAD.LONGWATCH", "HOMESTEAD", "LONGWATCH", RouteClass::frontier},
  {"GATE.LONGWATCH.FARSIDE", "LONGWATCH", "FARSIDE", RouteClass::secondary},
  {"GATE.LONGWATCH.SCRAPYARD", "LONGWATCH", "SCRAPYARD", RouteClass::backwater},

  {"GATE.SOLARIS.DOMINION", "SOLARIS", "DOMINION", RouteClass::factionCore},
  {"GATE.DOMINION.CIPHER", "DOMINION", "CIPHER", RouteClass::factionCore},
  {"GATE.CIPHER.HELIOS_EXCHANGE", "CIPHER", "HELIOS_EXCHANGE", RouteClass::secondary},
  {"GATE.CIPHER.BLACKGLASS", "CIPHER", "BLACKGLASS", RouteClass::secondary},
  {"GATE.BLACKGLASS.ORDER", "BLACKGLASS", "ORDER", RouteClass::secondary},
  {"GATE.ORDER.WATCHPOINT", "ORDER", "WATCHPOINT", RouteClass::secondary},
  {"GATE.ORDER.CAELUM", "ORDER", "CAELUM", RouteClass::frontier},
  {"GATE.CAELUM.OBSIDIAN", "CAELUM", "OBSIDIAN", RouteClass::frontier},
  {"GATE.OBSIDIAN.SENTINEL", "OBSIDIAN", "SENTINEL", RouteClass::secondary},
  {"GATE.OBSIDIAN.SILENT_REACH", "OBSIDIAN", "SILENT_REACH", RouteClass::backwater},

  {"GATE.MERIDIAN_FORUM.ARCADIA_EXCHANGE", "MERIDIAN_FORUM", "ARCADIA_EXCHANGE", RouteClass::factionCore},
  {"GATE.ARCADIA_EXCHANGE.CARAVAN", "ARCADIA_EXCHANGE", "CARAVAN", RouteClass::secondary},
  {"GATE.CARAVAN.MERIDIAN_PRIME", "CARAVAN", "MERIDIAN_PRIME", RouteClass::secondary},
  {"GATE.ARCADIA_EXCHANGE.COPPERLINE", "ARCADIA_EXCHANGE", "COPPERLINE", RouteClass::secondary},
  {"GATE.COPPERLINE.STARLING", "COPPERLINE", "STARLING", RouteClass::frontier},
  {"GATE.STARLING.GOLDEN_GATE", "STARLING", "GOLDEN_GATE", RouteClass::secondary},
  {"GATE.STARLING.GILDED_REST", "STARLING", "GILDED_REST", RouteClass::frontier},
  {"GATE.GILDED_REST.PROSPERITY", "GILDED_REST", "PROSPERITY", RouteClass::secondary},
  {"GATE.GILDED_REST.OPEN_MARKET", "GILDED_REST", "OPEN_MARKET", RouteClass::backwater},

  {"GATE.AURELIA.LUMEN", "AURELIA", "LUMEN", RouteClass::factionCore},
  {"GATE.LUMEN.OBSERVATORY", "LUMEN", "OBSERVATORY", RouteClass::factionCore},
  {"GATE.LUMEN.CANTICLE", "LUMEN", "CANTICLE", RouteClass::secondary},
  {"GATE.OBSERVATORY.EIDOLON", "OBSERVATORY", "EIDOLON", RouteClass::secondary},
  {"GATE.CANTICLE.EIDOLON", "CANTICLE", "EIDOLON", RouteClass::secondary},
  {"GATE.EIDOLON.HALO", "EIDOLON", "HALO", RouteClass::frontier},
  {"GATE.CANTICLE.NEW_PROVIDENCE", "CANTICLE", "NEW_PROVIDENCE", RouteClass::frontier},
  {"GATE.HALO.NEW_PROVIDENCE", "HALO", "NEW_PROVIDENCE", RouteClass::secondary},
  {"GATE.LUMEN.HALO", "LUMEN", "HALO", RouteClass::secondary},
  {"GATE.HALO.PARALLAX", "HALO", "PARALLAX", RouteClass::frontier},
  {"GATE.PARALLAX.QUIETUS", "PARALLAX", "QUIETUS", RouteClass::backwater},
  {"GATE.QUIETUS.PILGRIMS_END", "QUIETUS", "PILGRIMS_END", RouteClass::backwater},

  {"TRUNK.GOLDEN_GATE.NEW_PROVIDENCE", "GOLDEN_GATE", "NEW_PROVIDENCE", RouteClass::majorInterfactionTrunk},
  {"TRUNK.HALO.WATCHPOINT", "HALO", "WATCHPOINT", RouteClass::majorInterfactionTrunk},

  {"GATE.FORT_WARDEN.MISTRAL_VOID", "FORT_WARDEN", "MISTRAL_VOID", RouteClass::frontier},
  {"GATE.ORION_SPUR.BLACK_REEF", "ORION_SPUR", "BLACK_REEF", RouteClass::frontier},
  {"GATE.ANVIL.SHROUD_EDGE", "ANVIL", "SHROUD_EDGE", RouteClass::frontier},
  {"GATE.HOMESTEAD.PALE_CURRENT", "HOMESTEAD", "PALE_CURRENT", RouteClass::frontier},
  {"GATE.CAELUM.NIGHTGLASS", "CAELUM", "NIGHTGLASS", RouteClass::frontier},
  {"GATE.OBSIDIAN.BROKEN_COMPASS", "OBSIDIAN", "BROKEN_COMPASS", RouteClass::frontier},
  {"GATE.STARLING.RELAY_GRAVE", "STARLING", "RELAY_GRAVE", RouteClass::frontier},
  {"GATE.GILDED_REST.CINDER_DEEP", "GILDED_REST", "CINDER_DEEP", RouteClass::frontier},
  {"GATE.PARALLAX.SILENT_CARTOGRAPHY", "PARALLAX", "SILENT_CARTOGRAPHY", RouteClass::frontier},
  {"GATE.QUIETUS.TWIN_SHADOWS", "QUIETUS", "TWIN_SHADOWS", RouteClass::frontier},

  {"GATE.MISTRAL_VOID.BLACK_REEF", "MISTRAL_VOID", "BLACK_REEF", RouteClass::secondary},
  {"GATE.BLACK_REEF.VERGE_ANCHOR", "BLACK_REEF", "VERGE_ANCHOR", RouteClass::secondary},
  {"GATE.MISTRAL_VOID.SHROUD_EDGE", "MISTRAL_VOID", "SHROUD_EDGE", RouteClass::secondary},
  {"GATE.SHROUD_EDGE.PALE_CURRENT", "SHROUD_EDGE", "PALE_CURRENT", RouteClass::secondary},
  {"GATE.PALE_CURRENT.PATHFINDER_REACH", "PALE_CURRENT", "PATHFINDER_REACH", RouteClass::secondary},
  {"GATE.SHROUD_EDGE.NIGHTGLASS", "SHROUD_EDGE", "NIGHTGLASS", RouteClass::secondary},
  {"GATE.NIGHTGLASS.BROKEN_COMPASS", "NIGHTGLASS", "BROKEN_COMPASS", RouteClass::secondary},
  {"GATE.BROKEN_COMPASS.COALITION_MARCH", "BROKEN_COMPASS", "COALITION_MARCH", RouteClass::secondary},
  {"GATE.NIGHTGLASS.RELAY_GRAVE", "NIGHTGLASS", "RELAY_GRAVE", RouteClass::secondary},
  {"GATE.RELAY_GRAVE.CINDER_DEEP", "RELAY_GRAVE", "CINDER_DEEP", RouteClass::secondary},
  {"GATE.CINDER_DEEP.DARKWARD", "CINDER_DEEP", "DARKWARD", RouteClass::secondary},
  {"GATE.RELAY_GRAVE.SILENT_CARTOGRAPHY", "RELAY_GRAVE", "SILENT_CARTOGRAPHY", RouteClass::secondary},
  {"GATE.SILENT_CARTOGRAPHY.TWIN_SHADOWS", "SILENT_CARTOGRAPHY", "TWIN_SHADOWS", RouteClass::backwater},
  {"GATE.TWIN_SHADOWS.DARKWARD", "TWIN_SHADOWS", "DARKWARD", RouteClass::secondary},

  {"GATE.DARKWARD.RIFTWARD", "DARKWARD", "RIFTWARD", RouteClass::backwater},
  {"GATE.RIFTWARD.COLD_ARCHIVE", "RIFTWARD", "COLD_ARCHIVE", RouteClass::backwater},
  {"GATE.COLD_ARCHIVE.EMBER_WELL", "COLD_ARCHIVE", "EMBER_WELL", RouteClass::backwater},
  {"GATE.EMBER_WELL.WANDERERS_REST", "EMBER_WELL", "WANDERERS_REST", RouteClass::backwater},
  {"GATE.WANDERERS_REST.DEAD_RECKONING", "WANDERERS_REST", "DEAD_RECKONING", RouteClass::backwater},
  {"GATE.DEAD_RECKONING.OBSIDIAN_TIDE", "DEAD_RECKONING", "OBSIDIAN_TIDE", RouteClass::backwater},
  {"GATE.OBSIDIAN_TIDE.GHOSTLIGHT", "OBSIDIAN_TIDE", "GHOSTLIGHT", RouteClass::backwater},
  {"GATE.GHOSTLIGHT.FALLEN_GATE", "GHOSTLIGHT", "FALLEN_GATE", RouteClass::backwater},
  {"GATE.FALLEN_GATE.ASH_DRIFT", "FALLEN_GATE", "ASH_DRIFT", RouteClass::backwater},
  {"GATE.ASH_DRIFT.UNBOUND", "ASH_DRIFT", "UNBOUND", RouteClass::backwater},
  {"GATE.UNBOUND.WAYWARD", "UNBOUND", "WAYWARD", RouteClass::backwater},
  {"GATE.WAYWARD.DUSK_MERIDIAN", "WAYWARD", "DUSK_MERIDIAN", RouteClass::backwater},
  {"GATE.DUSK_MERIDIAN.FAR_LANTERN", "DUSK_MERIDIAN", "FAR_LANTERN", RouteClass::backwater},
  {"GATE.FAR_LANTERN.TERMINUS_ECHO", "FAR_LANTERN", "TERMINUS_ECHO", RouteClass::backwater},
  {"GATE.RIFTWARD.TWIN_SHADOWS", "RIFTWARD", "TWIN_SHADOWS", RouteClass::secondary},
  {"GATE.EMBER_WELL.CINDER_DEEP", "EMBER_WELL", "CINDER_DEEP", RouteClass::secondary},
  {"GATE.WANDERERS_REST.GHOSTLIGHT", "WANDERERS_REST", "GHOSTLIGHT", RouteClass::secondary},
  {"GATE.OBSIDIAN_TIDE.UNBOUND", "OBSIDIAN_TIDE", "UNBOUND", RouteClass::secondary},
  {"GATE.FALLEN_GATE.WAYWARD", "FALLEN_GATE", "WAYWARD", RouteClass::secondary},
  {"GATE.DUSK_MERIDIAN.DARKWARD", "DUSK_MERIDIAN", "DARKWARD", RouteClass::secondary}
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

std::string infrastructureText(const SystemProfile& profile) {
  struct Entry { Infrastructure value; const char* name; };
  constexpr Entry entries[]{{Infrastructure::station, "STATION"},
    {Infrastructure::orbitalSettlement, "ORBITAL_SETTLEMENT"}, {Infrastructure::shipyard, "SHIPYARD"},
    {Infrastructure::refinery, "REFINERY"}, {Infrastructure::factory, "FACTORY"},
    {Infrastructure::warehouse, "WAREHOUSE"}, {Infrastructure::researchFacility, "RESEARCH"},
    {Infrastructure::militaryFacility, "MILITARY"}, {Infrastructure::gatewatchFacility, "GATEWATCH"},
    {Infrastructure::agriculturalHabitat, "AGRICULTURE"},
    {Infrastructure::extractionPlatform, "EXTRACTION"}, {Infrastructure::frontierOutpost, "OUTPOST"}};
  std::string result;
  for (const auto& entry : entries) {
    if (!hasInfrastructure(profile, entry.value)) continue;
    if (!result.empty()) result += ',';
    result += entry.name;
  }
  return result.empty() ? "NONE" : result;
}

std::string resourceText(const SystemProfile& profile) {
  struct Entry { ResourceFamily value; const char* name; };
  constexpr Entry entries[]{{ResourceFamily::structuralMetals, "STRUCTURAL_METALS"},
    {ResourceFamily::industrialMinerals, "INDUSTRIAL_MINERALS"},
    {ResourceFamily::rareMetals, "RARE_METALS"}, {ResourceFamily::iceVolatiles, "ICE_VOLATILES"},
    {ResourceFamily::fuelIsotopes, "FUEL_ISOTOPES"}, {ResourceFamily::industrialGases, "INDUSTRIAL_GASES"},
    {ResourceFamily::organics, "ORGANICS"}, {ResourceFamily::salvage, "SALVAGE"},
    {ResourceFamily::scientificSamples, "SCIENTIFIC_SAMPLES"},
    {ResourceFamily::gateMaterials, "GATE_MATERIALS_PLANNED"},
    {ResourceFamily::xenologicalTraces, "XENOLOGICAL_TRACES_RESTRICTED"}};
  std::string result;
  for (const auto& entry : entries) {
    if (!hasResource(profile, entry.value)) continue;
    if (!result.empty()) result += ',';
    result += entry.name;
  }
  return result.empty() ? "NONE" : result;
}

std::string commodityText(CommodityFamily value) {
  struct Entry { CommodityFamily value; const char* name; };
  constexpr Entry entries[]{{CommodityFamily::food, "FOOD"}, {CommodityFamily::ore, "ORE"},
    {CommodityFamily::parts, "PARTS"}, {CommodityFamily::fuel, "FUEL"},
    {CommodityFamily::medicine, "MEDICINE_PLANNED"}, {CommodityFamily::machinery, "MACHINERY_PLANNED"},
    {CommodityFamily::electronics, "ELECTRONICS_PLANNED"},
    {CommodityFamily::processedMaterials, "PROCESSED_MATERIALS_PLANNED"},
    {CommodityFamily::consumerGoods, "CONSUMER_GOODS_PLANNED"},
    {CommodityFamily::scientificEquipment, "SCIENTIFIC_EQUIPMENT_PLANNED"}};
  std::string result;
  for (const auto& entry : entries) {
    if ((static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(entry.value)) == 0) continue;
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
  for (std::size_t index = 0; index < kSystems.size(); ++index) {
    const auto& system = kSystems[index];
    const auto& profile = kSystemProfiles[index];
    if (system.id.empty() || system.displayName.empty() || !findRegion(system.regionId) ||
        !systemIds.emplace(system.id).second) return false;
    if (profile.systemId != system.id || profile.population > 20'000'000'000ULL ||
        profile.planetCount > 32 || profile.moonCount > 64 || profile.asteroidFieldCount > 8 ||
        profile.resources == ResourceFamily::none) return false;
    const bool inhabited = profile.population != 0;
    if (inhabited != (profile.settlement != SettlementScale::uninhabited) ||
        inhabited != (profile.infrastructure != Infrastructure::none) ||
        (inhabited && (profile.production == CommodityFamily::none ||
                       profile.consumption == CommodityFamily::none)) ||
        (!inhabited && (profile.production != CommodityFamily::none ||
                        profile.consumption != CommodityFamily::none))) return false;
    if ((hasRole(system, SystemRole::capital) || hasRole(system, SystemRole::commerceHub)) &&
        profile.settlement != SettlementScale::metropolitan) return false;
    if (hasRole(system, SystemRole::industrialResource) &&
        !hasResource(profile, ResourceFamily::structuralMetals) &&
        !hasResource(profile, ResourceFamily::industrialMinerals) &&
        !hasResource(profile, ResourceFamily::rareMetals) &&
        !hasResource(profile, ResourceFamily::industrialGases)) return false;
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
  if (trunkCount != 6 || std::any_of(routeCounts.begin(), routeCounts.end(),
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
const std::array<SystemProfile, kSystemCount>& systemProfiles() { return kSystemProfiles; }
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
const SystemProfile* findSystemProfile(std::string_view systemId) {
  for (const auto& profile : kSystemProfiles) if (profile.systemId == systemId) return &profile;
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
const char* settlementScaleName(SettlementScale value) {
  switch (value) {
    case SettlementScale::uninhabited: return "UNINHABITED";
    case SettlementScale::outpost: return "OUTPOST";
    case SettlementScale::colony: return "COLONY";
    case SettlementScale::established: return "ESTABLISHED";
    case SettlementScale::metropolitan: return "METROPOLITAN";
  }
  return "UNKNOWN";
}
const char* stellarTypeName(StellarType value) {
  switch (value) {
    case StellarType::redDwarf: return "RED_DWARF";
    case StellarType::orangeDwarf: return "ORANGE_DWARF";
    case StellarType::yellowDwarf: return "YELLOW_DWARF";
    case StellarType::whiteStar: return "WHITE_STAR";
    case StellarType::binary: return "BINARY";
    case StellarType::neutronStar: return "NEUTRON_STAR";
  }
  return "UNKNOWN";
}
const char* trafficProfileName(TrafficProfile value) {
  switch (value) {
    case TrafficProfile::dormant: return "DORMANT";
    case TrafficProfile::sparse: return "SPARSE";
    case TrafficProfile::frontier: return "FRONTIER";
    case TrafficProfile::regular: return "REGULAR";
    case TrafficProfile::dense: return "DENSE";
  }
  return "UNKNOWN";
}
const char* economicRoleName(EconomicRole value) {
  switch (value) {
    case EconomicRole::government: return "GOVERNMENT";
    case EconomicRole::commerce: return "COMMERCE";
    case EconomicRole::industry: return "INDUSTRY";
    case EconomicRole::shipbuilding: return "SHIPBUILDING";
    case EconomicRole::extraction: return "EXTRACTION";
    case EconomicRole::refining: return "REFINING";
    case EconomicRole::agriculture: return "AGRICULTURE";
    case EconomicRole::research: return "RESEARCH";
    case EconomicRole::military: return "MILITARY";
    case EconomicRole::tourism: return "TOURISM";
    case EconomicRole::salvage: return "SALVAGE";
    case EconomicRole::gateLogistics: return "GATE_LOGISTICS";
    case EconomicRole::frontierMixed: return "FRONTIER_MIXED";
  }
  return "UNKNOWN";
}
bool hasRole(const SystemDefinition& system, SystemRole role) {
  return (static_cast<std::uint32_t>(system.roles) & static_cast<std::uint32_t>(role)) != 0;
}
bool hasInfrastructure(const SystemProfile& profile, Infrastructure infrastructure) {
  return (static_cast<std::uint32_t>(profile.infrastructure) &
          static_cast<std::uint32_t>(infrastructure)) != 0;
}
bool hasResource(const SystemProfile& profile, ResourceFamily resource) {
  return (static_cast<std::uint32_t>(profile.resources) & static_cast<std::uint32_t>(resource)) != 0;
}
bool produces(const SystemProfile& profile, CommodityFamily commodity) {
  return (static_cast<std::uint32_t>(profile.production) & static_cast<std::uint32_t>(commodity)) != 0;
}
bool consumes(const SystemProfile& profile, CommodityFamily commodity) {
  return (static_cast<std::uint32_t>(profile.consumption) & static_cast<std::uint32_t>(commodity)) != 0;
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
  const auto* profile = findSystemProfile(definition.id);
  if (!profile) return "SYSTEM INVALID";
  return "SYSTEM id=" + std::string(definition.id) + " name=" + wireToken(definition.displayName) +
    " region=" + std::string(definition.regionId) + " security=" + securityValue(state.securitySteps) +
    " class=" + (classification ? securityClassName(*classification) : std::string("INVALID")) +
    " sovereign-type=" + sovereignTypeName(state.sovereignty.type) +
    " sovereign-id=" + (state.sovereignty.id.empty() ? std::string("none") : state.sovereignty.id) +
    " discovery=" + discoveryStateName(state.discovery) + " roles=" + rolesText(definition) +
    " future-infrastructure=" + std::to_string(canHostFuturePlayerInfrastructure(definition, state)) +
    " population=" + std::to_string(profile->population) +
    " settlement=" + settlementScaleName(profile->settlement) +
    " star=" + stellarTypeName(profile->stellarType) +
    " planets=" + std::to_string(profile->planetCount) +
    " moons=" + std::to_string(profile->moonCount) +
    " asteroid-fields=" + std::to_string(profile->asteroidFieldCount) +
    " traffic=" + trafficProfileName(profile->traffic) +
    " economy=" + economicRoleName(profile->primaryEconomy) +
    " infrastructure=" + infrastructureText(*profile) +
    " resources=" + resourceText(*profile) +
    " produces=" + commodityText(profile->production) +
    " consumes=" + commodityText(profile->consumption);
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
