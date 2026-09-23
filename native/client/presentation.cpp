#include "client/presentation.h"

#include "client/render.h"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace helion::client {
namespace {
bool playerFacingMessage(std::string_view line) {
  constexpr std::string_view routinePrefixes[] = {
    "CONTACT ", "CONTACTS ", "PROFILE ", "CAREER ", "LOADOUT ",
    "FLIGHT ", "FUEL ", "STATE ", "GALNET id="
  };
  if (line == "GALNET END" || line == "WELCOME Helion/2") return false;
  for (const auto prefix : routinePrefixes)
    if (line.rfind(prefix, 0) == 0) return false;
  return !line.empty();
}

PresentationColor colorFor(PresentationClass classification) {
  switch (classification) {
    case PresentationClass::station: return {0.95f, 0.65f, 0.25f};
    case PresentationClass::asteroid: return {0.30f, 0.72f, 0.68f};
    case PresentationClass::hauler: return {0.95f, 0.65f, 0.25f};
    case PresentationClass::commander: return {0.30f, 0.85f, 0.76f};
    case PresentationClass::hostile: return {1.0f, 0.22f, 0.22f};
  }
  return {};
}
}

PresentationSnapshot makePresentationSnapshot(const View& view) {
  PresentationSnapshot snapshot;
  snapshot.player = view.ship;
  snapshot.credits = view.credits;
  snapshot.experience = view.experience;
  snapshot.reputation = view.reputation;
  snapshot.commanderName = view.commanderName;
  snapshot.missionSummary = view.missionSummary;
  snapshot.selectedTarget = view.targetId;
  snapshot.miningActive = view.time < view.beamUntil;
  snapshot.weaponFired = view.time < view.weaponUntil;
  snapshot.incomingDamage = view.time < view.damageUntil;
  snapshot.authenticated = view.authenticated;
  snapshot.connected = view.connected;
  snapshot.time = view.time;
  snapshot.ui = view.ui;
  snapshot.ui.consoleOpen = view.console;
  snapshot.ui.telemetryEnabled = view.showTelemetry;
  snapshot.ui.connected = view.connected;
  snapshot.ui.authenticated = view.authenticated;
  snapshot.ui.typed = maskedCommand(view.typed);
  populateUiDerived(snapshot.ui, snapshot.player);
  if (view.ship.docked && view.ship.station >= 0 &&
      view.ship.station < static_cast<int>(flight::kStations.size()))
    snapshot.stationContext = flight::kStations[static_cast<std::size_t>(view.ship.station)].name;
  else snapshot.stationContext = "KEPLER SYSTEM";
  for (const auto& station : flight::kStations)
    snapshot.stations.push_back({station.name, station.x, station.y, colorFor(PresentationClass::station)});
  for (const auto& rock : flight::kRocks)
    snapshot.asteroids.push_back({rock.x, rock.y, rock.radius, false});
  snapshot.contacts.reserve(view.contacts.size());
  for (const auto& contact : view.contacts) {
    if (snapshot.contacts.size() >= kMaxPresentationContacts) break;
    PresentationClass classification = PresentationClass::commander;
    if (contact.hostile) classification = PresentationClass::hostile;
    else if (contact.kind == "hauler") classification = PresentationClass::hauler;
    snapshot.contacts.push_back({contact.id, contact.kind, contact.affiliationId, contact.affiliationName,
      classification, colorFor(classification), contact.x, contact.y, contact.yaw,
      contact.hull, contact.maxHull, contact.docked, contact.hostile, contact.id == view.targetId});
  }
  snapshot.recentMessages.reserve(12);
  for (auto line = view.log.rbegin(); line != view.log.rend() && snapshot.recentMessages.size() < 12; ++line)
    if (playerFacingMessage(*line)) snapshot.recentMessages.push_back(*line);
  std::reverse(snapshot.recentMessages.begin(), snapshot.recentMessages.end());
  return snapshot;
}

CameraFrame cameraFrame(int width, int height) {
  const int safeWidth = std::max(width, 1);
  const int safeHeight = std::max(height, 1);
  CameraFrame result;
  result.aspect = static_cast<float>(safeWidth) / static_cast<float>(safeHeight);
  result.halfHeight = 300.0f;
  result.halfWidth = result.halfHeight * result.aspect;
  return result;
}

float clampHudValue(double value, double maximum) {
  if (!std::isfinite(value) || !std::isfinite(maximum) || maximum <= 0) return 0.0f;
  return static_cast<float>(std::clamp(value / maximum, 0.0, 1.0));
}

TargetIndicator targetIndicator(const flight::State& player, const PresentationContact* target) {
  TargetIndicator result;
  if (!target) return result;
  const double dx = target->x - player.x;
  const double dy = target->y - player.y;
  const double distance = std::hypot(dx, dy);
  if (!std::isfinite(distance) || distance <= 0.0001) return result;
  result.directionX = static_cast<float>(dx / distance);
  result.directionY = static_cast<float>(dy / distance);
  result.distance = static_cast<float>(distance);
  result.visible = true;
  return result;
}

bool leftTurnInput() {
  return flight::controls(false, true, false, false, true).turn == 1;
}

bool rightTurnInput() {
  return flight::controls(false, false, true, false, true).turn == -1;
}

} // namespace helion::client
