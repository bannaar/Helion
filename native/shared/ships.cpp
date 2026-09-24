#include "shared/ships.h"

#include <stdexcept>
#include <unordered_set>

namespace helion::ships {
namespace {
const std::vector<VisualProfile> kManufacturers{
  {"INDEPENDENT_YARDS", "Independent Yards", TechnologyFamily::human_engineered,
   "compact general-purpose wedges", "serviceable civilian aerospace frames", "painted alloy and graphite",
   "panel wear and owner repairs"},
  {"ASTER_DYNAMICS", "Aster Dynamics", TechnologyFamily::human_engineered,
   "long flowing forms, narrow noses and swept geometry", "integrated propulsion, recessed equipment and tight seams",
   "white, silver, graphite and dark glass", "precision-panel wear and heat discoloration"},
  {"TITAN_FORGE", "Titan Forge", TechnologyFamily::human_engineered,
   "broad silhouettes and blunt bows", "overlapping armor, external weapons and replaceable engines",
   "gunmetal, naval gray and warning markings", "scraped and replaced armor"},
  {"ORION_EXTRACTION_GROUP", "Orion Extraction Group", TechnologyFamily::human_engineered,
   "asymmetric industrial machinery", "external tanks, booms, cranes, tools and cargo cages",
   "construction yellow, orange and industrial white", "industrial grime and worn hazard markings"},
  {"HORIZON_SYSTEMS", "Horizon Systems", TechnologyFamily::human_engineered,
   "narrow dark faceted profiles", "recessed propulsion, concealed weapons and conformal sensors",
   "charcoal, black and blue-black", "damaged stealth and sensor surfaces"},
  {"COMPACT_YARDS", "Compact Yards", TechnologyFamily::human_engineered,
   "standardized modular hull blocks", "replaceable pods, mounting rails and accessible machinery",
   "owner paint and mismatched replacement panels", "mismatched field-repaired modules"},
  {"MERIDIAN_SHIPYARDS", "Meridian Shipyards", TechnologyFamily::human_engineered,
   "cargo or passenger volume-led silhouettes", "standard cargo interfaces and efficient engineering blocks",
   "commercial markings and corporate liveries", "loading damage and repainted branding"},
  {"HELIX_INTERSTELLAR", "Helix Interstellar", TechnologyFamily::human_engineered,
   "human frames interrupted by rings and field arrays", "instrumentation, projectors and isolated experiments",
   "pale scientific surfaces with restrained cyan fields", "instrument replacement and conduit scorching"}
};

const std::vector<VisualProfile> kTechnologies{
  {"HUMAN_ENGINEERED", "Human / Engineered", TechnologyFamily::human_engineered,
   "function remains legible from construction", "fabricated compartments and serviceable systems",
   "metal, composite, glass and applied coatings", "mechanical wear and field repair"},
  {"KHEPRI_GROWN", "Khepri / Grown", TechnologyFamily::khepri_grown,
   "biological symmetry, layered carapace and subtle movement", "living organic-mineral structure without human hardpoints",
   "translucent tissue, mineral shell and internal luminescence", "healing, scarring and carapace renewal"},
  {"VAEL_CONSTRUCTED", "Vael / Mathematically Constructed", TechnologyFamily::vael_constructed,
   "precise geometry and disconnected components", "uninterrupted surfaces and spatial reconfiguration",
   "seamless material with glowing channels", "unknown; no conventional weathering"}
};

const std::vector<OperatorProfile> kOperators{
  {"CIVILIAN", "Civilian", "registration marks and owner-selected livery"},
  {"COMMONWEALTH_NAVY", "Commonwealth Navy", "standard naval paint, ship numbers, fleet marks and standardized weapons"},
  {"FREE_SYSTEMS_COMPACT", "Free Systems Compact", "local militia marks, modular equipment and owner-system identification"},
  {"SOLAR_DIRECTORATE", "Solar Directorate", "symmetry, smooth armor, concealed systems and severe identification marks"},
  {"VANTA_CONVERSION", "Vanta Conversion", "stolen or disguised identity with concealed or intimidating illegal modifications"}
};

const std::vector<VisualVariantProfile> kVisualVariants{
  {"CIVILIAN", "Civilian", "CIVILIAN", "ordinary registered paint and markings"},
  {"CORPORATE", "Corporate", "CIVILIAN", "commercial livery and fleet registration"},
  {"COMMONWEALTH_NAVY", "Commonwealth Navy", "COMMONWEALTH_NAVY", "naval paint, ship number and fleet marks"},
  {"DIRECTORATE", "Directorate", "SOLAR_DIRECTORATE", "severe identification and concealed sensor treatment"},
  {"MILITIA", "Militia", "FREE_SYSTEMS_COMPACT", "local militia marks and modular equipment"},
  {"FRONTIER_WEATHERED", "Frontier / Weathered", "CIVILIAN", "owner paint and field-repaired panels"},
  {"PIRATE_CONVERSION", "Pirate Conversion", "VANTA_CONVERSION", "intimidating visible illegal modification"},
  {"SMUGGLER_CONVERSION", "Smuggler Conversion", "VANTA_CONVERSION", "deliberately concealed illicit alteration"},
  {"PROTOTYPE", "Prototype", "CIVILIAN", "test markings and exposed experimental equipment"}
};

std::vector<SlotSize> core(SlotSize plant, SlotSize thrusters, SlotSize fsd, SlotSize life,
                           SlotSize distributor, SlotSize sensors, SlotSize tank) {
  return {plant, thrusters, fsd, life, distributor, sensors, tank};
}

const std::vector<Definition> kRegistry{
  {"SIDEWINDER", "INDEPENDENT_YARDS", "CIVILIAN", "Sidewinder", "Sidewinder",
   "Scout", "Starter multi-role", PadSize::small, 0, 25, 200, 260, 130, 6, 2.2, 100, 0, 8, 100, 6.2, 7.0,
   {{SlotSize::size1, "fixed"}, {SlotSize::size1, "fixed"}}, {SlotSize::size1},
   core(SlotSize::size1, SlotSize::size1, SlotSize::size1, SlotSize::size1, SlotSize::size1, SlotSize::size1, SlotSize::size1),
   {SlotSize::size1, SlotSize::size1, SlotSize::size1}, 4, 3, 1, true, false, false, false, 0, false,
   "sidewinder", "compact_wedge", "civilian_alloy", "twin_exposed", "single_canopy", "service_panels",
   "civilian_used", "independent_civilian", {"forgiving", "starter", "serviceable"}},
  {"TITAN_MULE", "TITAN_FORGE", "CIVILIAN", "Mule", "Titan Forge Mule",
   "Shuttle", "Utility Transport", PadSize::small, 2800, 45, 180, 260, 120, 4, 1.5, 120, 60, 14, 130, 9.2, 11.5,
   {}, {SlotSize::size1, SlotSize::size1},
   core(SlotSize::size2, SlotSize::size2, SlotSize::size2, SlotSize::size1, SlotSize::size1, SlotSize::size1, SlotSize::size2),
   {SlotSize::size3, SlotSize::size2, SlotSize::size1}, 4, 2, 1, true, true, true, false, 0, false,
   "titan_utility", "broad_blunt_shuttle", "titan_gunmetal", "replaceable_twin_pods", "armored_work_canopy",
   "external_access_panels", "titan_scraped_armor", "titan_civilian", {"rugged", "durable", "cargo", "industrial"}},
  {"COMPACT_MILITIA", "COMPACT_YARDS", "FREE_SYSTEMS_COMPACT", "Militia", "Compact Militia",
   "Fighter", "Multi-Role Fighter", PadSize::small, 4200, 50, 260, 380, 175, 7, 2.45, 150, 120, 2, 95, 6.8, 8.4,
   {{SlotSize::size1, "modular"}, {SlotSize::size1, "modular"},
    {SlotSize::size1, "modular"}, {SlotSize::size1, "modular"}}, {SlotSize::size1, SlotSize::size1},
   core(SlotSize::size3, SlotSize::size3, SlotSize::size3, SlotSize::size2, SlotSize::size3, SlotSize::size2, SlotSize::size2),
   {SlotSize::size3, SlotSize::size2, SlotSize::size1}, 5, 4, 2, false, true, false, false, 0, false,
   "compact_militia", "modular_fighter_blocks", "compact_owner_paint", "replaceable_engine_pods", "simple_armored_canopy",
   "rails_and_attachment_points", "compact_mismatched_modules", "compact_militia", {"affordable", "adaptable", "modular"}},
  {"ASTER_RAPTOR", "ASTER_DYNAMICS", "CIVILIAN", "Raptor", "Aster Dynamics Raptor",
   "Fighter", "Interceptor", PadSize::small, 8000, 30, 420, 620, 280, 10, 3.25, 80, 150, 0, 72, 6.2, 7.8,
   {{SlotSize::size1, "integrated"}, {SlotSize::size1, "integrated"},
    {SlotSize::size2, "integrated"}, {SlotSize::size2, "integrated"}},
   {SlotSize::size1, SlotSize::size1},
   core(SlotSize::size3, SlotSize::size3, SlotSize::size3, SlotSize::size1, SlotSize::size3, SlotSize::size2, SlotSize::size1),
   {SlotSize::size2, SlotSize::size1}, 6, 8, 3, false, false, false, false, 0, false,
   "aster_raptor", "needle_swept_interceptor", "aster_white_graphite", "integrated_high_ratio", "dark_narrow_canopy",
   "tight_recessed_panels", "aster_heat_discoloration", "aster_civilian", {"fast", "agile", "fragile", "low_signature"}},
  {"COMMONWEALTH_VALIANT", "TITAN_FORGE", "COMMONWEALTH_NAVY", "Valiant", "Commonwealth Valiant",
   "Corvette", "Patrol / Escort", PadSize::medium, 14000, 180, 220, 340, 150, 6, 1.75, 500, 400, 40, 150, 8.5, 10.2,
   {{SlotSize::size2, "external"}, {SlotSize::size2, "external"},
    {SlotSize::size1, "external"}, {SlotSize::size1, "external"}},
   {SlotSize::size2, SlotSize::size2, SlotSize::size1},
   core(SlotSize::size4, SlotSize::size4, SlotSize::size4, SlotSize::size3, SlotSize::size4, SlotSize::size3, SlotSize::size4),
   {SlotSize::size4, SlotSize::size4, SlotSize::size3, SlotSize::size3, SlotSize::size2, SlotSize::size2},
   6, 4, 3, false, true, true, false, 0, true,
   "titan_valiant", "balanced_armored_corvette", "commonwealth_naval_gray", "replaceable_quad_bank", "naval_command_canopy",
   "armor_and_fleet_marks", "titan_scraped_armor", "commonwealth_navy", {"balanced", "patrol", "naval", "durable"}},
  {"COMMONWEALTH_INTREPID", "TITAN_FORGE", "COMMONWEALTH_NAVY", "Intrepid", "Commonwealth Intrepid",
   "Frigate", "Multi-Role", PadSize::medium, 24000, 450, 200, 310, 135, 5, 1.45, 1200, 800, 120, 190, 10.5, 12.8,
   {{SlotSize::size3, "external"}, {SlotSize::size3, "external"},
    {SlotSize::size2, "external"}, {SlotSize::size2, "external"},
    {SlotSize::size1, "external"}, {SlotSize::size1, "external"}},
   {SlotSize::size2, SlotSize::size2, SlotSize::size2, SlotSize::size2},
   core(SlotSize::size4, SlotSize::size4, SlotSize::size4, SlotSize::size4, SlotSize::size4, SlotSize::size4, SlotSize::size4),
   {SlotSize::size4, SlotSize::size4, SlotSize::size4, SlotSize::size4, SlotSize::size4,
    SlotSize::size3, SlotSize::size3, SlotSize::size3, SlotSize::size2, SlotSize::size2},
   7, 4, 4, true, true, true, true, 0, true,
   "titan_intrepid", "practical_naval_frigate", "commonwealth_naval_gray", "serviceable_engine_bank", "raised_command_module",
   "compartments_and_service_structures", "titan_scraped_armor", "commonwealth_navy", {"multirole", "naval", "mission_flexible"}},
  {"COMPACT_RANGER", "COMPACT_YARDS", "CIVILIAN", "Ranger", "Compact Ranger",
   "Frigate", "Exploration", PadSize::medium, 17500, 420, 220, 340, 150, 5, 1.55, 900, 600, 150, 220, 25.5, 32.8,
   {{SlotSize::size2, "modular"}, {SlotSize::size2, "modular"}},
   {SlotSize::size1, SlotSize::size1, SlotSize::size1, SlotSize::size1},
   core(SlotSize::size4, SlotSize::size4, SlotSize::size4, SlotSize::size3, SlotSize::size4, SlotSize::size4, SlotSize::size4),
   {SlotSize::size4, SlotSize::size4, SlotSize::size4, SlotSize::size4, SlotSize::size4,
    SlotSize::size3, SlotSize::size3, SlotSize::size2, SlotSize::size2},
   9, 5, 4, true, true, true, true, 0, true,
   "compact_ranger", "rugged_module_spine", "compact_owner_paint", "replaceable_long_range_pods", "wide_expedition_canopy",
   "external_mission_modules", "compact_mismatched_modules", "compact_frontier", {"long_range", "rugged", "modular", "science"}},
  {"ASTER_CLIPPER", "ASTER_DYNAMICS", "CIVILIAN", "Clipper", "Aster Dynamics Clipper",
   "Freighter", "Fast Trade", PadSize::medium, 12500, 600, 300, 450, 200, 6, 1.9, 500, 600, 150, 125, 22.5, 28.8,
   {{SlotSize::size2, "integrated"}, {SlotSize::size2, "integrated"}},
   {SlotSize::size1, SlotSize::size1, SlotSize::size1, SlotSize::size1},
   core(SlotSize::size4, SlotSize::size4, SlotSize::size4, SlotSize::size3, SlotSize::size4, SlotSize::size4, SlotSize::size4),
   {SlotSize::size4, SlotSize::size4, SlotSize::size4, SlotSize::size4,
    SlotSize::size4, SlotSize::size4, SlotSize::size3, SlotSize::size3},
   6, 7, 2, false, false, false, false, 0, false,
   "aster_clipper", "long_tapered_transport", "aster_white_graphite", "integrated_commercial_twin", "dark_panorama_canopy",
   "flush_cargo_interfaces", "aster_heat_discoloration", "aster_commercial", {"fast", "commercial", "courier", "fragile"}}
};
}

const std::vector<VisualProfile>& manufacturerProfiles() { return kManufacturers; }
const std::vector<VisualProfile>& technologyProfiles() { return kTechnologies; }
const std::vector<OperatorProfile>& operatorProfiles() { return kOperators; }
const std::vector<VisualVariantProfile>& visualVariantProfiles() { return kVisualVariants; }
const std::vector<Definition>& registry() { return kRegistry; }

const VisualProfile* findManufacturer(std::string_view id) {
  for (const auto& profile : kManufacturers) if (profile.id == id) return &profile;
  return nullptr;
}
const VisualProfile* findTechnologyProfile(TechnologyFamily family) {
  for (const auto& profile : kTechnologies) if (profile.technology == family) return &profile;
  return nullptr;
}
const OperatorProfile* findOperator(std::string_view id) {
  for (const auto& profile : kOperators) if (profile.id == id) return &profile;
  return nullptr;
}
const VisualVariantProfile* findVisualVariant(std::string_view id) {
  for (const auto& profile : kVisualVariants) if (profile.id == id) return &profile;
  return nullptr;
}
const Definition* find(std::string_view id) {
  for (const auto& definition : kRegistry) if (definition.id == id) return &definition;
  return nullptr;
}
std::string_view defaultVisualVariant(const Definition& definition) {
  if (definition.operatorId == "COMMONWEALTH_NAVY") return "COMMONWEALTH_NAVY";
  if (definition.operatorId == "FREE_SYSTEMS_COMPACT") return "MILITIA";
  if (definition.operatorId == "SOLAR_DIRECTORATE") return "DIRECTORATE";
  if (definition.operatorId == "VANTA_CONVERSION") return "PIRATE_CONVERSION";
  return "CIVILIAN";
}
const Definition& sidewinder() {
  const auto* definition = find("SIDEWINDER");
  if (!definition) throw std::logic_error("SIDEWINDER missing from ship registry");
  return *definition;
}
const char* padSizeName(PadSize size) {
  switch (size) { case PadSize::small: return "SMALL"; case PadSize::medium: return "MEDIUM"; case PadSize::large: return "LARGE"; }
  return "UNKNOWN";
}
const char* slotSizeName(SlotSize size) {
  switch (size) { case SlotSize::size1: return "SIZE_1"; case SlotSize::size2: return "SIZE_2";
    case SlotSize::size3: return "SIZE_3"; case SlotSize::size4: return "SIZE_4"; }
  return "UNKNOWN";
}
bool supportsPad(PadSize maximum, PadSize required) {
  return static_cast<int>(maximum) >= static_cast<int>(required);
}
bool validateRegistry() {
  std::unordered_set<std::string_view> ids;
  std::unordered_set<std::string_view> variants;
  for (const auto& variant : kVisualVariants)
    if (variant.id.empty() || !variants.emplace(variant.id).second || !findOperator(variant.operatorId) ||
        variant.treatment.empty()) return false;
  for (const auto& definition : kRegistry) {
    if (definition.id.empty() || !ids.emplace(definition.id).second || !findManufacturer(definition.manufacturerId) ||
        !findOperator(definition.operatorId) || !findVisualVariant(defaultVisualVariant(definition)) ||
        definition.purchasePrice < 0 || definition.mass <= 0 ||
        definition.topSpeed <= 0 || definition.boostSpeed < definition.topSpeed || definition.acceleration <= 0 ||
        definition.maneuverability < 1 || definition.maneuverability > 10 || definition.turnRate <= 0 ||
        definition.baseHull <= 0 || definition.baseShields < 0 || definition.cargoCapacity < 0 ||
        definition.fuelCapacity <= 0 || definition.jumpRangeLaden <= 0 ||
        definition.jumpRangeUnladen < definition.jumpRangeLaden || definition.coreSlots.size() != 7 ||
        definition.hullFamily.empty() || definition.silhouetteFamily.empty() || definition.materialFamily.empty() ||
        definition.engineProfile.empty() || definition.cockpitProfile.empty() || definition.surfaceDetailProfile.empty() ||
        definition.wearProfile.empty() || definition.defaultLiveryFamily.empty() || definition.visualTags.empty()) return false;
  }
  return ids.count("SIDEWINDER") == 1;
}

} // namespace helion::ships
