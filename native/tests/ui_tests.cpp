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
  std::cout << "UI model tests passed\n";
  return failures == 0 ? 0 : 1;
}
