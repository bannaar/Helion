#include "client/cockpit.h"

#include "shared/organizations.h"

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
