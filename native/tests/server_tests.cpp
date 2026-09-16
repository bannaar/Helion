#include <arpa/inet.h>
#include <csignal>
#include <cstdlib>
#include <cmath>
#include <sstream>
#include "shared/flight.h"
#include "shared/tls.h"
#include <map>
#include <memory>
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
helion::tls::Context tlsContext(nullptr, SSL_CTX_free);
std::map<int, std::unique_ptr<helion::tls::Connection>> connections;
void closeConnection(int fd) { connections.erase(fd); close(fd); }
ssize_t tlsSend(int fd, const void* bytes, std::size_t size, int) {
  return connections.at(fd)->sendAll(std::string(static_cast<const char*>(bytes),size)) ? static_cast<ssize_t>(size) : -1;
}
pid_t childPid = -1;
pid_t competingPid = -1;
std::string testFile;
std::string testDirectory;

void cleanup() {
  if (competingPid > 0) {
    kill(competingPid, SIGTERM);
    waitpid(competingPid, nullptr, 0);
    competingPid = -1;
  }
  if (childPid > 0) {
    kill(childPid, SIGTERM);
    waitpid(childPid, nullptr, 0);
    childPid = -1;
  }
  if (!testFile.empty()) unlink(testFile.c_str());
  if (!testFile.empty()) unlink((testFile + ".lock").c_str());
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

int connectTo(int port, bool encrypted = true) {
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(static_cast<uint16_t>(port));
  if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
    if (!encrypted) return fd;
    try {
      auto connection = std::make_unique<helion::tls::Connection>(tlsContext.get(),fd);
      connection->handshake(false,"127.0.0.1");
      connections.emplace(fd,std::move(connection));
      return fd;
    } catch (...) { close(fd); return -1; }
  }
  close(fd);
  return -1;
}

std::string receiveUntil(int fd, const std::string& needle) {
  std::string received;
  for (int i = 0; i < 40 && received.find(needle) == std::string::npos; ++i) {
    char bytes[1024];
    const ssize_t count = connections.at(fd)->receive(bytes, sizeof(bytes), 100);
    if (count < 0 && errno == ETIMEDOUT) continue;
    if (count <= 0) break;
    received.append(bytes, static_cast<std::size_t>(count));
  }
  return received;
}

std::string command(int fd,const std::string& value,const std::string& expected) {
  const auto bytes=value+"\n";
  check(tlsSend(fd,bytes.data(),bytes.size(),0)==static_cast<ssize_t>(bytes.size()),"send gameplay command");
  const auto response=receiveUntil(fd,expected);
  check(response.find(expected)!=std::string::npos,"expected gameplay response");
  return response;
}

helion::flight::State flightState(int fd) {
  const auto response=command(fd,"FLIGHT","FLIGHT ");
  const auto start=response.find("FLIGHT ");
  const auto end=response.find('\n',start);
  helion::flight::State state; int credits=0,xp=0;
  check(helion::flight::readSnapshot(response.substr(start,end-start),state,credits,xp),"valid live flight state");
  return state;
}

void flyTo(int fd,double x,double y) {
  for(int i=0;i<220;++i) {
    const auto s=flightState(fd);
    const double distance=std::hypot(x-s.x,y-s.y);
    if(distance<30 && helion::flight::speed(s)<12) {
      command(fd,"INPUT 0 0 1","FLIGHT "); return;
    }
    const double delta=std::remainder(std::atan2(-(x-s.x),y-s.y)-s.yaw,2*helion::flight::kPi);
    const int turn=std::abs(delta)<0.08 ? 0 : delta>0 ? 1 : -1;
    const bool brake=distance<45 || std::abs(delta)>0.35;
    command(fd,"INPUT "+std::to_string(!brake)+" "+std::to_string(turn)+" "+std::to_string(brake),"FLIGHT ");
    usleep(50000);
  }
  check(false,"live flight reaches destination");
}
} // namespace

int main(int argc, char** argv) {
  check(argc == 4, "server executable and certificate paths");
  std::signal(SIGPIPE,SIG_IGN);
  tlsContext = helion::tls::clientContext(argv[2]);
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
    execl(argv[1], argv[1], portText.c_str(), data.c_str(), "--max-clients", "1", "--cert", argv[2], "--key", argv[3], static_cast<char*>(nullptr));
    _exit(127);
  }
  childPid = child;

  int first = -1;
  for (int i = 0; i < 80 && first < 0; ++i) {
    usleep(50000);
    first = connectTo(port);
  }
  check(first >= 0, "server starts after migration");
  check(receiveUntil(first, "INFO commands=").find("WELCOME Helion/2") != std::string::npos, "version greeting");
  command(first,"LAUNCH","ERR login-required");
  // A different port must not bypass exclusive ownership of the same save.
  const std::string otherPort = std::to_string(freePort());
  competingPid = fork();
  check(competingPid >= 0, "fork competing server");
  if (competingPid == 0) {
    execl(argv[1], argv[1], otherPort.c_str(), data.c_str(), "--cert", argv[2], "--key", argv[3], static_cast<char*>(nullptr));
    _exit(127);
  }
  int competingStatus = 0;
  pid_t competingResult = 0;
  for (int i = 0; i < 80 && competingResult == 0; ++i) {
    usleep(50000);
    competingResult = waitpid(competingPid, &competingStatus, WNOHANG);
  }
  check(competingResult == competingPid && WIFEXITED(competingStatus) && WEXITSTATUS(competingStatus) != 0,
        "second server cannot open the same persistence file");
  competingPid = -1;
  const int second = connectTo(port, false);
  check(second >= 0, "second socket connects");
  pollfd busy{second,POLLIN,0};
  check(poll(&busy,1,1000)>0,"overload closes promptly");
  char closed;
  check(recv(second,&closed,1,0)==0,"overload does not emit plaintext");
  close(second);

  const std::string login = "LOGIN pilot legacy-pass-one\n";
  check(tlsSend(first, login.data(), login.size(), 0) == static_cast<ssize_t>(login.size()), "send legacy login");
  check(receiveUntil(first, "OK LOGIN").find("OK LOGIN user=pilot") != std::string::npos, "legacy login works");
  const std::string create = "CREATE explorer synthetic-password Explorer One\n";
  check(tlsSend(first, create.data(), create.size(), 0) == static_cast<ssize_t>(create.size()), "create durable profile");
  check(receiveUntil(first, "OK CREATED").find("OK CREATED user=explorer") != std::string::npos,
        "profile saved successfully");
  command(first,"BUY food 2","OK BOUGHT food quantity=2 total=40");
  command(first,"SELL food 1","OK SOLD food quantity=1 total=16");
  check(command(first,"CONTACTS","CONTACTS END").find("CONTACT HAULER-7 hauler") != std::string::npos,
        "moving hauler contact stream");
  command(first,"LAUNCH","OK LAUNCHED");
  command(first,"MINE","ERR asteroid-out-of-range");
  flyTo(first,0,220);
  command(first,"MINE","OK MINED cargo=1");
  command(first,"MINE","ERR mining-cooldown");
  command(first,"DOCK","ERR station-out-of-range");
  flyTo(first,0,35);
  // Make the save directory temporarily unavailable without relying on mode
  // bits (the test may run as root). A failed sale must preserve the cargo.
  const std::string movedDirectory=testDirectory+".away";
  check(rename(testDirectory.c_str(),movedDirectory.c_str())==0,"make save unavailable");
  const std::string failedDock="DOCK\n";
  const auto sentDock=tlsSend(first,failedDock.data(),failedDock.size(),0);
  const auto failedSale=receiveUntil(first,"ERR persistence-failed");
  check(rename(movedDirectory.c_str(),testDirectory.c_str())==0,"restore save directory");
  check(sentDock==static_cast<ssize_t>(failedDock.size()) &&
        failedSale.find("ERR persistence-failed")!=std::string::npos,"failed save rejects sale");
  const auto recoveredFlight=flightState(first);
  check(!recoveredFlight.docked && recoveredFlight.cargo==1,"failed sale preserves cargo and flight");
  command(first,"DOCK","OK DOCKED earned=60");
  check(flightState(first).docked,"docked live ship");
  const std::string chat = "CHAT Ready for launch\n";
  check(tlsSend(first, chat.data(), chat.size(), 0) == static_cast<ssize_t>(chat.size()), "send durable chat");
  check(receiveUntil(first, "CHAT user=explorer").find("text=Ready for launch") != std::string::npos,
        "chat saved successfully");
  const std::string shortPassword = "CREATE newcomer short New Pilot\n";
  check(tlsSend(first, shortPassword.data(), shortPassword.size(), 0) == static_cast<ssize_t>(shortPassword.size()), "send short password");
  check(receiveUntil(first, "ERR password-length").find("ERR password-length") != std::string::npos,
        "short new password rejected");
  const std::string invalidLogin = "LOGIN pilot wrong-password\n";
  for (int i = 0; i < 4; ++i) {
    check(tlsSend(first, invalidLogin.data(), invalidLogin.size(), 0) == static_cast<ssize_t>(invalidLogin.size()),
          "send invalid login");
    check(receiveUntil(first, "ERR invalid-login").find("ERR invalid-login") != std::string::npos,
          "invalid login rejected");
  }
  closeConnection(first);

  int replacement = -1;
  for (int i = 0; i < 40 && replacement < 0; ++i) {
    usleep(50000);
    replacement = connectTo(port);
    if (replacement >= 0 && receiveUntil(replacement, "INFO commands=").find("WELCOME Helion/2") == std::string::npos) {
      closeConnection(replacement);
      replacement = -1;
    }
  }
  check(replacement >= 0, "slot released after disconnect");
  closeConnection(replacement);

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

  check(stat((data + ".lock").c_str(), &info) == 0 && (info.st_mode & 0777) == 0600,
        "stable owner-only lock file remains after shutdown");
  // Restart over the retained lock inode and verify actual persisted data.
  childPid = fork();
  check(childPid >= 0, "fork restarted server");
  if (childPid == 0) {
    execl(argv[1], argv[1], portText.c_str(), data.c_str(), "--cert", argv[2], "--key", argv[3], static_cast<char*>(nullptr));
    _exit(127);
  }
  int restarted = -1;
  for (int i = 0; i < 80 && restarted < 0; ++i) {
    usleep(50000);
    restarted = connectTo(port);
  }
  check(restarted >= 0, "restart reacquires persistence lock");
  check(receiveUntil(restarted, "INFO commands=").find("WELCOME Helion/2") != std::string::npos,
        "restarted greeting");
  const std::string restoredLogin = "LOGIN explorer synthetic-password\n";
  check(tlsSend(restarted, restoredLogin.data(), restoredLogin.size(), 0) == static_cast<ssize_t>(restoredLogin.size()),
        "send restored login");
  check(receiveUntil(restarted, "OK LOGIN").find("OK LOGIN user=explorer display=Explorer One") != std::string::npos,
        "created account survives restart");
  const std::string queries = "PROFILE\nSTATE\n";
  check(tlsSend(restarted, queries.data(), queries.size(), 0) == static_cast<ssize_t>(queries.size()), "query restored state");
  const std::string restored = receiveUntil(restarted, "STATE profiles=3 messages=1");
  check(restored.find("ship=sidewinder credits=1536 experience=5") != std::string::npos &&
        restored.find("STATE profiles=3 messages=1") != std::string::npos, "profile and chat survive restart");
  closeConnection(restarted);
  kill(childPid, SIGTERM);
  check(waitpid(childPid, &status, 0) == childPid && WIFEXITED(status) && WEXITSTATUS(status) == 0,
        "restarted server shuts down cleanly");
  childPid = -1;

  const std::string malformed = "P\tpilot\tlegacy-pass-one\tPilot\tfree-traders\tsidewinder\tbad-credits\t0\n";
  {
    std::ofstream fixture(data, std::ios::trunc);
    fixture << malformed;
  }
  const pid_t rejected = fork();
  check(rejected >= 0, "fork malformed-file server");
  if (rejected == 0) {
    execl(argv[1], argv[1], portText.c_str(), data.c_str(), "--cert", argv[2], "--key", argv[3], static_cast<char*>(nullptr));
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
