#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace helion::protocol {

inline constexpr int kVersion = 2;
inline constexpr std::size_t kMaxLine = 4096; // Bytes before LF, including an optional CR.

enum class FrameKind { line, too_long, malformed };
struct Frame {
  FrameKind kind;
  std::string line;
};

class LineDecoder {
 public:
  std::vector<Frame> feed(std::string_view bytes);
  bool atMessageBoundary() const { return pending_.empty() && !oversized_ && !malformed_; }

 private:
  std::string pending_;
  bool oversized_ = false;
  bool malformed_ = false;
};

enum class Command { protocol, create, login, companion_issue, companion_auth, companion_list, companion_revoke, chat, profile, state, quit, galnet, economy, galaxy, launch, input, flight, mine, dock, contacts, buy, sell, mission, accept, turnin, upgrade, repair, refuel, outfit, shipyard, fire, recover, career, invalid };
struct Request {
  Command command = Command::invalid;
  std::string first;
  std::string second;
  std::string payload;
  std::string error;
  int protocolVersion = 0;
  int thrust = 0, turn = 0, brake = 0;
};

Request parseRequest(std::string_view line);
bool validWireLine(std::string_view line);
std::string welcomeLine();
bool compatibleWelcome(std::string_view line);

} // namespace helion::protocol
