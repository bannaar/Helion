#pragma once

#include "client/ui.h"
#include "shared/protocol.h"

#include <cstdint>
#include <string>
#include <vector>

namespace helion::client {

enum class TransportState { offline, connecting, tlsVerified, ready, lost, failed };
enum class SessionState {
  unauthenticated, authenticating, authenticated,
  companionAuthenticating, playerAuthenticatingCompanion, companionReadOnly
};
enum class AuthoritativeFlightMode { unknown, docked, inFlight, recovery };
enum class GameEngineState {
  BOOT, CONNECTING, LOGIN, ORBIT_VIEW, STATION_SERVICES, STATION_CAREER,
  STATION_MARKET, STATION_OUTFITTING, STATION_SHIPYARD, SPACE_FLIGHT,
  GALNET, OPTIONS, PROFILE, GRAPHICS, HELP, COMPLETION, ACTION_ERROR,
  DISCONNECTED, FATAL_ERROR
};

struct FlightInputLatch {
  bool heldForward = false, heldLeft = false, heldRight = false, heldBrake = false;
  bool thrust = false;
};

// Local navigation and input policy only. No socket, credentials, mutable ship,
// inventory, persistence, simulation, or command sender belongs to this class.
class GameStateController {
 public:
  GameEngineState state() const { return state_; }
  TransportState transport() const { return transport_; }
  SessionState session() const { return session_; }
  AuthoritativeFlightMode flightMode() const { return flightMode_; }
  bool connected() const;
  bool commandReady() const { return transport_ == TransportState::ready; }
  bool authenticated() const { return session_ == SessionState::authenticated; }
  bool flightInputAllowed() const;
  bool allowsCommand(const protocol::Request&) const;

  bool beginConnection(); // BOOT or explicit retry from DISCONNECTED only
  bool tlsVerified();
  bool protocolReady();
  bool authenticationRequested();
  bool authenticationAccepted();
  bool companionAuthenticationRequested();
  bool companionAuthenticationAccepted();
  void authenticationRejected();
  bool observeFlight(const flight::State&); // validated server data only
  bool navigate(GameEngineState);
  bool back();
  void actionError(std::string message);
  void disconnected(std::string reason);
  void fatal(std::string reason);
  void clearFlightInput() { input_ = {}; }
  FlightInputLatch& input() { return input_; }
  const FlightInputLatch& input() const { return input_; }
  void project(UiState&) const;

 private:
  GameEngineState state_ = GameEngineState::BOOT;
  TransportState transport_ = TransportState::offline;
  SessionState session_ = SessionState::unauthenticated;
  AuthoritativeFlightMode flightMode_ = AuthoritativeFlightMode::unknown;
  std::vector<GameEngineState> returns_; // bounded local modal history
  FlightInputLatch input_;
  std::string message_;
  std::uint64_t revision_ = 1;
  GameEngineState basePresentation() const;
  bool permitted(GameEngineState) const;
  void enter(GameEngineState);
  void forgetSession();
};

UiScreen engineUiScreen(GameEngineState);
const char* engineStateName(GameEngineState);

} // namespace helion::client
