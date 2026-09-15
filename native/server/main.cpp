#include <arpa/inet.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <cstdlib>
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

namespace {
constexpr int kDefaultPort = 4242;
constexpr std::size_t kDefaultMaxClients = 32;
constexpr int kReadTimeoutSeconds = 30;
constexpr int kWriteTimeoutSeconds = 5;
constexpr int kMaxAuthFailures = 5;
constexpr int kMaxAuthRequests = 10;
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
};

struct ChatMessage {
  std::string user;
  std::string text;
};

std::mutex stateMutex;
std::mutex writeMutex;
std::unordered_map<std::string, Profile> profiles;
std::vector<ChatMessage> messages;
std::vector<int> clients;

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
         << profile.experience << '\n';
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
  std::string data = line;
  data.push_back('\n');
  std::lock_guard<std::mutex> lock(writeMutex);
  std::size_t sent = 0;
  while (sent < data.size()) {
    const ssize_t n = send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
    if (n <= 0) return false;
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

std::string stateLine() {
  std::lock_guard<std::mutex> lock(stateMutex);
  std::ostringstream out;
  out << "STATE profiles=" << profiles.size() << " messages=" << messages.size();
  return out.str();
}

void broadcast(const std::string& line) {
  std::lock_guard<std::mutex> lock(stateMutex);
  for (const int fd : clients) sendLine(fd, line);
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

void clientLoop(int fd) {
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    clients.push_back(fd);
  }
  sendLine(fd, helion::protocol::welcomeLine());
  sendLine(fd, "INFO commands=CREATE LOGIN CHAT PROFILE STATE QUIT");

  std::string user;
  int authFailures = 0;
  int authRequests = 0;
  helion::protocol::LineDecoder decoder;
  bool running = true;
  auto lastCompleteLine = std::chrono::steady_clock::now();
  char bytes[1024];
  while (running) {
    const ssize_t received = recv(fd, bytes, sizeof(bytes), 0);
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
        std::lock_guard<std::mutex> lock(stateMutex);
        if (profiles.count(name) != 0) {
          sendLine(fd, "ERR profile-exists");
          if (++authFailures >= kMaxAuthFailures) { running = false; break; }
        } else {
          profiles.emplace(name, Profile{encoded, display});
          try { saveStateLocked(); } catch (const std::exception& error) {
            profiles.erase(name);
            sendLine(fd, "ERR persistence-failed");
            std::cerr << error.what() << '\n';
            continue;
          }
          user = name;
          sendLine(fd, "OK CREATED user=" + name + " display=" + display);
        }
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
          {
            std::lock_guard<std::mutex> lock(stateMutex);
            messages.push_back({user, text});
            try { saveStateLocked(); } catch (const std::exception& error) {
              messages.pop_back();
              sendLine(fd, "ERR persistence-failed");
              std::cerr << error.what() << '\n';
              continue;
            }
          }
          broadcast("CHAT user=" + user + " text=" + text);
        }
      } else if (request.command == helion::protocol::Command::profile) {
        if (user.empty()) {
          sendLine(fd, "ERR login-required");
        } else {
          std::lock_guard<std::mutex> lock(stateMutex);
          const auto it = profiles.find(user);
          if (it == profiles.end()) sendLine(fd, "ERR profile-missing");
          else {
            const Profile& profile = it->second;
            sendLine(fd, "PROFILE user=" + user + " display=" + profile.display +
              " faction=" + profile.faction + " ship=" + profile.ship +
              " credits=" + std::to_string(profile.credits) +
              " experience=" + std::to_string(profile.experience));
          }
        }
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
      else if (arg == "--max-clients" && i + 1 < argc) maxClients = number(argv[++i], 1024);
      else if (arg.rfind("--", 0) == 0) throw std::invalid_argument("unknown option");
      else if (positional++ == 0) port = static_cast<int>(number(arg, 65535));
      else if (positional == 2) dataPath = arg;
      else throw std::invalid_argument("too many arguments");
    }
  } catch (const std::exception&) {
    std::cerr << "usage: helion_server [port] [data-file] [--bind IPv4-address] [--max-clients 1..1024]\n";
    return 2;
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(port));
  if (inet_pton(AF_INET, bindAddress.c_str(), &address.sin_addr) != 1) {
    std::cerr << "--bind requires a numeric IPv4 address\n";
    return 2;
  }
  const bool loopback = (ntohl(address.sin_addr.s_addr) & 0xff000000U) == 0x7f000000U;
  if (!loopback) std::cerr << "WARNING: non-loopback bind; credentials travel without TLS. Use an encrypted tunnel and restrict access.\n";
  try {
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
            << " max-clients=" << maxClients << '\n';
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
  while (!stopping) {
    reap();
    pollfd ready{server, POLLIN, 0};
    const int polled = poll(&ready, 1, 500);
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
      sendLine(client, "ERR server-busy");
      close(client);
      continue;
    }
    try {
      auto done = std::make_shared<std::atomic<bool>>(false);
      workers.push_back({std::thread([client, done, &limit]() {
        try { clientLoop(client); } catch (...) { /* Close this connection. */ }
        removeClient(client);
        shutdown(client, SHUT_RDWR);
        close(client);
        done->store(true);
        limit.release();
      }), done});
    } catch (const std::exception&) {
      close(client);
      limit.release();
    }
  }
  close(server);
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    for (int fd : clients) shutdown(fd, SHUT_RDWR);
  }
  for (auto& worker : workers) worker.thread.join();
  return 0;
}
