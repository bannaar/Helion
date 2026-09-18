#pragma once

#include <array>
#include <string>

namespace helion::flight {
constexpr double kPi = 3.141592653589793;
constexpr int kCargoCapacity = 8;
constexpr int kOrePrice = 60;
constexpr double kMineRange = 85;
constexpr double kDockRange = 85;
constexpr double kWorkSpeed = 35;
struct Rock { double x, y, radius; };
inline constexpr std::array<Rock, 7> kRocks{{
  {0, 280, 30}, {170, 340, 38}, {-190, 380, 24},
  {330, 140, 42}, {-330, 80, 36}, {150, -300, 32}, {-230, -270, 28}
}};
struct Station { const char* name; double x, y; int foodBuy, foodSell, partsBuy, partsSell, oreSell; };
inline constexpr std::array<Station,2> kStations{{
  {"KEPLER",0,0,20,16,65,52,60}, {"CINDER",650,0,40,32,40,32,75}
}};
struct Contact { std::string id, kind; double x=0, y=0, yaw=0; bool docked=false; };
std::string contactLine(const Contact& contact);
bool readContact(const std::string& line, Contact& contact);
struct Input { int thrust = 0, turn = 0, brake = 0; };
inline Input controls(bool forward, bool left, bool right, bool brake, bool active) {
  return active ? Input{forward, static_cast<int>(left)-static_cast<int>(right), brake} : Input{0,0,1};
}
struct State {
  double x = 0, y = 0, vx = 0, vy = 0, yaw = 0;
  double inputAge = 1, cooldown = 0;
  Input input;
  bool docked = true;
  int cargo = 0, food = 0, parts = 0, station = 0;
  int hull = 100, maxHull = 100;
};
void step(State& state, double dt, int engineLevel = 1);
double speed(const State& state);
int nearestRock(const State& state);
int nearestStation(const State& state);
int cargoUsed(const State& state);
std::string trade(State& state, int& credits, bool buying, const std::string& commodity, int quantity);
std::string launch(State& state);
std::string mine(State& state);
std::string dock(State& state, int& credits, int& experience);
int hullCapacity(int hullLevel);
std::string repair(State& state, int& credits, int hullLevel = 1);
std::string snapshot(const State& state, int credits, int experience);
bool readSnapshot(const std::string& line, State& state, int& credits, int& experience);
} // namespace helion::flight
