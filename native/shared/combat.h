#pragma once

#include "shared/flight.h"

#include <string>

namespace helion::combat {

inline constexpr double kNpcDetectionRange = 520.0;
inline constexpr double kNpcWeaponRange = 185.0;
inline constexpr double kNpcFireCooldown = 2.0;
inline constexpr int kNpcDamage = 8;

struct Hostile {
  std::string id = "RAIDER-1";
  std::string faction = "raiders";
  int generation = 1;
  double x = 0, y = 420, vx = 0, vy = 0;
  int hull = 100, maxHull = 100;
  double fireCooldown = 0;
  bool hostile = true;
  bool destroyed = false;
};

void resetHostile(Hostile& hostile, int generation);
double distance(const Hostile& hostile, const flight::State& player);
bool canNpcFire(const Hostile& hostile, const flight::State& player);
void step(Hostile& hostile, const flight::State& player, double dt);

} // namespace helion::combat
