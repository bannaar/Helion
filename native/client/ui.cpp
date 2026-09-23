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
    case UiScreen::help: return "KEPLER / PILOT GUIDE";
    case UiScreen::completion: return "KEPLER / ESTABLISHED PILOT";
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
    case UiControlId::help: return "help";
    case UiControlId::introDismiss: return "intro-dismiss";
    case UiControlId::accountName: return "account-name";
    case UiControlId::accountPassword: return "account-password";
    case UiControlId::accountDisplay: return "account-display";
    case UiControlId::accountLogin: return "account-login";
    case UiControlId::accountCreate: return "account-create";
    case UiControlId::missionRow: return "mission-row";
    case UiControlId::mine: return "mine";
    case UiControlId::dock: return "dock";
    case UiControlId::target: return "target";
    case UiControlId::fire: return "fire";
    case UiControlId::recover: return "recover";
    case UiControlId::upgradeEngine: return "upgrade-engine";
    case UiControlId::upgradeHull: return "upgrade-hull";
    case UiControlId::quit: return "quit";
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
    case UiControlId::help: return "HELP / F9";
    case UiControlId::introDismiss: return "CONTINUE / SKIP INTRO";
    case UiControlId::accountLogin: return "LOG IN";
    case UiControlId::accountCreate: return "CREATE COMMANDER";
    case UiControlId::mine: return "MINE / E";
    case UiControlId::dock: return "DOCK / F";
    case UiControlId::target: return "TARGET / TAB";
    case UiControlId::fire: return "FIRE / SPACE";
    case UiControlId::recover: return "RECOVER / R";
    case UiControlId::upgradeEngine: return "ENGINE +";
    case UiControlId::upgradeHull: return "HULL +";
    case UiControlId::quit: return "EXIT / SAVED";
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
      addControl(controls, UiControlId::accountName, 24, 146, 890, 36, 0, true);
      addControl(controls, UiControlId::accountPassword, 24, 196, 890, 36, 1, true);
      addControl(controls, UiControlId::accountDisplay, 24, 246, 890, 36, 2, true);
      addControl(controls, UiControlId::accountLogin, 24, 330, 260, 38, 3, state.connected && !state.commandPending);
      addControl(controls, UiControlId::accountCreate, 310, 330, 300, 38, 4, state.connected && !state.commandPending);
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
      for (int i = 0; i < 3; ++i)
        addControl(controls, UiControlId::missionRow, 20 + i * 310, 116, 300, 36, i, state.authenticated);
      {
        const int stage = state.missionSelected == 0 ? state.missionStage :
          state.missionSelected == 1 ? state.career.supply : state.career.response;
        const bool unlocked = state.missionSelected == 0 || (state.missionStage == 2 &&
          (state.missionSelected == 1 || state.career.supply == 2));
        addControl(controls, UiControlId::missionAccept, 20, 400, 300, 34, 0, active && unlocked && stage == 0);
        addControl(controls, UiControlId::missionTurnIn, 330, 400, 300, 34, 0, active && stage == 1 && state.missionSelected != 2);
      }
      break;
    case UiScreen::outfitting: {
      const std::size_t maxRows = std::min<std::size_t>(7, loadout::kCatalogue.size());
      for (std::size_t i = 0; i < maxRows; ++i)
        addControl(controls, UiControlId::outfitRow, 20, 146 + static_cast<float>(i) * 35,
          920, 34, static_cast<int>(i), state.authenticated);
      addControl(controls, UiControlId::outfitBuy, 20, 442, 180, 34, 0, active);
      addControl(controls, UiControlId::outfitFit, 215, 442, 180, 34, 0, active);
      addControl(controls, UiControlId::outfitRemove, 410, 442, 220, 34, 0, active);
      addControl(controls, UiControlId::upgradeEngine, 650, 442, 130, 34, 0, active);
      addControl(controls, UiControlId::upgradeHull, 800, 442, 130, 34, 0, active);
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
      if (state.authenticated) {
        addControl(controls, UiControlId::mine, 365, 346, 125, 26, 0, !state.docked && !state.destroyed && !state.commandPending);
        addControl(controls, UiControlId::dock, 505, 346, 125, 26, 0, !state.docked && !state.destroyed && !state.commandPending);
        addControl(controls, UiControlId::target, 645, 346, 125, 26, 0, !state.destroyed);
        addControl(controls, UiControlId::fire, 785, 346, 145, 26, 0, !state.docked && !state.destroyed && !state.commandPending);
        if (state.destroyed) addControl(controls, UiControlId::recover, 365, 544, 230, 30, 0, !state.commandPending);
        if (state.docked) addControl(controls, UiControlId::stationLaunch, 365, 544, 230, 30, 0, active);
      }
      break;
    case UiScreen::help:
      addControl(controls, UiControlId::introDismiss, 24, 470, 340, 34, 0, state.authenticated && !state.commandPending);
      break;
    case UiScreen::completion:
      addControl(controls, UiControlId::dismiss, 24, 440, 420, 34, 0, state.authenticated);
      break;
  }
  if (state.screen != UiScreen::account && state.screen != UiScreen::help && state.screen != UiScreen::flight)
    addControl(controls, UiControlId::help, 640, 480, 130, 28, -1, true);
  if (state.screen != UiScreen::account && state.screen != UiScreen::flight && state.screen != UiScreen::completion)
    addControl(controls, UiControlId::dismiss, 780, 480, 150, 28, -1, true);
  if (state.screen == UiScreen::options)
    addControl(controls, UiControlId::quit, 24, 400, 230, 34, 0, true);
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
    case UiControlId::missionAccept: return state.missionSelected == 0 ? "ACCEPT" :
      "CAREER ACCEPT " + std::string(state.missionSelected == 1 ? career::kSupply : career::kResponse);
    case UiControlId::missionTurnIn: return state.missionSelected == 0 ? "TURNIN" :
      "CAREER TURNIN " + std::string(career::kSupply);
    case UiControlId::introDismiss: return "CAREER DISMISS";
    case UiControlId::mine: return "MINE";
    case UiControlId::dock: return "DOCK";
    case UiControlId::recover: return "RECOVER";
    case UiControlId::upgradeEngine: return "UPGRADE engine";
    case UiControlId::upgradeHull: return "UPGRADE hull";
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
