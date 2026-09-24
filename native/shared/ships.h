#pragma once

#include <string_view>
#include <vector>

namespace helion::ships {

enum class PadSize { small = 1, medium = 2, large = 3 };
enum class SlotSize { size1 = 1, size2 = 2, size3 = 3, size4 = 4 };
enum class TechnologyFamily { human_engineered, khepri_grown, vael_constructed };

struct Hardpoint {
  SlotSize size = SlotSize::size1;
  std::string_view mount;
};

struct VisualProfile {
  std::string_view id;
  std::string_view displayName;
  TechnologyFamily technology = TechnologyFamily::human_engineered;
  std::string_view silhouetteLanguage;
  std::string_view constructionLanguage;
  std::string_view materialLanguage;
  std::string_view agingLanguage;
};

struct OperatorProfile {
  std::string_view id;
  std::string_view displayName;
  std::string_view treatment;
};

struct VisualVariantProfile {
  std::string_view id;
  std::string_view displayName;
  std::string_view operatorId;
  std::string_view treatment;
};

struct Definition {
  std::string_view id;
  std::string_view manufacturerId;
  std::string_view operatorId;
  std::string_view model;
  std::string_view displayName;
  std::string_view shipClass;
  std::string_view role;
  PadSize padSize = PadSize::small;
  int purchasePrice = 0;
  double mass = 0;
  double topSpeed = 0;
  double boostSpeed = 0;
  double acceleration = 0;
  int maneuverability = 0;
  double turnRate = 0;
  int baseHull = 0;
  int baseShields = 0;
  int cargoCapacity = 0;
  double fuelCapacity = 0;
  double jumpRangeLaden = 0;
  double jumpRangeUnladen = 0;
  std::vector<Hardpoint> hardpoints;
  std::vector<SlotSize> utilityMounts;
  std::vector<SlotSize> coreSlots;
  std::vector<SlotSize> optionalSlots;
  int sensorRating = 0;
  int stealthRating = 0;
  int electronicWarfareRating = 0;
  bool miningCapability = false;
  bool salvageCapability = false;
  bool repairCapability = false;
  bool scienceCapability = false;
  int fighterBay = 0;
  bool srvCapability = false;
  std::string_view hullFamily;
  std::string_view silhouetteFamily;
  std::string_view materialFamily;
  std::string_view engineProfile;
  std::string_view cockpitProfile;
  std::string_view surfaceDetailProfile;
  std::string_view wearProfile;
  std::string_view defaultLiveryFamily;
  std::vector<std::string_view> visualTags;
};

const std::vector<VisualProfile>& manufacturerProfiles();
const std::vector<VisualProfile>& technologyProfiles();
const std::vector<OperatorProfile>& operatorProfiles();
const std::vector<VisualVariantProfile>& visualVariantProfiles();
const std::vector<Definition>& registry();
const VisualProfile* findManufacturer(std::string_view id);
const VisualProfile* findTechnologyProfile(TechnologyFamily family);
const OperatorProfile* findOperator(std::string_view id);
const VisualVariantProfile* findVisualVariant(std::string_view id);
const Definition* find(std::string_view id);
std::string_view defaultVisualVariant(const Definition& definition);
const Definition& sidewinder();
const char* padSizeName(PadSize size);
const char* slotSizeName(SlotSize size);
bool supportsPad(PadSize stationMaximum, PadSize required);
bool validateRegistry();

} // namespace helion::ships
