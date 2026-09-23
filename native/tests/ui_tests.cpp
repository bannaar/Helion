#include "client/ui.h"

#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main() {
  using namespace helion::client;
  check(uiScreenName(UiScreen::market) == "MARKET / COMMODITIES", "screen names are stable");
  check(maskedCommand("/login pilot secret-password") == "/login pilot ********",
        "login passwords are masked");
  check(maskedCommand("/create pilot secret-password Pilot One") == "/create pilot ******** Pilot One",
        "create passwords are masked");
  check(maskedCommand("/profile") == "/profile", "non-auth commands remain readable");
  UiState state;
  state.quantity = 99;
  state.statusMessage.assign(300, 'X');
  state.galnet.resize(40);
  for (auto& entry : state.galnet) {
    entry.id.assign(100, 'I');
    entry.headline.assign(300, 'H');
  }
  helion::flight::State ship;
  populateUiDerived(state, ship);
  check(state.quantity == helion::flight::kCargoCapacity && state.statusMessage.size() == 220,
        "UI values are bounded");
  check(state.galnet.size() == 32 && state.galnet.front().id.size() == 64 &&
        state.galnet.front().headline.size() == 180, "GalNet retention and strings are bounded");
  state.connected = true;
  state.authenticated = true;
  state.docked = true;
  state.screen = UiScreen::station;
  const auto stationControls = buildUiControls(state);
  check(!stationControls.empty() && stationControls.front().id == UiControlId::stationMarket,
        "station controls have stable layout-derived hit regions");
  const auto stationHit = hitTestUi(state, {30, 190}, 960, 600);
  check(stationHit.id == UiControlId::stationMarket && stationHit.enabled,
        "station row hit testing selects the rendered control");
  state.screen = UiScreen::market;
  state.quantity = 1;
  const auto buyHit = hitTestUi(state, {700, 300}, 960, 600);
  check(buyHit.id == UiControlId::marketBuy && uiCommandFor(buyHit.id, buyHit.index, state) == "BUY food 1",
        "market hit maps to the existing validated buy command");
  state.commandPending = true;
  check(!hitTestUi(state, {700, 300}, 960, 600).enabled,
        "pending transactions disable the hit region");
  state.commandPending = false;
  state.screen = UiScreen::galnet;
  state.galnet.resize(20);
  state.scrollOffset = 100;
  populateUiDerived(state, ship);
  check(state.scrollOffset == 11 && uiMaxScroll(state) == 11,
        "GalNet scrolling clamps to the bounded list");
  check(uiPointFromWindow(480, 300, 1280, 720).x > 0 &&
        uiPointFromWindow(480, 300, 1280, 720).y > 0,
        "mouse coordinates convert through the letterboxed layout");
  state.screen = UiScreen::profile;
  const auto profileBack = hitTestUi(state, {30, 455}, 960, 600);
  check(profileBack.id == UiControlId::dismiss && profileBack.enabled,
        "profile screen exposes a layout-aligned mouse back control");
  state.screen = UiScreen::graphics;
  const auto graphicsBack = hitTestUi(state, {30, 455}, 960, 600);
  check(graphicsBack.id == UiControlId::dismiss && graphicsBack.enabled,
        "graphics screen exposes a layout-aligned mouse back control");
  state.screen = UiScreen::mission;
  state.missionSelected = 1;
  state.missionStage = 2;
  state.career.supply = 0;
  const auto supplyControls = buildUiControls(state);
  check(uiCommandFor(UiControlId::missionAccept, 0, state) == "CAREER ACCEPT career.kepler_supply",
        "supply contract maps to the stable career identifier");
  state.career.supply = 1;
  check(uiCommandFor(UiControlId::missionTurnIn, 0, state) == "CAREER TURNIN career.kepler_supply",
        "supply delivery maps to the stable career identifier");
  check(!supplyControls.empty(), "career screen has bounded controls");
  std::cout << "UI model tests passed\n";
  return failures == 0 ? 0 : 1;
}
