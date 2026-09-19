#include "client/ui.h"

#include "shared/loadout.h"
#include "shared/organizations.h"

#include <algorithm>
#include <cctype>

namespace helion::client {
std::string uiScreenName(UiScreen screen) {
  switch (screen) {
    case UiScreen::account: return "ACCOUNT / CONNECTION";
    case UiScreen::flight: return "FLIGHT / KEPLER REACH";
    case UiScreen::station: return "STATION / SERVICES";
    case UiScreen::market: return "MARKET / COMMODITIES";
    case UiScreen::mission: return "MISSION / CONTRACTS";
    case UiScreen::outfitting: return "OUTFITTING / LOADOUT";
    case UiScreen::profile: return "PROFILE / PROGRESSION";
    case UiScreen::galnet: return "GALNET / NEWS";
    case UiScreen::options: return "OPTIONS / CONTROLS";
    case UiScreen::graphics: return "GRAPHICS / DIAGNOSTICS";
  }
  return "HELION";
}

std::string uiStandingLabel(int value) {
  return helion::organizations::standingLabel(value);
}

std::string maskedCommand(std::string_view command) {
  std::string result(command);
  std::string normalized(command);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
    [](unsigned char character) { return static_cast<char>(std::toupper(character)); });
  const auto start = normalized.find_first_not_of(" /\t");
  if (start == std::string::npos) return result;
  const auto end = normalized.find_first_of(" \t", start);
  const auto verb = normalized.substr(start, end - start);
  if (verb != "LOGIN" && verb != "CREATE") return result;
  const auto user = normalized.find_first_not_of(" \t", end);
  if (user == std::string::npos) return result;
  const auto userEnd = normalized.find_first_of(" \t", user);
  const auto password = normalized.find_first_not_of(" \t", userEnd);
  if (password == std::string::npos) return result;
  const auto passwordEnd = normalized.find_first_of(" \t", password);
  result.replace(password, passwordEnd == std::string::npos ? result.size() - password : passwordEnd - password, "********");
  return result;
}

void populateUiDerived(UiState& state, const flight::State&) {
  state.quantity = std::clamp(state.quantity, 1, flight::kCargoCapacity);
  if (state.statusMessage.size() > 220) state.statusMessage.resize(220);
  if (state.graphicsReport.size() > 220) state.graphicsReport.resize(220);
  if (state.ownedModules.size() > helion::loadout::kCatalogue.size())
    state.ownedModules.resize(helion::loadout::kCatalogue.size());
  if (state.galnet.size() > 32) state.galnet.erase(state.galnet.begin(), state.galnet.end() - 32);
  for (auto& entry : state.galnet) {
    if (entry.id.size() > 64) entry.id.resize(64);
    if (entry.headline.size() > 180) entry.headline.resize(180);
  }
}

} // namespace helion::client
