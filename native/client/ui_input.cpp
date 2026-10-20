#include "client/ui_input.h"
#include "shared/loadout.h"
#include "shared/protocol.h"
#include <algorithm>
#include <cctype>

namespace helion::client {
namespace {
constexpr int kVisibleShipyardRows = 6;

bool open(View& view, GameEngineState state) {
  const bool changed = view.engine.navigate(state);
  view.engine.project(view.ui);
  view.thrust = view.engine.input().thrust;
  return changed;
}
}
void UiInput::clearCredentials() { std::fill(password_.begin(), password_.end(), '\0'); password_.clear(); }
void UiInput::publishDraft(UiState& ui) const {
  ui.accountName = username_; ui.accountDisplay = display_;
  ui.passwordMask.assign(password_.size(), '*');
}
std::string UiInput::activate(View& view, UiHitResult hit) {
  auto& ui = view.ui;
  if (hit.id == UiControlId::none || !hit.enabled) return {};
  ui.focusedControl = hit.id;
  switch (hit.id) {
    case UiControlId::reconnect:
      reconnectRequested_ = view.engine.state() == GameEngineState::DISCONNECTED;
      return {};
    case UiControlId::accountName: case UiControlId::accountPassword: case UiControlId::accountDisplay:
      field_ = hit.id; return {};
    case UiControlId::accountLogin: case UiControlId::accountCreate: {
      if (username_.empty() || password_.empty()) { ui.statusMessage = "ENTER ACCOUNT NAME AND PASSWORD"; return {}; }
      const auto command = hit.id == UiControlId::accountLogin ? "LOGIN " + username_ + " " + password_ :
        "CREATE " + username_ + " " + password_ + " " + (display_.empty() ? username_ : display_);
      if (protocol::parseRequest(command).command == protocol::Command::invalid) {
        ui.statusMessage = "CHECK ACCOUNT FIELDS"; return {};
      }
      clearCredentials(); publishDraft(ui); return command;
    }
    case UiControlId::stationMarket: return open(view, GameEngineState::STATION_MARKET) ? "FLIGHT" : "";
    case UiControlId::stationMission: return open(view, GameEngineState::STATION_CAREER) ? "CAREER" : "";
    case UiControlId::stationOutfitting: return open(view, GameEngineState::STATION_OUTFITTING) ? "OUTFIT LIST" : "";
    case UiControlId::stationShipyard:
      if (!open(view, GameEngineState::STATION_SHIPYARD)) return {};
      ui.shipyardRows.clear(); return "SHIPYARD LIST";
    case UiControlId::stationProfile: return open(view, GameEngineState::PROFILE) ? "PROFILE" : "";
    case UiControlId::stationGalnet: return open(view, GameEngineState::GALNET) ? "GALNET" : "";
    case UiControlId::stationOptions: open(view, GameEngineState::OPTIONS); return {};
    case UiControlId::stationGraphics: open(view, GameEngineState::GRAPHICS); return {};
    case UiControlId::help: open(view, GameEngineState::HELP); return {};
    case UiControlId::dismiss:
      view.engine.back(); view.engine.project(ui); return {};
    case UiControlId::introDismiss:
      view.engine.back(); view.engine.project(ui); return "CAREER DISMISS";
    case UiControlId::marketFood: case UiControlId::marketParts: case UiControlId::outfitRow:
      ui.selected = hit.index; return {};
    case UiControlId::shipyardRow: ui.selected = hit.index; return {};
    case UiControlId::missionRow: ui.missionSelected = std::clamp(hit.index, 0, 2); return {};
    case UiControlId::galnetEntry: ui.selected = hit.index - ui.scrollOffset; return {};
    case UiControlId::marketQuantityDown: ui.quantity = std::max(1, ui.quantity - 1); return {};
    case UiControlId::marketQuantityUp: ui.quantity = std::min(std::max(1, ui.cargoCapacity), ui.quantity + 1); return {};
    case UiControlId::optionsTelemetry:
      view.showTelemetry = !view.showTelemetry; ui.telemetryEnabled = view.showTelemetry; return {};
    case UiControlId::target:
      for (const auto& contact : view.contacts) if (contact.hostile) { view.targetId = contact.id; break; }
      return {};
    case UiControlId::fire: return view.targetId.empty() ? std::string{} : "FIRE " + view.targetId;
    case UiControlId::quit: return "QUIT";
    default: return uiCommandFor(hit.id, ui.selected, ui);
  }
}
std::string UiInput::handle(View& view, const SDL_Event& event, int width, int height) {
  auto& ui = view.ui;
  populateUiDerived(ui, view.ship);
  view.engine.project(ui);
  if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) leftDown_ = false;
  if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) { leftDown_ = false; return {}; }
  if (event.type == SDL_MOUSEMOTION) {
    const auto hit = hitTestUi(ui, {float(event.motion.x), float(event.motion.y)}, width, height);
    ui.hoveredControl = hit.id; ui.hoveredIndex = hit.index; return {};
  }
  if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
    if (leftDown_) return {};
    leftDown_ = true;
    if (width <= 0 || height <= 0) return {};
    return activate(view, hitTestUi(ui, {float(event.button.x), float(event.button.y)}, width, height));
  }
  if (event.type == SDL_MOUSEWHEEL) {
    ui.scrollOffset = std::clamp(ui.scrollOffset - std::clamp(event.wheel.y, -32, 32), 0, uiMaxScroll(ui));
    if (ui.screen == UiScreen::shipyard && !ui.shipyardRows.empty()) {
      ui.selected = std::clamp(ui.selected, ui.scrollOffset,
        std::min(static_cast<int>(ui.shipyardRows.size()) - 1, ui.scrollOffset + kVisibleShipyardRows - 1));
    } else if (ui.screen == UiScreen::galnet) {
      ui.selected = std::clamp(ui.selected, 0,
        std::max(0, std::min(9, int(ui.galnet.size()) - ui.scrollOffset) - 1));
    }
    return {};
  }
  if (event.type == SDL_TEXTINPUT && view.engine.state() == GameEngineState::LOGIN) {
    auto& text = field_ == UiControlId::accountPassword ? password_ : field_ == UiControlId::accountDisplay ? display_ : username_;
    const std::size_t limit = field_ == UiControlId::accountPassword ? 128 : field_ == UiControlId::accountDisplay ? 64 : 32;
    for (const unsigned char c : std::string(event.text.text)) {
      if (text.size() >= limit) break;
      if (c >= 33 && c <= 126 && (field_ == UiControlId::accountPassword || std::isalnum(c) || c == '-' || c == '_')) text += char(c);
      else if (c == ' ' && field_ == UiControlId::accountDisplay) text += ' ';
    }
    publishDraft(ui); return {};
  }
  if (event.type != SDL_KEYDOWN || event.key.repeat) return {};
  const auto key = event.key.keysym.sym;
  if (view.engine.state() == GameEngineState::DISCONNECTED) {
    if (key == SDLK_RETURN || key == SDLK_r) reconnectRequested_ = true;
    return {};
  }
  const auto invoke = [&](UiControlId id) {
    for (const auto& control : buildUiControls(ui)) if (control.id == id)
      return activate(view, {id, control.index, control.enabled});
    return std::string{};
  };
  if (view.engine.state() == GameEngineState::LOGIN) {
    if (key == SDLK_TAB) { field_ = field_ == UiControlId::accountName ? UiControlId::accountPassword :
      field_ == UiControlId::accountPassword ? UiControlId::accountDisplay : UiControlId::accountName; ui.focusedControl = field_; }
    if (key == SDLK_BACKSPACE) {
      auto& text = field_ == UiControlId::accountPassword ? password_ : field_ == UiControlId::accountDisplay ? display_ : username_;
      if (!text.empty()) text.pop_back();
      publishDraft(ui);
    }
    if (key == SDLK_RETURN) return invoke(event.key.keysym.mod & KMOD_SHIFT ? UiControlId::accountCreate : UiControlId::accountLogin);
    return {};
  }
  if (!view.engine.authenticated()) return {};
  if (key == SDLK_ESCAPE) return activate(view, {UiControlId::dismiss, 0, true});
  if (key == SDLK_F1) { open(view, GameEngineState::STATION_SERVICES); return {}; }
  const UiControlId shortcuts[] = {UiControlId::stationProfile, UiControlId::stationOptions, UiControlId::stationMarket,
    UiControlId::stationMission, UiControlId::stationOutfitting, UiControlId::stationGalnet, UiControlId::stationGraphics, UiControlId::help};
  if (key >= SDLK_F2 && key <= SDLK_F9) return activate(view, {shortcuts[key - SDLK_F2], 0, true});
  if (key == SDLK_PAGEUP || key == SDLK_PAGEDOWN) {
    ui.scrollOffset = std::clamp(ui.scrollOffset + (key == SDLK_PAGEDOWN ? 1 : -1), 0, uiMaxScroll(ui));
    if (ui.screen == UiScreen::shipyard && !ui.shipyardRows.empty())
      ui.selected = std::clamp(ui.selected, ui.scrollOffset,
        std::min(static_cast<int>(ui.shipyardRows.size()) - 1, ui.scrollOffset + kVisibleShipyardRows - 1));
    return {};
  }
  if (key == SDLK_UP || key == SDLK_DOWN) {
    if (ui.screen == UiScreen::mission) ui.missionSelected = std::clamp(ui.missionSelected + (key == SDLK_DOWN ? 1 : -1), 0, 2);
    else {
      const int maximum = ui.screen == UiScreen::station ? 10 : ui.screen == UiScreen::market ? 1 :
        ui.screen == UiScreen::outfitting ? int(loadout::kCatalogue.size()) - 1 :
        ui.screen == UiScreen::shipyard ? std::max(0, int(ui.shipyardRows.size()) - 1) :
        ui.screen == UiScreen::galnet ? std::max(0, std::min(9, int(ui.galnet.size()) - ui.scrollOffset) - 1) : 0;
      ui.selected = std::clamp(ui.selected + (key == SDLK_DOWN ? 1 : -1), 0, maximum);
      if (ui.screen == UiScreen::shipyard) {
        if (ui.selected < ui.scrollOffset) ui.scrollOffset = ui.selected;
        else if (ui.selected >= ui.scrollOffset + kVisibleShipyardRows)
          ui.scrollOffset = ui.selected - kVisibleShipyardRows + 1;
      }
    }
    ui.focusedControl = UiControlId::none;
  }
  if (key == SDLK_RETURN) {
    if (ui.screen == UiScreen::station) {
      const auto controls = buildUiControls(ui); return activate(view, {controls[ui.selected].id, ui.selected, controls[ui.selected].enabled});
    }
    if (ui.screen == UiScreen::market) return invoke(UiControlId::marketBuy);
    if (ui.screen == UiScreen::mission) {
      const int stage = ui.missionSelected == 0 ? ui.missionStage : ui.missionSelected == 1 ? ui.career.supply : ui.career.response;
      return invoke(stage == 0 ? UiControlId::missionAccept : UiControlId::missionTurnIn);
    }
    if (ui.screen == UiScreen::outfitting) {
      const auto id = loadout::kCatalogue[std::clamp(ui.selected, 0, int(loadout::kCatalogue.size()) - 1)].id;
      return invoke(std::find(ui.ownedModules.begin(), ui.ownedModules.end(), id) == ui.ownedModules.end() ? UiControlId::outfitBuy : UiControlId::outfitFit);
    }
    if (ui.screen == UiScreen::shipyard) {
      if (ui.selected < 0 || ui.selected >= static_cast<int>(ui.shipyardRows.size())) return {};
      return invoke(ui.shipyardRows[static_cast<std::size_t>(ui.selected)].owned ?
        UiControlId::shipyardSwitch : UiControlId::shipyardBuy);
    }
    return invoke(ui.screen == UiScreen::help ? UiControlId::introDismiss : UiControlId::dismiss);
  }
  if (ui.screen == UiScreen::market) {
    if (key == SDLK_b) return invoke(UiControlId::marketBuy);
    if (key == SDLK_s) return invoke(UiControlId::marketSell);
    if (key == SDLK_EQUALS || key == SDLK_KP_PLUS) return invoke(UiControlId::marketQuantityUp);
    if (key == SDLK_MINUS || key == SDLK_KP_MINUS) return invoke(UiControlId::marketQuantityDown);
  }
  if (ui.screen == UiScreen::outfitting) {
    if (key == SDLK_b) return invoke(UiControlId::outfitBuy);
    if (key == SDLK_f) return invoke(UiControlId::outfitFit);
    if (key == SDLK_r) return invoke(UiControlId::outfitRemove);
    if (key == SDLK_1) return invoke(UiControlId::upgradeEngine);
    if (key == SDLK_2) return invoke(UiControlId::upgradeHull);
  }
  if (ui.screen == UiScreen::shipyard) {
    if (key == SDLK_b) return invoke(UiControlId::shipyardBuy);
    if (key == SDLK_f) return invoke(UiControlId::shipyardSwitch);
  }
  if (ui.screen == UiScreen::flight) {
    if (key == SDLK_e) return invoke(UiControlId::mine);
    if (key == SDLK_f) return invoke(UiControlId::dock);
    if (key == SDLK_r) return invoke(UiControlId::recover);
    if (key == SDLK_TAB) return invoke(UiControlId::target);
    if (key == SDLK_SPACE) return invoke(UiControlId::fire);
    if (key == SDLK_l) return invoke(UiControlId::stationLaunch);
  }
  return {};
}
}
