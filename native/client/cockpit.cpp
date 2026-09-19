#include "client/cockpit.h"

#include "shared/organizations.h"
#include "shared/loadout.h"

#include <algorithm>
#include <cmath>

namespace helion::client {
namespace {
constexpr FontColor white{0.83f, 0.89f, 0.90f, 1.0f};
constexpr FontColor muted{0.42f, 0.57f, 0.61f, 1.0f};
constexpr FontColor teal{0.25f, 0.88f, 0.76f, 1.0f};
constexpr FontColor amber{1.0f, 0.67f, 0.29f, 1.0f};
constexpr FontColor red{1.0f, 0.24f, 0.24f, 1.0f};

void appendLine(CockpitTextBatch& batch, const HudLayout& layout, float x, float y,
                float scale, FontColor color, std::string_view value, float width,
                std::size_t maxCharacters = 160) {
  if (batch.vertices.size() >= kMaxTextVertices) return;
  const auto normalized = normalizeText(value, maxCharacters);
  auto vertices = buildTextVertices(normalized, layout.originX + x * layout.scale,
    layout.originY + y * layout.scale, scale * layout.scale, color, maxCharacters, width * layout.scale);
  const std::size_t available = kMaxTextVertices - batch.vertices.size();
  if (vertices.size() > available) vertices.resize(available - available % 6);
  batch.vertices.insert(batch.vertices.end(), vertices.begin(), vertices.end());
  batch.glyphs += normalized.size();
}

void appendUiLine(CockpitTextBatch& batch, const HudLayout& layout, float y,
                 bool selected, std::string_view value, FontColor color = white,
                 float width = 900) {
  appendLine(batch, layout, 34, y, 1.35f, selected ? amber : color,
    std::string(selected ? "> " : "  ") + std::string(value), width, 150);
}

bool ownsModule(const UiState& state, std::string_view id) {
  return std::find(state.ownedModules.begin(), state.ownedModules.end(), id) != state.ownedModules.end();
}

bool fittedModule(const UiState& state, helion::loadout::Slot slot, std::string_view id) {
  return state.fittedModules[helion::loadout::slotIndex(slot)] == id;
}

void buildUiScreenText(CockpitTextBatch& batch, const HudLayout& layout,
                       const PresentationSnapshot& snapshot) {
  const auto& ui = snapshot.ui;
  appendLine(batch, layout, 24, 18, 2.0f, white, "HELION / " + uiScreenName(ui.screen), 900, 80);
  appendLine(batch, layout, 24, 44, 1.15f, muted,
    ui.connectionStatus.empty() ? "COMMAND LINK / TLS VERIFIED" : ui.connectionStatus, 900, 100);
  appendLine(batch, layout, 24, 76, 1.05f, teal,
    "F1 STATION  F2 PROFILE  F3 OPTIONS  F4 MARKET  F5 MISSION  F6 OUTFIT  F7 GALNET  F8 GRAPHICS  ESC BACK", 900, 150);

  if (ui.screen == UiScreen::account) {
    appendUiLine(batch, layout, 132, false, snapshot.authenticated ? "COMMANDER AUTHENTICATED" : "SECURE ACCOUNT ACCESS");
    appendUiLine(batch, layout, 164, false, "CREATE: /CREATE USER PASSWORD DISPLAY");
    appendUiLine(batch, layout, 192, false, "LOGIN:  /LOGIN USER PASSWORD");
    appendUiLine(batch, layout, 230, false, "INPUT / " + (ui.typed.empty() ? std::string("_") : ui.typed), white, 880);
    appendUiLine(batch, layout, 262, false, "PASSWORDS ARE MASKED IN THIS DISPLAY", muted);
  } else if (ui.screen == UiScreen::station) {
    const int selected = std::clamp(ui.selected, 0, 9);
    const std::array<std::string_view, 10> items = {"MARKET / BUY OR SELL", "MISSION / FIRST ORE", "OUTFITTING / MODULES",
      "PROFILE / REPUTATION", "GALNET / NEWS", "OPTIONS / CONTROLS", "GRAPHICS / DIAGNOSTICS",
      "LAUNCH INTO KEPLER", "REPAIR SHIP", "REFUEL SHIP"};
    appendUiLine(batch, layout, 124, false, snapshot.stationContext + " / DOCKED SERVICES", teal);
    appendUiLine(batch, layout, 150, false, std::to_string(snapshot.credits) + " CR / HULL " +
      std::to_string(snapshot.player.hull) + "/" + std::to_string(snapshot.player.maxHull) + " / FUEL " +
      std::to_string(static_cast<int>(std::ceil(snapshot.player.fuel))) + "/" +
      std::to_string(static_cast<int>(std::ceil(snapshot.player.maxFuel))));
    for (std::size_t i = 0; i < items.size(); ++i)
      appendUiLine(batch, layout, 190 + static_cast<float>(i) * 27, selected == static_cast<int>(i), items[i]);
    appendUiLine(batch, layout, 478, false, "UP/DOWN SELECT  ENTER CONFIRM  ESC BACK", muted);
  } else if (ui.screen == UiScreen::market) {
    const int station = std::clamp(snapshot.player.station, 0, static_cast<int>(flight::kStations.size()) - 1);
    const auto& prices = flight::kStations[static_cast<std::size_t>(station)];
    appendUiLine(batch, layout, 124, false, snapshot.stationContext + " / PRICES ARE SERVER-VALIDATED", teal);
    appendUiLine(batch, layout, 154, false, "CREDITS " + std::to_string(snapshot.credits) + " / HOLD " +
      std::to_string(flight::cargoUsed(snapshot.player)) + "/" + std::to_string(flight::kCargoCapacity));
    const std::array<std::string, 2> rows = {"FOOD / BUY " + std::to_string(prices.foodBuy) + " / SELL " +
      std::to_string(prices.foodSell) + " / OWNED " + std::to_string(snapshot.player.food),
      "PARTS / BUY " + std::to_string(prices.partsBuy) + " / SELL " + std::to_string(prices.partsSell) +
      " / OWNED " + std::to_string(snapshot.player.parts)};
    for (std::size_t i = 0; i < rows.size(); ++i)
      appendUiLine(batch, layout, 204 + static_cast<float>(i) * 36, static_cast<int>(i) == ui.selected, rows[i]);
    appendUiLine(batch, layout, 300, false, "QTY " + std::to_string(ui.quantity) + " / B BUY  S SELL  +/- QUANTITY", muted);
    appendUiLine(batch, layout, 340, false, "STATION PRICES / KEPLER AND CINDER DIFFER", muted);
  } else if (ui.screen == UiScreen::mission) {
    const std::string stage = ui.missionStage == 0 ? "AVAILABLE" : ui.missionStage == 1 ? "ACCEPTED / ACTIVE" : "COMPLETED";
    appendUiLine(batch, layout, 124, false, "FIRST ORE / " + stage, teal);
    appendUiLine(batch, layout, 158, false, "ISSUER / ORION EXTRACTION GROUP");
    appendUiLine(batch, layout, 186, false, "JURISDICTION / KEPLER AUTHORITY");
    appendUiLine(batch, layout, 214, false, "OBJECTIVE / MINE 1 ORE AND RETURN TO STATION");
    appendUiLine(batch, layout, 242, false, "REWARD / 250 CR / 25 XP / ORION +10 / KEPLER +5");
    appendUiLine(batch, layout, 278, false, "PROGRESS / " + std::string(ui.missionOreMined ? "ORE MINED" : "NOT YET COMPLETE"));
    appendUiLine(batch, layout, 326, false, ui.missionStage == 0 ? "ENTER ACCEPT CONTRACT" : "MISSION STATE IS SERVER-AUTHORITATIVE", muted);
  } else if (ui.screen == UiScreen::outfitting) {
    appendUiLine(batch, layout, 124, false, "MODULE CATALOGUE / OWNERSHIP AND FITTING ARE SERVER-VALIDATED", teal);
    const std::size_t maxRows = std::min<std::size_t>(7, helion::loadout::kCatalogue.size());
    for (std::size_t i = 0; i < maxRows; ++i) {
      const auto& module = helion::loadout::kCatalogue[i];
      const bool owned = ownsModule(ui, module.id);
      const bool fitted = fittedModule(ui, module.slot, module.id);
      const std::string row = std::string(module.id) + " / " + helion::loadout::slotName(module.slot) +
        " / " + (owned ? (fitted ? "FITTED" : "OWNED") : std::to_string(module.price) + " CR");
      appendUiLine(batch, layout, 158 + static_cast<float>(i) * 35, static_cast<int>(i) == ui.selected, row);
    }
    appendUiLine(batch, layout, 430, false, "B BUY  F FIT  R REMOVE SLOT  ENTER FIT/BUY", muted);
  } else if (ui.screen == UiScreen::profile) {
    appendUiLine(batch, layout, 124, false, "COMMANDER / " + snapshot.commanderName, teal);
    appendUiLine(batch, layout, 154, false, std::to_string(snapshot.credits) + " CR / " + std::to_string(snapshot.experience) + " XP");
    appendUiLine(batch, layout, 184, false, "SALVAGE " + std::to_string(ui.salvage) + " / ENGINE LEVEL " + std::to_string(ui.engineLevel));
    appendUiLine(batch, layout, 214, false, "HULL LEVEL " + std::to_string(ui.hullLevel) + " / CARGO " +
      std::to_string(flight::cargoUsed(snapshot.player)) + "/" + std::to_string(flight::kCargoCapacity));
    appendUiLine(batch, layout, 262, false, standingText(snapshot, "authority.kepler"));
    appendUiLine(batch, layout, 292, false, standingText(snapshot, "corp.orion"));
    appendUiLine(batch, layout, 322, false, standingText(snapshot, "criminal.red_wake"));
    appendUiLine(batch, layout, 352, false, standingText(snapshot, "criminal.vanta"));
    appendUiLine(batch, layout, 382, false, standingText(snapshot, "faction.commonwealth"));
  } else if (ui.screen == UiScreen::galnet) {
    appendUiLine(batch, layout, 124, false, "PERSISTED EVENTS / OPENING THIS VIEW DOES NOT CREATE EVENTS", teal);
    if (ui.galnet.empty()) appendUiLine(batch, layout, 166, false, "NO GALNET EVENTS AVAILABLE", muted);
    else {
      const std::size_t first = ui.galnet.size() > 10 ? ui.galnet.size() - 10 : 0;
      for (std::size_t i = first; i < ui.galnet.size(); ++i) {
        appendUiLine(batch, layout, 164 + static_cast<float>(i - first) * 34,
          static_cast<int>(i - first) == ui.selected, ui.galnet[i].headline);
      }
    }
  } else if (ui.screen == UiScreen::options) {
    appendUiLine(batch, layout, 124, false, "INPUT / KEYBOARD NAVIGATION", teal);
    appendUiLine(batch, layout, 158, false, "F3 TELEMETRY / " + std::string(snapshot.ui.telemetryEnabled ? "ON" : "OFF"));
    appendUiLine(batch, layout, 192, false, "W/A/D/S FLIGHT CONTROLS REMAIN OUTSIDE MENUS");
    appendUiLine(batch, layout, 226, false, "ENTER CONFIRM / ESC BACK / UP DOWN SELECT");
    appendUiLine(batch, layout, 260, false, "AUDIO / EXISTING NATIVE AUDIO SETTINGS UNCHANGED", muted);
  } else if (ui.screen == UiScreen::graphics) {
    appendUiLine(batch, layout, 124, false, "RENDERER MODE / CORE (RESTART TO CHANGE)", teal);
    appendUiLine(batch, layout, 158, false, ui.graphicsReport.empty() ? "GRAPHICS REPORT / UNAVAILABLE" : ui.graphicsReport, white, 880);
    appendUiLine(batch, layout, 206, false, "OPENGL 3.3 CORE / GLSL 3.30 / FIXED-FUNCTION CALLS DISABLED");
    appendUiLine(batch, layout, 240, false, "AUTO MODE CONTINUES TO SELECT LEGACY", muted);
  }
  if (!ui.statusMessage.empty()) appendUiLine(batch, layout, 520, false, ui.statusMessage, amber, 880);
}

const PresentationContact* selectedTarget(const PresentationSnapshot& snapshot) {
  for (const auto& contact : snapshot.contacts)
    if (contact.selected && contact.hostile) return &contact;
  return nullptr;
}

std::string stateText(const PresentationSnapshot& snapshot) {
  if (!snapshot.connected) return "COMMAND LINK / OFFLINE";
  if (!snapshot.authenticated) return "COMMANDER ACCESS / SIGN IN";
  if (snapshot.player.destroyed) return "SHIP DISABLED / RECOVERY REQUIRED";
  if (snapshot.player.docked) return "DOCKED / STATION SERVICES AVAILABLE";
  if (snapshot.miningActive) return "MINING BEAM ACTIVE / EXTRACTION IN PROGRESS";
  if (snapshot.weaponFired) return "PULSE LASER FIRED / IMPACT CONFIRMED";
  return "FREE FLIGHT / COMMAND LINK ONLINE";
}
}

HudLayout hudLayout(int width, int height) {
  const float safeWidth = static_cast<float>(std::max(width, 1));
  const float safeHeight = static_cast<float>(std::max(height, 1));
  const float scale = std::max(0.25f, std::min(safeWidth / 960.0f, safeHeight / 600.0f));
  return {(safeWidth - 960.0f * scale) * 0.5f,
          (safeHeight - 600.0f * scale) * 0.5f, scale, safeWidth, safeHeight};
}

std::string standingText(const PresentationSnapshot& snapshot, std::string_view id) {
  const auto* definition = helion::organizations::find(id);
  const auto found = snapshot.reputation.find(std::string(id));
  const int value = found == snapshot.reputation.end() ? 0 : found->second;
  return std::string(definition ? definition->displayName : id) + " / " +
    helion::organizations::standingLabel(value) + " / " + std::to_string(value);
}

std::string targetText(const PresentationSnapshot& snapshot) {
  const auto* target = selectedTarget(snapshot);
  if (!target) return "TARGET / NO HOSTILE CONTACT";
  const int range = static_cast<int>(std::hypot(target->x - snapshot.player.x, target->y - snapshot.player.y));
  return "TARGET / " + target->id + " / " + target->affiliationName +
    " / HULL " + std::to_string(target->hull) + "/" + std::to_string(target->maxHull) +
    " / RANGE " + std::to_string(range) + " M";
}

CockpitTextBatch buildCockpitText(const PresentationSnapshot& snapshot, int width, int height) {
  const auto layout = hudLayout(width, height);
  CockpitTextBatch batch;
  batch.vertices.reserve(12000);
  if (snapshot.ui.screen != UiScreen::flight) {
    buildUiScreenText(batch, layout, snapshot);
    return batch;
  }
  appendLine(batch, layout, 24, 18, 2.0f, white,
    "HELION / COMMANDER " + (snapshot.commanderName.empty() ? std::string("UNREGISTERED") : snapshot.commanderName), 540, 64);
  appendLine(batch, layout, 24, 42, 1.35f, muted,
    snapshot.stationContext + " / " + (snapshot.player.docked ? "DOCKED" : "FLIGHT"), 500, 64);
  appendLine(batch, layout, 650, 18, 1.6f, amber,
    std::to_string(snapshot.credits) + " CR", 270, 24);
  appendLine(batch, layout, 650, 42, 1.35f, muted,
    std::to_string(snapshot.experience) + " XP", 270, 24);

  appendLine(batch, layout, 24, 108, 1.35f, teal, "SHIP STATUS", 300, 32);
  appendLine(batch, layout, 24, 132, 1.25f, white,
    "HULL " + std::to_string(std::max(0, snapshot.player.hull)) + " / " +
      std::to_string(std::max(1, snapshot.player.maxHull)), 300, 40);
  appendLine(batch, layout, 24, 154, 1.25f, white,
    "FUEL " + std::to_string(static_cast<int>(std::ceil(std::max(0.0, snapshot.player.fuel)))) +
      " / " + std::to_string(static_cast<int>(std::ceil(std::max(1.0, snapshot.player.maxFuel)))), 300, 40);
  appendLine(batch, layout, 24, 176, 1.25f, amber,
    "CARGO " + std::to_string(std::max(0, snapshot.player.cargo)) + " / " +
      std::to_string(flight::kCargoCapacity), 300, 32);
  appendLine(batch, layout, 24, 198, 1.25f, snapshot.player.weaponCooldown > 0 ? amber : teal,
    std::string("LASER ") + (snapshot.player.weaponCooldown > 0 ? "RECHARGING" : "READY"), 300, 32);

  appendLine(batch, layout, 368, 108, 1.35f, teal, "MISSION / FIRST ORE", 568, 54);
  appendLine(batch, layout, 368, 132, 1.15f, white,
    snapshot.missionSummary.empty() ? "CHECK CONTRACTS" : snapshot.missionSummary, 568, 86);
  appendLine(batch, layout, 368, 158, 1.15f, muted,
    "ISSUER / ORION EXTRACTION GROUP", 568, 50);
  appendLine(batch, layout, 368, 180, 1.15f, muted,
    "JURISDICTION / KEPLER AUTHORITY", 568, 50);
  appendLine(batch, layout, 368, 202, 1.15f, teal, stateText(snapshot), 568, 86);

  appendLine(batch, layout, 24, 252, 1.25f, teal, "ORGANIZATION STANDINGS", 330, 40);
  appendLine(batch, layout, 24, 276, 1.05f, white, standingText(snapshot, "authority.kepler"), 330, 54);
  appendLine(batch, layout, 24, 296, 1.05f, white, standingText(snapshot, "corp.orion"), 330, 54);
  appendLine(batch, layout, 24, 316, 1.05f, red, standingText(snapshot, "criminal.red_wake"), 330, 54);

  appendLine(batch, layout, 368, 252, 1.25f, teal, "CONTACT / COMBAT", 568, 32);
  appendLine(batch, layout, 368, 276, 1.05f, selectedTarget(snapshot) ? red : muted,
    targetText(snapshot), 568, 86);
  appendLine(batch, layout, 368, 300, 1.05f, muted,
    snapshot.miningActive ? "MINING / ACTIVE" : "MINING / INACTIVE", 568, 30);
  appendLine(batch, layout, 368, 324, 1.05f, snapshot.player.destroyed ? red : amber,
    snapshot.player.destroyed ? "RECOVERY / PRESS R AT SAFE STATION" :
      (snapshot.player.docked ? "DOCKED / L LAUNCH  R REPAIR  T REFUEL" :
       "W THRUST  A/D TURN  E MINE  SPACE FIRE  F DOCK"), 568, 86);

  appendLine(batch, layout, 24, 392, 1.25f, teal, "GALNET / RECENT ACTIVITY", 912, 42);
  const std::size_t messageCount = std::min<std::size_t>(4, snapshot.recentMessages.size());
  if (messageCount == 0) {
    appendLine(batch, layout, 24, 418, 1.05f, muted, "NO RECENT TRANSMISSIONS", 912, 32);
  } else {
    for (std::size_t i = snapshot.recentMessages.size() - messageCount;
         i < snapshot.recentMessages.size(); ++i)
      appendLine(batch, layout, 24, 418 + static_cast<float>(i - (snapshot.recentMessages.size() - messageCount)) * 22,
        1.05f, i + 1 == snapshot.recentMessages.size() ? amber : muted,
        snapshot.recentMessages[i], 912, 128);
  }
  return batch;
}

} // namespace helion::client
