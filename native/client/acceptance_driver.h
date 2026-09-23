#pragma once

#include "client/render.h"

#include <SDL2/SDL.h>

#include <string>
#include <set>

namespace helion::client {

// Guarded release-acceptance automation. It observes presentation state and
// injects ordinary SDL events; authoritative outcomes still come from the
// server over the production TLS/protocol path.
class AcceptanceDriver {
 public:
  enum class Phase { journey, reconnect, soak };

  AcceptanceDriver(Phase phase, std::string captureDirectory, double soakSeconds = 600.0);
  void tick(const View& view, int windowWidth, int windowHeight, double now);

  bool enabled() const { return !captureDirectory_.empty(); }
  bool done() const { return done_; }
  bool failed() const { return failed_; }
  const std::string& failure() const { return failure_; }
  std::string consumeCapture();

 private:
  void advance(int next, double now);
  void fail(std::string reason);
  void click(const UiState& ui, UiControlId id, int index, int width, int height);
  void type(std::string_view text);
  void tap(SDL_Keycode key);
  void setFlightKeys(bool forward, bool left, bool right, bool brake);
  void navigate(const View& view, double x, double y);
  bool capture(std::string name);

  Phase phase_;
  std::string captureDirectory_;
  int stage_ = 0;
  double stageStarted_ = 0;
  double nextAction_ = 0;
  bool done_ = false;
  bool failed_ = false;
  std::string failure_;
  std::string pendingCapture_;
  std::set<std::string> captured_;
  bool forward_ = false, left_ = false, right_ = false, brake_ = false;
  int baselineCredits_ = -1;
  double soakSeconds_ = 600.0;
  double soakStarted_ = 0;
  int cycles_ = 0;
};

} // namespace helion::client
