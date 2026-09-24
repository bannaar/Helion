#include "shared/protocol.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
using helion::protocol::Command;
using helion::protocol::FrameKind;
using helion::protocol::LineDecoder;

void check(bool condition, const char* caseName) {
  if (!condition) {
    std::cerr << "FAILED: " << caseName << '\n';
    std::exit(1);
  }
}
}

int main() {
  check(helion::protocol::kVersion == 2, "protocol version");
  check(helion::protocol::compatibleWelcome("WELCOME Helion/2"), "matching protocol accepted");
  check(!helion::protocol::compatibleWelcome("WELCOME Helion/3"), "different protocol rejected");

  LineDecoder partial;
  check(partial.feed("PRO").empty(), "partial frame waits");
  auto frames = partial.feed("FILE\r\n");
  check(frames.size() == 1 && frames[0].kind == FrameKind::line && frames[0].line == "PROFILE", "partial CRLF frame completes");

  LineDecoder multiple;
  frames = multiple.feed("STATE\nPROFILE\nQUIT\n");
  check(frames.size() == 3 && frames[0].line == "STATE" && frames[1].line == "PROFILE" && frames[2].line == "QUIT", "multiple frames in one read");

  LineDecoder malformed;
  frames = malformed.feed(std::string("CHAT bad\0text\nSTATE\n", 20));
  check(frames.size() == 2 && frames[0].kind == FrameKind::malformed && frames[1].line == "STATE", "malformed frame resynchronizes");

  LineDecoder oversized;
  check(oversized.feed(std::string(helion::protocol::kMaxLine, 'x')).empty(), "maximum line waits");
  frames = oversized.feed("x\nSTATE\n");
  check(frames.size() == 2 && frames[0].kind == FrameKind::too_long && frames[1].line == "STATE", "oversized frame discarded once then recovers");

  LineDecoder maximum;
  frames = maximum.feed(std::string(helion::protocol::kMaxLine, 'x') + "\n");
  check(frames.size() == 1 && frames[0].kind == FrameKind::line && frames[0].line.size() == helion::protocol::kMaxLine, "maximum-size line accepted");

  check(helion::protocol::parseRequest("CREATE pilot pass Cmdr Pilot").command == Command::create, "CREATE preserved");
  check(helion::protocol::parseRequest("LOGIN pilot pass").command == Command::login, "LOGIN preserved");
  check(helion::protocol::parseRequest("CHAT hello pilots").command == Command::chat, "CHAT preserved");
  check(helion::protocol::parseRequest("PROFILE").command == Command::profile, "PROFILE preserved");
  check(helion::protocol::parseRequest("STATE").command == Command::state, "STATE preserved");
  check(helion::protocol::parseRequest("GALNET").command == Command::galnet, "GALNET preserved");
  check(helion::protocol::parseRequest("GALNET now").command == Command::invalid, "GALNET arguments rejected");
  check(helion::protocol::parseRequest("QUIT").command == Command::quit, "QUIT preserved");
  check(helion::protocol::parseRequest("REPAIR").command == Command::repair, "REPAIR preserved");
  check(helion::protocol::parseRequest("REFUEL").command == Command::refuel, "REFUEL preserved");
  const auto fire = helion::protocol::parseRequest("FIRE RAIDER-1");
  check(fire.command == Command::fire && fire.first == "RAIDER-1", "FIRE target accepted");
  check(helion::protocol::parseRequest("RECOVER").command == Command::recover, "RECOVER preserved");
  const auto career = helion::protocol::parseRequest("CAREER ACCEPT career.kepler_supply");
  check(career.command == Command::career && career.first == "ACCEPT" &&
    career.second == "career.kepler_supply", "CAREER action accepted");
  check(helion::protocol::parseRequest("CAREER ACCEPT career.unknown").command == Command::invalid,
    "unknown career identifier rejected");
  check(helion::protocol::parseRequest("CAREER TURNIN career.red_wake_response").command == Command::career,
    "career action remains domain-validated");
  check(helion::protocol::parseRequest("OUTFIT LIST").command == Command::outfit, "OUTFIT LIST accepted");
  check(helion::protocol::parseRequest("OUTFIT BUY mining-mk2").command == Command::outfit, "OUTFIT BUY accepted");
  check(helion::protocol::parseRequest("SHIPYARD LIST").command == Command::shipyard, "SHIPYARD LIST accepted");
  const auto shipPurchase = helion::protocol::parseRequest("SHIPYARD BUY TITAN_MULE");
  check(shipPurchase.command == Command::shipyard && shipPurchase.first == "BUY" && shipPurchase.second == "TITAN_MULE",
    "SHIPYARD BUY accepted");
  check(helion::protocol::parseRequest("SHIPYARD SWITCH ship-1234-0002").command == Command::shipyard,
    "SHIPYARD SWITCH accepted");
  const auto engineUpgrade = helion::protocol::parseRequest("UPGRADE engine");
  const auto hullUpgrade = helion::protocol::parseRequest("UPGRADE hull");
  check(engineUpgrade.command == Command::upgrade && engineUpgrade.first == "engine", "engine upgrade accepted");
  check(hullUpgrade.command == Command::upgrade && hullUpgrade.first == "hull", "hull upgrade accepted");
  for (const auto* invalidUpgrade : {"UPGRADE", "UPGRADE engine extra", "UPGRADE warp", "UPGRADE hull extra"})
    check(helion::protocol::parseRequest(invalidUpgrade).command == Command::invalid, "invalid upgrade rejected");
  for (const auto* invalidOutfit : {"OUTFIT", "OUTFIT LIST extra", "OUTFIT BUY", "OUTFIT REMOVE invalid extra"})
    check(helion::protocol::parseRequest(invalidOutfit).command == Command::invalid, "invalid outfit rejected");
  for (const auto* invalidShipyard : {"SHIPYARD", "SHIPYARD LIST extra", "SHIPYARD BUY", "SHIPYARD SELL TITAN_MULE"})
    check(helion::protocol::parseRequest(invalidShipyard).command == Command::invalid, "invalid shipyard rejected");
  for (const auto* invalidCombat : {"FIRE", "FIRE bad target", "FIRE ", "RECOVER extra"})
    check(helion::protocol::parseRequest(invalidCombat).command == Command::invalid, "invalid combat request rejected");
  check(helion::protocol::parseRequest("LOGIN pilot").error == "usage=LOGIN username password", "malformed LOGIN rejected");
  check(helion::protocol::parseRequest("STATE extra").error == "malformed-message", "unexpected arguments rejected");
  check(helion::protocol::parseRequest("").error == "malformed-message", "empty request rejected");
  check(helion::protocol::parseRequest("UNKNOWN").error == "unknown-command", "unknown command rejected");
  check(!helion::protocol::validWireLine("CHAT newline\n"), "embedded newline rejected");

  std::cout << "protocol tests passed\n";
  return 0;
}
