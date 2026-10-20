#include "client/engine_state.h"

#include <cmath>

namespace helion::client {
namespace {
bool stationOnly(GameEngineState state) {
  return state == GameEngineState::STATION_SERVICES || state == GameEngineState::STATION_CAREER ||
    state == GameEngineState::STATION_MARKET || state == GameEngineState::STATION_OUTFITTING ||
    state == GameEngineState::STATION_SHIPYARD;
}
bool modal(GameEngineState state) {
  return state == GameEngineState::GALNET || state == GameEngineState::OPTIONS ||
    state == GameEngineState::PROFILE || state == GameEngineState::GRAPHICS ||
    state == GameEngineState::HELP || state == GameEngineState::COMPLETION ||
    state == GameEngineState::ACTION_ERROR;
}
}

bool GameStateController::connected() const {
  return transport_ == TransportState::tlsVerified || transport_ == TransportState::ready;
}
bool GameStateController::flightInputAllowed() const {
  return commandReady() && authenticated() && flightMode_ == AuthoritativeFlightMode::inFlight &&
    state_ == GameEngineState::SPACE_FLIGHT;
}
void GameStateController::enter(GameEngineState next) {
  if (state_ == next) return;
  state_ = next;
  ++revision_;
  if (!flightInputAllowed()) clearFlightInput();
}
void GameStateController::forgetSession() {
  session_ = SessionState::unauthenticated;
  flightMode_ = AuthoritativeFlightMode::unknown;
  returns_.clear();
  clearFlightInput();
}
bool GameStateController::beginConnection() {
  if (state_ != GameEngineState::BOOT && state_ != GameEngineState::DISCONNECTED) return false;
  forgetSession();
  message_.clear();
  transport_ = TransportState::connecting;
  enter(GameEngineState::CONNECTING);
  return true;
}
bool GameStateController::tlsVerified() {
  if (transport_ != TransportState::connecting) return false;
  transport_ = TransportState::tlsVerified;
  return true;
}
bool GameStateController::protocolReady() {
  if (transport_ != TransportState::tlsVerified) return false;
  transport_ = TransportState::ready;
  enter(GameEngineState::LOGIN);
  return true;
}
bool GameStateController::authenticationRequested() {
  if (!commandReady() || state_ != GameEngineState::LOGIN ||
      session_ != SessionState::unauthenticated) return false;
  session_ = SessionState::authenticating;
  message_.clear();
  return true;
}
bool GameStateController::authenticationAccepted() {
  if (!commandReady() || session_ != SessionState::authenticating) return false;
  session_ = SessionState::authenticated;
  // No use of the default-constructed ship's docked flag at login/reconnect.
  enter(basePresentation());
  return true;
}
void GameStateController::authenticationRejected() {
  if (session_ == SessionState::playerAuthenticatingCompanion) {
    // The server leaves the existing player session intact when token auth fails.
    session_ = SessionState::authenticated;
    enter(basePresentation());
    return;
  }
  if (session_ != SessionState::authenticating && session_ != SessionState::companionAuthenticating) return;
  forgetSession();
  enter(GameEngineState::LOGIN);
}
bool GameStateController::companionAuthenticationRequested() {
  if (!commandReady() || (session_ != SessionState::unauthenticated &&
                         session_ != SessionState::authenticated)) return false;
  session_ = session_ == SessionState::authenticated ?
    SessionState::playerAuthenticatingCompanion : SessionState::companionAuthenticating;
  returns_.clear();
  clearFlightInput();
  enter(GameEngineState::LOGIN);
  return true;
}
bool GameStateController::companionAuthenticationAccepted() {
  if (!commandReady() || (session_ != SessionState::companionAuthenticating &&
                         session_ != SessionState::playerAuthenticatingCompanion)) return false;
  session_ = SessionState::companionReadOnly;
  flightMode_ = AuthoritativeFlightMode::unknown;
  returns_.clear();
  clearFlightInput();
  return true;
}
GameEngineState GameStateController::basePresentation() const {
  if (!authenticated()) return GameEngineState::LOGIN;
  if (flightMode_ == AuthoritativeFlightMode::docked) return GameEngineState::STATION_SERVICES;
  if (flightMode_ == AuthoritativeFlightMode::unknown) return GameEngineState::ORBIT_VIEW;
  return GameEngineState::SPACE_FLIGHT; // disabled ship uses existing recovery cockpit
}
bool GameStateController::permitted(GameEngineState next) const {
  if (!commandReady() || !authenticated()) return false;
  if (stationOnly(next)) return flightMode_ == AuthoritativeFlightMode::docked;
  if (next == GameEngineState::SPACE_FLIGHT)
    return flightMode_ == AuthoritativeFlightMode::inFlight || flightMode_ == AuthoritativeFlightMode::recovery;
  if (next == GameEngineState::ORBIT_VIEW) return flightMode_ == AuthoritativeFlightMode::unknown;
  return modal(next);
}
bool GameStateController::navigate(GameEngineState next) {
  if (!permitted(next)) return false;
  if (next == state_) return true;
  if (modal(next)) {
    if (returns_.size() == 8) returns_.erase(returns_.begin());
    returns_.push_back(state_);
  } else returns_.clear();
  message_.clear();
  enter(next);
  return true;
}
bool GameStateController::back() {
  if (!commandReady() || !authenticated()) return false;
  while (!returns_.empty()) {
    const auto next = returns_.back();
    returns_.pop_back();
    if (permitted(next)) { message_.clear(); enter(next); return true; }
  }
  message_.clear();
  enter(basePresentation());
  return true;
}
bool GameStateController::observeFlight(const flight::State& ship) {
  if (!commandReady() || !authenticated()) return false;
  if (!std::isfinite(ship.x) || !std::isfinite(ship.y) || ship.maxHull <= 0 ||
      ship.hull < 0 || ship.hull > ship.maxHull || ship.destroyed != (ship.hull == 0) ||
      (ship.destroyed && ship.docked) ||
      (ship.docked && (ship.station < 0 || ship.station >= static_cast<int>(flight::kStations.size())))) {
    fatal("INCONSISTENT AUTHORITATIVE FLIGHT STATE");
    return false;
  }
  const auto previous = flightMode_;
  flightMode_ = ship.destroyed ? AuthoritativeFlightMode::recovery :
    ship.docked ? AuthoritativeFlightMode::docked : AuthoritativeFlightMode::inFlight;
  if (!flightInputAllowed()) clearFlightInput();
  if (previous != flightMode_) {
    // Modal context survives, but back() validates it against the new truth.
    if (!modal(state_)) { returns_.clear(); enter(basePresentation()); }
  }
  return true;
}
void GameStateController::actionError(std::string message) {
  if (navigate(GameEngineState::ACTION_ERROR)) message_ = message.substr(0, 220);
}
void GameStateController::disconnected(std::string reason) {
  if (state_ == GameEngineState::FATAL_ERROR) return;
  transport_ = TransportState::lost;
  forgetSession();
  message_ = reason.substr(0, 220);
  enter(GameEngineState::DISCONNECTED);
}
void GameStateController::fatal(std::string reason) {
  transport_ = TransportState::failed;
  forgetSession();
  message_ = reason.substr(0, 220);
  enter(GameEngineState::FATAL_ERROR);
}

bool GameStateController::allowsCommand(const protocol::Request& request) const {
  using C = protocol::Command;
  if (!commandReady()) return false;
  if (request.command == C::quit || request.command == C::state) return true;
  if (request.command == C::login || request.command == C::create)
    return state_ == GameEngineState::LOGIN && session_ == SessionState::unauthenticated;
  // Companion auth remains scoped server-side and never authenticates gameplay.
  if (request.command == C::companion_auth)
    return session_ == SessionState::unauthenticated || session_ == SessionState::authenticated;
  if (session_ == SessionState::companionReadOnly) return request.command == C::profile;
  if (!authenticated()) return false;
  switch (request.command) {
    case C::input:
      // Preserve M4's neutral INPUT liveness in menus, station and recovery.
      return flightInputAllowed() || (!request.thrust && !request.turn && request.brake);
    case C::launch: case C::buy: case C::sell: case C::accept: case C::turnin:
    case C::upgrade: case C::repair: case C::refuel:
      return flightMode_ == AuthoritativeFlightMode::docked && stationOnly(state_);
    case C::outfit: case C::shipyard:
      return request.first == "LIST" ||
        (flightMode_ == AuthoritativeFlightMode::docked && stationOnly(state_));
    case C::mine: case C::dock: case C::fire: return flightInputAllowed();
    case C::recover:
      return state_ == GameEngineState::SPACE_FLIGHT && flightMode_ == AuthoritativeFlightMode::recovery;
    case C::career:
      return request.first.empty() || request.first == "DISMISS" ||
        (flightMode_ == AuthoritativeFlightMode::docked && stationOnly(state_));
    case C::protocol: case C::invalid: return false;
    default: return true; // existing authenticated read models/chat/token management
  }
}

void GameStateController::project(UiState& ui) const {
  if (ui.controllerRevision != revision_) {
    if (ui.screen == UiScreen::error && engineUiScreen(state_) != UiScreen::error)
      ui.statusMessage.clear();
    ui.selected = 0; ui.scrollOffset = 0;
    ui.focusedControl = UiControlId::none;
    ui.hoveredControl = UiControlId::none; ui.hoveredIndex = -1;
    ui.controllerRevision = revision_;
  }
  ui.screen = engineUiScreen(state_);
  ui.connected = commandReady();
  ui.authenticated = authenticated();
  ui.playerLoginAvailable = session_ == SessionState::unauthenticated;
  ui.reconnectAvailable = state_ == GameEngineState::DISCONNECTED;
  ui.docked = flightMode_ == AuthoritativeFlightMode::docked;
  ui.destroyed = flightMode_ == AuthoritativeFlightMode::recovery;
  ui.connectionStatus = state_ == GameEngineState::DISCONNECTED ? "DISCONNECTED / ENTER OR R TO RETRY" :
    state_ == GameEngineState::FATAL_ERROR ? "FATAL ERROR / CLOSE AND RESTART CLIENT" :
    !commandReady() ? "CONNECTING / TLS AND PROTOCOL VERIFICATION" :
    session_ == SessionState::companionReadOnly ? "TLS VERIFIED / COMPANION / PROFILE READ ONLY" :
    !authenticated() ? "TLS VERIFIED / AWAITING COMMANDER LOGIN" :
    flightMode_ == AuthoritativeFlightMode::unknown ? "AUTHENTICATED / AWAITING SERVER FLIGHT STATE" :
    "TLS VERIFIED / COMMANDER AUTHENTICATED";
  if (!message_.empty()) ui.statusMessage = message_;
}

UiScreen engineUiScreen(GameEngineState state) {
  switch (state) {
    case GameEngineState::BOOT: case GameEngineState::CONNECTING: case GameEngineState::LOGIN:
    case GameEngineState::ORBIT_VIEW: return UiScreen::account;
    case GameEngineState::STATION_SERVICES: return UiScreen::station;
    case GameEngineState::STATION_CAREER: return UiScreen::mission;
    case GameEngineState::STATION_MARKET: return UiScreen::market;
    case GameEngineState::STATION_OUTFITTING: return UiScreen::outfitting;
    case GameEngineState::STATION_SHIPYARD: return UiScreen::shipyard;
    case GameEngineState::SPACE_FLIGHT: return UiScreen::flight;
    case GameEngineState::GALNET: return UiScreen::galnet;
    case GameEngineState::OPTIONS: return UiScreen::options;
    case GameEngineState::PROFILE: return UiScreen::profile;
    case GameEngineState::GRAPHICS: return UiScreen::graphics;
    case GameEngineState::HELP: return UiScreen::help;
    case GameEngineState::COMPLETION: return UiScreen::completion;
    case GameEngineState::ACTION_ERROR: case GameEngineState::DISCONNECTED:
    case GameEngineState::FATAL_ERROR: return UiScreen::error;
  }
  return UiScreen::error;
}
const char* engineStateName(GameEngineState state) {
  switch (state) {
#define HELION_STATE_NAME(name) case GameEngineState::name: return #name
    HELION_STATE_NAME(BOOT); HELION_STATE_NAME(CONNECTING); HELION_STATE_NAME(LOGIN);
    HELION_STATE_NAME(ORBIT_VIEW); HELION_STATE_NAME(STATION_SERVICES); HELION_STATE_NAME(STATION_CAREER);
    HELION_STATE_NAME(STATION_MARKET); HELION_STATE_NAME(STATION_OUTFITTING); HELION_STATE_NAME(STATION_SHIPYARD);
    HELION_STATE_NAME(SPACE_FLIGHT); HELION_STATE_NAME(GALNET); HELION_STATE_NAME(OPTIONS);
    HELION_STATE_NAME(PROFILE); HELION_STATE_NAME(GRAPHICS); HELION_STATE_NAME(HELP);
    HELION_STATE_NAME(COMPLETION); HELION_STATE_NAME(ACTION_ERROR); HELION_STATE_NAME(DISCONNECTED);
    HELION_STATE_NAME(FATAL_ERROR);
#undef HELION_STATE_NAME
  }
  return "FATAL_ERROR";
}
} // namespace helion::client
