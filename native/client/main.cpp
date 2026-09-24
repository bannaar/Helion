#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <memory>
#include <stdexcept>
#include <cmath>
#include <cctype>
#include <cstring>
#include <deque>
#include <iostream>
#include <netdb.h>
#include <poll.h>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include "shared/protocol.h"
#include "shared/organizations.h"
#include "shared/loadout.h"
#include "shared/ships.h"
#include "client/ui_input.h"
#include "client/acceptance_driver.h"
#include "client/render.h"
#include "client/core_renderer.h"
#include "client/graphics_runtime.h"
#include "shared/tls.h"

namespace {
int connectTo(const std::string& host, const std::string& port) {
  addrinfo hints{}; hints.ai_family=AF_UNSPEC; hints.ai_socktype=SOCK_STREAM;
  addrinfo* result=nullptr;
  if (getaddrinfo(host.c_str(),port.c_str(),&hints,&result)!=0) return -1;
  int fd=-1;
  for (auto* item=result;item;item=item->ai_next) {
    fd=socket(item->ai_family,item->ai_socktype,item->ai_protocol);
    if (fd>=0 && connect(fd,item->ai_addr,item->ai_addrlen)==0) break;
    if (fd>=0) { close(fd); fd=-1; }
  }
  freeaddrinfo(result); return fd;
}
std::string commandLine(std::string typed) {
  if (!typed.empty() && typed.front()=='/') typed.erase(0,1);
  const auto end=typed.find_first_of(" \t");
  std::transform(typed.begin(),end==std::string::npos ? typed.end() : typed.begin()+end,
    typed.begin(),[](unsigned char c){return std::toupper(c);});
  return typed;
}
bool saveFrame(const std::string& path,int width,int height) {
  std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*3);
  glPixelStorei(GL_PACK_ALIGNMENT,1); glReadBuffer(GL_BACK);
  glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
  if (glGetError()!=GL_NO_ERROR) return false;
  const std::size_t stride=static_cast<std::size_t>(width)*3;
  for(int y=0;y<height/2;++y)
    std::swap_ranges(pixels.begin()+y*stride,pixels.begin()+(y+1)*stride,pixels.begin()+(height-1-y)*stride);
  SDL_Surface* surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),width,height,24,width*3,SDL_PIXELFORMAT_RGB24);
  if (!surface) return false;
  const bool saved=SDL_SaveBMP(surface,path.c_str())==0;
  SDL_FreeSurface(surface); return saved;
}
int terminalClient(int fd, helion::tls::Connection& connection) {
  std::cout<<"Commands: /create, /login, /chat, /profile, /mission, /accept, /turnin, /galnet, /upgrade engine|hull, /repair, /refuel, /outfit LIST|BUY|FIT|REMOVE, /fire target-id, /recover, /contacts, /buy, /sell, /launch, /input, /flight, /mine, /dock, /quit\n";
  helion::protocol::LineDecoder decoder;
  bool greeted=false, running=true, stdinClosed=false, quitQueued=false;
  std::string outgoing, typed;
  while(running) {
    pollfd ready[2]={{fd,static_cast<short>(POLLIN|(outgoing.empty()?0:POLLOUT)),0},
      {STDIN_FILENO,static_cast<short>(stdinClosed?0:POLLIN),0}};
    if(poll(ready,2,1000)<0) { if(errno==EINTR) continue; return 1; }
    if(true) {
      char bytes[1024]; auto n=connection.read(bytes,sizeof(bytes));
      if(n==0 || (n<0 && errno!=EAGAIN)) break;
      if(n<0) n=0;
      for(const auto& frame:decoder.feed(std::string_view(bytes,static_cast<std::size_t>(n)))) {
        if(frame.kind!=helion::protocol::FrameKind::line) return 1;
        if(!greeted) {
          if(!helion::protocol::compatibleWelcome(frame.line)) { std::cerr<<"Incompatible server\n"; return 1; }
          greeted=true;
        }
        std::cout<<frame.line<<std::endl;
        if(frame.line=="OK BYE") running=false;
      }
    }
    if(ready[0].revents&(POLLHUP|POLLERR|POLLNVAL)) break;
    if(greeted && !stdinClosed && ready[1].revents&(POLLIN|POLLHUP)) {
      // Read bytes rather than getline: buffered stdin must not stall piped commands.
      char bytes[512]; const auto n=read(STDIN_FILENO,bytes,sizeof(bytes));
      if(n<=0) {
        stdinClosed=true;
        if(!quitQueued) { outgoing += "QUIT\n"; quitQueued=true; }
      }
      else for(int i=0;i<n;++i) {
        if(bytes[i]=='\n') {
          const auto request=commandLine(typed); typed.clear();
          if(helion::protocol::parseRequest(request).command==helion::protocol::Command::invalid)
            std::cout<<"Unknown or malformed command\n";
          else {
            if(request=="QUIT") quitQueued=true;
            outgoing+=request+"\n";
          }
        } else if(typed.size()<helion::protocol::kMaxLine) typed+=bytes[i];
      }
    }
    if(outgoing.size()>65536) return 1;
    if(!outgoing.empty()) {
      if (!connection.sendAll(outgoing, 5000)) return greeted ? 0 : 1;
      outgoing.clear();
    }
  }
  return greeted?0:1;
}

std::string fieldValue(std::string_view line, std::string_view key, std::string_view endMarker = {}) {
  const auto start = line.find(key);
  if (start == std::string_view::npos) return {};
  const auto valueStart = start + key.size();
  const auto end = endMarker.empty() ? line.find(' ', valueStart) : line.find(endMarker, valueStart);
  return std::string(line.substr(valueStart, end == std::string_view::npos ? std::string_view::npos : end - valueStart));
}

void updatePresentationMetadata(helion::client::View& view, std::string_view line) {
  if (line.rfind("PROFILE ", 0) == 0)
    view.commanderName = fieldValue(line, "display=", " faction=");
  else if (line.rfind("OK LOGIN ", 0) == 0 || line.rfind("OK CREATED ", 0) == 0)
    view.commanderName = fieldValue(line, "display=");
  if (line.rfind("MISSION ", 0) == 0) {
    if (line.find("active") != std::string_view::npos)
      view.missionSummary = "ACTIVE / MINE ORE AND RETURN TO STATION";
    else if (line.find("complete") != std::string_view::npos)
      view.missionSummary = "COMPLETE / ORION DELIVERY REWARDED";
    else view.missionSummary = "AVAILABLE / ACCEPT FIRST ORE CONTRACT";
  } else if (line.rfind("OK MISSION ACCEPTED", 0) == 0)
    view.missionSummary = "ACTIVE / MINE ORE AND RETURN TO STATION";
  else if (line.rfind("OK MISSION COMPLETE", 0) == 0)
    view.missionSummary = "COMPLETE / ORION DELIVERY REWARDED";
}

std::string boundedUiText(std::string_view value, std::size_t maximum = 220) {
  return std::string(value.substr(0, std::min(value.size(), maximum)));
}

int integerField(std::string_view line, std::string_view key, int fallback = 0) {
  const auto value = fieldValue(line, key);
  if (value.empty()) return fallback;
  try {
    std::size_t used = 0;
    const int parsed = std::stoi(value, &used);
    return used == value.size() ? parsed : fallback;
  } catch (...) { return fallback; }
}

double doubleField(std::string_view line, std::string_view key, double fallback = 0) {
  const auto value = fieldValue(line, key);
  if (value.empty()) return fallback;
  try {
    std::size_t used = 0;
    const double parsed = std::stod(value, &used);
    return used == value.size() && std::isfinite(parsed) ? parsed : fallback;
  } catch (...) { return fallback; }
}

std::string displayToken(std::string value) {
  std::replace(value.begin(), value.end(), '_', ' ');
  return value;
}

helion::client::UiShipyardRow shipyardRow(std::string_view line, bool owned) {
  helion::client::UiShipyardRow row;
  row.hullId = boundedUiText(fieldValue(line, "hull="), 64);
  if (!owned) row.hullId = boundedUiText(fieldValue(line, "id="), 64);
  row.instanceId = boundedUiText(fieldValue(line, "instance="), 64);
  const auto* hull = helion::ships::find(row.hullId);
  row.displayName = displayToken(fieldValue(line, "name="));
  if (hull) {
    if (row.displayName.empty()) row.displayName = std::string(hull->displayName);
    const auto* manufacturer = helion::ships::findManufacturer(hull->manufacturerId);
    const auto* operatorProfile = helion::ships::findOperator(hull->operatorId);
    row.manufacturer = manufacturer ? std::string(manufacturer->displayName) : std::string(hull->manufacturerId);
    row.operatorName = operatorProfile ? std::string(operatorProfile->displayName) : std::string(hull->operatorId);
    row.shipClass = std::string(hull->shipClass); row.role = std::string(hull->role);
    row.padSize = helion::ships::padSizeName(hull->padSize);
    row.price = hull->purchasePrice; row.speed = static_cast<int>(hull->topSpeed);
    row.boost = static_cast<int>(hull->boostSpeed); row.acceleration = static_cast<int>(hull->acceleration);
    row.handling = hull->maneuverability; row.hull = hull->baseHull; row.shields = hull->baseShields;
    row.cargo = hull->cargoCapacity; row.jumpRange = hull->jumpRangeLaden;
  }
  if (!owned) {
    if (const auto value = fieldValue(line, "manufacturer="); !value.empty())
      row.manufacturer = displayToken(value);
    if (const auto value = fieldValue(line, "operator="); !value.empty())
      row.operatorName = displayToken(value);
    if (const auto value = fieldValue(line, "class="); !value.empty())
      row.shipClass = displayToken(value);
    if (const auto value = fieldValue(line, "role="); !value.empty())
      row.role = displayToken(value);
    if (const auto value = fieldValue(line, "pad="); !value.empty()) row.padSize = value;
    row.price = integerField(line, "price=", row.price);
    row.speed = integerField(line, "speed=", row.speed); row.boost = integerField(line, "boost=", row.boost);
    row.acceleration = integerField(line, "acceleration=", row.acceleration);
    row.handling = integerField(line, "handling=", row.handling); row.hull = integerField(line, "hull=", row.hull);
    row.shields = integerField(line, "shields=", row.shields); row.cargo = integerField(line, "cargo=", row.cargo);
    row.jumpRange = doubleField(line, "jump-laden=", row.jumpRange);
  } else {
    row.state = fieldValue(line, "state="); row.active = row.state == "ACTIVE";
    const auto ownedOperator = fieldValue(line, "operator=");
    if (!ownedOperator.empty()) row.operatorName = displayToken(ownedOperator);
  }
  row.owned = owned;
  return row;
}

void splitModules(std::string_view value, std::vector<std::string>& modules) {
  modules.clear();
  std::size_t start = 0;
  while (start < value.size() && modules.size() < helion::loadout::kCatalogue.size()) {
    const auto end = value.find(',', start);
    const auto token = value.substr(start, end == std::string_view::npos ? value.size() - start : end - start);
    if (!token.empty() && token.size() <= 64) modules.emplace_back(token);
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
}

void parseLoadout(helion::client::View& view, std::string_view line) {
  const auto ownedStart = line.find("owned=");
  const auto fittedStart = line.find(" fitted-mining=");
  if (ownedStart != std::string_view::npos && fittedStart != std::string_view::npos)
    splitModules(line.substr(ownedStart + 6, fittedStart - (ownedStart + 6)), view.ui.ownedModules);
  view.ui.fittedModules[0] = fieldValue(line, "fitted-mining=");
  view.ui.fittedModules[1] = fieldValue(line, "fitted-engine=");
  view.ui.fittedModules[2] = fieldValue(line, "fitted-defense=");
  view.ui.fittedModules[3] = fieldValue(line, "fitted-weapon=");
}

void updateUiFromLine(helion::client::View& view, std::string_view line) {
  auto& ui = view.ui;
  if (line.rfind("CAREER ", 0) == 0) {
    const bool first = !ui.careerKnown;
    const bool wasComplete = ui.career.complete;
    ui.missionStage = std::clamp(integerField(line, "first="), 0, 2);
    ui.career.supply = std::clamp(integerField(line, "supply="), 0, 2);
    ui.career.response = std::clamp(integerField(line, "response="), 0, 2);
    ui.career.purchased = std::clamp(integerField(line, "purchased="), 0, 2);
    ui.career.introDismissed = integerField(line, "intro=") != 0;
    ui.career.complete = integerField(line, "complete=") != 0;
    ui.careerKnown = true;
    if (first && !ui.career.introDismissed) ui.screen = helion::client::UiScreen::help;
    if (!first && !wasComplete && ui.career.complete) ui.screen = helion::client::UiScreen::completion;
  }
  if (line.rfind("SHIPYARD BEGIN ", 0) == 0) {
    ui.shipyardRows.clear(); ui.selected = 0; ui.scrollOffset = 0;
  } else if (line.rfind("ACTIVE SHIP ", 0) == 0) {
    ui.activeHullId = boundedUiText(fieldValue(line, "hull="), 64);
    ui.activeHullName = displayToken(boundedUiText(fieldValue(line, "display="), 64));
    ui.cargoCapacity = std::max(0, integerField(line, "cargo-capacity=", helion::flight::kCargoCapacity));
  } else if (line.rfind("SHIPDEF ", 0) == 0) {
    auto row = shipyardRow(line, false);
    const auto existing = std::find_if(ui.shipyardRows.begin(), ui.shipyardRows.end(),
      [&row](const auto& item) { return !item.owned && item.hullId == row.hullId; });
    if (existing == ui.shipyardRows.end()) ui.shipyardRows.push_back(std::move(row)); else *existing = std::move(row);
  } else if (line.rfind("OWNEDSHIP ", 0) == 0) {
    auto row = shipyardRow(line, true);
    const auto existing = std::find_if(ui.shipyardRows.begin(), ui.shipyardRows.end(),
      [&row](const auto& item) { return item.owned && item.instanceId == row.instanceId; });
    if (existing == ui.shipyardRows.end()) ui.shipyardRows.push_back(std::move(row)); else *existing = std::move(row);
  } else if (line.rfind("PROFILE ", 0) == 0) {
    ui.engineLevel = std::clamp(integerField(line, "engine-level="), 1, 5);
    ui.hullLevel = std::clamp(integerField(line, "hull-level="), 1, 5);
    ui.salvage = std::max(0, integerField(line, "salvage="));
    ui.missionStage = std::clamp(integerField(line, "mission-stage="), 0, 2);
    ui.missionOreMined = integerField(line, "mission-ore-mined=") != 0;
    const auto reputationStart = line.find("reputation=");
    const auto loadoutStart = line.find(" LOADOUT");
    if (reputationStart != std::string_view::npos && loadoutStart != std::string_view::npos) {
      std::map<std::string, int> restored;
      if (helion::organizations::parseReputationSummary(
            line.substr(reputationStart + 11, loadoutStart - (reputationStart + 11)), restored))
        view.reputation = std::move(restored);
    }
    parseLoadout(view, line);
  } else if (line.rfind("MISSION ", 0) == 0) {
    ui.missionStage = line.find(" active") != std::string_view::npos ? 1 :
      line.find(" complete") != std::string_view::npos ? 2 : 0;
    ui.missionReward = std::max(0, integerField(line, "reward=", ui.missionReward));
    ui.missionTitle = "First Ore";
    ui.missionIssuerId = fieldValue(line, "issuer=");
    ui.missionJurisdictionId = fieldValue(line, "jurisdiction=");
  } else if (line.rfind("GALNET id=", 0) == 0) {
    const auto id = boundedUiText(fieldValue(line, "id="), 64);
    const auto headlineStart = line.find("headline=");
    if (headlineStart != std::string_view::npos && !id.empty()) {
      const auto existing = std::find_if(ui.galnet.begin(), ui.galnet.end(),
        [&id](const auto& entry) { return entry.id == id; });
      if (existing == ui.galnet.end()) {
        if (ui.galnet.size() >= 32) ui.galnet.erase(ui.galnet.begin());
        ui.galnet.push_back({id, boundedUiText(line.substr(headlineStart + 9), 180)});
      }
    }
  } else if (line.rfind("REPUTATION CHANGE ", 0) == 0) {
    const auto id = fieldValue(line, "id=");
    const int value = integerField(line, "value=", 0);
    if (helion::organizations::find(id)) view.reputation[id] = value;
  }
  if (line.rfind("ERR ", 0) == 0 || line.rfind("OK ", 0) == 0 ||
      line.rfind("TRANSACTION ", 0) == 0 || line.rfind("COMBAT ", 0) == 0 ||
      line.rfind("REPUTATION CHANGE ", 0) == 0)
    ui.statusMessage = boundedUiText(line);
}

bool validRenderState(std::string_view state) {
  return state == "normal" || state == "mining" || state == "target" || state == "combat" ||
    state == "docked" || state == "destroyed" || state == "galnet" || state == "galnet-ui" || state == "account" ||
    state == "station" || state == "market" || state == "mission" || state == "outfit" || state == "shipyard" ||
    state == "profile" || state == "options" || state == "graphics" || state == "error" ||
    state == "help" || state == "completion";
}

void configureRenderFixture(helion::client::View& view, std::string_view state) {
  view.connected = true;
  view.authenticated = true;
  view.console = false;
  view.commanderName = "ALPHA PILOT";
  view.missionSummary = "ACTIVE / MINE ORE AND RETURN TO STATION";
  view.reputation = {{"authority.kepler", 25}, {"corp.orion", 35}, {"criminal.red_wake", -25}};
  view.ui.connectionStatus = "TLS VERIFIED / COMMAND LINK ONLINE";
  view.ui.graphicsReport = "GRAPHICS requested=3.3-core actual=3.3-core first-compat=no fallback-21=no "
    "renderer-class=hardware vendor=Intel renderer=Mesa Intel(R) HD Graphics 3000 (SNB GT2) "
    "version=3.3 (Core Profile) Mesa 25.2.8 glsl=3.30";
  view.ui.ownedModules = {"mining-basic", "engine-basic", "hull-standard", "pulse-laser"};
  view.ui.fittedModules = {"mining-basic", "engine-basic", "hull-standard", "pulse-laser"};
  view.ui.missionStage = 1;
  view.ui.missionOreMined = state == "mission";
  view.ui.careerKnown = true;
  view.ui.career.supply = state == "completion" ? 2 : 1;
  view.ui.career.response = state == "completion" ? 2 : 0;
  view.ui.career.complete = state == "completion";
  view.ui.career.introDismissed = state != "help";
  view.time = 2;
  view.log = {"GALNET / Kepler Authority security watch active"};
  if (state == "docked") {
    view.ship.cargo = 4;
    view.credits = 1820;
    view.log.push_back("OK DOCKED cargo=4 sold=0 credits=1820");
    return;
  }
  if (state == "account") {
    view.authenticated = false;
    view.console = true;
    view.ui.consoleOpen = true;
    view.ui.screen = helion::client::UiScreen::account;
    view.ui.typed = "/login alpha ********";
    view.ui.connectionStatus = "TLS VERIFIED / AWAITING COMMANDER LOGIN";
    view.log.push_back("TLS VERIFIED / SERVER CERTIFICATE ACCEPTED");
    return;
  }
  if (state == "station" || state == "market" || state == "mission" || state == "outfit" || state == "shipyard" || state == "galnet-ui" ||
      state == "profile" || state == "options" || state == "graphics" || state == "error" ||
      state == "help" || state == "completion") {
    view.ui.screen = state == "station" ? helion::client::UiScreen::station :
      state == "market" ? helion::client::UiScreen::market :
      state == "mission" ? helion::client::UiScreen::mission :
      state == "outfit" ? helion::client::UiScreen::outfitting :
      state == "shipyard" ? helion::client::UiScreen::shipyard :
      state == "profile" ? helion::client::UiScreen::profile :
      state == "galnet-ui" ? helion::client::UiScreen::galnet :
      state == "options" ? helion::client::UiScreen::options :
      state == "graphics" ? helion::client::UiScreen::graphics :
      state == "error" ? helion::client::UiScreen::error :
      state == "help" ? helion::client::UiScreen::help :
      state == "completion" ? helion::client::UiScreen::completion : helion::client::UiScreen::station;
    view.ui.selected = state == "market" ? 0 : state == "outfit" ? 6 : 0;
    if (state == "error") view.ui.statusMessage = "ERR insufficient-credits / TRANSACTION REJECTED / NO STATE CHANGED";
    if (state == "galnet-ui") view.ui.galnet = {
      {"first-ore-available", "Orion Extraction Group requests first ore in Kepler"},
      {"supply-complete", "Kepler Authority confirms delivery from Cinder"},
      {"red-wake-defeated", "Red Wake raider defeated near Kepler"}};
    if (state == "shipyard") view.ui.shipyardRows = {
      shipyardRow("SHIPDEF id=TITAN_MULE", false),
      shipyardRow("SHIPDEF id=COMPACT_MILITIA", false),
      shipyardRow("OWNEDSHIP hull=SIDEWINDER instance=ship-render-0001 state=ACTIVE", true)};
    view.ship.docked = true;
    view.ship.station = 0;
    return;
  }
  helion::flight::launch(view.ship);
  view.ship.y = 205;
  view.ship.cargo = state == "mining" ? 1 : 0;
  view.beamUntil = state == "mining" ? 2.8 : 0;
  if (state == "destroyed") {
    view.ship.hull = 0;
    view.ship.destroyed = true;
    view.damageUntil = 2.8;
    view.log.push_back("COMBAT DESTROYED player=1 cargo-lost=1 recovery=RECOVER");
  } else if (state == "combat") {
    view.ship.hull = 70;
    view.ship.weaponCooldown = 0.4;
    view.weaponUntil = 2.5;
    view.damageUntil = 2.7;
    view.log.push_back("COMBAT HIT target=RAIDER-1 damage=25 hull=75");
  } else if (state == "galnet") {
    view.missionSummary = "COMPLETE / ORION DELIVERY REWARDED";
    view.reputation = {{"authority.kepler", 30}, {"corp.orion", 45}, {"criminal.red_wake", -35}};
    view.log = {"GALNET / Orion Extraction Group completed First Ore delivery",
      "GALNET / Red Wake raider defeated in Kepler",
      "REPUTATION CHANGE / Kepler Authority / +5 / Friendly",
      "OK MISSION COMPLETE reward=250"};
  }
  if (state == "target" || state == "combat" || state == "galnet") {
    view.contacts.push_back({"RAIDER-1", "hostile", 320, 310, 0, false, true, 75, 100,
      "criminal.red_wake", "Red Wake"});
    view.targetId = "RAIDER-1";
  }
}
} // namespace

int main(int argc,char** argv) {
  std::signal(SIGPIPE, SIG_IGN);
  bool renderCheck=false, graphicsInfo=false, terminal=false, acceptanceRun=false;
  helion::graphics::RendererMode rendererMode = helion::graphics::RendererMode::auto_mode;
  std::string host="127.0.0.1", port="4242", caFile, renderPath, renderState="combat";
  std::string acceptanceDirectory, acceptancePhase="journey", coreFault;
  double acceptanceSeconds = 600.0;
  int renderWidth = 960, renderHeight = 600;
  int positional=0;
  try {
    for(int i=1;i<argc;++i) {
      const std::string arg=argv[i];
      if(arg=="--terminal") terminal=true;
      else if(arg=="--graphics-info") graphicsInfo=true;
      else if(arg=="--renderer" && i+1<argc) {
        if(!helion::graphics::parseRendererMode(argv[++i],rendererMode))
          throw std::runtime_error("unknown renderer; use auto, legacy, or core");
      }
      else if(arg=="--ca" && i+1<argc) caFile=argv[++i];
      else if(arg=="--render-check" && i+1<argc) { renderCheck=true; renderPath=argv[++i]; }
      else if(arg=="--acceptance-run" && i+1<argc) { acceptanceRun=true; acceptanceDirectory=argv[++i]; }
      else if(arg=="--acceptance-phase" && i+1<argc) {
        acceptancePhase=argv[++i];
        if(acceptancePhase!="journey" && acceptancePhase!="reconnect" && acceptancePhase!="soak")
          throw std::runtime_error("acceptance phase must be journey, reconnect, or soak");
      }
      else if(arg=="--acceptance-seconds" && i+1<argc) {
        try { acceptanceSeconds = std::stod(argv[++i]); }
        catch (...) { throw std::runtime_error("invalid acceptance duration"); }
        if (!std::isfinite(acceptanceSeconds) || acceptanceSeconds < 1 || acceptanceSeconds > 3600)
          throw std::runtime_error("acceptance duration must be 1..3600 seconds");
      }
      else if(arg=="--test-core-failure" && i+1<argc) coreFault=argv[++i];
      else if(arg=="--render-state" && i+1<argc) {
        renderState=argv[++i];
        if (!validRenderState(renderState)) throw std::runtime_error("unknown render state");
      }
      else if(arg=="--render-size" && i+2<argc) {
        try { renderWidth = std::stoi(argv[++i]); renderHeight = std::stoi(argv[++i]); }
        catch (...) { throw std::runtime_error("invalid render size"); }
        if (renderWidth < 1 || renderHeight < 1 || renderWidth > 4096 || renderHeight > 4096)
          throw std::runtime_error("invalid render size");
      }
      else if(arg.rfind("--",0)==0) throw std::runtime_error("unknown or incomplete option");
      else if(positional++==0) host=arg;
      else if(positional==2) port=arg;
      else throw std::runtime_error("too many arguments");
    }
  } catch(const std::exception& error) {
    std::cerr<<error.what()<<"\nUsage: helion_client [host] [port] [--ca certificate.pem] [--terminal|--graphics-info] [--renderer auto|legacy|core] [--render-check path --render-state state --render-size width height]\n"; return 2;
  }
  if (acceptanceRun) {
    const char* enabled = std::getenv("HELION_ACCEPTANCE");
    if (!enabled || std::string_view(enabled) != "1" ||
        (host != "127.0.0.1" && host != "localhost" && host != "::1") ||
        rendererMode != helion::graphics::RendererMode::core || terminal || renderCheck || graphicsInfo) {
      std::cerr << "--acceptance-run requires HELION_ACCEPTANCE=1, loopback TLS, and --renderer core\n";
      return 2;
    }
  }
  if (!coreFault.empty()) {
    static const std::array<std::string_view, 10> faults{{"context", "version", "profile", "missing-function",
      "shader", "program", "atlas", "buffer", "renderer", "software"}};
    const char* enabled = std::getenv("HELION_GRAPHICS_TEST_FAULTS");
    if (!enabled || std::string_view(enabled) != "1" || !graphicsInfo ||
        std::find(faults.begin(), faults.end(), coreFault) == faults.end()) {
      std::cerr << "--test-core-failure is restricted to guarded --graphics-info validation\n";
      return 2;
    }
  }
  if (terminal && rendererMode == helion::graphics::RendererMode::core) {
    std::cerr << "--terminal cannot be used with the core renderer\n";
    return 2;
  }
  const bool noConnection = renderCheck || graphicsInfo;
  const int fd=noConnection ? -1 : connectTo(host,port);
  if(!noConnection && fd<0) { std::cerr<<"Unable to connect to "<<host<<':'<<port<<'\n'; return 1; }
  helion::tls::Context tlsContext(nullptr,SSL_CTX_free);
  std::unique_ptr<helion::tls::Connection> connection;
  if(!noConnection) {
    try {
      tlsContext=helion::tls::clientContext(caFile);
      connection=std::make_unique<helion::tls::Connection>(tlsContext.get(),fd);
      connection->handshake(false,host);
    } catch(const std::exception& error) {
      std::cerr<<error.what()<<'\n'; close(fd); return 1;
    }
  }
  if(terminal && !noConnection) {
    if(!connection) return 2;
    const int result=terminalClient(fd,*connection);
    connection->closeNotify(); shutdown(fd,SHUT_RDWR); close(fd); return result;
  }
  if(SDL_Init(SDL_INIT_VIDEO)!=0) { std::cerr<<SDL_GetError()<<'\n'; if(fd>=0) close(fd); return 1; }
  helion::client::GraphicsContext graphics;
  std::unique_ptr<helion::client::CoreRenderer> coreRenderer;
  bool coreMode = false;
  std::string coreFailure;
  if (rendererMode != helion::graphics::RendererMode::legacy) {
    if (coreFault == "context") coreFailure = "context: injected test failure";
    else graphics = helion::client::createCoreGraphicsContext(
      "Helion / Kepler Reach / Core", renderWidth, renderHeight, renderCheck || graphicsInfo);
    if (coreFailure.empty() && !graphics.result.selected) coreFailure = "context: " + graphics.result.error;
    else if (coreFailure.empty() && (coreFault == "version" || coreFault == "profile"))
      coreFailure = coreFault + ": injected test failure";
    else if (coreFailure.empty() && rendererMode == helion::graphics::RendererMode::auto_mode &&
             helion::graphics::classifyRenderer(graphics.result.actual.vendor,
                                                graphics.result.actual.renderer) !=
             helion::graphics::RendererClass::hardware) {
      coreFailure = "renderer-policy: automatic core requires a recognized hardware renderer";
    } else if (coreFailure.empty() && coreFault == "software") {
      coreFailure = "renderer-policy: injected software renderer";
    } else if (coreFailure.empty()) {
      coreRenderer = std::make_unique<helion::client::CoreRenderer>();
      std::string error;
      if (!coreRenderer->initialize(error, coreFault)) coreFailure = coreFault.empty() ? "resources: " + error : coreFault + ": " + error;
      else coreMode = true;
    }
    if (!coreMode) {
      coreRenderer.reset();
      helion::client::destroyGraphicsContext(graphics);
      if (rendererMode == helion::graphics::RendererMode::core) {
        std::cerr << "CORE-RENDERER initialization failed at " << coreFailure << '\n';
        SDL_Quit();
        if(fd>=0) close(fd);
        return 1;
      }
      std::cerr << "GRAPHICS AUTO core-failed=" << coreFailure << "; falling back to legacy\n";
    }
  }
  if (!coreMode)
    graphics = helion::client::createGraphicsContext("Helion / Kepler Reach", renderWidth, renderHeight,
                                                      renderCheck || graphicsInfo);
  if(!graphics.result.selected) {
    std::cerr << helion::graphics::diagnosticLine(graphics.result) << '\n';
    helion::client::destroyGraphicsContext(graphics);
    SDL_Quit();
    if(fd>=0) close(fd);
    return 1;
  }
  std::cout << helion::graphics::diagnosticLine(graphics.result) << '\n';
  std::cout << "GRAPHICS SELECT requested=" << helion::graphics::rendererModeName(rendererMode)
            << " selected=" << (coreMode ? "core" : "legacy")
            << (rendererMode == helion::graphics::RendererMode::auto_mode && !coreMode ? " fallback=yes" : " fallback=no")
            << '\n';
  SDL_Window* window=graphics.window;
  if (coreMode) {
    std::cout << "CORE-RENDERER shader=#version " << helion::client::kCoreShaderVersion
              << " scene=ship-station-grid\n";
  }
  if(graphicsInfo) {
    coreRenderer.reset();
    helion::client::destroyGraphicsContext(graphics);
    SDL_Quit();
    return 0;
  }
  SDL_SetWindowMinimumSize(window,960,600); SDL_GL_SetSwapInterval(1);
  helion::client::View view; view.connected=!renderCheck;
  view.ui.connectionStatus = renderCheck ? "TLS VERIFIED / RENDER FIXTURE" : "TLS VERIFIED / CONNECTED TO " + host + ":" + port;
  view.ui.graphicsReport = helion::graphics::diagnosticLine(graphics.result);
  view.log={helion::graphics::diagnosticLine(graphics.result), "TLS verified / Connected to "+host+":"+port};
  if(renderCheck) {
    configureRenderFixture(view, renderState);
    bool saved = false;
    if (coreRenderer) {
      std::string coreError;
      helion::client::CoreRenderStats stats;
      const auto snapshot = helion::client::makePresentationSnapshot(view);
      bool rendered = true;
      for (int frame = 0; frame < 3; ++frame)
        rendered = coreRenderer->render(renderWidth, renderHeight, snapshot, true, &stats, coreError) && rendered;
      saved = rendered && saveFrame(renderPath, renderWidth, renderHeight);
      std::cout << "CORE-STATE state=" << renderState << '\n';
      std::cout << "CORE-PERF frames=3 frame-ms=" << stats.frameMilliseconds << " draw-calls=" << stats.drawCalls
                << " static-vertices=" << stats.staticVertices << " world-vertices=" << stats.worldVertices
                << " hud-vertices=" << stats.hudVertices << " text-vertices=" << stats.textVertices
                << " text-indices=" << stats.textIndices
                << " text-glyph-vertices=" << stats.textGlyphVertices
                << " text-components=" << stats.textComponents << " text-bytes=" << stats.textBytes
                << " text-draw-calls=" << stats.textDrawCalls << " glyphs=" << stats.glyphs
                << " textures=" << stats.textures << " atlas=" << stats.atlasWidth << "x" << stats.atlasHeight
                << " atlas-bytes=" << stats.atlasBytes << " cpu-build-ms=" << stats.cpuBuildMilliseconds
                << " cpu-ui-build-ms=" << stats.cpuUiBuildMilliseconds
                << " cpu-text-build-ms=" << stats.cpuTextBuildMilliseconds
                << " render-ms=" << stats.renderMilliseconds << '\n';
      if (!coreError.empty()) std::cerr << "CORE-RENDERER render failed: " << coreError << '\n';
    } else {
      const auto snapshot = helion::client::makePresentationSnapshot(view);
      helion::client::render(renderWidth,renderHeight,view,snapshot);
      saved=saveFrame(renderPath,renderWidth,renderHeight);
      view.console=true; view.authenticated=false; view.typed="/login explorer ********";
      const auto consoleSnapshot = helion::client::makePresentationSnapshot(view);
      helion::client::render(renderWidth,renderHeight,view,consoleSnapshot);
      saved=saveFrame(renderPath+".console.bmp",renderWidth,renderHeight)&&saved;
    }
    coreRenderer.reset();
    helion::client::destroyGraphicsContext(graphics); SDL_Quit(); return saved?0:1;
  }
  bool running=true,greetingSeen=false,focused=true;
  helion::protocol::LineDecoder decoder;
  std::string outgoing;
  std::string pendingUiAction;
  auto queue=[&](const std::string& request) {
    if(!view.connected || !greetingSeen) { view.log.push_back("Command link not ready"); return; }
    if(!helion::protocol::validWireLine(request) || outgoing.size()+request.size()>65536) {
      view.log.push_back("Command queue full or invalid request"); return;
    }
    outgoing+=request+"\n";
  };
  auto queueUi=[&](const std::string& request) {
    // Background INPUT/CONTACTS traffic is expected during flight and may
    // share the bounded output queue with one UI action. Only another pending
    // transactional UI action should suppress activation.
    if (view.ui.commandPending) {
      view.ui.statusMessage = "WAIT / COMMAND IN PROGRESS";
      return false;
    }
    if (!view.connected || !greetingSeen || !helion::protocol::validWireLine(request)) return false;
    queue(request);
    view.ui.commandPending = true;
    const auto firstSpace = request.find(' ');
    pendingUiAction = request.substr(0, firstSpace);
    if (pendingUiAction == "CAREER" || pendingUiAction == "OUTFIT") {
      const auto secondEnd = request.find(' ', firstSpace + 1);
      pendingUiAction = request.substr(0, secondEnd);
    }
    return true;
  };
  helion::client::UiInput uiInput;
  helion::client::AcceptanceDriver acceptance(
    acceptancePhase == "reconnect" ? helion::client::AcceptanceDriver::Phase::reconnect :
    acceptancePhase == "soak" ? helion::client::AcceptanceDriver::Phase::soak :
                                 helion::client::AcceptanceDriver::Phase::journey,
    acceptanceRun ? acceptanceDirectory : std::string{}, acceptanceSeconds);
  const bool soakMode = acceptanceRun && acceptancePhase == "soak";
  bool soakFailed = false;
  int soakGlErrors = 0, soakRenderErrors = 0, soakSdlErrors = 0, soakDisconnects = 0;
  double soakStart = -1, soakFrameSum = 0, soakWorst = 0;
  std::uint64_t soakFrames = 0;
  int soakMaxDrawCalls = 0;
  std::size_t soakMaxWorldVertices = 0, soakMaxUiVertices = 0, soakMaxTextVertices = 0;
  std::size_t soakMaxTextBytes = 0, soakMaxAtlases = 0;
  int soakResizeStep = -1;
  if (coreMode) view.console = false;
  double lastTime=SDL_GetTicks64()/1000.0,lastInput=0,lastReply=lastTime;
  helion::flight::State visual=view.ship;
  bool heldForward=false, heldLeft=false, heldRight=false, heldBrake=false;
  SDL_StartTextInput();
  while(running) {
    const double now=SDL_GetTicks64()/1000.0,dt=std::min(now-lastTime,0.1); lastTime=now; view.time=now;
    view.ui.connected = view.connected;
    view.ui.authenticated = view.authenticated;
    helion::client::populateUiDerived(view.ui, view.ship);
    if (soakMode && soakStart < 0 && view.authenticated && view.ui.career.complete) soakStart = now;
    if (soakMode && soakStart >= 0) {
      if (!view.connected) {
        std::cerr << "SOAK FAIL unexpected-disconnect\n";
        ++soakDisconnects;
        soakFailed = true; running = false;
      }
      const int resizeStep = static_cast<int>((now - soakStart) / 30.0);
      if (resizeStep != soakResizeStep) {
        SDL_ClearError();
        SDL_SetWindowSize(window, resizeStep % 2 == 0 ? 960 : 1280,
                          resizeStep % 2 == 0 ? 600 : 720);
        if (const char* sdlError = SDL_GetError(); *sdlError) {
          std::cerr << "SOAK FAIL resize SDL error=" << sdlError << '\n';
          ++soakSdlErrors; soakFailed = true; running = false;
        }
        soakResizeStep = resizeStep;
      }
    }
    if (acceptance.enabled()) {
      int windowWidth = 0, windowHeight = 0;
      SDL_GetWindowSize(window, &windowWidth, &windowHeight);
      acceptance.tick(view, windowWidth, windowHeight, now);
    }
    SDL_Event event{};
    while(SDL_PollEvent(&event)) {
      if(event.type==SDL_QUIT) running=false;
      if(event.type==SDL_WINDOWEVENT) {
        if(event.window.event==SDL_WINDOWEVENT_FOCUS_LOST) {
          focused=false; heldForward=heldLeft=heldRight=heldBrake=false;
          if(view.authenticated) queue("INPUT 0 0 1");
        }
        if(event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED) focused=true;
      }
      if (coreMode) {
        if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && !event.key.repeat) {
          const bool down = event.type == SDL_KEYDOWN;
          const SDL_Keycode key = event.key.keysym.sym;
          if (key == SDLK_w || key == SDLK_UP) heldForward = down;
          if (key == SDLK_a || key == SDLK_LEFT) heldLeft = down;
          if (key == SDLK_d || key == SDLK_RIGHT) heldRight = down;
          if (key == SDLK_s || key == SDLK_DOWN) heldBrake = down;
        }
        int ww = 0, wh = 0; SDL_GetWindowSize(window, &ww, &wh);
        const auto request = uiInput.handle(view, event, ww, wh);
        if (request == "QUIT") running = false;
        else if (!request.empty()) queueUi(request);
        continue;
      }
      if(view.console && event.type==SDL_TEXTINPUT && view.typed.size()+std::strlen(event.text.text)<=helion::protocol::kMaxLine)
        view.typed+=event.text.text;
      if(event.type!=SDL_KEYDOWN || event.key.repeat) continue;
      const auto key=event.key.keysym.sym;
      if(key==SDLK_ESCAPE) {
        if (coreMode && !view.console && view.ui.screen != helion::client::UiScreen::flight &&
            view.ui.screen != helion::client::UiScreen::account) {
          view.ui.screen = view.ship.docked ? helion::client::UiScreen::station : helion::client::UiScreen::flight;
          view.ui.selected = 0;
        } else {
          view.console=!view.console; view.ui.consoleOpen=view.console; view.typed.clear();
        }
      }
      else if(key==SDLK_RETURN && (!coreMode || view.console ||
               view.ui.screen == helion::client::UiScreen::flight || !view.authenticated)) {
        if(!view.console) { view.console=true; view.ui.consoleOpen=true; view.typed.clear(); }
        else if(!view.typed.empty()) {
          const auto request=commandLine(view.typed);
          if(request=="QUIT") running=false;
          else if(helion::protocol::parseRequest(request).command==helion::protocol::Command::invalid)
            view.log.push_back("Unknown or malformed command");
          else { queue(request); view.log.push_back("> "+helion::client::redactCommand(view.typed)); }
          view.typed.clear();
        } else if(view.authenticated) { view.console=false; view.ui.consoleOpen=false; }
      } else if(view.console) {
        if(key==SDLK_BACKSPACE && !view.typed.empty()) {
          auto at=view.typed.size()-1;
          while(at>0 && (static_cast<unsigned char>(view.typed[at])&0xc0)==0x80) --at;
          view.typed.erase(at);
        }
      } else if(view.authenticated) {
        if(key==SDLK_l) queue("LAUNCH");
        if(key==SDLK_e) queue("MINE");
        if(key==SDLK_f) queue("DOCK");
        if(key==SDLK_r && view.ship.destroyed) queue("RECOVER");
        else if(key==SDLK_r && view.ship.docked) { view.console=true; view.typed="REPAIR"; }
        if(key==SDLK_t && view.ship.docked) queue("REFUEL");
        if(key==SDLK_u && view.ship.docked) { view.console=true; view.typed="OUTFIT LIST"; }
        if(key==SDLK_SPACE && !view.ship.docked && !view.ship.destroyed && !view.targetId.empty())
          queue("FIRE "+view.targetId);
        if(key==SDLK_TAB && !view.contacts.empty()) {
          std::size_t start = 0;
          for (std::size_t i = 0; i < view.contacts.size(); ++i)
            if (view.contacts[i].id == view.targetId) { start = (i + 1) % view.contacts.size(); break; }
          for (std::size_t i = 0; i < view.contacts.size(); ++i) {
            const auto& contact = view.contacts[(start + i) % view.contacts.size()];
            if (contact.hostile) { view.targetId = contact.id; break; }
          }
        }
        if(key==SDLK_b) { view.console=true; view.typed="BUY food 1"; }
        if(key==SDLK_F2) { view.console=true; view.typed="PROFILE"; }
        if(key==SDLK_F3) { view.showTelemetry=!view.showTelemetry; }
      }
    }
    const Uint8* keys=SDL_GetKeyboardState(nullptr);
    const bool controls=focused && !view.console && view.authenticated && view.connected && !view.ship.destroyed &&
      (!coreMode || view.ui.screen == helion::client::UiScreen::flight);
    const auto input=helion::flight::controls(coreMode ? heldForward : keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP],
      coreMode ? heldLeft : keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT],
      coreMode ? heldRight : keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT],
      coreMode ? heldBrake : keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN],controls);
    view.thrust=input.thrust;
    if(view.connected && greetingSeen && now-lastInput>=0.1) {
      if(view.authenticated) queue("INPUT "+std::to_string(input.thrust)+" "+std::to_string(input.turn)+" "+std::to_string(input.brake));
      else if(now-lastInput>=5) queue("STATE");
      if(view.authenticated || now-lastInput>=5) lastInput=now;
    }
    static double lastContacts=0;
    if(view.authenticated && now-lastContacts>1.0) { view.contacts.clear(); queue("CONTACTS"); lastContacts=now; }
    if(view.connected && !outgoing.empty()) {
      const auto count=connection->write(outgoing.data(),outgoing.size());
      if(count>0) outgoing.erase(0,static_cast<std::size_t>(count));
      else if(count<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR) view.connected=false;
    }
    for(int batch=0;view.connected && batch<16;++batch) {
      char bytes[1024]; const ssize_t count=connection->read(bytes,sizeof(bytes));
      if(count<=0) {
        if(count==0 || (errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR)) view.connected=false;
        break;
      }
      lastReply=now;
      for(auto& frame:decoder.feed(std::string_view(bytes,static_cast<std::size_t>(count)))) {
        if(!greetingSeen) {
          if(frame.kind!=helion::protocol::FrameKind::line || !helion::protocol::compatibleWelcome(frame.line)) {
            view.log.push_back("ERR incompatible server protocol"); view.connected=false; break;
          }
          greetingSeen=true;
          view.commandReady=true;
        }
        if(frame.kind!=helion::protocol::FrameKind::line) { view.connected=false; view.log.push_back("ERR malformed response"); break; }
        updatePresentationMetadata(view, frame.line);
        updateUiFromLine(view, frame.line);
        const auto& response = frame.line;
        const bool genericResult = response.rfind("OK ", 0) == 0 || response.rfind("ERR ", 0) == 0;
        const bool uiComplete = genericResult ||
          (pendingUiAction == "PROFILE" && response.rfind("PROFILE ", 0) == 0) ||
          (pendingUiAction == "FLIGHT" && response.rfind("FLIGHT ", 0) == 0) ||
          (pendingUiAction == "GALNET" && response == "GALNET END") ||
          (pendingUiAction == "CAREER" && response.rfind("CAREER ", 0) == 0) ||
          (pendingUiAction == "OUTFIT LIST" && response.rfind("LOADOUT ", 0) == 0) ||
          (pendingUiAction == "SHIPYARD" && response.rfind("SHIPYARD END", 0) == 0) ||
          (pendingUiAction == "CONTACTS" && response == "CONTACTS END") ||
          (pendingUiAction == "FIRE" && response.rfind("COMBAT HIT ", 0) == 0);
        if (view.ui.commandPending && uiComplete) {
          view.ui.commandPending = false;
          pendingUiAction.clear();
        }
        if(frame.line.rfind("FLIGHT ",0)==0) {
          if(!helion::flight::readSnapshot(frame.line,view.ship,view.credits,view.experience)) {
            view.connected=false; view.log.push_back("ERR malformed flight state"); break;
          }
          if(view.ship.docked) visual=view.ship;
        } else {
          if (frame.line.rfind("PROFILE ", 0) == 0) {
            const auto marker = frame.line.find(" reputation=");
            if (marker != std::string::npos) {
              const auto start = marker + 12;
              const auto end = frame.line.find(' ', start);
              std::map<std::string, int> restored;
              if (helion::organizations::parseReputationSummary(frame.line.substr(start, end - start), restored))
                view.reputation = std::move(restored);
            }
          } else if (frame.line.rfind("GALNET ", 0) == 0 && frame.line != "GALNET END") {
            const auto headline = frame.line.find(" headline=");
            if (headline != std::string::npos) view.log.push_back("GALNET / " + frame.line.substr(headline + 10));
          }
          const bool fuelFrame = helion::flight::readFuelLine(frame.line, view.ship);
          const bool combatStatus = helion::flight::readCombatStatus(frame.line, view.ship);
          helion::flight::Contact contact;
          if (fuelFrame || combatStatus) {
            // Fuel is an additive frame so older clients can still parse FLIGHT snapshots.
          } else if (helion::flight::readContact(frame.line, contact)) {
            view.contacts.push_back(std::move(contact));
          } else if (frame.line == "CONTACTS END") {
            // Contact frames are replaced atomically once the stream terminates.
            const auto selected = std::find_if(view.contacts.begin(), view.contacts.end(),
              [&view](const auto& item) { return item.hostile && item.id == view.targetId; });
            if (selected == view.contacts.end()) {
              const auto firstHostile = std::find_if(view.contacts.begin(), view.contacts.end(),
                [](const auto& item) { return item.hostile; });
              view.targetId = firstHostile == view.contacts.end() ? std::string{} : firstHostile->id;
            }
            view.log.push_back("CONTACTS / "+std::to_string(view.contacts.size())+" IN SECTOR");
          }
          if(frame.line.rfind("OK LOGIN",0)==0 || frame.line.rfind("OK CREATED",0)==0) {
            uiInput.clearCredentials();
            view.authenticated=true; view.console=false; view.ui.consoleOpen=false;
            view.ui.screen = view.ship.docked ? helion::client::UiScreen::station : helion::client::UiScreen::flight;
            view.ui.connectionStatus = "TLS VERIFIED / COMMANDER AUTHENTICATED";
            queue("FLIGHT"); queue("PROFILE"); queue("GALNET"); queue("OUTFIT LIST"); queue("CAREER");
          }
          if (frame.line.rfind("OK ", 0) == 0 && frame.line.rfind("OK LOGIN",0) != 0 &&
              frame.line.rfind("OK CREATED",0) != 0 && frame.line.rfind("OK BYE",0) != 0) {
            queue("PROFILE"); queue("CAREER"); queue("GALNET"); queue("OUTFIT LIST");
          }
          if (coreMode && (frame.line.rfind("OK LAUNCHED",0) == 0)) view.ui.screen = helion::client::UiScreen::flight;
          if (coreMode && (frame.line.rfind("OK DOCKED",0) == 0 || frame.line.rfind("OK RECOVERED",0) == 0))
            view.ui.screen = helion::client::UiScreen::station;
          if(frame.line.rfind("OK MINED",0)==0) view.beamUntil=now+0.45;
          if(frame.line.rfind("COMBAT HIT",0)==0 || frame.line.rfind("COMBAT DESTROYED target",0)==0)
            view.weaponUntil=now+0.28;
          if(frame.line.rfind("COMBAT DAMAGE",0)==0 || frame.line.rfind("COMBAT DESTROYED player",0)==0)
            view.damageUntil=now+0.55;
          if(frame.line.rfind("OK RECOVERED",0)==0) view.damageUntil=now+0.25;
          if(frame.line.rfind("COMBAT DESTROYED",0)==0) queue("PROFILE");
          helion::flight::DockTransaction sale;
          helion::flight::FuelTransaction refuel;
          if(!fuelFrame && !combatStatus && frame.line.rfind("STATE ",0)!=0) {
            if (helion::flight::readDockTransaction(frame.line, sale)) {
              view.log.push_back(std::move(frame.line));
              view.log.push_back("SALE / "+std::to_string(sale.cargoSold)+" ORE / +"+
                                 std::to_string(sale.creditsEarned)+" CR / +"+
                                 std::to_string(sale.experienceEarned)+" XP");
            } else if (helion::flight::readFuelTransaction(frame.line, refuel)) {
              view.log.push_back(std::move(frame.line));
              view.log.push_back("REFUEL / +"+std::to_string(static_cast<int>(std::ceil(refuel.fuelAdded)))+
                                 " FUEL / -"+std::to_string(refuel.creditsSpent)+" CR");
            } else {
              view.log.push_back(std::move(frame.line));
            }
          }
        }
      }
    }
    if(view.connected && now-lastReply>10) { view.connected=false; view.log.push_back("ERR command link timed out"); }
    if(!view.connected) { outgoing.clear(); view.thrust=false; view.ui.commandPending=false; pendingUiAction.clear(); }
    if(view.log.size()>64) view.log.erase(view.log.begin(),view.log.end()-64);
    int width,height; SDL_GL_GetDrawableSize(window,&width,&height);
    if(width>0 && height>0) {
      if (coreRenderer) {
        std::string coreError;
        const auto snapshot = helion::client::makePresentationSnapshot(view);
        helion::client::CoreRenderStats soakStats;
        if (!coreRenderer->render(width, height, snapshot, soakMode,
                                  soakMode ? &soakStats : nullptr, coreError)) {
          view.log.push_back("CORE RENDER ERROR / " + coreError);
          if (soakMode) {
            std::cerr << "SOAK FAIL renderer=" << coreError << '\n';
            ++soakRenderErrors;
            if (coreError.find("OpenGL error") != std::string::npos) ++soakGlErrors;
            soakFailed = true;
          }
          running = false;
        } else if (soakMode && soakStart >= 0) {
          ++soakFrames;
          soakFrameSum += soakStats.frameMilliseconds;
          soakWorst = std::max(soakWorst, soakStats.frameMilliseconds);
          soakMaxDrawCalls = std::max(soakMaxDrawCalls, soakStats.drawCalls);
          soakMaxWorldVertices = std::max(soakMaxWorldVertices, soakStats.worldVertices);
          soakMaxUiVertices = std::max(soakMaxUiVertices, soakStats.hudVertices);
          soakMaxTextVertices = std::max(soakMaxTextVertices, soakStats.textVertices);
          soakMaxTextBytes = std::max(soakMaxTextBytes, soakStats.textBytes);
          soakMaxAtlases = std::max(soakMaxAtlases, soakStats.textures);
        }
      } else {
        const double alpha=1-std::exp(-18*dt);
        visual.x+=(view.ship.x-visual.x)*alpha; visual.y+=(view.ship.y-visual.y)*alpha;
        visual.yaw+=std::remainder(view.ship.yaw-visual.yaw,2*helion::flight::kPi)*alpha;
        const auto authoritative=view.ship;
        view.ship.x=visual.x; view.ship.y=visual.y; view.ship.yaw=visual.yaw;
        const auto snapshot = helion::client::makePresentationSnapshot(view);
        helion::client::render(width,height,view,snapshot);
        view.ship=authoritative;
      }
      if (acceptance.enabled()) {
        const std::string capturePath = acceptance.consumeCapture();
        if (!capturePath.empty() && !saveFrame(capturePath, width, height)) {
          std::cerr << "ACCEPTANCE FAIL screenshot=" << capturePath << '\n';
          running = false;
        }
      }
      if (soakMode) SDL_ClearError();
      SDL_GL_SwapWindow(window);
      if (soakMode) {
        if (const char* sdlError = SDL_GetError(); *sdlError) {
          std::cerr << "SOAK FAIL swap SDL error=" << sdlError << '\n';
          ++soakSdlErrors; soakFailed = true; running = false;
        }
      }
    }
    if (acceptance.done() || acceptance.failed()) running=false;
    SDL_Delay(8);
  }
  if (soakMode) {
    std::cout << "SOAK duration-sec=" << (soakStart < 0 ? 0 : lastTime - soakStart)
              << " frames=" << soakFrames
              << " avg-frame-ms=" << (soakFrames ? soakFrameSum / static_cast<double>(soakFrames) : 0)
              << " worst-frame-ms=" << soakWorst
              << " max-draw-calls=" << soakMaxDrawCalls
              << " max-world-vertices=" << soakMaxWorldVertices
              << " max-ui-vertices=" << soakMaxUiVertices
              << " max-text-vertices=" << soakMaxTextVertices
              << " max-text-bytes=" << soakMaxTextBytes
              << " max-atlases=" << soakMaxAtlases
              << " gl-errors=" << soakGlErrors
              << " render-errors=" << soakRenderErrors
              << " sdl-errors=" << soakSdlErrors
              << " disconnects=" << soakDisconnects << '\n';
  }
  SDL_StopTextInput(); connection->sendAll("QUIT\n",500); connection->closeNotify();
  shutdown(fd,SHUT_RDWR); close(fd);
  coreRenderer.reset();
  helion::client::destroyGraphicsContext(graphics); SDL_Quit(); return acceptance.failed() || soakFailed ? 1 : 0;
}
