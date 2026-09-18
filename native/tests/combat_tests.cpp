#include "shared/combat.h"

#include <cmath>
#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main() {
  using helion::combat::Hostile;
  helion::flight::State player;
  check(helion::flight::launch(player) == "OK LAUNCHED", "test pilot launches");
  player.x = 0;
  player.y = 250;
  Hostile hostile;
  helion::combat::resetHostile(hostile, 7);
  check(hostile.faction == "criminal.red_wake", "hostile uses canonical Red Wake affiliation");
  const double before = helion::combat::distance(hostile, player);
  for (int tick = 0; tick < 60; ++tick) helion::combat::step(hostile, player, 1.0 / 60);
  check(helion::combat::distance(hostile, player) < before, "hostile pursues inside detection range");
  hostile.x = player.x;
  hostile.y = player.y + helion::combat::kNpcWeaponRange;
  hostile.fireCooldown = 0;
  check(helion::combat::canNpcFire(hostile, player), "hostile can fire in range");
  player.docked = true;
  check(!helion::combat::canNpcFire(hostile, player), "docked player is not attackable");
  player.docked = false;
  player.destroyed = true;
  check(!helion::combat::canNpcFire(hostile, player), "destroyed player is not attackable");
  check(hostile.id == "RAIDER-7", "hostile id is deterministic by generation");
  return failures == 0 ? 0 : 1;
}
