#pragma once

#include "shared/flight.h"
#include "client/ui.h"

#include <array>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace helion::client {

struct View;

inline constexpr std::size_t kMaxPresentationContacts = 64;

enum class PresentationClass { station, asteroid, hauler, commander, hostile };

struct PresentationColor {
  float r = 1.0f;
  float g = 1.0f;
  float b = 1.0f;
};

struct PresentationStation {
  std::string name;
  double x = 0;
  double y = 0;
  PresentationColor color;
};

struct PresentationAsteroid {
  double x = 0;
  double y = 0;
  double radius = 0;
  bool depleted = false;
};

struct PresentationContact {
  std::string id;
  std::string kind;
  std::string affiliationId;
  std::string affiliationName;
  PresentationClass classification = PresentationClass::commander;
  PresentationColor color;
  double x = 0;
  double y = 0;
  double yaw = 0;
  int hull = 0;
  int maxHull = 0;
  bool docked = false;
  bool hostile = false;
  bool selected = false;
};

struct PresentationSnapshot {
  flight::State player;
  int credits = 0;
  int experience = 0;
  std::map<std::string, int> reputation;
  std::string commanderName;
  std::string stationContext;
  std::string missionSummary;
  std::vector<PresentationStation> stations;
  std::vector<PresentationAsteroid> asteroids;
  std::vector<PresentationContact> contacts;
  std::string selectedTarget;
  bool miningActive = false;
  bool weaponFired = false;
  bool incomingDamage = false;
  bool authenticated = false;
  bool connected = false;
  double time = 0;
  std::vector<std::string> recentMessages;
  UiState ui;
};

struct CameraFrame {
  float aspect = 1.0f;
  float halfHeight = 300.0f;
  float halfWidth = 300.0f;
};

struct TargetIndicator {
  float directionX = 0;
  float directionY = 1;
  float distance = 0;
  bool visible = false;
};

PresentationSnapshot makePresentationSnapshot(const View& view);
CameraFrame cameraFrame(int width, int height);
float clampHudValue(double value, double maximum);
TargetIndicator targetIndicator(const flight::State& player, const PresentationContact* target);
bool leftTurnInput();
bool rightTurnInput();

} // namespace helion::client
