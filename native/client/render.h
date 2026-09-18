#pragma once
#include "shared/flight.h"
#include <string>
#include <map>
#include <vector>

namespace helion::client {
struct View {
  flight::State ship;
  int credits = 1500, experience = 0;
  bool connected = false, authenticated = false, console = true, thrust = false;
  double time = 0, beamUntil = 0;
  std::string typed;
  std::vector<std::string> log;
  std::vector<flight::Contact> contacts;
  std::string targetId;
  std::map<std::string, int> reputation;
  bool showTelemetry = true;
};
// All visual assets are original, procedural fixed-function geometry.
void render(int width, int height, const View& view);
std::string redactCommand(const std::string& command);
} // namespace helion::client
