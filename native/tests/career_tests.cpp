#include "shared/career.h"
#include <cassert>
#include <string>

static helion::flight::State dockedShip(int station = 0) {
  helion::flight::State ship;
  ship.docked = true;
  ship.station = station;
  ship.parts = 2;
  return ship;
}

int main() {
  using namespace helion::career;
  State state;
  assert(valid(state, 0));
  auto ship = dockedShip();
  int credits = 1000;
  int xp = 0;

  auto result = act(state, 2, ship, credits, xp, false, "ACCEPT", kSupply);
  assert(result.line == "OK CAREER ACCEPTED id=career.kepler_supply");
  result = act(state, 2, ship, credits, xp, false, "TURNIN", kSupply);
  assert(result.line.find("ERR buy-two-parts-at-cinder") != std::string::npos);
  purchased(state, 1, "parts", 2);
  assert(state.purchased == 2);
  result = act(state, 2, ship, credits, xp, false, "TURNIN", kSupply);
  assert(result.keplerDelta == 10 && credits == 1250 && xp == 30);
  assert(state.supply == 2 && ship.parts == 0);

  result = act(state, 2, ship, credits, xp, false, "ACCEPT", kResponse);
  assert(result.line.find("fit-pulse-laser-first") != std::string::npos);
  result = act(state, 2, ship, credits, xp, true, "ACCEPT", kResponse);
  assert(state.response == 1);
  assert(defeated(state));
  assert(state.complete && !defeated(state));

  const auto encoded = serialize(state);
  State roundTrip;
  assert(parse(encoded, 2, roundTrip));
  assert(roundTrip.complete && roundTrip.response == 2);
  State malformed;
  assert(!parse("career-v1 2 2 99 0 1", 2, malformed));
  assert(!parse("career-v1 1 0 0 0 0", 1, malformed));
  const auto lines = news(state, 2);
  assert(lines.size() == 7);
  assert(lines.back().find("established Kepler pilot") != std::string::npos);
}
