#include "shared/flight.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace helion::flight {
int hullCapacity(int hullLevel) { return 100 + (std::clamp(hullLevel, 1, 5) - 1) * 25; }
int cargoUsed(const State& s) { return s.cargo+s.food+s.parts; }
int nearestStation(const State& s) {
  return std::hypot(s.x-650,s.y)<std::hypot(s.x,s.y) ? 1 : 0;
}
double speed(const State& s) { return std::hypot(s.vx, s.vy); }

void step(State& s, double dt, int engineLevel) {
  if (!std::isfinite(dt) || dt <= 0) return;
  dt = std::min(dt, 0.05);
  s.cooldown = std::max(0.0, s.cooldown - dt);
  s.inputAge += dt;
  if (s.docked) return;
  // Missing input (disconnect, console, lost focus) engages flight assist.
  const Input input = s.inputAge <= 0.5 ? s.input : Input{0, 0, 1};
  s.yaw = std::remainder(s.yaw + input.turn * 2.2 * dt, 2 * kPi);
  const double engineMultiplier = 1.0 + 0.12 * (std::clamp(engineLevel, 1, 5) - 1);
  const double acceleration = input.brake ? 0 : input.thrust * 130.0 * engineMultiplier;
  s.vx -= std::sin(s.yaw) * acceleration * dt;
  s.vy += std::cos(s.yaw) * acceleration * dt;
  const double drag = std::exp(-(input.brake ? 4.5 : 0.65) * dt);
  s.vx *= drag;
  s.vy *= drag;
  s.x += s.vx * dt;
  s.y += s.vy * dt;
  // Soft collision hulls keep the ship outside the faceted asteroid meshes.
  for (const auto& rock : kRocks) {
    const double dx = s.x - rock.x, dy = s.y - rock.y;
    const double distance = std::hypot(dx, dy), radius = rock.radius + 12;
    if (distance < radius) {
      const double nx = distance > 0.001 ? dx / distance : 1;
      const double ny = distance > 0.001 ? dy / distance : 0;
      s.x = rock.x + nx * radius;
      s.y = rock.y + ny * radius;
      const double inward = s.vx * nx + s.vy * ny;
      if (inward < 0) {
        const int damage = std::max(1, static_cast<int>(std::ceil((-inward - 18.0) * 0.18)));
        s.hull = std::max(0, s.hull - damage);
        s.vx -= 1.3 * inward * nx; s.vy -= 1.3 * inward * ny;
        if (s.hull == 0) {
          const int station = std::clamp(s.station, 0, static_cast<int>(kStations.size()) - 1);
          s.x = kStations[station].x; s.y = kStations[station].y;
          s.vx = 0; s.vy = 0; s.docked = true; s.cargo = 0;
        }
      }
    }
  }
  if (std::hypot(s.x, s.y) > 1200) {
    const double scale = 1200 / std::hypot(s.x, s.y);
    s.x *= scale; s.y *= scale; s.vx = 0; s.vy = 0;
  }
}

int nearestRock(const State& s) {
  int result = 0;
  double best = std::numeric_limits<double>::max();
  for (std::size_t i = 0; i < kRocks.size(); ++i) {
    const double distance = std::hypot(s.x - kRocks[i].x, s.y - kRocks[i].y);
    if (distance < best) { best = distance; result = static_cast<int>(i); }
  }
  return result;
}

std::string launch(State& s) {
  if (!s.docked) return "ERR already-in-flight";
  if (s.hull <= 0) return "ERR repair-required";
  const int food=s.food, parts=s.parts, station=s.station, hull=s.hull, maxHull=s.maxHull;
  s = State{};
  s.food=food; s.parts=parts; s.station=station; s.hull=hull; s.maxHull=maxHull;
  s.docked = false;
  s.x=kStations[station].x; s.y=kStations[station].y+100;
  return "OK LAUNCHED";
}

std::string mine(State& s) {
  if (s.docked) return "ERR launch-required";
  if (cargoUsed(s) >= kCargoCapacity) return "ERR cargo-full";
  if (speed(s) > kWorkSpeed) return "ERR slow-down";
  const Rock& rock = kRocks[nearestRock(s)];
  if (std::hypot(s.x - rock.x, s.y - rock.y) > kMineRange) return "ERR asteroid-out-of-range";
  if (s.cooldown > 0) return "ERR mining-cooldown";
  ++s.cargo;
  s.cooldown = 1.25;
  return "OK MINED cargo=" + std::to_string(s.cargo);
}

std::string dock(State& s, int& credits, int& experience) {
  if (s.docked) return "ERR already-docked";
  const int station=nearestStation(s);
  if (std::hypot(s.x-kStations[station].x, s.y-kStations[station].y) > kDockRange) return "ERR station-out-of-range";
  if (speed(s) > kWorkSpeed) return "ERR slow-down";
  const int earned = s.cargo * kStations[station].oreSell;
  if (credits > std::numeric_limits<int>::max() - earned ||
      experience > std::numeric_limits<int>::max() - s.cargo * 5) return "ERR profile-limit";
  credits += earned;
  experience += s.cargo * 5;
  const int food=s.food, parts=s.parts, hull=s.hull, maxHull=s.maxHull;
  s = State{};
  s.food=food; s.parts=parts; s.station=station; s.hull=hull; s.maxHull=maxHull;
  s.x=kStations[station].x; s.y=kStations[station].y;
  return "OK DOCKED earned=" + std::to_string(earned);
}

std::string repair(State& s, int& credits, int hullLevel) {
  if (!s.docked) return "ERR dock-required";
  s.maxHull = hullCapacity(hullLevel);
  if (s.hull >= s.maxHull) return "ERR hull-full";
  const int missing = s.maxHull - s.hull;
  const int cost = missing * 3;
  if (credits < cost) return "ERR insufficient-credits";
  credits -= cost;
  s.hull = s.maxHull;
  return "OK REPAIRED hull=" + std::to_string(s.hull) + " cost=" + std::to_string(cost);
}

std::string trade(State& s, int& credits, bool buying, const std::string& commodity, int quantity) {
  if (!s.docked) return "ERR dock-required";
  if (quantity<1 || quantity>kCargoCapacity) return "ERR invalid-quantity";
  if (commodity!="food" && commodity!="parts") return "ERR unknown-commodity";
  int& inventory=commodity=="food" ? s.food : s.parts;
  const auto& market=kStations[s.station];
  const int price=commodity=="food" ? (buying?market.foodBuy:market.foodSell) : (buying?market.partsBuy:market.partsSell);
  const int total=price*quantity;
  if (buying) {
    if (cargoUsed(s)+quantity>kCargoCapacity) return "ERR cargo-full";
    if (credits<total) return "ERR insufficient-credits";
    inventory+=quantity; credits-=total;
  } else {
    if (inventory<quantity) return "ERR insufficient-cargo";
    if (credits>std::numeric_limits<int>::max()-total) return "ERR profile-limit";
    inventory-=quantity; credits+=total;
  }
  return std::string("OK ")+(buying?"BOUGHT ":"SOLD ")+commodity+" quantity="+std::to_string(quantity)+" total="+std::to_string(total);
}

std::string contactLine(const Contact& c) {
  std::ostringstream out;
  out<<std::fixed<<std::setprecision(3)<<"CONTACT "<<c.id<<' '<<c.kind<<' '<<c.x<<' '<<c.y<<' '<<c.yaw<<' '<<c.docked;
  return out.str();
}
bool readContact(const std::string& line, Contact& c) {
  std::istringstream in(line); std::string tag,extra; Contact next; int docked;
  if (!(in>>tag>>next.id>>next.kind>>next.x>>next.y>>next.yaw>>docked) || tag!="CONTACT" || (in>>extra)) return false;
  if (next.id.empty() || next.id.size()>32 || (next.kind!="pilot" && next.kind!="hauler") ||
      !std::isfinite(next.x) || !std::isfinite(next.y) || !std::isfinite(next.yaw) ||
      std::abs(next.x)>1201 || std::abs(next.y)>1201 || std::abs(next.yaw)>kPi+0.001 || docked<0 || docked>1) return false;
  next.docked=docked!=0; c=next; return true;
}

std::string snapshot(const State& s, int credits, int experience) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(3) << "FLIGHT " << s.x << ' ' << s.y << ' '
      << s.vx << ' ' << s.vy << ' ' << s.yaw << ' ' << s.docked << ' ' << s.cargo
      << ' ' << credits << ' ' << experience << ' ' << s.cooldown
      << ' ' << s.food << ' ' << s.parts << ' ' << s.station << ' ' << s.hull << ' ' << s.maxHull;
  return out.str();
}

bool readSnapshot(const std::string& line, State& s, int& credits, int& experience) {
  std::istringstream in(line);
  std::string kind, extra;
  State next;
  int docked, nextCredits, nextExperience;
  if (!(in >> kind >> next.x >> next.y >> next.vx >> next.vy >> next.yaw >> docked >> next.cargo
        >> nextCredits >> nextExperience >> next.cooldown >> next.food >> next.parts >> next.station) || kind != "FLIGHT") return false;
  if (in >> next.hull >> next.maxHull) {
    if (in >> extra) return false;
  } else {
    in.clear();
  }
  if (!std::isfinite(next.x) || !std::isfinite(next.y) || !std::isfinite(next.vx) ||
      !std::isfinite(next.vy) || !std::isfinite(next.yaw) || !std::isfinite(next.cooldown) ||
      std::abs(next.x) > 1201 || std::abs(next.y) > 1201 || std::abs(next.vx) > 250 ||
      std::abs(next.vy) > 250 || std::abs(next.yaw) > kPi + 0.001 ||
      docked < 0 || docked > 1 || next.cargo < 0 || next.cargo > kCargoCapacity || next.food<0 || next.parts<0 ||
      next.food>8 || next.parts>8 || cargoUsed(next)>kCargoCapacity || next.station<0 || next.station>1 ||
      next.cooldown < 0 || next.cooldown > 1.251) return false;
  if (next.hull < 0 || next.maxHull < 1 || next.maxHull > 200 || next.hull > next.maxHull) return false;
  next.docked = docked != 0;
  s = next; credits = nextCredits; experience = nextExperience;
  return true;
}
} // namespace helion::flight
