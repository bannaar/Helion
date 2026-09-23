#include "shared/career.h"
#include <algorithm>
#include <limits>
#include <sstream>

namespace helion::career {
bool valid(const State& s, int first) {
  return first >= 0 && first <= 2 && s.supply >= 0 && s.supply <= 2 &&
    s.response >= 0 && s.response <= 2 && s.purchased >= 0 && s.purchased <= 2 &&
    (s.supply == 0 || first == 2) && (s.response == 0 || s.supply == 2) &&
    (s.supply != 0 || s.purchased == 0) && s.complete == (s.response == 2);
}
std::string serialize(const State& s) {
  return "career-v1 " + std::to_string(s.supply) + " " + std::to_string(s.response) +
    " " + std::to_string(s.purchased) + " " + std::to_string(s.introDismissed) + " " + std::to_string(s.complete);
}
bool parse(std::string_view record, int first, State& state) {
  State next;
  std::istringstream in{std::string(record)};
  std::string version, extra;
  int intro, complete;
  if (!(in >> version >> next.supply >> next.response >> next.purchased >> intro >> complete) ||
      (in >> extra) || version != "career-v1" || (intro != 0 && intro != 1) ||
      (complete != 0 && complete != 1)) return false;
  next.introDismissed = intro != 0; next.complete = complete != 0;
  if (!valid(next, first)) return false;
  state = next; return true;
}
std::string status(const State& s, int first) {
  return "CAREER first=" + std::to_string(first) + " supply=" + std::to_string(s.supply) +
    " response=" + std::to_string(s.response) + " purchased=" + std::to_string(s.purchased) +
    " intro=" + std::to_string(s.introDismissed) + " complete=" + std::to_string(s.complete);
}
Result act(State& s, int first, flight::State& ship, int& credits, int& xp,
           bool weapon, std::string_view action, std::string_view id) {
  if (action == "DISMISS" && id.empty()) { s.introDismissed = true; return {"OK INTRO DISMISSED"}; }
  if (id != kSupply && id != kResponse) return {"ERR unknown-career-id"};
  if (!ship.docked || ship.destroyed) return {"ERR dock-required"};
  if (first != 2 || (id == kResponse && s.supply != 2)) return {"ERR career-prerequisite"};
  int& stage = id == kSupply ? s.supply : s.response;
  if (action == "ACCEPT") {
    if (stage != 0) return {"ERR mission-unavailable"};
    if (id == kResponse && !weapon) return {"ERR fit-pulse-laser-first"};
    stage = 1; return {"OK CAREER ACCEPTED id=" + std::string(id)};
  }
  if (action != "TURNIN" || id != kSupply) return {"ERR invalid-career-action"};
  if (stage != 1) return {"ERR mission-not-active"};
  if (ship.station != 0) return {"ERR deliver-to-kepler"};
  if (s.purchased < kSupplyQuantity || ship.parts < kSupplyQuantity) return {"ERR buy-two-parts-at-cinder"};
  if (credits > std::numeric_limits<int>::max() - kSupplyCredits ||
      xp > std::numeric_limits<int>::max() - kSupplyXp) return {"ERR profile-limit"};
  ship.parts -= kSupplyQuantity; credits += kSupplyCredits; xp += kSupplyXp; stage = 2;
  return {"OK CAREER COMPLETE id=career.kepler_supply credits=250 experience=30 parts-consumed=2", 10};
}
void purchased(State& s, int station, std::string_view commodity, int quantity) {
  if (s.supply == 1 && station == 1 && commodity == "parts" && quantity > 0)
    s.purchased = std::min(kSupplyQuantity, s.purchased + quantity);
}
bool defeated(State& s) {
  if (s.response != 1) return false;
  s.response = 2; s.complete = true; s.introDismissed = true; return true;
}
std::vector<std::string> news(const State& s, int first) {
  std::vector<std::string> lines{"GALNET id=career-first-ore-available headline=Orion Extraction Group seeks new pilots for First Ore in Kepler."};
  if (first == 2) {
    lines.push_back("GALNET id=career-first-ore-complete headline=Orion confirms your First Ore contract. Kepler Authority recognizes the delivery.");
    lines.push_back("GALNET id=career-supply-request headline=Kepler Authority requests two industrial parts from Cinder after Red Wake disruption.");
  }
  if (s.supply == 2) {
    lines.push_back("GALNET id=career-supply-complete headline=Your Cinder supplies reached Kepler. Local repair crews resume work.");
    lines.push_back("GALNET id=career-response-available headline=Kepler Authority authorizes a Red Wake response. Fit a pulse laser before accepting.");
  }
  if (s.complete) {
    lines.push_back("GALNET id=career-response-complete headline=Your Red Wake response is complete. Kepler Authority credits your security service.");
    lines.push_back("GALNET id=career-established-pilot headline=Recognized as an established Kepler pilot: trade, mine and patrol the active frontier.");
  }
  return lines;
}
}
