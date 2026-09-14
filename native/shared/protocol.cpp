#include "shared/protocol.h"

#include <sstream>
#include <utility>

namespace helion::protocol {

bool validWireLine(std::string_view line) {
  if (line.size() > kMaxLine) return false;
  for (unsigned char ch : line) {
    if (ch == '\n' || ch == '\r' || ch == '\0' || (ch < 0x20 && ch != '\t') || ch == 0x7f) return false;
  }
  return true;
}

std::string welcomeLine() { return "WELCOME Helion/" + std::to_string(kVersion); }

bool compatibleWelcome(std::string_view line) { return line == welcomeLine(); }

std::vector<Frame> LineDecoder::feed(std::string_view bytes) {
  std::vector<Frame> frames;
  for (unsigned char ch : bytes) {
    if (ch == '\n') {
      if (oversized_) frames.push_back({FrameKind::too_long, {}});
      else if (malformed_) frames.push_back({FrameKind::malformed, {}});
      else {
        if (!pending_.empty() && pending_.back() == '\r') pending_.pop_back();
        frames.push_back({FrameKind::line, std::move(pending_)});
      }
      pending_.clear();
      oversized_ = false;
      malformed_ = false;
    } else if (!oversized_) {
      if (pending_.size() == kMaxLine) {
        pending_.clear();
        oversized_ = true;
      } else {
        if (ch == '\0' || (ch < 0x20 && ch != '\t' && ch != '\r') || ch == 0x7f ||
            (ch == '\r' && !pending_.empty() && pending_.back() == '\r')) malformed_ = true;
        if (!pending_.empty() && pending_.back() == '\r') malformed_ = true;
        pending_.push_back(static_cast<char>(ch));
      }
    }
  }
  return frames;
}

Request parseRequest(std::string_view line) {
  Request request;
  if (!validWireLine(line)) { request.error = "malformed-message"; return request; }
  std::istringstream input{std::string(line)};
  std::string command;
  input >> command;
  if (command == "CREATE" || command == "LOGIN") {
    input >> request.first >> request.second;
    std::getline(input, request.payload);
    if (!request.payload.empty() && request.payload.front() == ' ') request.payload.erase(0, 1);
    if (request.first.empty() || request.second.empty() || request.first.size() > 32 ||
        request.second.size() > 128 || request.first.find_first_of("\t\r ") != std::string::npos ||
        (command == "CREATE" && (request.payload.empty() || request.payload.size() > 64)) ||
        (command == "LOGIN" && !request.payload.empty())) {
      request.error = command == "CREATE" ? "usage=CREATE username password display" : "usage=LOGIN username password";
      return request;
    }
    request.command = command == "CREATE" ? Command::create : Command::login;
  } else if (command == "CHAT") {
    std::getline(input, request.payload);
    if (!request.payload.empty() && request.payload.front() == ' ') request.payload.erase(0, 1);
    if (request.payload.empty() || request.payload.size() > 512) {
      request.error = "usage=CHAT message";
      return request;
    }
    request.command = Command::chat;
  } else if (command == "PROFILE" || command == "STATE" || command == "QUIT") {
    std::string extra;
    if (input >> extra) { request.error = "malformed-message"; return request; }
    request.command = command == "PROFILE" ? Command::profile : command == "STATE" ? Command::state : Command::quit;
  } else {
    request.error = command.empty() ? "malformed-message" : "unknown-command";
  }
  return request;
}

} // namespace helion::protocol
