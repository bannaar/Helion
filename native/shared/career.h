#pragma once
#include "shared/flight.h"
#include <string>
#include <string_view>
#include <vector>

namespace helion::career {
inline constexpr std::string_view kOre = "career.first_ore";
inline constexpr std::string_view kSupply = "career.kepler_supply";
inline constexpr std::string_view kResponse = "career.red_wake_response";
// First Ore stays in its existing fields. Stages: 0 available (subject to
// prerequisites), 1 accepted, 2 completed. Only the server mutates progress.
struct State {
  int supply = 0, response = 0, purchased = 0;
  bool introDismissed = false, complete = false;
};
inline constexpr int kSupplyQuantity = 2, kSupplyCredits = 250, kSupplyXp = 30;
struct Result { std::string line; int keplerDelta = 0; };
bool valid(const State&, int firstOre);
std::string serialize(const State&);
bool parse(std::string_view record, int firstOre, State&);
std::string status(const State&, int firstOre);
Result act(State&, int firstOre, flight::State&, int& credits, int& xp,
           bool fittedWeapon, std::string_view action, std::string_view id);
void purchased(State&, int station, std::string_view commodity, int quantity);
bool defeated(State&);
// Derived from durable transitions; reading news never changes state.
std::vector<std::string> news(const State&, int firstOre);
}
