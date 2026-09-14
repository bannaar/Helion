#include <arpa/inet.h>
#include <csignal>
#include <cstring>
#include <iostream>
#include <fstream>
#include <mutex>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>

namespace {
constexpr int kDefaultPort = 4242;
constexpr std::size_t kMaxLine = 4096;
std::string dataPath = "helion-server.db";

struct Profile {
  std::string password;
  std::string display;
  std::string faction = "free-traders";
  std::string ship = "sidewinder";
  int credits = 1500;
  int experience = 0;
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
  const std::string temp = dataPath + ".tmp";
  std::ofstream file(temp, std::ios::trunc);
  if (!file) throw std::runtime_error("cannot open persistence file");
  for (const auto& [name, profile] : profiles) {
    file << "P\t" << encode(name) << '\t' << encode(profile.password) << '\t'
         << encode(profile.display) << '\t' << encode(profile.faction) << '\t'
         << encode(profile.ship) << '\t' << profile.credits << '\t'
         << profile.experience << '\n';
  }
  for (const auto& message : messages) {
    file << "M\t" << encode(message.user) << '\t' << encode(message.text) << '\n';
  }
  file.close();
  if (!file) throw std::runtime_error("cannot write persistence file");
  if (std::rename(temp.c_str(), dataPath.c_str()) != 0) throw std::runtime_error("cannot replace persistence file");
}

void loadState() {
  std::ifstream file(dataPath);
  if (!file) return;
  std::string line;
  while (std::getline(file, line)) {
    std::istringstream input(line);
    std::string kind, first, second, third;
    std::getline(input, kind, '\t');
    std::getline(input, first, '\t');
    std::getline(input, second, '\t');
    std::getline(input, third, '\t');
    if (kind == "P" && !first.empty() && !second.empty() && !third.empty()) {
      std::string faction, ship, creditsText, experienceText;
      std::getline(input, faction, '\t');
      std::getline(input, ship, '\t');
      std::getline(input, creditsText, '\t');
      std::getline(input, experienceText, '\t');
      Profile profile{decode(second), decode(third)};
      if (!faction.empty()) profile.faction = decode(faction);
      if (!ship.empty()) profile.ship = decode(ship);
      try { if (!creditsText.empty()) profile.credits = std::stoi(creditsText); } catch (...) {}
      try { if (!experienceText.empty()) profile.experience = std::stoi(experienceText); } catch (...) {}
      profiles[decode(first)] = profile;
    } else if (kind == "M" && !first.empty() && !second.empty()) {
      messages.push_back({decode(first), decode(second)});
    }
  }
}

bool sendLine(int fd, const std::string& line) {
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

bool readLine(int fd, std::string& line) {
  line.clear();
  char ch = '\0';
  while (line.size() < kMaxLine) {
    const ssize_t n = recv(fd, &ch, 1, 0);
    if (n == 0) return false;
    if (n < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    if (ch == '\n') {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      return true;
    }
    line.push_back(ch);
  }
  sendLine(fd, "ERR line-too-long");
  return false;
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
  sendLine(fd, "WELCOME Helion/1");
  sendLine(fd, "INFO commands=CREATE LOGIN CHAT PROFILE STATE QUIT");

  std::string user;
  std::string line;
  while (readLine(fd, line)) {
    std::istringstream input(line);
    std::string command;
    input >> command;
    if (command == "CREATE") {
      std::string name, password, display;
      input >> name >> password;
      std::getline(input, display);
      if (!display.empty() && display.front() == ' ') display.erase(0, 1);
      if (name.empty() || password.empty() || display.empty() || name.size() > 32 ||
          password.size() > 128 || display.size() > 64 || name.find(' ') != std::string::npos) {
        sendLine(fd, "ERR usage=CREATE username password display");
        continue;
      }
      std::lock_guard<std::mutex> lock(stateMutex);
      if (profiles.count(name) != 0) {
        sendLine(fd, "ERR profile-exists");
      } else {
        profiles.emplace(name, Profile{password, display});
        try { saveStateLocked(); } catch (const std::exception& error) {
          profiles.erase(name);
          sendLine(fd, "ERR persistence-failed");
          std::cerr << error.what() << '\n';
          continue;
        }
        user = name;
        sendLine(fd, "OK CREATED user=" + name + " display=" + display);
      }
    } else if (command == "LOGIN") {
      std::string name, password;
      input >> name >> password;
      std::lock_guard<std::mutex> lock(stateMutex);
      const auto it = profiles.find(name);
      if (it == profiles.end() || it->second.password != password) {
        sendLine(fd, "ERR invalid-login");
      } else {
        user = name;
        sendLine(fd, "OK LOGIN user=" + name + " display=" + it->second.display);
      }
    } else if (command == "CHAT") {
      std::string text;
      std::getline(input, text);
      if (!text.empty() && text.front() == ' ') text.erase(0, 1);
      if (user.empty()) sendLine(fd, "ERR login-required");
      else if (text.empty() || text.size() > 512) sendLine(fd, "ERR usage=CHAT message");
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
    } else if (command == "PROFILE") {
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
    } else if (command == "STATE") {
      sendLine(fd, stateLine());
    } else if (command == "QUIT") {
      sendLine(fd, "OK BYE");
      break;
    } else if (!command.empty()) {
      sendLine(fd, "ERR unknown-command");
    }
  }
  removeClient(fd);
  shutdown(fd, SHUT_RDWR);
  close(fd);
}
}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGPIPE, SIG_IGN);
  int port = kDefaultPort;
  if (argc > 1) {
    try { port = std::stoi(argv[1]); } catch (const std::exception&) {
      std::cerr << "usage: helion_server [port] [data-file]\n";
      return 2;
    }
  }
  if (argc > 2) dataPath = argv[2];
  if (port < 1 || port > 65535) {
    std::cerr << "port must be between 1 and 65535\n";
    return 2;
  }
  try {
    std::lock_guard<std::mutex> lock(stateMutex);
    loadState();
  } catch (const std::exception& error) {
    std::cerr << "unable to load persistence: " << error.what() << '\n';
    return 1;
  }
  const int server = socket(AF_INET, SOCK_STREAM, 0);
  if (server < 0) { std::perror("socket"); return 1; }
  int yes = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(static_cast<uint16_t>(port));
  if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
      listen(server, 32) < 0) {
    std::perror("bind/listen");
    close(server);
    return 1;
  }
  std::cout << "Helion server listening on 0.0.0.0:" << port
            << " data=" << dataPath << '\n';
  while (true) {
    sockaddr_in peer{};
    socklen_t length = sizeof(peer);
    const int client = accept(server, reinterpret_cast<sockaddr*>(&peer), &length);
    if (client < 0) {
      if (errno == EINTR) continue;
      std::perror("accept");
      break;
    }
    std::thread(clientLoop, client).detach();
  }
  close(server);
  return 0;
}
