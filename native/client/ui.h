#pragma once

#include "shared/flight.h"

#include <array>
#include <cstddef>
#include <string>
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
  graphics
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
std::string uiStandingLabel(int value);
std::string maskedCommand(std::string_view command);
void populateUiDerived(UiState& state, const flight::State& ship);

} // namespace helion::client
