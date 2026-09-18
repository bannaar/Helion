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

 private:
  std::string pending_;
  bool oversized_ = false;
  bool malformed_ = false;
};

enum class Command { create, login, chat, profile, state, quit, galnet, launch, input, flight, mine, dock, contacts, buy, sell, mission, accept, turnin, upgrade, repair, refuel, outfit, fire, recover, invalid };
struct Request {
  Command command = Command::invalid;
  std::string first;
  std::string second;
  std::string payload;
  std::string error;
  int thrust = 0, turn = 0, brake = 0;
};

Request parseRequest(std::string_view line);
bool validWireLine(std::string_view line);
std::string welcomeLine();
bool compatibleWelcome(std::string_view line);

} // namespace helion::protocol
