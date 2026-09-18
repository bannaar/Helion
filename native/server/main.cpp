#include <arpa/inet.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "shared/protocol.h"
#include "server/security.h"
#include "server/connection_limit.h"
#include "server/data_lock.h"
#include "shared/flight.h"
#include "shared/tls.h"

namespace {
constexpr int kDefaultPort = 4242;
constexpr std::size_t kDefaultMaxClients = 32;
constexpr int kReadTimeoutSeconds = 30;
constexpr int kWriteTimeoutSeconds = 5;
constexpr int kMaxAuthFailures = 5;
constexpr int kMaxAuthRequests = 10;
constexpr double kFlightCheckpointSeconds = 1.0;
std::string dataPath = "helion-server.db";
std::string dummyPasswordHash;
volatile std::sig_atomic_t stopping = 0;

void stopSignal(int) { stopping = 1; }

struct Profile {
  std::string passwordHash;
  std::string display;
  std::string faction = "free-traders";
  std::string ship = "sidewinder";
  int credits = 1500;
  int experience = 0;
  bool legacyCredential = false;
  helion::flight::State flight{};
  int missionStage = 0;
  int hullLevel = 1;
  int engineLevel = 1;
  bool missionOreMined = false;
};

struct ChatMessage {
  std::string user;
  std::string text;
};

std::mutex stateMutex;
std::unordered_map<std::string, Profile> profiles;
std::vector<ChatMessage> messages;
std::vector<int> clients;
bool flightStateDirty = false;
std::mutex transportMutex;
struct Transport {
  explicit Transport(std::shared_ptr<helion::tls::Connection> value) : connection(std::move(value)) {}
  std::shared_ptr<helion::tls::Connection> connection;
  std::mutex writeMutex;
};
std::unordered_map<int, std::shared_ptr<Transport>> transports;

std::string encode(const std::string& value) {
  std::string out;
  for (const char ch : value) {
    if (ch == '\\' || ch == '\t' || ch == '\n') out.push_back('\\');
    if (ch == '\t') out.push_back('t');
    else if (ch == '\n') out.push_back('n');
    else out.push_back(ch);
  }
  return out;
}

std::string decode(const std::string& value) {
  std::string out;
  bool escaped = false;
  for (const char ch : value) {
    if (escaped) {
      out.push_back(ch == 't' ? '\t' : ch == 'n' ? '\n' : ch);
      escaped = false;
    } else if (ch == '\\') escaped = true;
    else out.push_back(ch);
  }
  if (escaped) out.push_back('\\');
  return out;
}

void saveStateLocked() {
  std::ostringstream snapshot;
  for (const auto& [name, profile] : profiles) {
    snapshot << "H\t" << encode(name) << '\t' << encode(profile.passwordHash) << '\t'
         << encode(profile.display) << '\t' << encode(profile.faction) << '\t'
         << encode(profile.ship) << '\t' << profile.credits << '\t'
         << profile.experience << '\t' << profile.missionStage << '\t' << profile.hullLevel << '\t'
         << profile.engineLevel << '\t' << profile.flight.x << '\t' << profile.flight.y << '\t'
         << profile.flight.vx << '\t' << profile.flight.vy << '\t' << profile.flight.yaw << '\t'
         << profile.flight.docked << '\t' << profile.flight.cargo << '\t' << profile.flight.food << '\t'
         << profile.flight.parts << '\t' << profile.flight.station << '\t' << profile.flight.hull << '\t'
         << profile.flight.maxHull << '\t' << profile.missionOreMined << '\n';
  }
  for (const auto& message : messages) {
    snapshot << "M\t" << encode(message.user) << '\t' << encode(message.text) << '\n';
  }
  std::string temp = dataPath + ".tmp.XXXXXX";
  const int fd = mkstemp(temp.data());
  if (fd < 0) throw std::runtime_error("cannot create persistence file");
  bool complete = false;
  try {
    if (fchmod(fd, S_IRUSR | S_IWUSR) != 0) throw std::runtime_error("cannot protect persistence file");
    const std::string content = snapshot.str();
    std::size_t written = 0;
    while (written < content.size()) {
      const ssize_t count = write(fd, content.data() + written, content.size() - written);
      if (count < 0 && errno == EINTR) continue;
      if (count <= 0) throw std::runtime_error("cannot write persistence file");
      written += static_cast<std::size_t>(count);
    }
    if (fsync(fd) != 0) throw std::runtime_error("cannot sync persistence file");
    if (close(fd) != 0) throw std::runtime_error("cannot close persistence file");
    complete = true;
    if (std::rename(temp.c_str(), dataPath.c_str()) != 0) throw std::runtime_error("cannot replace persistence file");
  } catch (...) {
    if (!complete) close(fd);
    unlink(temp.c_str());
    throw;
  }
}

void persistStateLocked() {
  saveStateLocked();
  flightStateDirty = false;
}

void loadState() {
  const int fd = open(dataPath.c_str(), O_RDONLY | O_NOFOLLOW);
  if (fd < 0) {
    if (errno == ENOENT) return;
    throw std::runtime_error("cannot open persistence file");
  }
  struct stat info{};
  if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_nlink != 1 ||
      fchmod(fd, S_IRUSR | S_IWUSR) != 0) {
    close(fd);
    throw std::runtime_error("cannot protect persistence file");
  }
  FILE* file = fdopen(fd, "r");
  if (!file) { close(fd); throw std::runtime_error("cannot read persistence file"); }
  char* buffer = nullptr;
  std::size_t capacity = 0;
  try {
    ssize_t length;
    while ((length = ::getline(&buffer, &capacity, file)) >= 0) {
      std::string line(buffer, static_cast<std::size_t>(length));
      if (!line.empty() && line.back() == '\n') line.pop_back();
      if (!line.empty() && line.back() == '\r') line.pop_back();
      std::istringstream input(line);
      std::string kind, first, second, third;
      std::getline(input, kind, '\t');
      std::getline(input, first, '\t');
      std::getline(input, second, '\t');
      std::getline(input, third, '\t');
      if ((kind == "P" || kind == "H") && !first.empty() && !second.empty() && !third.empty()) {
        std::string faction, ship, creditsText, experienceText;
        std::getline(input, faction, '\t');
        std::getline(input, ship, '\t');
        std::getline(input, creditsText, '\t');
        std::getline(input, experienceText, '\t');
        Profile profile{decode(second), decode(third)};
        profile.legacyCredential = kind == "P";
        if (!profile.legacyCredential && !helion::security::isEncodedHash(profile.passwordHash))
          throw std::runtime_error("malformed password hash record");
        if (!faction.empty()) profile.faction = decode(faction);
        if (!ship.empty()) profile.ship = decode(ship);
        auto parseNumber = [](const std::string& value, int fallback) {
          if (value.empty()) return fallback;
          std::size_t used = 0;
          const int result = std::stoi(value, &used);
          if (used != value.size()) throw std::runtime_error("malformed persistence number");
          return result;
        };
        profile.credits = parseNumber(creditsText, profile.credits);
        profile.experience = parseNumber(experienceText, profile.experience);
        if (profile.credits < 0 || profile.experience < 0)
          throw std::runtime_error("malformed persistence number");
        std::string value;
        std::getline(input, value, '\t'); profile.missionStage = parseNumber(value, 0);
        std::getline(input, value, '\t'); profile.hullLevel = std::clamp(parseNumber(value, 1), 1, 5);
        std::getline(input, value, '\t'); profile.engineLevel = std::clamp(parseNumber(value, 1), 1, 5);
        std::getline(input, value, '\t'); profile.flight.x = std::stod(value.empty()?"0":value);
        std::getline(input, value, '\t'); profile.flight.y = std::stod(value.empty()?"0":value);
        std::getline(input, value, '\t'); profile.flight.vx = std::stod(value.empty()?"0":value);
        std::getline(input, value, '\t'); profile.flight.vy = std::stod(value.empty()?"0":value);
        std::getline(input, value, '\t'); profile.flight.yaw = std::stod(value.empty()?"0":value);
        std::getline(input, value, '\t'); profile.flight.docked = value.empty() || value == "1";
        std::getline(input, value, '\t'); profile.flight.cargo = parseNumber(value, 0);
        std::getline(input, value, '\t'); profile.flight.food = parseNumber(value, 0);
        std::getline(input, value, '\t'); profile.flight.parts = parseNumber(value, 0);
        std::getline(input, value, '\t'); profile.flight.station = parseNumber(value, 0);
        std::getline(input, value, '\t'); profile.flight.hull = std::clamp(parseNumber(value, 100), 0, 200);
        std::getline(input, value, '\t'); profile.flight.maxHull = std::clamp(parseNumber(value, helion::flight::hullCapacity(profile.hullLevel)), 1, 200);
        profile.flight.maxHull = helion::flight::hullCapacity(profile.hullLevel);
        profile.flight.hull = std::min(profile.flight.hull, profile.flight.maxHull);
        std::string objective;
        if (std::getline(input, objective, '\t')) {
          if (objective != "0" && objective != "1") throw std::runtime_error("malformed mission objective");
          profile.missionOreMined = objective == "1";
          std::string trailing;
          if (std::getline(input, trailing)) throw std::runtime_error("unexpected persistence fields");
        }
        if (profile.missionStage == 2) profile.missionOreMined = false;
        profile.flight.inputAge = 1;
        if (!profiles.emplace(decode(first), std::move(profile)).second)
          throw std::runtime_error("duplicate profile in persistence file");
      } else if (kind == "M" && !first.empty() && !second.empty()) {
        messages.push_back({decode(first), decode(second)});
      } else if (!line.empty()) {
        throw std::runtime_error("malformed persistence record");
      }
    }
    if (ferror(file)) throw std::runtime_error("cannot read persistence file");
  } catch (...) {
    free(buffer);
    fclose(file);
    throw;
  }
  free(buffer);
  fclose(file);
}

void migrateLegacyProfiles() {
  auto migrated = profiles;
  bool changed = false;
  for (auto& [name, profile] : migrated) {
    (void)name;
    if (helion::security::migrateLegacyCredential(profile.passwordHash, profile.legacyCredential)) {
      profile.legacyCredential = false;
      changed = true;
    }
  }
  if (!changed) return;
  profiles.swap(migrated);
  try { saveStateLocked(); } catch (...) { profiles.swap(migrated); throw; }
  for (auto& [name, profile] : migrated) {
    (void)name;
    std::fill(profile.passwordHash.begin(), profile.passwordHash.end(), '\0');
  }
}

bool sendLine(int fd, const std::string& line) {
  if (!helion::protocol::validWireLine(line)) return false;
  std::shared_ptr<Transport> transport;
  {
    std::lock_guard<std::mutex> lock(transportMutex);
    const auto found = transports.find(fd);
    if (found == transports.end()) return false;
    transport = found->second;
  }
  std::lock_guard<std::mutex> lock(transport->writeMutex);
  return transport->connection->sendAll(line + "\n");
}

std::string stateLine() {
  std::lock_guard<std::mutex> lock(stateMutex);
  std::ostringstream out;
  out << "STATE profiles=" << profiles.size() << " messages=" << messages.size();
  return out.str();
}

void broadcast(const std::string& line) {
  std::vector<int> recipients;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    recipients = clients;
  }
  for (const int fd : recipients) sendLine(fd, line);
}

std::vector<helion::flight::Contact> contactsFor(const std::string& user) {
  std::vector<helion::flight::Contact> result;
  for (const auto& [name, profile] : profiles) {
    if (name == user) continue;
    result.push_back({name, "pilot", profile.flight.x, profile.flight.y, profile.flight.yaw, profile.flight.docked});
  }
  const double t = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  result.push_back({"HAULER-7", "hauler", 420.0 + std::cos(t * 0.08) * 180.0,
                    160.0 + std::sin(t * 0.08) * 120.0, 0, false});
  return result;
}

void removeClient(int fd) {
  std::lock_guard<std::mutex> lock(stateMutex);
  for (auto it = clients.begin(); it != clients.end(); ++it) {
    if (*it == fd) {
      clients.erase(it);
      break;
    }
  }
}

void clientLoop(int fd, helion::tls::Connection& connection) {
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    clients.push_back(fd);
  }
  sendLine(fd, helion::protocol::welcomeLine());
  sendLine(fd, "INFO commands=CREATE LOGIN CHAT PROFILE STATE CONTACTS BUY SELL MISSION ACCEPT TURNIN UPGRADE REPAIR LAUNCH INPUT FLIGHT MINE DOCK QUIT");

  std::string user;
  int authFailures = 0;
  int authRequests = 0;
  helion::protocol::LineDecoder decoder;
  bool running = true;
  auto lastCompleteLine = std::chrono::steady_clock::now();
  char bytes[1024];
  while (running) {
    const ssize_t received = connection.receive(bytes, sizeof(bytes));
    if (received == 0) break;
    if (received < 0) {
      if (errno == EINTR) continue;
      break;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - lastCompleteLine > std::chrono::seconds(kReadTimeoutSeconds)) break;
    for (const auto& frame : decoder.feed(std::string_view(bytes, static_cast<std::size_t>(received)))) {
      lastCompleteLine = now;
      if (frame.kind != helion::protocol::FrameKind::line) {
        sendLine(fd, frame.kind == helion::protocol::FrameKind::too_long ? "ERR line-too-long" : "ERR malformed-message");
        continue;
      }
      const auto request = helion::protocol::parseRequest(frame.line);
      const bool authRequest = request.command == helion::protocol::Command::create ||
        request.command == helion::protocol::Command::login ||
        frame.line.rfind("CREATE", 0) == 0 || frame.line.rfind("LOGIN", 0) == 0;
      if (authRequest && ++authRequests > kMaxAuthRequests) {
        sendLine(fd, "ERR too-many-auth-attempts");
        running = false;
        break;
      }
      if (request.command == helion::protocol::Command::invalid) {
        sendLine(fd, "ERR " + request.error);
        continue;
      }
      if (request.command == helion::protocol::Command::create) {
        const auto& name = request.first;
        const auto& password = request.second;
        const auto& display = request.payload;
        if (password.size() < helion::security::kMinNewPassword || password.size() > helion::security::kMaxPassword) {
          sendLine(fd, "ERR password-length");
          if (++authFailures >= kMaxAuthFailures) { running = false; break; }
          continue;
        }
        std::string encoded;
        try { encoded = helion::security::hashPassword(password); }
        catch (const std::exception&) { sendLine(fd, "ERR password-hashing-unavailable"); continue; }
        std::string response;
        bool tooManyFailures = false;
        bool created = false;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          if (profiles.count(name) != 0) {
            response = "ERR profile-exists";
            tooManyFailures = ++authFailures >= kMaxAuthFailures;
          } else {
            profiles.emplace(name, Profile{encoded, display});
            try {
              persistStateLocked();
              response = "OK CREATED user=" + name + " display=" + display;
              created = true;
            } catch (const std::exception& error) {
              profiles.erase(name);
              response = "ERR persistence-failed";
              std::cerr << error.what() << '\n';
            }
          }
        }
        if (created) user = name;
        sendLine(fd, response);
        if (tooManyFailures) { running = false; break; }
      } else if (request.command == helion::protocol::Command::login) {
        const auto& name = request.first;
        const auto& password = request.second;
        std::string encoded;
        std::string display;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          const auto it = profiles.find(name);
          if (it != profiles.end()) { encoded = it->second.passwordHash; display = it->second.display; }
        }
        const bool found = !encoded.empty();
        if (!found) encoded = dummyPasswordHash;
        const bool authenticated = helion::security::verifyPassword(password, encoded);
        if (!found || !authenticated) {
          sendLine(fd, "ERR invalid-login");
          if (++authFailures >= kMaxAuthFailures) { sendLine(fd, "ERR too-many-auth-attempts"); running = false; break; }
        } else {
          user = name;
          sendLine(fd, "OK LOGIN user=" + name + " display=" + display);
        }
      } else if (request.command == helion::protocol::Command::chat) {
        const auto& text = request.payload;
        if (user.empty()) sendLine(fd, "ERR login-required");
        else {
          bool saved = false;
          {
            std::lock_guard<std::mutex> lock(stateMutex);
            messages.push_back({user, text});
            try { persistStateLocked(); saved = true; }
            catch (const std::exception& error) {
              messages.pop_back();
              std::cerr << error.what() << '\n';
            }
          }
          if (!saved) { sendLine(fd, "ERR persistence-failed"); continue; }
          broadcast("CHAT user=" + user + " text=" + text);
        }
      } else if (request.command == helion::protocol::Command::profile) {
        if (user.empty()) {
          sendLine(fd, "ERR login-required");
        } else {
          std::string response;
          {
            std::lock_guard<std::mutex> lock(stateMutex);
            const auto it = profiles.find(user);
            if (it == profiles.end()) response = "ERR profile-missing";
            else {
              const Profile& profile = it->second;
              response = "PROFILE user=" + user + " display=" + profile.display +
                " faction=" + profile.faction + " ship=" + profile.ship +
                " credits=" + std::to_string(profile.credits) +
                " experience=" + std::to_string(profile.experience) +
                " hull=" + std::to_string(profile.flight.hull) +
                " max-hull=" + std::to_string(profile.flight.maxHull) +
                " engine-level=" + std::to_string(profile.engineLevel) +
                " hull-level=" + std::to_string(profile.hullLevel) +
                " mission-stage=" + std::to_string(profile.missionStage) +
                " mission-ore-mined=" + std::to_string(profile.missionOreMined);
            }
          }
          sendLine(fd, response);
        }
      } else if (request.command == helion::protocol::Command::mission || request.command == helion::protocol::Command::accept || request.command == helion::protocol::Command::turnin) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::string response;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          if (request.command == helion::protocol::Command::mission) {
            response = profile.missionStage == 0 ? "MISSION 1 title=First Ore objective=mine-1 reward=250" : profile.missionStage == 1 ? "MISSION 1 active objective=mine-1 reward=250" : "MISSION 1 complete";
          } else if (request.command == helion::protocol::Command::accept) {
            if (profile.missionStage != 0) response = "ERR mission-unavailable";
            else {
              const auto before = profile;
              const bool dirtyBefore = flightStateDirty;
              profile.missionStage = 1;
              profile.missionOreMined = false;
              try { persistStateLocked(); response = "OK MISSION ACCEPTED id=1"; }
              catch (const std::exception& error) {
                profile = before;
                flightStateDirty = dirtyBefore;
                response = "ERR persistence-failed";
                std::cerr << error.what() << '\n';
              }
            }
          } else if (profile.missionStage != 1) response = "ERR mission-not-active";
          else if (!profile.missionOreMined || !profile.flight.docked) response = "ERR objective-incomplete";
          else {
            const auto before = profile;
            const bool dirtyBefore = flightStateDirty;
            profile.missionStage = 2;
            profile.missionOreMined = false;
            profile.credits += 250;
            profile.experience += 25;
            try { persistStateLocked(); response = "OK MISSION COMPLETE reward=250"; }
            catch (const std::exception& error) {
              profile = before;
              flightStateDirty = dirtyBefore;
              response = "ERR persistence-failed";
              std::cerr << error.what() << '\n';
            }
          }
        }
        sendLine(fd, response);
      } else if (request.command == helion::protocol::Command::upgrade || request.command == helion::protocol::Command::repair) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<std::string> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          if (!profile.flight.docked) responses.push_back("ERR dock-required");
          else if (request.command == helion::protocol::Command::repair) {
            const auto before = profile;
            const bool dirtyBefore = flightStateDirty;
            const auto result = helion::flight::repair(profile.flight, profile.credits, profile.hullLevel);
            if (result.rfind("OK", 0) == 0) {
              try { persistStateLocked(); }
              catch (const std::exception& error) {
                profile = before;
                flightStateDirty = dirtyBefore;
                responses.push_back("ERR persistence-failed");
                std::cerr << error.what() << '\n';
              }
            }
            if (responses.empty()) responses.push_back(result);
            responses.push_back(helion::flight::snapshot(profile.flight, profile.credits, profile.experience));
          } else {
            int& level = request.first == "hull" ? profile.hullLevel : profile.engineLevel;
            const int cost = level * 500;
            if (level >= 5) responses.push_back("ERR upgrade-max");
            else if (profile.credits < cost) responses.push_back("ERR insufficient-credits");
            else {
              const auto before = profile;
              const bool dirtyBefore = flightStateDirty;
              profile.credits -= cost;
              ++level;
              profile.flight.maxHull = helion::flight::hullCapacity(profile.hullLevel);
              try {
                persistStateLocked();
                responses.push_back("OK UPGRADE " + request.first + " level=" + std::to_string(level) + " cost=" + std::to_string(cost));
              } catch (const std::exception& error) {
                profile = before;
                flightStateDirty = dirtyBefore;
                responses.push_back("ERR persistence-failed");
                std::cerr << error.what() << '\n';
              }
            }
            responses.push_back(helion::flight::snapshot(profile.flight, profile.credits, profile.experience));
          }
        }
        for (const auto& response : responses) sendLine(fd, response);
      } else if (request.command == helion::protocol::Command::contacts) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<std::string> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          for (const auto& contact : contactsFor(user)) responses.push_back(helion::flight::contactLine(contact));
          responses.push_back("CONTACTS END");
        }
        for (const auto& response : responses) sendLine(fd, response);
      } else if (request.command == helion::protocol::Command::buy || request.command == helion::protocol::Command::sell) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<std::string> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          int quantity = 0;
          try { quantity = std::stoi(request.second); }
          catch (...) { responses.push_back("ERR invalid-quantity"); }
          if (responses.empty()) {
            const auto before = profile;
            const bool dirtyBefore = flightStateDirty;
            const auto result = helion::flight::trade(profile.flight, profile.credits,
              request.command == helion::protocol::Command::buy, request.first, quantity);
            if (result.rfind("OK", 0) == 0) {
              try { persistStateLocked(); }
              catch (const std::exception& error) {
                profile = before;
                flightStateDirty = dirtyBefore;
                responses.push_back("ERR persistence-failed");
                std::cerr << error.what() << '\n';
              }
            }
            if (responses.empty()) responses.push_back(result);
            responses.push_back(helion::flight::snapshot(profile.flight, profile.credits, profile.experience));
          }
        }
        for (const auto& response : responses) sendLine(fd, response);
      } else if (request.command == helion::protocol::Command::launch ||
                 request.command == helion::protocol::Command::input ||
                 request.command == helion::protocol::Command::flight ||
                 request.command == helion::protocol::Command::mine ||
                 request.command == helion::protocol::Command::dock) {
        if (user.empty()) { sendLine(fd, "ERR login-required"); continue; }
        std::vector<std::string> responses;
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          auto& profile = profiles.at(user);
          auto& flight = profile.flight;
          if (request.command == helion::protocol::Command::input) {
            flight.input = {request.thrust, request.turn, request.brake};
            flight.inputAge = 0;
            flightStateDirty = true;
          } else if (request.command != helion::protocol::Command::flight) {
            const auto before = profile;
            const bool dirtyBefore = flightStateDirty;
            const bool docking = request.command == helion::protocol::Command::dock;
            helion::flight::DockTransaction transaction;
            std::string result = request.command == helion::protocol::Command::launch ? helion::flight::launch(flight) :
              request.command == helion::protocol::Command::mine ? helion::flight::mine(flight) :
              helion::flight::dock(flight, profile.credits, profile.experience,
                                   docking ? &transaction : nullptr);
            if (result.rfind("OK", 0) == 0) {
              if (request.command == helion::protocol::Command::mine && profile.missionStage == 1)
                profile.missionOreMined = true;
              try { persistStateLocked(); }
              catch (const std::exception& error) {
                profile = before;
                flightStateDirty = dirtyBefore;
                responses.push_back("ERR persistence-failed");
                std::cerr << error.what() << '\n';
              }
            }
            if (responses.empty()) {
              responses.push_back(result);
              if (docking && result.rfind("OK", 0) == 0)
                responses.push_back(helion::flight::dockTransactionLine(transaction));
            }
          }
          responses.push_back(helion::flight::snapshot(profile.flight, profile.credits, profile.experience));
        }
        for (const auto& response : responses) sendLine(fd, response);
      } else if (request.command == helion::protocol::Command::state) {
        sendLine(fd, stateLine());
      } else if (request.command == helion::protocol::Command::quit) {
        sendLine(fd, "OK BYE");
        running = false;
        break;
      }
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGPIPE, SIG_IGN);
  std::signal(SIGINT, stopSignal);
  std::signal(SIGTERM, stopSignal);
  int port = kDefaultPort;
  std::size_t maxClients = kDefaultMaxClients;
  std::string bindAddress = "127.0.0.1";
  std::string certificate, privateKey;
  int positional = 0;
  auto number = [](const std::string& value, unsigned long maximum) -> unsigned long {
    std::size_t end = 0;
    const unsigned long parsed = std::stoul(value, &end);
    if (end != value.size() || parsed == 0 || parsed > maximum) throw std::invalid_argument("invalid number");
    return parsed;
  };
  try {
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--bind" && i + 1 < argc) bindAddress = argv[++i];
      else if (arg == "--cert" && i + 1 < argc) certificate = argv[++i];
      else if (arg == "--key" && i + 1 < argc) privateKey = argv[++i];
      else if (arg == "--max-clients" && i + 1 < argc) maxClients = number(argv[++i], 1024);
      else if (arg.rfind("--", 0) == 0) throw std::invalid_argument("unknown option");
      else if (positional++ == 0) port = static_cast<int>(number(arg, 65535));
      else if (positional == 2) dataPath = arg;
      else throw std::invalid_argument("too many arguments");
    }
  } catch (const std::exception&) {
    std::cerr << "usage: helion_server [port] [data-file] [--bind IPv4-address] [--max-clients 1..1024] --cert certificate.pem --key private-key.pem\n";
    return 2;
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(port));
  if (inet_pton(AF_INET, bindAddress.c_str(), &address.sin_addr) != 1) {
    std::cerr << "--bind requires a numeric IPv4 address\n";
    return 2;
  }
  helion::tls::Context tlsContext(nullptr, SSL_CTX_free);
  try { tlsContext = helion::tls::serverContext(certificate, privateKey); }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
  std::unique_ptr<helion::server::DataLock> dataLock;
  try {
    dataLock = std::make_unique<helion::server::DataLock>(dataPath);
    std::lock_guard<std::mutex> lock(stateMutex);
    dummyPasswordHash = helion::security::hashPassword(std::string(12, 'x'));
    loadState();
    migrateLegacyProfiles();
  } catch (const std::exception& error) {
    std::cerr << "unable to load or securely migrate persistence: " << error.what() << '\n';
    return 1;
  }
  const int server = socket(AF_INET, SOCK_STREAM, 0);
  if (server < 0) { std::perror("socket"); return 1; }
  int yes = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
      listen(server, static_cast<int>(maxClients)) < 0) {
    std::perror("bind/listen");
    close(server);
    return 1;
  }
  std::cout << "Helion server listening on " << bindAddress << ':' << port
            << " TLS enabled; max-clients=" << maxClients << '\n';
  helion::server::ConnectionLimit limit(maxClients);
  struct Worker { std::thread thread; std::shared_ptr<std::atomic<bool>> done; };
  std::vector<Worker> workers;
  workers.reserve(maxClients + 1);
  auto reap = [&workers]() {
    for (auto it = workers.begin(); it != workers.end();) {
      if (it->done->load()) {
        it->thread.join();
        it = workers.erase(it);
      } else ++it;
    }
  };
  auto lastTick = std::chrono::steady_clock::now();
  auto nextCheckpoint = lastTick + std::chrono::duration<double>(kFlightCheckpointSeconds);
  double accumulator = 0;
  while (!stopping) {
    const auto now = std::chrono::steady_clock::now();
    accumulator += std::min(0.1, std::chrono::duration<double>(now - lastTick).count());
    lastTick = now;
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      while (accumulator >= 1.0 / 60) {
        for (auto& entry : profiles) {
          const auto before = entry.second.flight;
          helion::flight::step(entry.second.flight, 1.0 / 60, entry.second.engineLevel);
          if (!entry.second.flight.docked || before.docked != entry.second.flight.docked ||
              before.hull != entry.second.flight.hull || before.cargo != entry.second.flight.cargo) {
            flightStateDirty = true;
          }
        }
        accumulator -= 1.0 / 60;
      }
    }
    const auto checkpointNow = std::chrono::steady_clock::now();
    if (checkpointNow >= nextCheckpoint) {
      std::lock_guard<std::mutex> lock(stateMutex);
      if (flightStateDirty) {
        try { persistStateLocked(); }
        catch (const std::exception& error) { std::cerr << "flight checkpoint failed: " << error.what() << '\n'; }
      }
      nextCheckpoint = checkpointNow + std::chrono::duration<double>(kFlightCheckpointSeconds);
    }
    reap();
    pollfd ready{server, POLLIN, 0};
    const int polled = poll(&ready, 1, 16);
    if (polled < 0 && errno == EINTR) continue;
    if (polled < 0) { std::perror("poll"); break; }
    if (polled == 0 || !(ready.revents & POLLIN)) continue;
    sockaddr_in peer{};
    socklen_t length = sizeof(peer);
    const int client = accept(server, reinterpret_cast<sockaddr*>(&peer), &length);
    if (client < 0) {
      if (errno == EINTR) continue;
      std::perror("accept");
      break;
    }
    timeval readTimeout{kReadTimeoutSeconds, 0};
    timeval writeTimeout{kWriteTimeoutSeconds, 0};
    if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &readTimeout, sizeof(readTimeout)) != 0 ||
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &writeTimeout, sizeof(writeTimeout)) != 0) {
      close(client);
      continue;
    }
    if (!limit.tryAcquire()) {
      // Refuse overload before a handshake; never emit plaintext on the TLS port.
      close(client);
      continue;
    }
    try {
      auto connection = std::make_shared<helion::tls::Connection>(tlsContext.get(), client);
      auto transport = std::make_shared<Transport>(connection);
      {
        std::lock_guard<std::mutex> lock(transportMutex);
        transports.emplace(client, transport);
      }
      auto done = std::make_shared<std::atomic<bool>>(false);
      workers.push_back({std::thread([client, transport, done, &limit]() {
        try {
          transport->connection->handshake(true);
          clientLoop(client, *transport->connection);
          std::lock_guard<std::mutex> lock(transport->writeMutex);
          transport->connection->closeNotify();
        } catch (const std::exception& error) {
          std::cerr << "client connection rejected: " << error.what() << '\n';
        }
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          if (flightStateDirty) {
            try { persistStateLocked(); }
            catch (const std::exception& error) {
              std::cerr << "disconnect flight checkpoint failed: " << error.what() << '\n';
            }
          }
        }
        removeClient(client);
        {
          std::lock_guard<std::mutex> lock(transportMutex);
          transports.erase(client);
        }
        shutdown(client, SHUT_RDWR);
        close(client);
        done->store(true);
        limit.release();
      }), done});
    } catch (const std::exception&) {
      { std::lock_guard<std::mutex> lock(transportMutex); transports.erase(client); }
      close(client);
      limit.release();
    }
  }
  close(server);
  {
    std::lock_guard<std::mutex> lock(transportMutex);
    for (const auto& entry : transports) shutdown(entry.first, SHUT_RDWR);
  }
  for (auto& worker : workers) worker.thread.join();
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (flightStateDirty) {
      try { persistStateLocked(); }
      catch (const std::exception& error) { std::cerr << "shutdown flight checkpoint failed: " << error.what() << '\n'; }
    }
  }
  return 0;
}
