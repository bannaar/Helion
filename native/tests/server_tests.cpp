#include <arpa/inet.h>
#include <atomic>
#include <chrono>
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
#include <thread>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <vector>
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

std::string expectPersistenceFailure(int fd, const std::string& value) {
  const std::string movedDirectory = testDirectory + ".away";
  check(rename(testDirectory.c_str(), movedDirectory.c_str()) == 0, "make save unavailable for rollback");
  const std::string bytes = value + "\n";
  check(tlsSend(fd, bytes.data(), bytes.size(), 0) == static_cast<ssize_t>(bytes.size()), "send rollback command");
  const auto response = receiveUntil(fd, "ERR persistence-failed");
  check(rename(movedDirectory.c_str(), testDirectory.c_str()) == 0, "restore save directory after rollback");
  check(response.find("ERR persistence-failed") != std::string::npos, "durable action rolls back on save failure");
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
  check(receiveUntil(first, "OK LOGIN").find("OK LOGIN user=pilot") != std::string::npos,
        "legacy persistence without mission objective field loads");
  check(flightState(first).docked, "legacy commander starts docked");
  command(first,"LAUNCH","OK LAUNCHED");
  command(first,"INPUT 1 0 0","FLIGHT ");
  usleep(200000);
  const auto pilotFlightProfile = command(first,"PROFILE","PROFILE");
  check(pilotFlightProfile.find("max-fuel=100") != std::string::npos &&
        pilotFlightProfile.find(" fuel=100") == std::string::npos,
        "authoritative thrust consumes fuel");
  flyTo(first,0,35);
  command(first,"DOCK","TRANSACTION DOCK_SALE");
  check(command(first,"REFUEL","TRANSACTION REFUEL").find("OK REFUELED") != std::string::npos,
        "docked commander can purchase server-priced fuel");
  check(command(first,"OUTFIT BUY engine-efficient","TRANSACTION MODULE_PURCHASE").find("credits=700") != std::string::npos,
        "server prices module purchase");
  command(first,"OUTFIT FIT engine-efficient","OK MODULE FIT");
  command(first,"OUTFIT FIT pulse-laser","ERR module-not-owned");
  expectPersistenceFailure(first,"OUTFIT BUY pulse-laser");
  check(command(first,"OUTFIT LIST","LOADOUT").find("pulse-laser") == std::string::npos,
        "failed module purchase rolls back ownership");
  check(command(first,"OUTFIT LIST","LOADOUT").find("fitted-engine=engine-efficient") != std::string::npos,
        "module fit is visible");
  const std::string create = "CREATE explorer synthetic-password Explorer One\n";
  check(tlsSend(first, create.data(), create.size(), 0) == static_cast<ssize_t>(create.size()), "create durable profile");
  check(receiveUntil(first, "OK CREATED").find("OK CREATED user=explorer") != std::string::npos,
        "profile saved successfully");
  const auto starter = flightState(first);
  check(starter.docked && starter.cargo == 0 && starter.hull == starter.maxHull,
        "new account starts docked and ready");
  command(first,"REPAIR","ERR hull-full");
  command(first,"MISSION","MISSION 1 title=First Ore");
  expectPersistenceFailure(first,"ACCEPT");
  check(command(first,"MISSION","MISSION 1 title=First Ore").find("title=First Ore") != std::string::npos,
        "failed mission acceptance rolls back stage");
  expectPersistenceFailure(first,"UPGRADE engine");
  const auto failedUpgradeProfile = command(first,"PROFILE","PROFILE");
  check(failedUpgradeProfile.find("credits=1500") != std::string::npos &&
        failedUpgradeProfile.find("engine-level=1") != std::string::npos,
        "failed engine upgrade rolls back profile");
  command(first,"ACCEPT","OK MISSION ACCEPTED");
  command(first,"TURNIN","ERR objective-incomplete");
  command(first,"BUY food 2","OK BOUGHT food quantity=2 total=40");
  expectPersistenceFailure(first,"BUY parts 1");
  check(command(first,"PROFILE","PROFILE").find("credits=1460") != std::string::npos,
        "failed trade rolls back credits");
  command(first,"SELL food 1","OK SOLD food quantity=1 total=16");
  check(command(first,"CONTACTS","CONTACTS END").find("CONTACT HAULER-7 hauler") != std::string::npos,
        "moving hauler contact stream");
  expectPersistenceFailure(first,"LAUNCH");
  check(flightState(first).docked,"failed launch rolls back docked state");
  command(first,"LAUNCH","OK LAUNCHED");
  command(first,"MINE","ERR asteroid-out-of-range");
  flyTo(first,0,220);
  expectPersistenceFailure(first,"MINE");
  check(flightState(first).cargo == 0,"failed mining rolls back cargo");
  check(command(first,"PROFILE","PROFILE").find("mission-ore-mined=0") != std::string::npos,
        "failed mining rolls back mission objective");
  command(first,"TURNIN","ERR objective-incomplete");
  command(first,"MINE","OK MINED cargo=1");
  check(flightState(first).cargo == 1,"successful mining updates authoritative cargo");
  check(command(first,"PROFILE","PROFILE").find("mission-ore-mined=1") != std::string::npos,
        "successful mining marks mission objective");
  command(first,"MINE","ERR mining-cooldown");
  command(first,"DOCK","ERR station-out-of-range");
  flyTo(first,0,35);
  const auto failedSale = expectPersistenceFailure(first,"DOCK");
  check(failedSale.find("TRANSACTION DOCK_SALE") == std::string::npos,
        "failed sale emits no transaction result");
  const auto recoveredFlight=flightState(first);
  check(!recoveredFlight.docked && recoveredFlight.cargo==1,"failed sale preserves cargo and flight");
  const auto sale = command(first,"DOCK","TRANSACTION DOCK_SALE");
  check(sale.find("OK DOCKED earned=60") != std::string::npos &&
        sale.find("TRANSACTION DOCK_SALE station=0 quantity=1 unit-price=60 credits=60 experience=5") != std::string::npos,
        "server reports authoritative dock sale result");
  check(command(first,"PROFILE","PROFILE").find("credits=1536 experience=5") != std::string::npos,
        "sale commits credits and experience");
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
  check(tlsSend(replacement, login.data(), login.size(), 0) == static_cast<ssize_t>(login.size()),
        "reconnect legacy commander");
  check(receiveUntil(replacement, "OK LOGIN").find("OK LOGIN user=pilot") != std::string::npos,
        "reconnected commander authenticates");
  const auto restoredPilot = command(replacement,"PROFILE","PROFILE");
  check(restoredPilot.find("credits=1500") == std::string::npos &&
        restoredPilot.find(" fuel=100") != std::string::npos &&
        restoredPilot.find("fitted-engine=engine-efficient") != std::string::npos,
        "fuel and loadout survive reconnect");
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
  {
    std::ifstream input(data);
    std::ostringstream rewritten;
    std::string line;
    while (std::getline(input, line)) {
      std::vector<std::string> fields;
      std::size_t start = 0;
      while (start <= line.size()) {
        const auto end = line.find('\t', start);
        fields.push_back(line.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
      }
      if (fields.size() >= 23 && fields[0] == "H" && fields[1] == "explorer") {
        fields[21] = "50";
        fields[22] = "100";
        line.clear();
        for (std::size_t i = 0; i < fields.size(); ++i) {
          if (i != 0) line.push_back('\t');
          line += fields[i];
        }
      }
      rewritten << line << '\n';
    }
    std::ofstream output(data, std::ios::trunc);
    output << rewritten.str();
  }
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
  const auto restoredFlight = flightState(restarted);
  check(restoredFlight.docked && restoredFlight.cargo == 0 && restoredFlight.hull == 50,
        "docked sold state survives reconnect");
  expectPersistenceFailure(restarted,"REPAIR");
  const auto failedRepairProfile = command(restarted,"PROFILE","PROFILE");
  check(failedRepairProfile.find("credits=1536") != std::string::npos &&
        failedRepairProfile.find("hull=50") != std::string::npos &&
        failedRepairProfile.find("mission-ore-mined=1") != std::string::npos,
        "failed hull repair rolls back profile");
  command(restarted,"REPAIR","OK REPAIRED hull=100 cost=150");
  const std::string queries = "PROFILE\nSTATE\n";
  check(tlsSend(restarted, queries.data(), queries.size(), 0) == static_cast<ssize_t>(queries.size()), "query restored state");
  const std::string restored = receiveUntil(restarted, "STATE profiles=3 messages=1");
  check(restored.find("ship=sidewinder credits=1386 experience=5") != std::string::npos &&
        restored.find("STATE profiles=3 messages=1") != std::string::npos, "profile and chat survive restart");
  command(restarted,"TURNIN","OK MISSION COMPLETE");
  check(command(restarted,"PROFILE","PROFILE").find("mission-stage=2 mission-ore-mined=0") != std::string::npos,
        "turnin clears objective after completion");
  command(restarted,"TURNIN","ERR mission-not-active");
  command(restarted,"LAUNCH","OK LAUNCHED");
  std::ifstream checkpointBeforeFile(data);
  const std::string checkpointBefore((std::istreambuf_iterator<char>(checkpointBeforeFile)), std::istreambuf_iterator<char>());
  command(restarted,"INPUT 1 0 0","FLIGHT");
  bool checkpointObserved = false;
  for (int i = 0; i < 150 && !checkpointObserved; ++i) {
    usleep(20000);
    std::ifstream checkpointAfterFile(data);
    const std::string checkpointAfter((std::istreambuf_iterator<char>(checkpointAfterFile)), std::istreambuf_iterator<char>());
    checkpointObserved = checkpointAfter != checkpointBefore;
  }
  check(checkpointObserved, "dirty flight state reaches periodic checkpoint");
  closeConnection(restarted);
  kill(childPid, SIGTERM);
  check(waitpid(childPid, &status, 0) == childPid && WIFEXITED(status) && WEXITSTATUS(status) == 0,
        "restarted server shuts down cleanly");
  childPid = -1;

  const int isolationPort = freePort();
  childPid = fork();
  check(childPid >= 0, "fork slow-client server");
  if (childPid == 0) {
    const std::string isolationPortText = std::to_string(isolationPort);
    execl(argv[1], argv[1], isolationPortText.c_str(), data.c_str(), "--max-clients", "4", "--cert", argv[2], "--key", argv[3], static_cast<char*>(nullptr));
    _exit(127);
  }
  int slow = -1;
  for (int i = 0; i < 80 && slow < 0; ++i) {
    usleep(50000);
    slow = connectTo(isolationPort);
  }
  check(slow >= 0, "slow-client server accepts first TLS client");
  check(receiveUntil(slow, "INFO commands=").find("WELCOME Helion/2") != std::string::npos, "slow-client greeting");
  command(slow,"LOGIN explorer synthetic-password","OK LOGIN");
  command(slow,"TURNIN","ERR mission-not-active");
  int receiveBuffer = 1024;
  check(setsockopt(slow, SOL_SOCKET, SO_RCVBUF, &receiveBuffer, sizeof(receiveBuffer)) == 0,
        "constrain slow client receive buffer");
  int fast = -1;
  for (int i = 0; i < 80 && fast < 0; ++i) {
    usleep(50000);
    fast = connectTo(isolationPort);
  }
  check(fast >= 0, "slow-client server accepts second TLS client");
  check(receiveUntil(fast, "INFO commands=").find("WELCOME Helion/2") != std::string::npos, "fast-client greeting");
  command(fast,"LOGIN explorer synthetic-password","OK LOGIN");
  command(fast,"INPUT 1 0 0","FLIGHT");
  const auto baseline = flightState(fast);
  std::string flood;
  for (int i = 0; i < 160; ++i) flood += "CONTACTS\n";
  std::atomic<bool> floodStarted{false};
  std::thread slowFlood([&]() {
    floodStarted.store(true);
    (void)tlsSend(slow, flood.data(), flood.size(), 0);
  });
  for (int i = 0; i < 20 && !floodStarted.load(); ++i) usleep(10000);
  usleep(150000);
  const auto fastStart = std::chrono::steady_clock::now();
  const std::string stateRequest = "STATE\n";
  const auto stateSent = tlsSend(fast, stateRequest.data(), stateRequest.size(), 0);
  const auto stateResponse = receiveUntil(fast, "STATE profiles=3");
  const auto fastElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - fastStart).count();
  const std::string flightRequest = "FLIGHT\n";
  const auto flightSent = tlsSend(fast, flightRequest.data(), flightRequest.size(), 0);
  const auto flightResponse = receiveUntil(fast, "FLIGHT ");
  helion::flight::State after;
  int afterCredits = 0, afterExperience = 0;
  const auto flightStart = flightResponse.find("FLIGHT ");
  const auto flightEnd = flightResponse.find('\n', flightStart);
  const bool parsedFlight = flightStart != std::string::npos && flightEnd != std::string::npos &&
    helion::flight::readSnapshot(flightResponse.substr(flightStart, flightEnd - flightStart), after, afterCredits, afterExperience);
  shutdown(slow, SHUT_RDWR);
  slowFlood.join();
  closeConnection(slow);
  closeConnection(fast);
  kill(childPid, SIGTERM);
  check(waitpid(childPid, &status, 0) == childPid && WIFEXITED(status) && WEXITSTATUS(status) == 0,
        "slow-client server shuts down cleanly");
  childPid = -1;
  check(stateSent == static_cast<ssize_t>(stateRequest.size()) &&
        stateResponse.find("STATE profiles=3") != std::string::npos && fastElapsed < 2000,
        "slow client does not block another client response");
  check(flightSent == static_cast<ssize_t>(flightRequest.size()) &&
        flightResponse.find("FLIGHT ") != std::string::npos &&
        parsedFlight && (std::abs(after.x - baseline.x) > 0.001 || std::abs(after.y - baseline.y) > 0.001 ||
          std::abs(after.vx - baseline.vx) > 0.001 || std::abs(after.vy - baseline.vy) > 0.001),
        "authoritative simulation remains available during slow client");

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
  std::string trailingObjective = content;
  const auto explorerRecord = trailingObjective.find("H\texplorer\t");
  const auto explorerEnd = trailingObjective.find('\n', explorerRecord);
  check(explorerRecord != std::string::npos && explorerEnd != std::string::npos, "find persisted explorer record");
  trailingObjective.insert(explorerEnd, "\textra");
  {
    std::ofstream fixture(data, std::ios::trunc);
    fixture << trailingObjective;
  }
  const pid_t trailingRejected = fork();
  check(trailingRejected >= 0, "fork trailing-field server");
  if (trailingRejected == 0) {
    execl(argv[1], argv[1], portText.c_str(), data.c_str(), "--cert", argv[2], "--key", argv[3], static_cast<char*>(nullptr));
    _exit(127);
  }
  childPid = trailingRejected;
  check(waitpid(trailingRejected, &status, 0) == trailingRejected && WIFEXITED(status) && WEXITSTATUS(status) != 0,
        "unexpected persistence fields block startup");
  childPid = -1;
  std::ifstream trailingPreserved(data);
  const std::string afterTrailingFailure((std::istreambuf_iterator<char>(trailingPreserved)), std::istreambuf_iterator<char>());
  check(afterTrailingFailure == trailingObjective, "trailing-field failure preserves source file");
  std::string oldFormat = content;
  const auto oldRecord = oldFormat.find("H\texplorer\t");
  const auto oldEnd = oldFormat.find('\n', oldRecord);
  const auto fieldTab = [](const std::string& source, std::size_t record, std::size_t field) {
    auto position = record;
    for (std::size_t index = 0; index < field; ++index) {
      position = source.find('\t', position + 1);
      if (position == std::string::npos) return position;
    }
    return position;
  };
  const auto oldObjectiveField = fieldTab(oldFormat, oldRecord, 23);
  check(oldRecord != std::string::npos && oldEnd != std::string::npos &&
        oldObjectiveField != std::string::npos && oldObjectiveField > oldRecord,
        "find old-format mission objective field");
  oldFormat.erase(oldObjectiveField, oldEnd - oldObjectiveField);
  {
    std::ofstream fixture(data, std::ios::trunc);
    fixture << oldFormat;
  }
  const pid_t oldFormatServer = fork();
  check(oldFormatServer >= 0, "fork old-format server");
  if (oldFormatServer == 0) {
    execl(argv[1], argv[1], portText.c_str(), data.c_str(), "--cert", argv[2], "--key", argv[3], static_cast<char*>(nullptr));
    _exit(127);
  }
  childPid = oldFormatServer;
  int oldFormatConnection = -1;
  for (int i = 0; i < 80 && oldFormatConnection < 0; ++i) {
    usleep(50000);
    oldFormatConnection = connectTo(port);
  }
  check(oldFormatConnection >= 0, "old-format persistence starts server");
  check(receiveUntil(oldFormatConnection, "INFO commands=").find("WELCOME Helion/2") != std::string::npos,
        "old-format persistence greeting");
  command(oldFormatConnection,"LOGIN explorer synthetic-password","OK LOGIN");
  check(command(oldFormatConnection,"PROFILE","PROFILE").find("mission-ore-mined=0") != std::string::npos,
        "old-format mission objective defaults false");
  closeConnection(oldFormatConnection);
  kill(childPid, SIGTERM);
  check(waitpid(childPid, &status, 0) == childPid && WIFEXITED(status) && WEXITSTATUS(status) == 0,
        "old-format server shuts down cleanly");
  childPid = -1;
  std::string invalidObjective = content;
  const auto invalidRecord = invalidObjective.find("H\texplorer\t");
  const auto invalidEnd = invalidObjective.find('\n', invalidRecord);
  const auto objectiveField = fieldTab(invalidObjective, invalidRecord, 23);
  check(invalidRecord != std::string::npos && invalidEnd != std::string::npos &&
        objectiveField != std::string::npos && objectiveField > invalidRecord,
        "find mission objective field");
  invalidObjective.replace(objectiveField + 1, invalidEnd - objectiveField - 1, "2");
  {
    std::ofstream fixture(data, std::ios::trunc);
    fixture << invalidObjective;
  }
  const pid_t invalidObjectiveRejected = fork();
  check(invalidObjectiveRejected >= 0, "fork invalid-objective server");
  if (invalidObjectiveRejected == 0) {
    execl(argv[1], argv[1], portText.c_str(), data.c_str(), "--cert", argv[2], "--key", argv[3], static_cast<char*>(nullptr));
    _exit(127);
  }
  childPid = invalidObjectiveRejected;
  check(waitpid(invalidObjectiveRejected, &status, 0) == invalidObjectiveRejected && WIFEXITED(status) && WEXITSTATUS(status) != 0,
        "invalid mission objective blocks startup");
  childPid = -1;
  std::ifstream invalidObjectivePreserved(data);
  const std::string afterInvalidObjectiveFailure((std::istreambuf_iterator<char>(invalidObjectivePreserved)),
                                                  std::istreambuf_iterator<char>());
  check(afterInvalidObjectiveFailure == invalidObjective, "invalid objective failure preserves source file");
  cleanup();
  std::cout << "server integration tests passed\n";
  return 0;
}
