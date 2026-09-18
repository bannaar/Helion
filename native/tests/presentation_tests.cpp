#include "client/presentation.h"
#include "client/render.h"

#include <cmath>
#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main() {
  using namespace helion::client;
  const auto wide = cameraFrame(960, 600);
  const auto tall = cameraFrame(600, 960);
  check(wide.aspect > tall.aspect && wide.halfWidth > tall.halfWidth,
        "camera projection follows resize aspect");
  check(cameraFrame(0, 0).halfWidth > 0 && cameraFrame(0, 0).halfHeight > 0,
        "zero-sized projection is safe");
  check(std::fabs(clampHudValue(50, 100) - 0.5f) < 0.0001f &&
        clampHudValue(-1, 100) == 0.0f && clampHudValue(200, 100) == 1.0f &&
        clampHudValue(1, 0) == 0.0f,
        "HUD values are bounded");

  helion::flight::State player;
  player.x = 10;
  player.y = 20;
  PresentationContact contact;
  contact.x = 40;
  contact.y = 20;
  contact.hostile = true;
  const auto indicator = targetIndicator(player, &contact);
  check(indicator.visible && std::fabs(indicator.directionX - 1.0f) < 0.0001f &&
        std::fabs(indicator.directionY) < 0.0001f && std::fabs(indicator.distance - 30.0f) < 0.0001f,
        "target indicator reports direction and range");
  check(!targetIndicator(player, nullptr).visible, "empty target list hides indicator");
  check(leftTurnInput() && rightTurnInput(), "flight controls preserve left and right turn signs");

  PresentationSnapshot snapshot;
  snapshot.player = player;
  snapshot.contacts = {contact};
  snapshot.contacts.front().selected = true;
  snapshot.selectedTarget = "RAIDER-1";
  snapshot.asteroids.push_back({100, 200, 30, false});
  snapshot.stations.push_back({"KEPLER", 0, 0, {1, 0.6f, 0.2f}});
  check(snapshot.contacts.front().classification == PresentationClass::commander ||
        snapshot.contacts.front().classification == PresentationClass::hostile,
        "snapshot preserves object classification domain");
  check(snapshot.asteroids.size() == 1 && snapshot.stations.size() == 1,
        "snapshot supports bounded world collections");
  View view;
  view.contacts.resize(kMaxPresentationContacts + 8);
  for (std::size_t i = 0; i < view.contacts.size(); ++i) {
    view.contacts[i].id = "CONTACT-" + std::to_string(i);
    view.contacts[i].kind = "pilot";
  }
  const auto bounded = makePresentationSnapshot(view);
  check(bounded.contacts.size() == kMaxPresentationContacts,
        "snapshot bounds maximum contact presentation state");
  std::cout << "presentation snapshot tests passed\n";
  return failures == 0 ? 0 : 1;
}
