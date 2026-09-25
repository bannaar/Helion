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

void step(State& s, double dt, int engineLevel, double fuelConsumptionMultiplier) {
  step(s, dt, ships::sidewinder(), engineLevel, fuelConsumptionMultiplier);
}

void step(State& s, double dt, const ships::Definition& hull, int engineLevel,
          double fuelConsumptionMultiplier) {
  if (!std::isfinite(dt) || dt <= 0) return;
  dt = std::min(dt, 0.05);
  s.cooldown = std::max(0.0, s.cooldown - dt);
  s.weaponCooldown = std::max(0.0, s.weaponCooldown - dt);
  s.inputAge += dt;
  if (s.docked || s.destroyed) return;
  // Missing input (disconnect, console, lost focus) engages flight assist.
  Input input = s.inputAge <= 0.5 ? s.input : Input{0, 0, 1};
  const double fuelMultiplier = std::clamp(fuelConsumptionMultiplier, 0.25, 2.0);
  const double fuelBurn = input.thrust ? dt * kFuelBurnPerSecond * fuelMultiplier : 0;
  if (fuelBurn > 0) {
    if (s.fuel <= 0 || s.fuel < fuelBurn) {
      s.fuel = 0;
      input.thrust = 0;
      input.brake = 1;
    } else {
      s.fuel -= fuelBurn;
    }
  }
  s.yaw = std::remainder(s.yaw + input.turn * hull.turnRate * dt, 2 * kPi);
  const double engineMultiplier = 1.0 + 0.12 * (std::clamp(engineLevel, 1, 5) - 1);
  const double acceleration = input.brake ? 0 : input.thrust * hull.acceleration * engineMultiplier;
  s.vx -= std::sin(s.yaw) * acceleration * dt;
  s.vy += std::cos(s.yaw) * acceleration * dt;
  const double drag = std::exp(-(input.brake ? 4.5 : 0.65) * dt);
  s.vx *= drag;
  s.vy *= drag;
  const double currentSpeed = speed(s);
  const double maximumSpeed = hull.topSpeed * engineMultiplier;
  if (currentSpeed > maximumSpeed) {
    const double scale = maximumSpeed / currentSpeed;
    s.vx *= scale;
    s.vy *= scale;
  }
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
          s.vx = 0; s.vy = 0; s.destroyed = true; s.cargo = 0;
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
  if (s.destroyed) return "ERR recovery-required";
  if (s.hull <= 0) return "ERR repair-required";
  if (!std::isfinite(s.fuel) || !std::isfinite(s.maxFuel) || s.fuel < 0 || s.maxFuel < 1 || s.fuel > s.maxFuel)
    return "ERR invalid-fuel-state";
  if (s.fuel <= 0) return "ERR insufficient-fuel";
  const int food=s.food, parts=s.parts, station=s.station, hull=s.hull, maxHull=s.maxHull;
  const double fuel=s.fuel, maxFuel=s.maxFuel;
  s = State{};
  s.food=food; s.parts=parts; s.station=station; s.hull=hull; s.maxHull=maxHull;
  s.fuel=fuel; s.maxFuel=maxFuel;
  s.destroyed = false;
  s.weaponCooldown = 0;
  s.docked = false;
  s.x=kStations[station].x; s.y=kStations[station].y+100;
  return "OK LAUNCHED";
}

std::string recover(State& s) {
  if (!s.destroyed) return "ERR recovery-not-required";
  const int station = std::clamp(s.station, 0, static_cast<int>(kStations.size()) - 1);
  s.x = kStations[station].x;
  s.y = kStations[station].y;
  s.vx = 0;
  s.vy = 0;
  s.docked = true;
  s.destroyed = false;
  s.hull = std::max(1, s.maxHull / 2);
  s.fuel = s.maxFuel;
  s.cargo = 0;
  s.cooldown = 0;
  s.weaponCooldown = 0;
  s.input = {};
  s.inputAge = 1;
  return "OK RECOVERED station=" + std::to_string(station) +
    " hull=" + std::to_string(s.hull) + " fuel=" + std::to_string(s.fuel) + " cargo-lost=1";
}

std::string mine(State& s, bool miningEnabled, double cooldownMultiplier) {
  return mine(s, kCargoCapacity, miningEnabled, cooldownMultiplier);
}

std::string mine(State& s, int cargoCapacity, bool miningEnabled, double cooldownMultiplier) {
  if (s.docked) return "ERR launch-required";
  if (s.destroyed) return "ERR recovery-required";
  if (!miningEnabled) return "ERR mining-module-required";
  if (!std::isfinite(cooldownMultiplier) || cooldownMultiplier <= 0) return "ERR invalid-mining-module";
  if (cargoCapacity < 0 || cargoUsed(s) >= cargoCapacity) return "ERR cargo-full";
  if (speed(s) > kWorkSpeed) return "ERR slow-down";
  const Rock& rock = kRocks[nearestRock(s)];
  if (std::hypot(s.x - rock.x, s.y - rock.y) > kMineRange) return "ERR asteroid-out-of-range";
  if (s.cooldown > 0) return "ERR mining-cooldown";
  ++s.cargo;
  s.cooldown = 1.25 * std::clamp(cooldownMultiplier, 0.5, 2.0);
  return "OK MINED cargo=" + std::to_string(s.cargo);
}

std::string dock(State& s, int& credits, int& experience, DockTransaction* transaction) {
  const int station = nearestStation(s);
  return dockAtPrice(s, credits, experience, kStations[station].oreSell, transaction);
}

std::string dockAtPrice(State& s, int& credits, int& experience, int oreUnitPrice,
                        DockTransaction* transaction) {
  if (s.destroyed) return "ERR recovery-required";
  if (s.docked) return "ERR already-docked";
  const int station=nearestStation(s);
  if (std::hypot(s.x-kStations[station].x, s.y-kStations[station].y) > kDockRange) return "ERR station-out-of-range";
  if (speed(s) > kWorkSpeed) return "ERR slow-down";
  const int cargoSold = s.cargo;
  if (oreUnitPrice < 0 || oreUnitPrice > 100000) return "ERR invalid-market-price";
  const int unitPrice = oreUnitPrice;
  const int earned = cargoSold * unitPrice;
  const int experienceEarned = cargoSold * 5;
  if (credits > std::numeric_limits<int>::max() - earned ||
      experience > std::numeric_limits<int>::max() - experienceEarned) return "ERR profile-limit";
  if (transaction) *transaction = {station, cargoSold, unitPrice, earned, experienceEarned};
  credits += earned;
  experience += experienceEarned;
  const int food=s.food, parts=s.parts, hull=s.hull, maxHull=s.maxHull;
  const double fuel=s.fuel, maxFuel=s.maxFuel;
  s = State{};
  s.food=food; s.parts=parts; s.station=station; s.hull=hull; s.maxHull=maxHull;
  s.fuel=fuel; s.maxFuel=maxFuel;
  s.x=kStations[station].x; s.y=kStations[station].y;
  return "OK DOCKED earned=" + std::to_string(earned);
}

int fuelCapacity(int engineLevel) {
  return 100 + (std::clamp(engineLevel, 1, 5) - 1) * 20;
}

std::string refuel(State& s, int& credits, FuelTransaction* transaction) {
  if (!s.docked) return "ERR dock-required";
  if (!std::isfinite(s.fuel) || !std::isfinite(s.maxFuel) || s.fuel < 0 || s.maxFuel < 1 || s.fuel > s.maxFuel)
    return "ERR invalid-fuel-state";
  if (s.fuel >= s.maxFuel - 0.000001) return "ERR fuel-full";
  const int station = std::clamp(s.station, 0, static_cast<int>(kStations.size()) - 1);
  const double amount = s.maxFuel - s.fuel;
  const int units = static_cast<int>(std::ceil(amount));
  const int price = kStations[station].fuelPrice;
  if (units <= 0 || units > std::numeric_limits<int>::max() / price) return "ERR profile-limit";
  const int cost = units * price;
  if (credits < cost) return "ERR insufficient-credits";
  if (transaction) *transaction = {station, amount, price, cost, s.maxFuel, s.maxFuel};
  credits -= cost;
  s.fuel = s.maxFuel;
  return "OK REFUELED amount=" + std::to_string(units) + " cost=" + std::to_string(cost);
}

std::string fuelTransactionLine(const FuelTransaction& transaction) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(2)
      << "TRANSACTION REFUEL station=" << transaction.station
      << " amount=" << transaction.fuelAdded
      << " unit-price=" << transaction.unitPrice
      << " credits=" << transaction.creditsSpent
      << " fuel=" << transaction.fuelAfter
      << " max-fuel=" << transaction.maxFuel;
  return out.str();
}

bool readFuelTransaction(const std::string& line, FuelTransaction& transaction) {
  std::istringstream in(line);
  std::string tag, kind, station, amount, unitPrice, credits, fuel, maxFuel, extra;
  FuelTransaction next;
  if (!(in >> tag >> kind >> station >> amount >> unitPrice >> credits >> fuel >> maxFuel) ||
      (in >> extra) || tag != "TRANSACTION" || kind != "REFUEL") return false;
  const auto parseIntField = [](const std::string& field, const char* name, int& value) {
    const std::string prefix = std::string(name) + "=";
    if (field.rfind(prefix, 0) != 0 || field.size() == prefix.size()) return false;
    std::size_t used = 0;
    try { value = std::stoi(field.substr(prefix.size()), &used); }
    catch (...) { return false; }
    return used == field.size() - prefix.size();
  };
  const auto parseDoubleField = [](const std::string& field, const char* name, double& value) {
    const std::string prefix = std::string(name) + "=";
    if (field.rfind(prefix, 0) != 0 || field.size() == prefix.size()) return false;
    std::size_t used = 0;
    try { value = std::stod(field.substr(prefix.size()), &used); }
    catch (...) { return false; }
    return used == field.size() - prefix.size() && std::isfinite(value);
  };
  if (!parseIntField(station, "station", next.station) ||
      !parseDoubleField(amount, "amount", next.fuelAdded) ||
      !parseIntField(unitPrice, "unit-price", next.unitPrice) ||
      !parseIntField(credits, "credits", next.creditsSpent) ||
      !parseDoubleField(fuel, "fuel", next.fuelAfter) ||
      !parseDoubleField(maxFuel, "max-fuel", next.maxFuel)) return false;
  if (next.station < 0 || next.station >= static_cast<int>(kStations.size()) || next.fuelAdded <= 0 ||
      next.fuelAdded > 1000 || next.unitPrice <= 0 || next.unitPrice > 1000 || next.creditsSpent <= 0 ||
      next.fuelAfter < 0 || next.maxFuel < 1 || next.fuelAfter > next.maxFuel) return false;
  transaction = next;
  return true;
}

std::string fuelLine(const State& s) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(2) << "FUEL " << s.fuel << ' ' << s.maxFuel;
  return out.str();
}

bool readFuelLine(const std::string& line, State& s) {
  std::istringstream in(line);
  std::string kind, extra;
  double fuel = 0, maxFuel = 0;
  if (!(in >> kind >> fuel >> maxFuel) || (in >> extra) || kind != "FUEL" ||
      !std::isfinite(fuel) || !std::isfinite(maxFuel) || fuel < 0 || maxFuel < 1 || fuel > maxFuel ||
      maxFuel > 1000) return false;
  s.fuel = fuel;
  s.maxFuel = maxFuel;
  return true;
}

std::string combatStatusLine(const State& s) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(2)
      << "COMBAT STATUS destroyed=" << (s.destroyed ? 1 : 0)
      << " weapon-cooldown=" << s.weaponCooldown;
  return out.str();
}

bool readCombatStatus(const std::string& line, State& s) {
  std::istringstream in(line);
  std::string tag, kind, destroyed, cooldown, extra;
  int destroyedValue = 0;
  double cooldownValue = 0;
  if (!(in >> tag >> kind >> destroyed >> cooldown) || (in >> extra) ||
      tag != "COMBAT" || kind != "STATUS") return false;
  const auto parseInt = [](const std::string& field, const char* name, int& value) {
    const std::string prefix = std::string(name) + "=";
    if (field.rfind(prefix, 0) != 0) return false;
    std::size_t used = 0;
    try { value = std::stoi(field.substr(prefix.size()), &used); }
    catch (...) { return false; }
    return used == field.size() - prefix.size();
  };
  const auto parseDouble = [](const std::string& field, const char* name, double& value) {
    const std::string prefix = std::string(name) + "=";
    if (field.rfind(prefix, 0) != 0) return false;
    std::size_t used = 0;
    try { value = std::stod(field.substr(prefix.size()), &used); }
    catch (...) { return false; }
    return used == field.size() - prefix.size() && std::isfinite(value);
  };
  if (!parseInt(destroyed, "destroyed", destroyedValue) ||
      !parseDouble(cooldown, "weapon-cooldown", cooldownValue) ||
      (destroyedValue != 0 && destroyedValue != 1) || cooldownValue < 0 || cooldownValue > 10) return false;
  s.destroyed = destroyedValue != 0;
  s.weaponCooldown = cooldownValue;
  return true;
}

std::string dockTransactionLine(const DockTransaction& transaction) {
  std::ostringstream out;
  out << "TRANSACTION DOCK_SALE station=" << transaction.station
      << " quantity=" << transaction.cargoSold
      << " unit-price=" << transaction.unitPrice
      << " credits=" << transaction.creditsEarned
      << " experience=" << transaction.experienceEarned;
  return out.str();
}

bool readDockTransaction(const std::string& line, DockTransaction& transaction) {
  std::istringstream in(line);
  std::string tag, kind, station, quantity, unitPrice, credits, experience, extra;
  DockTransaction next;
  if (!(in >> tag >> kind >> station >> quantity >> unitPrice >> credits >> experience) ||
      (in >> extra) || tag != "TRANSACTION" || kind != "DOCK_SALE") return false;
  const auto parseField = [](const std::string& field, const char* name, int& value) {
    const std::string prefix = std::string(name) + "=";
    if (field.rfind(prefix, 0) != 0 || field.size() == prefix.size()) return false;
    std::size_t used = 0;
    try { value = std::stoi(field.substr(prefix.size()), &used); }
    catch (...) { return false; }
    return used == field.size() - prefix.size();
  };
  if (!parseField(station, "station", next.station) ||
      !parseField(quantity, "quantity", next.cargoSold) ||
      !parseField(unitPrice, "unit-price", next.unitPrice) ||
      !parseField(credits, "credits", next.creditsEarned) ||
      !parseField(experience, "experience", next.experienceEarned)) return false;
  if (next.station < 0 || next.station >= static_cast<int>(kStations.size()) ||
      next.cargoSold < 0 || next.cargoSold > kMaximumSupportedCargo || next.unitPrice < 0 ||
      next.unitPrice > 100000 || next.creditsEarned < 0 || next.experienceEarned < 0 ||
      next.creditsEarned != next.cargoSold * next.unitPrice ||
      next.experienceEarned != next.cargoSold * 5) return false;
  transaction = next;
  return true;
}

std::string repair(State& s, int& credits, int hullLevel) {
  return repairToCapacity(s, credits, hullCapacity(hullLevel));
}

std::string repairToCapacity(State& s, int& credits, int maximumHull) {
  if (!s.docked) return "ERR dock-required";
  if (maximumHull < 1) return "ERR invalid-hull-state";
  s.maxHull = maximumHull;
  if (s.hull >= s.maxHull) return "ERR hull-full";
  const int missing = s.maxHull - s.hull;
  const int cost = missing * 3;
  if (credits < cost) return "ERR insufficient-credits";
  credits -= cost;
  s.hull = s.maxHull;
  return "OK REPAIRED hull=" + std::to_string(s.hull) + " cost=" + std::to_string(cost);
}

std::string trade(State& s, int& credits, bool buying, const std::string& commodity, int quantity) {
  return trade(s, credits, buying, commodity, quantity, kCargoCapacity);
}

std::string trade(State& s, int& credits, bool buying, const std::string& commodity, int quantity,
                  int cargoCapacity) {
  if (s.station < 0 || s.station >= static_cast<int>(kStations.size())) return "ERR invalid-station";
  const auto& market=kStations[static_cast<std::size_t>(s.station)];
  const int price=commodity=="food" ? (buying?market.foodBuy:market.foodSell) :
    commodity=="parts" ? (buying?market.partsBuy:market.partsSell) : 0;
  return tradeAtPrice(s, credits, buying, commodity, quantity, cargoCapacity, price);
}

std::string tradeAtPrice(State& s, int& credits, bool buying, const std::string& commodity, int quantity,
                         int cargoCapacity, int unitPrice) {
  if (!s.docked) return "ERR dock-required";
  if (quantity<1 || quantity>cargoCapacity) return "ERR invalid-quantity";
  if (commodity!="food" && commodity!="parts") return "ERR unknown-commodity";
  if (unitPrice <= 0 || unitPrice > 100000 || quantity > std::numeric_limits<int>::max() / unitPrice)
    return "ERR invalid-market-price";
  int& inventory=commodity=="food" ? s.food : s.parts;
  const int total=unitPrice*quantity;
  if (buying) {
    if (cargoUsed(s)+quantity>cargoCapacity) return "ERR cargo-full";
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
  if (c.hostile) {
    out << ' ' << c.hull << ' ' << c.maxHull;
    if (!c.affiliationId.empty() || !c.affiliationName.empty())
      out << ' ' << c.affiliationId << ' ' << c.affiliationName;
  }
  return out.str();
}
bool readContact(const std::string& line, Contact& c) {
  std::istringstream in(line); std::string tag,extra; Contact next; int docked;
  if (!(in>>tag>>next.id>>next.kind>>next.x>>next.y>>next.yaw>>docked) || tag!="CONTACT") return false;
  next.hostile = next.kind == "hostile";
  if (next.hostile && (!(in >> next.hull >> next.maxHull) || next.hull < 0 || next.maxHull < 1 || next.hull > next.maxHull)) return false;
  if (next.hostile && (in >> next.affiliationId)) {
    if (!(in >> next.affiliationName) || next.affiliationId.size() > 64 || next.affiliationName.size() > 64) return false;
  }
  if (in >> extra) return false;
  if (next.id.empty() || next.id.size()>32 || (next.kind!="pilot" && next.kind!="hauler" && next.kind!="hostile") ||
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
      std::abs(next.x) > 1201 || std::abs(next.y) > 1201 ||
      std::abs(next.vx) > kMaximumSupportedSpeed || std::abs(next.vy) > kMaximumSupportedSpeed ||
      std::abs(next.yaw) > kPi + 0.001 ||
      docked < 0 || docked > 1 || next.cargo < 0 || next.cargo > kMaximumSupportedCargo || next.food<0 || next.parts<0 ||
      next.food>kMaximumSupportedCargo || next.parts>kMaximumSupportedCargo ||
      cargoUsed(next)>kMaximumSupportedCargo || next.station<0 || next.station>1 ||
      next.cooldown < 0 || next.cooldown > 1.251) return false;
  if (next.hull < 0 || next.maxHull < 1 || next.maxHull > kMaximumSupportedHull ||
      next.hull > next.maxHull) return false;
  next.docked = docked != 0;
  s = next; credits = nextCredits; experience = nextExperience;
  return true;
}
} // namespace helion::flight
