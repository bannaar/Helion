#include "client/ui.h"

#include "shared/loadout.h"
#include "shared/organizations.h"

#include <algorithm>
#include <cctype>

namespace helion::client {
std::string uiScreenName(UiScreen screen) {
  switch (screen) {
    case UiScreen::account: return "ACCOUNT / CONNECTION";
    case UiScreen::flight: return "FLIGHT / KEPLER REACH";
    case UiScreen::station: return "STATION / SERVICES";
    case UiScreen::market: return "MARKET / COMMODITIES";
    case UiScreen::mission: return "MISSION / CONTRACTS";
    case UiScreen::outfitting: return "OUTFITTING / LOADOUT";
    case UiScreen::profile: return "PROFILE / PROGRESSION";
    case UiScreen::galnet: return "GALNET / NEWS";
    case UiScreen::options: return "OPTIONS / CONTROLS";
    case UiScreen::graphics: return "GRAPHICS / DIAGNOSTICS";
    case UiScreen::error: return "STATUS / CONFIRMATION";
  }
  return "HELION";
}

std::string uiControlName(UiControlId control) {
  switch (control) {
    case UiControlId::none: return "none";
    case UiControlId::accountInput: return "account-input";
    case UiControlId::stationMarket: return "station-market";
    case UiControlId::stationMission: return "station-mission";
    case UiControlId::stationOutfitting: return "station-outfitting";
    case UiControlId::stationProfile: return "station-profile";
    case UiControlId::stationGalnet: return "station-galnet";
    case UiControlId::stationOptions: return "station-options";
    case UiControlId::stationGraphics: return "station-graphics";
    case UiControlId::stationLaunch: return "station-launch";
    case UiControlId::stationRepair: return "station-repair";
    case UiControlId::stationRefuel: return "station-refuel";
    case UiControlId::marketFood: return "market-food";
    case UiControlId::marketParts: return "market-parts";
    case UiControlId::marketBuy: return "market-buy";
    case UiControlId::marketSell: return "market-sell";
    case UiControlId::marketQuantityDown: return "market-quantity-down";
    case UiControlId::marketQuantityUp: return "market-quantity-up";
    case UiControlId::missionAccept: return "mission-accept";
    case UiControlId::missionTurnIn: return "mission-turn-in";
    case UiControlId::outfitRow: return "outfit-row";
    case UiControlId::outfitBuy: return "outfit-buy";
    case UiControlId::outfitFit: return "outfit-fit";
    case UiControlId::outfitRemove: return "outfit-remove";
    case UiControlId::galnetEntry: return "galnet-entry";
    case UiControlId::optionsTelemetry: return "options-telemetry";
    case UiControlId::dismiss: return "dismiss";
  }
  return "none";
}

std::string uiStandingLabel(int value) {
  return helion::organizations::standingLabel(value);
}

std::string uiButtonLabel(UiControlId control) {
  switch (control) {
    case UiControlId::marketBuy: return "BUY";
    case UiControlId::marketSell: return "SELL";
    case UiControlId::marketQuantityDown: return "-";
    case UiControlId::marketQuantityUp: return "+";
    case UiControlId::missionAccept: return "ACCEPT";
    case UiControlId::missionTurnIn: return "TURN IN";
    case UiControlId::outfitBuy: return "BUY";
    case UiControlId::outfitFit: return "FIT";
    case UiControlId::outfitRemove: return "REMOVE";
    case UiControlId::dismiss: return "BACK / ENTER OR ESC";
    default: return {};
  }
}

std::string maskedCommand(std::string_view command) {
  std::string result(command);
  std::string normalized(command);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
    [](unsigned char character) { return static_cast<char>(std::toupper(character)); });
  const auto start = normalized.find_first_not_of(" /\t");
  if (start == std::string::npos) return result;
  const auto end = normalized.find_first_of(" \t", start);
  const auto verb = normalized.substr(start, end - start);
  if (verb != "LOGIN" && verb != "CREATE") return result;
  const auto user = normalized.find_first_not_of(" \t", end);
  if (user == std::string::npos) return result;
  const auto userEnd = normalized.find_first_of(" \t", user);
  const auto password = normalized.find_first_not_of(" \t", userEnd);
  if (password == std::string::npos) return result;
  const auto passwordEnd = normalized.find_first_of(" \t", password);
  result.replace(password, passwordEnd == std::string::npos ? result.size() - password : passwordEnd - password, "********");
  return result;
}

namespace {
void addControl(std::vector<UiControl>& controls, UiControlId id, float x, float y,
                float width, float height, int index, bool enabled) {
  controls.push_back({id, {x, y, width, height}, index, enabled});
}

bool actionEnabled(const UiState& state) {
  return state.connected && state.authenticated && state.docked && !state.destroyed &&
    !state.commandPending;
}
}

int uiMaxScroll(const UiState& state) {
  if (state.screen == UiScreen::galnet) {
    constexpr int visible = 9;
    return std::max(0, static_cast<int>(state.galnet.size()) - visible);
  }
  return 0;
}

std::vector<UiControl> buildUiControls(const UiState& state) {
  std::vector<UiControl> controls;
  const bool active = actionEnabled(state);
  switch (state.screen) {
    case UiScreen::account:
      addControl(controls, UiControlId::accountInput, 20, 210, 920, 54, 0,
        state.connected && !state.commandPending);
      break;
    case UiScreen::station: {
      const std::array<UiControlId, 10> ids = {
        UiControlId::stationMarket, UiControlId::stationMission,
        UiControlId::stationOutfitting, UiControlId::stationProfile,
        UiControlId::stationGalnet, UiControlId::stationOptions,
        UiControlId::stationGraphics, UiControlId::stationLaunch,
        UiControlId::stationRepair, UiControlId::stationRefuel};
      for (std::size_t i = 0; i < ids.size(); ++i)
        addControl(controls, ids[i], 20, 178 + static_cast<float>(i) * 27, 920, 28,
          static_cast<int>(i), i < 7 ? state.authenticated : active);
      break;
    }
    case UiScreen::market:
      addControl(controls, UiControlId::marketFood, 20, 192, 920, 32, 0, state.authenticated);
      addControl(controls, UiControlId::marketParts, 20, 228, 920, 32, 1, state.authenticated);
      addControl(controls, UiControlId::marketQuantityDown, 520, 286, 42, 32, 0, active);
      addControl(controls, UiControlId::marketQuantityUp, 568, 286, 42, 32, 0, active);
      addControl(controls, UiControlId::marketBuy, 650, 286, 130, 32, 0, active);
      addControl(controls, UiControlId::marketSell, 800, 286, 130, 32, 0, active);
      break;
    case UiScreen::mission:
      addControl(controls, UiControlId::missionAccept, 20, 302, 450, 34, 0,
        active && state.missionStage == 0);
      addControl(controls, UiControlId::missionTurnIn, 480, 302, 450, 34, 0,
        active && state.missionStage == 1 && state.missionOreMined);
      break;
    case UiScreen::outfitting: {
      const std::size_t maxRows = std::min<std::size_t>(7, loadout::kCatalogue.size());
      for (std::size_t i = 0; i < maxRows; ++i)
        addControl(controls, UiControlId::outfitRow, 20, 146 + static_cast<float>(i) * 35,
          920, 34, static_cast<int>(i), state.authenticated);
      addControl(controls, UiControlId::outfitBuy, 20, 442, 180, 34, 0, active);
      addControl(controls, UiControlId::outfitFit, 215, 442, 180, 34, 0, active);
      addControl(controls, UiControlId::outfitRemove, 410, 442, 220, 34, 0, active);
      break;
    }
    case UiScreen::galnet: {
      constexpr int visible = 9;
      const int first = std::clamp(state.scrollOffset, 0, uiMaxScroll(state));
      const int last = std::min(static_cast<int>(state.galnet.size()), first + visible);
      for (int i = first; i < last; ++i)
        addControl(controls, UiControlId::galnetEntry, 20,
          154 + static_cast<float>(i - first) * 34, 920, 33, i, state.authenticated);
      break;
    }
    case UiScreen::options:
      addControl(controls, UiControlId::optionsTelemetry, 20, 144, 920, 34, 0, state.authenticated);
      break;
    case UiScreen::error:
      addControl(controls, UiControlId::dismiss, 20, 442, 920, 34, 0, state.authenticated);
      break;
    case UiScreen::profile:
    case UiScreen::graphics:
      addControl(controls, UiControlId::dismiss, 20, 442, 920, 34, 0, state.authenticated);
      break;
    case UiScreen::flight:
      break;
  }
  return controls;
}

UiPoint uiPointFromWindow(float windowX, float windowY, int width, int height) {
  const float safeWidth = static_cast<float>(std::max(width, 1));
  const float safeHeight = static_cast<float>(std::max(height, 1));
  const float scale = std::max(0.25f, std::min(safeWidth / 960.0f, safeHeight / 600.0f));
  return {(windowX - (safeWidth - 960.0f * scale) * 0.5f) / scale,
          (windowY - (safeHeight - 600.0f * scale) * 0.5f) / scale};
}

UiHitResult hitTestUi(const UiState& state, UiPoint point, int width, int height) {
  const auto logical = uiPointFromWindow(point.x, point.y, width, height);
  const auto controls = buildUiControls(state);
  for (auto it = controls.rbegin(); it != controls.rend(); ++it) {
    if (it->rect.contains(logical)) return {it->id, it->index, it->enabled};
  }
  return {};
}

std::string uiCommandFor(UiControlId control, int index, const UiState& state) {
  switch (control) {
    case UiControlId::stationLaunch: return "LAUNCH";
    case UiControlId::stationRepair: return "REPAIR";
    case UiControlId::stationRefuel: return "REFUEL";
    case UiControlId::marketBuy:
      return "BUY " + std::string(state.selected == 0 ? "food" : "parts") + " " + std::to_string(state.quantity);
    case UiControlId::marketSell:
      return "SELL " + std::string(state.selected == 0 ? "food" : "parts") + " " + std::to_string(state.quantity);
    case UiControlId::missionAccept: return "ACCEPT";
    case UiControlId::missionTurnIn: return "TURNIN";
    case UiControlId::outfitBuy:
    case UiControlId::outfitFit:
      if (index >= 0 && index < static_cast<int>(loadout::kCatalogue.size()))
        return std::string(control == UiControlId::outfitBuy ? "OUTFIT BUY " : "OUTFIT FIT ") +
          std::string(loadout::kCatalogue[static_cast<std::size_t>(index)].id);
      return {};
    case UiControlId::outfitRemove:
      if (index >= 0 && index < static_cast<int>(loadout::kCatalogue.size()))
        return "OUTFIT REMOVE " + std::string(loadout::slotName(loadout::kCatalogue[static_cast<std::size_t>(index)].slot));
      return {};
    default: return {};
  }
}

void populateUiDerived(UiState& state, const flight::State& ship) {
  state.quantity = std::clamp(state.quantity, 1, flight::kCargoCapacity);
  state.scrollOffset = std::clamp(state.scrollOffset, 0, uiMaxScroll(state));
  state.docked = ship.docked;
  state.destroyed = ship.destroyed;
  if (state.statusMessage.size() > 220) state.statusMessage.resize(220);
  if (state.graphicsReport.size() > 220) state.graphicsReport.resize(220);
  if (state.ownedModules.size() > helion::loadout::kCatalogue.size())
    state.ownedModules.resize(helion::loadout::kCatalogue.size());
  if (state.galnet.size() > 32) state.galnet.erase(state.galnet.begin(), state.galnet.end() - 32);
  for (auto& entry : state.galnet) {
    if (entry.id.size() > 64) entry.id.resize(64);
    if (entry.headline.size() > 180) entry.headline.resize(180);
  }
}

} // namespace helion::client
