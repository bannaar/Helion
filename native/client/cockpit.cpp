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
  batch.glyphs += vertices.size() / 6;
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
    appendUiLine(batch, layout, 114, false, "WELCOME TO KEPLER / SECURE COMMANDER ACCESS", teal);
    appendUiLine(batch, layout, 158, false, "ACCOUNT / " + ui.accountName);
    appendUiLine(batch, layout, 208, false, "PASSWORD / " + ui.passwordMask);
    appendUiLine(batch, layout, 258, false, "DISPLAY NAME / " + ui.accountDisplay);
    appendUiLine(batch, layout, 296, false, "PASSWORD: 12-128 CHARACTERS / NEVER SHOWN OR LOGGED", muted);
    appendUiLine(batch, layout, 392, false, "TAB CHANGE FIELD / ENTER LOGIN / SHIFT+ENTER CREATE", muted);
    appendUiLine(batch, layout, 426, false, "NEW PILOT? CREATE A COMMANDER TO BEGIN YOUR KEPLER CAREER.", teal);
  } else if (ui.screen == UiScreen::station) {
    const int selected = std::clamp(ui.selected, 0, 9);
    const std::array<std::string_view, 10> items = {"MARKET / BUY OR SELL", "CAREER / KEPLER", "OUTFITTING / MODULES",
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
    appendUiLine(batch, layout, 300, false, "QTY " + std::to_string(ui.quantity) + " / B BUY  S SELL", muted, 460);
    appendUiLine(batch, layout, 340, false, "STATION PRICES / KEPLER AND CINDER DIFFER", muted);
  } else if (ui.screen == UiScreen::mission) {
    const int choice = std::clamp(ui.missionSelected, 0, 2);
    const int stage = choice == 0 ? ui.missionStage : choice == 1 ? ui.career.supply : ui.career.response;
    const bool unlocked = choice == 0 || (ui.missionStage == 2 && (choice == 1 || ui.career.supply == 2));
    const char* titles[] = {"FIRST ORE", "KEPLER SUPPLY", "RED WAKE RESPONSE"};
    for (int i = 0; i < 3; ++i) appendLine(batch, layout, 30 + i * 310, 128, 1.35f,
      i == choice ? amber : white, titles[i], 280);
    appendUiLine(batch, layout, 178, false, std::string("STATUS / ") +
      (!unlocked ? "LOCKED / COMPLETE PREVIOUS CONTRACT" : stage == 0 ? "AVAILABLE" : stage == 1 ? "ACCEPTED" : "COMPLETED / PAID"), teal);
    appendUiLine(batch, layout, 210, false, choice == 0 ? "ISSUER / ORION EXTRACTION GROUP" : "ISSUER / KEPLER AUTHORITY");
    appendUiLine(batch, layout, 238, false, "JURISDICTION / KEPLER AUTHORITY");
    appendUiLine(batch, layout, 270, false, choice == 0 ? "MINE 1 ORE / RETURN AND DOCK / TURN IN" :
      choice == 1 ? "BUY 2 PARTS AT CINDER (650, 0) / DELIVER TO KEPLER (0, 0)" :
      "FIT PULSE LASER / LAUNCH / DEFEAT A RED WAKE RAIDER");
    appendUiLine(batch, layout, 300, false, choice == 0 ? "REWARD / 250 CR / 25 XP / ORION +10 / KEPLER +5" :
      choice == 1 ? "REWARD / 250 CR / 30 XP / KEPLER +10 / CONSUMES 2 PARTS" :
      "KILL REWARD / 100 CR / 25 XP / 1 SALVAGE / KEPLER +5 / RED WAKE -10");
    appendUiLine(batch, layout, 334, false, choice == 0 ?
      std::string("ORE MINED / ") + (ui.missionOreMined ? "YES" : "NO") :
      choice == 1 ? "CINDER PURCHASES / " + std::to_string(ui.career.purchased) + "/2 / PARTS IN HOLD " + std::to_string(snapshot.player.parts) :
      "PULSE LASER / 650 CR / RANGE 240 / DAMAGE 25 / RECHARGE 1 SECOND");
    appendUiLine(batch, layout, 370, false, "UP/DOWN OR CLICK CONTRACT / ENTER ACCEPT OR TURN IN", muted);
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
      constexpr std::size_t visible = 9;
      const std::size_t maxOffset = ui.galnet.size() > visible ? ui.galnet.size() - visible : 0;
      const std::size_t first = std::min<std::size_t>(ui.scrollOffset, maxOffset);
      const std::size_t last = std::min(ui.galnet.size(), first + visible);
      for (std::size_t i = first; i < last; ++i) {
        appendUiLine(batch, layout, 164 + static_cast<float>(i - first) * 34,
          static_cast<int>(i - first) == ui.selected, ui.galnet[i].headline);
      }
      appendUiLine(batch, layout, 478, false,
        "SCROLL / WHEEL OR PAGE UP DOWN / " + std::to_string(first + 1) + "-" + std::to_string(last) +
        " OF " + std::to_string(ui.galnet.size()), muted);
    }
  } else if (ui.screen == UiScreen::options) {
    appendUiLine(batch, layout, 124, false, "INPUT / KEYBOARD NAVIGATION", teal);
    appendUiLine(batch, layout, 158, false, "F3 TELEMETRY / " + std::string(snapshot.ui.telemetryEnabled ? "ON" : "OFF"));
    appendUiLine(batch, layout, 192, false, "W/A/D/S FLIGHT CONTROLS REMAIN OUTSIDE MENUS");
    appendUiLine(batch, layout, 226, false, "ENTER CONFIRM / ESC BACK / UP DOWN SELECT");
    appendUiLine(batch, layout, 260, false, "AUDIO / EXISTING NATIVE AUDIO SETTINGS UNCHANGED", muted);
  } else if (ui.screen == UiScreen::graphics) {
    appendUiLine(batch, layout, 124, false, "RENDERER MODE / CORE (RESTART TO CHANGE)", teal);
    if (ui.graphicsReport.empty()) appendUiLine(batch, layout, 158, false, "GRAPHICS REPORT / UNAVAILABLE");
    else {
      std::string_view remaining(ui.graphicsReport);
      for (int line = 0; line < 4 && !remaining.empty(); ++line) {
        std::size_t count = std::min<std::size_t>(remaining.size(), 68);
        if (count < remaining.size()) {
          const auto space = remaining.rfind(' ', count);
          if (space != std::string_view::npos && space > 0) count = space;
        }
        appendUiLine(batch, layout, 158 + line * 30.0f, false, remaining.substr(0, count), white, 880);
        remaining.remove_prefix(count);
        while (!remaining.empty() && remaining.front() == ' ') remaining.remove_prefix(1);
      }
    }
    appendUiLine(batch, layout, 294, false, "OPENGL 3.3 CORE / FIXED-FUNCTION CALLS DISABLED");
    appendUiLine(batch, layout, 328, false, "AUTO PREFERS HARDWARE CORE / FALLS BACK TO LEGACY", muted);
  } else if (ui.screen == UiScreen::help) {
    const std::array<const char*, 10> guide{{
      "KEPLER NEEDS PILOTS. ORION NEEDS ORE. RED WAKE THREATENS THE SUPPLY ROUTE.",
      "1 / F5 CONTRACTS: FIRST ORE, CINDER SUPPLIES, THEN RED WAKE RESPONSE.",
      "2 / F1 STATION: LAUNCH, REPAIR AND REFUEL. EVERY REWARD SAVES AUTOMATICALLY.",
      "3 / W THRUST / A LEFT / D RIGHT / S BRAKE. RELEASE THRUST TO SAVE FUEL.",
      "4 / ORE FIELD AT (0,280). BRAKE WITHIN 85 M AND PRESS E TO MINE.",
      "5 / RETURN WITHIN 85 M OF A STATION, SPEED BELOW 35. F TO DOCK.",
      "6 / F4 MARKET: BUY/SELL FOOD OR PARTS. YOUR HOLD CARRIES 8 UNITS.",
      "7 / F6 OUTFIT: BUY THEN FIT PULSE LASER. TAB TARGET / SPACE FIRE.",
      "8 / DESTROYED? R RECOVERS AT YOUR LAST STATION. CARRIED ORE IS LOST.",
      "9 / F7 GALNET / F2 PROFILE / F8 GRAPHICS / F9 REOPEN THIS GUIDE."
    }};
    for (std::size_t i = 0; i < guide.size(); ++i)
      appendUiLine(batch, layout, 126 + i * 30, false, guide[i], i == 0 ? teal : white);
    appendUiLine(batch, layout, 438, false, ui.career.complete ? "CAREER COMPLETE / THE FRONTIER REMAINS OPEN" :
      ui.missionStage < 2 ? "NEXT / ACCEPT FIRST ORE IN CONTRACTS" :
      ui.career.supply < 2 ? "NEXT / COMPLETE THE CINDER SUPPLY DELIVERY" : "NEXT / EQUIP A LASER AND ANSWER RED WAKE", amber);
  } else if (ui.screen == UiScreen::completion) {
    appendUiLine(batch, layout, 136, false, "KEPLER AUTHORITY / ESTABLISHED PILOT RECOGNITION", teal);
    appendUiLine(batch, layout, 184, false, "YOU SUPPLIED ORION, RESTORED KEPLER SUPPLIES AND ANSWERED RED WAKE.");
    appendUiLine(batch, layout, 222, false, "YOUR THREE CONTRACTS ARE COMPLETE. YOUR COMMANDER IS SAVED.");
    appendUiLine(batch, layout, 270, false, std::to_string(snapshot.credits) + " CR / " +
      std::to_string(snapshot.experience) + " XP / SALVAGE " + std::to_string(ui.salvage), amber);
    appendUiLine(batch, layout, 310, false, standingText(snapshot, "authority.kepler"));
    appendUiLine(batch, layout, 354, false, "THE FRONTIER REMAINS ACTIVE. MINE, TRADE, REFIT AND PATROL.");
    appendUiLine(batch, layout, 386, false, "RECONNECT ANY TIME / YOUR RECOGNITION AND PROGRESSION REMAIN.", muted);
  } else if (ui.screen == UiScreen::error) {
    appendUiLine(batch, layout, 124, false, "ACTION RESULT / NO AUTHORITATIVE STATE CHANGE", amber);
    appendUiLine(batch, layout, 164, false, ui.statusMessage.empty() ? "NO ERROR DETAILS" : ui.statusMessage, white, 880);
  }
  for (const auto& control : buildUiControls(ui)) {
    const auto label = uiButtonLabel(control.id);
    if (label.empty()) continue;
    appendLine(batch, layout, control.rect.x + 8, control.rect.y + 12, 1.35f,
      control.enabled ? white : muted, label, control.rect.width - 16, 64);
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
    "HOLD " + std::to_string(flight::cargoUsed(snapshot.player)) + " / " +
      std::to_string(flight::kCargoCapacity), 300, 32);
  appendLine(batch, layout, 24, 198, 1.25f, snapshot.player.weaponCooldown > 0 ? amber : teal,
    std::string("LASER ") + (snapshot.player.weaponCooldown > 0 ? "RECHARGING" : "READY"), 300, 32);

  appendLine(batch, layout, 368, 108, 1.35f, teal, "MISSION / KEPLER CAREER", 568, 54);
  appendLine(batch, layout, 368, 132, 1.15f, white,
    snapshot.ui.career.complete ? "ESTABLISHED PILOT / FREE PLAY" :
    snapshot.ui.missionStage < 2 ? "FIRST ORE / MINE AT (0,280), RETURN TO KEPLER" :
    snapshot.ui.career.supply < 2 ? "SUPPLY / BUY 2 PARTS AT CINDER, RETURN TO KEPLER" :
    "RED WAKE / FIT LASER, ACCEPT CONTRACT, DEFEAT RAIDER", 568, 86);
  appendLine(batch, layout, 368, 158, 1.15f, muted,
    "POSITION / " + std::to_string(int(snapshot.player.x)) + ", " + std::to_string(int(snapshot.player.y)), 568, 50);
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
  for (const auto& control : buildUiControls(snapshot.ui)) {
    const auto label = uiButtonLabel(control.id);
    if (!label.empty()) appendLine(batch, layout, control.rect.x + 4, control.rect.y + 10,
      1.05f, control.enabled ? white : muted, label, control.rect.width - 8);
  }
  return batch;
}

} // namespace helion::client
