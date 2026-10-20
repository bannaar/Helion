#include "client/engine_state.h"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
void ready(helion::client::GameStateController& engine) {
  check(engine.beginConnection(), "begin connection");
  check(engine.tlsVerified(), "TLS verified");
  check(engine.protocolReady(), "protocol ready");
}
void login(helion::client::GameStateController& engine) {
  check(engine.authenticationRequested(), "authentication request");
  check(engine.authenticationAccepted(), "server authentication success");
}
}

int main() {
  using namespace helion::client;
  using S = GameEngineState;
  GameStateController engine;
  UiState ui;
  helion::flight::State ship;
  const auto command = [](const char* text) { return helion::protocol::parseRequest(text); };
  check(engine.state() == S::BOOT, "boot");
  check(!engine.protocolReady() && !engine.authenticationAccepted(), "out of order lifecycle rejected");
  check(!engine.navigate(S::SPACE_FLIGHT) && !engine.observeFlight(ship), "no gameplay before login");
  check(engine.beginConnection() && engine.state() == S::CONNECTING, "boot -> connecting");
  check(!engine.beginConnection(), "duplicate connection rejected");
  check(engine.tlsVerified() && engine.state() == S::CONNECTING, "TLS is not protocol ready");
  check(engine.protocolReady() && engine.state() == S::LOGIN, "connecting -> login");
  check(engine.authenticationRequested(), "login attempt");
  engine.authenticationRejected();
  check(engine.state() == S::LOGIN && !engine.authenticated(), "login failure stays unauthenticated");
  check(!engine.allowsCommand(command("LAUNCH")), "no launch before authentication");
  check(engine.allowsCommand(command("COMPANION AUTH test-token")), "companion command can reach server");
  check(!engine.authenticated(), "companion command grants no gameplay session");
  GameStateController companion;
  ready(companion);
  check(companion.companionAuthenticationRequested() && !companion.authenticated(), "companion attempt is not player authentication");
  companion.authenticationRejected();
  check(companion.session() == SessionState::unauthenticated, "companion failure permits retry");
  check(companion.companionAuthenticationRequested() && companion.companionAuthenticationAccepted(), "scoped companion session");
  check(companion.allowsCommand(command("PROFILE")) && !companion.authenticated() &&
        !companion.allowsCommand(command("LAUNCH")) && !companion.allowsCommand(command("INPUT 1 0 0")) &&
        !companion.allowsCommand(command("BUY food 1")) && !companion.navigate(S::STATION_SERVICES) &&
        !companion.observeFlight(ship), "companion profile reads remain possible without gameplay authority");
  companion.fatal("ERR companion-token-expired");
  check(!companion.allowsCommand(command("PROFILE")) && !companion.authenticated(), "expired scoped session fails closed");
  GameStateController demoted;
  ready(demoted); login(demoted);
  auto flying = ship; flying.docked = false;
  check(demoted.observeFlight(flying) && demoted.allowsCommand(command("COMPANION AUTH test-token")), "player can request scoped demotion");
  demoted.input() = {true, true, true, true, true};
  check(demoted.companionAuthenticationRequested() && !demoted.flightInputAllowed() &&
        !demoted.input().heldForward, "pending demotion inhibits gameplay and clears latches");
  demoted.authenticationRejected();
  check(demoted.authenticated() && demoted.state() == S::SPACE_FLIGHT, "rejected token leaves confirmed player session intact");
  check(demoted.companionAuthenticationRequested() && demoted.companionAuthenticationAccepted() &&
        !demoted.authenticated() && demoted.flightMode() == AuthoritativeFlightMode::unknown &&
        demoted.allowsCommand(command("PROFILE")) && !demoted.allowsCommand(command("LAUNCH")),
        "successful scoped demotion removes all local player authority");
  login(engine);
  check(engine.state() == S::ORBIT_VIEW && engine.flightMode() == AuthoritativeFlightMode::unknown,
        "login does not guess dock state");
  check(!engine.navigate(S::STATION_MARKET) && !engine.navigate(S::SPACE_FLIGHT), "unknown flight mode fails closed");
  check(engine.observeFlight(ship) && engine.state() == S::STATION_SERVICES, "server docked snapshot");
  check(!engine.navigate(S::SPACE_FLIGHT), "docked hotkey cannot manufacture flight");
  check(engine.allowsCommand(command("LAUNCH")) && engine.state() == S::STATION_SERVICES && ship.docked,
        "launch request alone cannot transition or mutate ship");
  ship.docked = false;
  check(engine.observeFlight(ship) && engine.state() == S::SPACE_FLIGHT, "authoritative launch snapshot -> flight");
  check(engine.flightInputAllowed(), "flight input enabled");
  check(!engine.navigate(S::STATION_SERVICES) && !engine.navigate(S::STATION_CAREER) &&
        !engine.navigate(S::STATION_MARKET) && !engine.navigate(S::STATION_OUTFITTING) &&
        !engine.navigate(S::STATION_SHIPYARD), "station views guarded in space");
  check(!engine.allowsCommand(command("BUY food 1")) && !engine.allowsCommand(command("LAUNCH")), "station mutations guarded");
  check(engine.allowsCommand(command("DOCK")) && engine.state() == S::SPACE_FLIGHT && !ship.docked,
        "dock request alone cannot transition or mutate ship");
  engine.input() = {true, true, true, true, true};
  check(engine.navigate(S::OPTIONS) && !engine.flightInputAllowed() && !engine.input().heldForward &&
        !engine.input().heldLeft && !engine.input().heldRight && !engine.input().heldBrake && !engine.input().thrust,
        "leaving flight clears every latch");
  check(!engine.allowsCommand(command("MINE")) && !engine.allowsCommand(command("INPUT 1 1 0")), "modal cannot send flight actions");
  check(engine.allowsCommand(command("INPUT 0 0 1")), "neutral liveness preserved");
  check(engine.back() && engine.state() == S::SPACE_FLIGHT, "options returns to flight");
  ship.docked = true;
  check(engine.observeFlight(ship) && engine.state() == S::STATION_SERVICES, "authoritative dock -> station");
  check(engine.navigate(S::STATION_MARKET), "market entry");
  engine.actionError("ERR insufficient-credits");
  engine.project(ui);
  check(engine.state() == S::ACTION_ERROR && ui.screen == UiScreen::error, "action error projection");
  check(engine.back() && engine.state() == S::STATION_MARKET, "error returns to market");
  check(engine.navigate(S::OPTIONS) && engine.navigate(S::HELP), "nested bounded modals");
  check(engine.back() && engine.state() == S::OPTIONS, "help back -> options");
  check(engine.back() && engine.state() == S::STATION_MARKET, "options back -> market");
  check(engine.back() && engine.state() == S::STATION_SERVICES, "station subview back -> services");
  check(engine.navigate(S::OPTIONS), "modal before authoritative launch");
  ship.docked = false;
  check(engine.observeFlight(ship) && engine.state() == S::OPTIONS, "server update retains modal");
  check(engine.back() && engine.state() == S::SPACE_FLIGHT, "back never restores stale station context");
  engine.input() = {true, true, true, true, true};
  ship.hull = 0; ship.destroyed = true;
  check(engine.observeFlight(ship) && engine.flightMode() == AuthoritativeFlightMode::recovery &&
        !engine.flightInputAllowed() && !engine.input().heldForward, "destruction disables and clears flight input");
  check(engine.allowsCommand(command("RECOVER")) && !engine.allowsCommand(command("FIRE RAIDER-1")), "recovery routing");
  ship.hull = ship.maxHull; ship.destroyed = false; ship.docked = true;
  check(engine.observeFlight(ship) && engine.state() == S::STATION_SERVICES, "authoritative recovery");
  engine.disconnected("transport lost");
  engine.project(ui);
  check(engine.state() == S::DISCONNECTED && !engine.authenticated() && !engine.connected() &&
        engine.flightMode() == AuthoritativeFlightMode::unknown && ui.reconnectAvailable, "disconnect forgets session truth");
  check(!engine.navigate(S::LOGIN) && !engine.protocolReady(), "no implicit reconnect");
  ready(engine); login(engine);
  check(engine.state() == S::ORBIT_VIEW, "retry waits for fresh server state");
  check(engine.observeFlight(ship) && engine.state() == S::STATION_SERVICES, "reconnect docked");
  engine.disconnected("retry"); ready(engine); login(engine);
  ship.docked = false;
  check(engine.observeFlight(ship) && engine.state() == S::SPACE_FLIGHT, "reconnect in flight");
  engine.input() = {true, true, true, true, true};
  engine.disconnected("loss while flying");
  check(!engine.input().heldForward && !engine.input().heldLeft && !engine.input().heldRight &&
        !engine.input().heldBrake && !engine.input().thrust, "transport loss clears latches");
  ready(engine); login(engine);
  ship.docked = true; ship.station = -1;
  check(!engine.observeFlight(ship) && engine.state() == S::FATAL_ERROR && !engine.authenticated(), "inconsistent docking fails closed");
  check(!engine.beginConnection() && !engine.allowsCommand(command("STATE")), "fatal state is terminal");
  GameStateController malformed;
  ready(malformed); login(malformed);
  ship.station = 0; ship.x = std::numeric_limits<double>::quiet_NaN();
  check(!malformed.observeFlight(ship), "nonfinite state fails closed");
  std::cout << "Engine lifecycle, guards, modal return, authority, retry and input safety passed\n";
}
