#include "client/ui_input.h"
#include <SDL.h>
#include <stdexcept>

static void check(bool condition) {
  if (!condition) throw std::runtime_error("UI input check failed");
}

static SDL_Event mouse(Uint32 type, int x, int y) {
  SDL_Event event{};
  event.type = type;
  event.button.button = SDL_BUTTON_LEFT;
  event.button.x = x;
  event.button.y = y;
  return event;
}

static SDL_Event key(SDL_Keycode value) {
  SDL_Event event{};
  event.type = SDL_KEYDOWN;
  event.key.keysym.sym = value;
  return event;
}

int main() {
  using namespace helion::client;
  View view;
  view.connected = true;
  view.authenticated = true;
  view.ship.docked = true;
  view.ui.connected = true;
  view.ui.authenticated = true;
  view.ui.docked = true;
  view.engine.beginConnection();
  view.engine.tlsVerified();
  view.engine.protocolReady();
  view.engine.authenticationRequested();
  view.engine.authenticationAccepted();
  view.engine.observeFlight(view.ship);
  view.engine.project(view.ui);
  UiInput input;

  auto press = mouse(SDL_MOUSEBUTTONDOWN, 30, 190);
  (void)press;
  check(input.handle(view, press, 960, 600) == "FLIGHT");
  check(view.ui.screen == UiScreen::market);
  check(input.handle(view, press, 960, 600).empty());
  auto release = mouse(SDL_MOUSEBUTTONUP, 30, 190);
  check(input.handle(view, release, 960, 600).empty());
  view.ui.commandPending = false;
  auto buy = mouse(SDL_MOUSEBUTTONDOWN, 700, 300);
  (void)buy;
  check(input.handle(view, buy, 960, 600) == "BUY food 1");
  check(input.handle(view, buy, 960, 600).empty());
  release = mouse(SDL_MOUSEBUTTONUP, 700, 300);
  input.handle(view, release, 960, 600);
  view.ui.commandPending = true;
  check(input.handle(view, buy, 960, 600).empty());
  input.handle(view, release, 960, 600);

  view.ui.commandPending = false;
  view.engine.navigate(GameEngineState::STATION_SHIPYARD);
  view.engine.project(view.ui);
  view.ui.selected = 0;
  view.ui.scrollOffset = 0;
  view.ui.shipyardRows.clear();
  for (int index = 0; index < 8; ++index) {
    UiShipyardRow row;
    row.hullId = "HULL_" + std::to_string(index);
    row.displayName = row.hullId;
    view.ui.shipyardRows.push_back(std::move(row));
  }
  for (int index = 0; index < 6; ++index) input.handle(view, key(SDLK_DOWN), 960, 600);
  check(view.ui.selected == 6 && view.ui.scrollOffset == 1);
  check(input.handle(view, key(SDLK_RETURN), 960, 600) == "SHIPYARD BUY HULL_6");

  view.ui.cargoCapacity = 20;
  view.ui.quantity = 8;
  view.engine.navigate(GameEngineState::STATION_MARKET);
  view.engine.project(view.ui);
  auto quantityUp = mouse(SDL_MOUSEBUTTONDOWN, 580, 300);
  input.handle(view, quantityUp, 960, 600);
  input.handle(view, mouse(SDL_MOUSEBUTTONUP, 580, 300), 960, 600);
  check(view.ui.quantity == 9);

  view.ui.commandPending = false;
  view.engine.disconnected("test reconnect");
  view.engine.beginConnection();
  view.engine.tlsVerified();
  view.engine.protocolReady();
  view.engine.project(view.ui);
  SDL_Event field = mouse(SDL_MOUSEBUTTONDOWN, 30, 210);
  input.handle(view, field, 960, 600);
  SDL_Event text{};
  text.type = SDL_TEXTINPUT;
  std::snprintf(text.text.text, sizeof(text.text.text), "secret-pass");
  input.handle(view, text, 960, 600);
  check(view.ui.passwordMask == "***********");
  check(view.ui.passwordMask.find("secret") == std::string::npos);

  // Real hotkeys cannot fabricate station/flight truth, and back preserves a modal's caller.
  input.clearCredentials();
  view.engine.authenticationRequested(); view.engine.authenticationAccepted();
  view.ship.docked = false;
  view.engine.observeFlight(view.ship);
  view.engine.project(view.ui);
  view.engine.input() = {true, true, true, true, true};
  input.handle(view, key(SDLK_F3), 960, 600);
  check(view.engine.state() == GameEngineState::OPTIONS);
  check(!view.engine.input().heldForward && !view.engine.input().heldLeft && !view.engine.input().thrust);
  input.handle(view, key(SDLK_ESCAPE), 960, 600);
  check(view.engine.state() == GameEngineState::SPACE_FLIGHT);
  for (auto shortcut : {SDLK_F1, SDLK_F4, SDLK_F5, SDLK_F6}) {
    check(input.handle(view, key(shortcut), 960, 600).empty());
    check(view.engine.state() == GameEngineState::SPACE_FLIGHT);
  }
  view.ship.docked = true;
  view.engine.observeFlight(view.ship); view.engine.project(view.ui);
  input.handle(view, key(SDLK_F4), 960, 600);
  view.engine.actionError("ERR rejected");
  input.handle(view, key(SDLK_ESCAPE), 960, 600);
  check(view.engine.state() == GameEngineState::STATION_MARKET);
  view.engine.disconnected("lost");
  input.handle(view, key(SDLK_r), 960, 600);
  check(input.consumeReconnectRequest());
  check(!input.consumeReconnectRequest());
  check(view.engine.state() == GameEngineState::DISCONNECTED);
}
