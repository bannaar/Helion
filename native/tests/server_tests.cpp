#include <arpa/inet.h>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
pid_t childPid = -1;
std::string testFile;
std::string testDirectory;

void cleanup() {
  if (childPid > 0) {
    kill(childPid, SIGTERM);
    waitpid(childPid, nullptr, 0);
    childPid = -1;
  }
  if (!testFile.empty()) unlink(testFile.c_str());
  if (!testDirectory.empty()) rmdir(testDirectory.c_str());
}

void check(bool condition, const char* label) {
  if (!condition) { std::cerr << "FAILED: " << label << '\n'; cleanup(); std::exit(1); }
}

int freePort() {
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  check(fd >= 0, "test socket");
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  check(bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "ephemeral port");
  socklen_t length = sizeof(address);
  check(getsockname(fd, reinterpret_cast<sockaddr*>(&address), &length) == 0, "assigned port");
  const int port = ntohs(address.sin_port);
  close(fd);
  return port;
}

int connectTo(int port) {
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(static_cast<uint16_t>(port));
  if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) return fd;
  close(fd);
  return -1;
}

std::string receiveUntil(int fd, const std::string& needle) {
  std::string received;
  for (int i = 0; i < 40 && received.find(needle) == std::string::npos; ++i) {
    pollfd ready{fd, POLLIN, 0};
    if (poll(&ready, 1, 100) <= 0) continue;
    char bytes[1024];
    const ssize_t count = recv(fd, bytes, sizeof(bytes), 0);
    if (count <= 0) break;
    received.append(bytes, static_cast<std::size_t>(count));
  }
  return received;
}
} // namespace

int main(int argc, char** argv) {
  check(argc == 2, "server executable path");
  char directory[] = "helion-server-test-XXXXXX";
  check(mkdtemp(directory) != nullptr, "temporary test directory");
  const std::string data = std::string(directory) + "/synthetic.db";
  testDirectory = directory;
  testFile = data;
  {
    std::ofstream fixture(data);
    fixture << "P\tpilot	legacy-pass-one	Pilot One\n";
    fixture << "P\twingman	$legacy-pass-two	Wingman Two\n";
  }
  const int port = freePort();
  const std::string portText = std::to_string(port);
  const pid_t child = fork();
  check(child >= 0, "fork test server");
  if (child == 0) {
    execl(argv[1], argv[1], portText.c_str(), data.c_str(), "--max-clients", "1", static_cast<char*>(nullptr));
    _exit(127);
  }
  childPid = child;

  int first = -1;
  for (int i = 0; i < 80 && first < 0; ++i) {
    usleep(50000);
    first = connectTo(port);
  }
  check(first >= 0, "server starts after migration");
  check(receiveUntil(first, "INFO commands=").find("WELCOME Helion/1") != std::string::npos, "version greeting");
  const int second = connectTo(port);
  check(second >= 0 && receiveUntil(second, "ERR server-busy").find("ERR server-busy") != std::string::npos,
        "second client rejected at limit");
  close(second);

  const std::string login = "LOGIN pilot legacy-pass-one\n";
  check(send(first, login.data(), login.size(), 0) == static_cast<ssize_t>(login.size()), "send legacy login");
  check(receiveUntil(first, "OK LOGIN").find("OK LOGIN user=pilot") != std::string::npos, "legacy login works");
  const std::string shortPassword = "CREATE newcomer short New Pilot\n";
  check(send(first, shortPassword.data(), shortPassword.size(), 0) == static_cast<ssize_t>(shortPassword.size()), "send short password");
  check(receiveUntil(first, "ERR password-length").find("ERR password-length") != std::string::npos,
        "short new password rejected");
  const std::string invalidLogin = "LOGIN pilot wrong-password\n";
  for (int i = 0; i < 4; ++i) {
    check(send(first, invalidLogin.data(), invalidLogin.size(), 0) == static_cast<ssize_t>(invalidLogin.size()),
          "send invalid login");
    check(receiveUntil(first, "ERR invalid-login").find("ERR invalid-login") != std::string::npos,
          "invalid login rejected");
  }
  close(first);

  int replacement = -1;
  for (int i = 0; i < 40 && replacement < 0; ++i) {
    usleep(50000);
    replacement = connectTo(port);
    if (replacement >= 0 && receiveUntil(replacement, "INFO commands=").find("WELCOME Helion/1") == std::string::npos) {
      close(replacement);
      replacement = -1;
    }
  }
  check(replacement >= 0, "slot released after disconnect");
  close(replacement);

  kill(child, SIGTERM);
  int status = 0;
  check(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0, "server shuts down cleanly");
  childPid = -1;
  struct stat info{};
  check(stat(data.c_str(), &info) == 0 && (info.st_mode & 0777) == 0600, "data file owner-only");
  std::ifstream file(data);
  const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  check(content.find("H\tpilot\t$scrypt$") != std::string::npos &&
        content.find("H\twingman\t$scrypt$") != std::string::npos, "all legacy profiles migrated");
  check(content.find("legacy-pass-one") == std::string::npos &&
        content.find("$legacy-pass-two") == std::string::npos, "plaintext removed from current data file");

  const std::string malformed = "P\tpilot\tlegacy-pass-one\tPilot\tfree-traders\tsidewinder\tbad-credits\t0\n";
  {
    std::ofstream fixture(data, std::ios::trunc);
    fixture << malformed;
  }
  const pid_t rejected = fork();
  check(rejected >= 0, "fork malformed-file server");
  if (rejected == 0) {
    execl(argv[1], argv[1], portText.c_str(), data.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }
  childPid = rejected;
  check(waitpid(rejected, &status, 0) == rejected && WIFEXITED(status) && WEXITSTATUS(status) != 0,
        "malformed legacy file blocks startup");
  childPid = -1;
  std::ifstream preserved(data);
  const std::string afterFailure((std::istreambuf_iterator<char>(preserved)), std::istreambuf_iterator<char>());
  check(afterFailure == malformed, "failed migration preserves source file");
  cleanup();
  std::cout << "server integration tests passed\n";
  return 0;
}
