#include "client/ui_input.h"
#include <SDL.h>
#include <cassert>

static SDL_Event mouse(Uint32 type, int x, int y) {
  SDL_Event event{};
  event.type = type;
  event.button.button = SDL_BUTTON_LEFT;
  event.button.x = x;
  event.button.y = y;
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
  view.ui.screen = UiScreen::station;
  UiInput input;

  auto press = mouse(SDL_MOUSEBUTTONDOWN, 30, 190);
  (void)press;
  assert(input.handle(view, press, 960, 600) == "FLIGHT");
  assert(view.ui.screen == UiScreen::market);
  assert(input.handle(view, press, 960, 600).empty());
  auto release = mouse(SDL_MOUSEBUTTONUP, 30, 190);
  assert(input.handle(view, release, 960, 600).empty());
  view.ui.commandPending = false;
  auto buy = mouse(SDL_MOUSEBUTTONDOWN, 700, 300);
  (void)buy;
  assert(input.handle(view, buy, 960, 600) == "BUY food 1");
  assert(input.handle(view, buy, 960, 600).empty());
  release = mouse(SDL_MOUSEBUTTONUP, 700, 300);
  input.handle(view, release, 960, 600);
  view.ui.commandPending = true;
  assert(input.handle(view, buy, 960, 600).empty());
  input.handle(view, release, 960, 600);

  view.ui.commandPending = false;
  view.ui.screen = UiScreen::account;
  SDL_Event field = mouse(SDL_MOUSEBUTTONDOWN, 30, 210);
  input.handle(view, field, 960, 600);
  SDL_Event text{};
  text.type = SDL_TEXTINPUT;
  std::snprintf(text.text.text, sizeof(text.text.text), "secret-pass");
  input.handle(view, text, 960, 600);
  assert(view.ui.passwordMask == "***********");
  assert(view.ui.passwordMask.find("secret") == std::string::npos);
}
