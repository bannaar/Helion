#include "shared/combat.h"

#include <algorithm>
#include <cmath>

namespace helion::combat {

void resetHostile(Hostile& hostile, int generation) {
  hostile = Hostile{};
  hostile.generation = std::max(1, generation);
  hostile.id = "RAIDER-" + std::to_string(hostile.generation);
  // The generation changes the patrol lane without introducing random state.
  hostile.x = static_cast<double>((hostile.generation % 3) - 1) * 120.0;
  hostile.y = 360.0 + static_cast<double>(hostile.generation % 2) * 60.0;
}

double distance(const Hostile& hostile, const flight::State& player) {
  return std::hypot(hostile.x - player.x, hostile.y - player.y);
}

bool canNpcFire(const Hostile& hostile, const flight::State& player) {
  return hostile.hostile && !hostile.destroyed && !player.docked && !player.destroyed &&
    hostile.fireCooldown <= 0 && distance(hostile, player) <= kNpcWeaponRange;
}

void step(Hostile& hostile, const flight::State& player, double dt) {
  if (!std::isfinite(dt) || dt <= 0) return;
  dt = std::min(dt, 0.05);
  hostile.fireCooldown = std::max(0.0, hostile.fireCooldown - dt);
  if (hostile.destroyed || player.docked || player.destroyed) return;

  const double range = distance(hostile, player);
  if (range > kNpcDetectionRange || range < 0.001) return;
  const double dx = (player.x - hostile.x) / range;
  const double dy = (player.y - hostile.y) / range;
  constexpr double kApproachSpeed = 24.0;
  hostile.vx = dx * kApproachSpeed;
  hostile.vy = dy * kApproachSpeed;
  hostile.x += hostile.vx * dt;
  hostile.y += hostile.vy * dt;
  const double radius = std::hypot(hostile.x, hostile.y);
  if (radius > 1000.0) {
    hostile.x *= 1000.0 / radius;
    hostile.y *= 1000.0 / radius;
  }
}

} // namespace helion::combat
