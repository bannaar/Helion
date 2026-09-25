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
  } else if (command == "CAREER") {
    std::string extra;
    if (input >> request.first) {
      if (request.first == "DISMISS") {
        if (input >> extra) { request.error = "malformed-message"; return request; }
      } else if ((request.first != "ACCEPT" && request.first != "TURNIN") ||
                 !(input >> request.second) || (input >> extra) ||
                 (request.second != "career.kepler_supply" && request.second != "career.red_wake_response")) {
        request.error = "invalid-career-request"; return request;
      }
    }
    request.command = Command::career;
  } else if (command == "CHAT") {
    std::getline(input, request.payload);
    if (!request.payload.empty() && request.payload.front() == ' ') request.payload.erase(0, 1);
    if (request.payload.empty() || request.payload.size() > 512) {
      request.error = "usage=CHAT message";
      return request;
    }
    request.command = Command::chat;
  } else if (command == "MISSION" || command == "ACCEPT" || command == "TURNIN" || command == "UPGRADE" || command == "REPAIR" || command == "REFUEL") {
    std::string extra;
    if (command == "UPGRADE") {
      if (!(input >> request.first) || (input >> extra) ||
          (request.first != "engine" && request.first != "hull")) {
        request.error = "usage=UPGRADE engine|hull";
        return request;
      }
      request.command = Command::upgrade;
    } else {
      if (input >> extra) { request.error = "malformed-message"; return request; }
      request.command = command == "MISSION" ? Command::mission : command == "ACCEPT" ? Command::accept :
        command == "TURNIN" ? Command::turnin : command == "REFUEL" ? Command::refuel : Command::repair;
    }
  } else if (command == "OUTFIT") {
    std::string extra;
    if (!(input >> request.first) || request.first.size() > 16 ||
        (request.first == "LIST" && (input >> extra)) ||
        (request.first != "LIST" && request.first != "BUY" && request.first != "FIT" && request.first != "REMOVE") ||
        (request.first != "LIST" && (!(input >> request.second) || request.second.size() > 32 || (input >> extra)))) {
      request.error = "usage=OUTFIT LIST|BUY|FIT module|REMOVE slot";
      return request;
    }
    request.command = Command::outfit;
  } else if (command == "SHIPYARD") {
    std::string extra;
    if (!(input >> request.first) || request.first.size() > 16 ||
        ((request.first == "LIST" || request.first == "OWNED") && (input >> extra)) ||
        (request.first != "LIST" && request.first != "OWNED" && request.first != "BUY" && request.first != "SWITCH") ||
        ((request.first == "BUY" || request.first == "SWITCH") &&
          (!(input >> request.second) || request.second.size() > 64 || (input >> extra)))) {
      request.error = "usage=SHIPYARD LIST|OWNED|BUY hull-id|SWITCH instance-id";
      return request;
    }
    request.command = Command::shipyard;
  } else if (command == "BUY" || command == "SELL") {
    std::string extra;
    if (!(input>>request.first>>request.second) || (input>>extra) ||
        (request.first!="food" && request.first!="parts") || request.second.size()!=1 ||
        request.second[0]<'1' || request.second[0]>'8') {
      request.error="usage=BUY|SELL food|parts quantity:1..8"; return request;
    }
    request.command=command=="BUY" ? Command::buy : Command::sell;
  } else if (command == "INPUT") {
    std::string thrust, turn, brake, extra;
    if (!(input >> thrust >> turn >> brake) || (input >> extra) ||
        (thrust != "0" && thrust != "1") ||
        (turn != "-1" && turn != "0" && turn != "1") || (brake != "0" && brake != "1")) {
      request.error = "usage=INPUT thrust:0|1 turn:-1|0|1 brake:0|1";
      return request;
    }
    request.thrust = thrust == "1";
    request.turn = turn == "-1" ? -1 : turn == "1" ? 1 : 0;
    request.brake = brake == "1";
    request.command = Command::input;
  } else if (command == "FIRE") {
    std::string extra;
    if (!(input >> request.first) || request.first.empty() || request.first.size() > 32 ||
        request.first.find_first_of("\t\r ") != std::string::npos || (input >> extra)) {
      request.error = "usage=FIRE target-id";
      return request;
    }
    request.command = Command::fire;
  } else if (command == "RECOVER") {
    std::string extra;
    if (input >> extra) { request.error = "malformed-message"; return request; }
    request.command = Command::recover;
  } else if (command == "LAUNCH" || command == "FLIGHT" || command == "MINE" || command == "DOCK" || command == "CONTACTS") {
    std::string extra;
    if (input >> extra) { request.error = "malformed-message"; return request; }
    request.command = command == "LAUNCH" ? Command::launch : command == "FLIGHT" ? Command::flight :
      command == "MINE" ? Command::mine : command == "CONTACTS" ? Command::contacts : Command::dock;
  } else if (command == "PROFILE" || command == "STATE" || command == "GALNET" || command == "ECONOMY" || command == "QUIT") {
    std::string extra;
    if (input >> extra) { request.error = "malformed-message"; return request; }
    request.command = command == "PROFILE" ? Command::profile : command == "STATE" ? Command::state :
      command == "GALNET" ? Command::galnet : command == "ECONOMY" ? Command::economy : Command::quit;
  } else {
    request.error = command.empty() ? "malformed-message" : "unknown-command";
  }
  return request;
}

} // namespace helion::protocol
