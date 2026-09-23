#pragma once

#include "shared/flight.h"
#include "shared/career.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace helion::client {

enum class UiScreen {
  account,
  flight,
  station,
  market,
  mission,
  outfitting,
  profile,
  galnet,
  options,
  graphics,
  error, help, completion
};

enum class UiControlId {
  none,
  accountInput,
  stationMarket,
  stationMission,
  stationOutfitting,
  stationProfile,
  stationGalnet,
  stationOptions,
  stationGraphics,
  stationLaunch,
  stationRepair,
  stationRefuel,
  marketFood,
  marketParts,
  marketBuy,
  marketSell,
  marketQuantityDown,
  marketQuantityUp,
  missionAccept,
  missionTurnIn,
  outfitRow,
  outfitBuy,
  outfitFit,
  outfitRemove,
  galnetEntry,
  optionsTelemetry,
  dismiss, help, introDismiss, accountName, accountPassword, accountDisplay,
  accountLogin, accountCreate, missionRow, mine, dock, target, fire, recover,
  upgradeEngine, upgradeHull, quit
};

struct UiPoint {
  float x = 0;
  float y = 0;
};

struct UiRect {
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;

  bool contains(UiPoint point) const {
    return point.x >= x && point.y >= y && point.x <= x + width && point.y <= y + height;
  }
};

struct UiControl {
  UiControlId id = UiControlId::none;
  UiRect rect;
  int index = 0;
  bool enabled = false;
};

struct UiHitResult {
  UiControlId id = UiControlId::none;
  int index = -1;
  bool enabled = false;
};

struct UiMarketRow {
  std::string id;
  int buyPrice = 0;
  int sellPrice = 0;
  int owned = 0;
};

struct UiModuleRow {
  std::string id;
  std::string name;
  std::string slot;
  std::string effect;
  int price = 0;
  bool owned = false;
  bool fitted = false;
};

struct UiGalnetEntry {
  std::string id;
  std::string headline;
};

struct UiState {
  UiScreen screen = UiScreen::account;
  int selected = 0;
  int quantity = 1;
  int scrollOffset = 0;
  int hoveredIndex = -1;
  UiControlId hoveredControl = UiControlId::none;
  UiControlId focusedControl = UiControlId::none;
  bool connected = false;
  bool authenticated = false;
  bool docked = false;
  bool destroyed = false;
  bool commandPending = false;
  int missionSelected = 0;
  helion::career::State career;
  bool careerKnown = false;
  std::string accountName, accountDisplay, passwordMask;
  bool consoleOpen = true;
  bool telemetryEnabled = true;
  std::string typed;
  std::string connectionStatus = "CONNECTING / TLS";
  std::string statusMessage;
  std::string graphicsReport;
  int engineLevel = 1;
  int hullLevel = 1;
  int salvage = 0;
  int missionStage = 0;
  bool missionOreMined = false;
  std::string missionTitle = "First Ore";
  std::string missionIssuerId = "corp.orion";
  std::string missionIssuer = "Orion Extraction Group";
  std::string missionJurisdictionId = "authority.kepler";
  std::string missionJurisdiction = "Kepler Authority";
  int missionReward = 250;
  std::vector<std::string> ownedModules;
  std::array<std::string, 4> fittedModules{};
  std::vector<UiGalnetEntry> galnet;
};

std::string uiScreenName(UiScreen screen);
std::string uiControlName(UiControlId control);
std::string uiButtonLabel(UiControlId control);
std::string uiStandingLabel(int value);
std::string maskedCommand(std::string_view command);
std::vector<UiControl> buildUiControls(const UiState& state);
UiPoint uiPointFromWindow(float windowX, float windowY, int width, int height);
UiHitResult hitTestUi(const UiState& state, UiPoint point, int width, int height);
int uiMaxScroll(const UiState& state);
std::string uiCommandFor(UiControlId control, int index, const UiState& state);
void populateUiDerived(UiState& state, const flight::State& ship);

} // namespace helion::client
