#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <algorithm>
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
  if (line.rfind("FLIGHT ", 0) == 0 || line.rfind("PROFILE ", 0) == 0 ||
      line.rfind("MISSION ", 0) == 0 || line.rfind("OUTFIT ", 0) == 0 ||
      line.rfind("GALNET ", 0) == 0 || line.rfind("OK ", 0) == 0 ||
      line.rfind("ERR ", 0) == 0 || line.rfind("TRANSACTION ", 0) == 0)
    ui.commandPending = false;
  if (line.rfind("PROFILE ", 0) == 0) {
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
      line.rfind("REPUTATION CHANGE ", 0) == 0 || line.rfind("GALNET ", 0) == 0)
    ui.statusMessage = boundedUiText(line);
}

bool validRenderState(std::string_view state) {
  return state == "normal" || state == "mining" || state == "target" || state == "combat" ||
    state == "docked" || state == "destroyed" || state == "galnet" || state == "account" ||
    state == "station" || state == "market" || state == "mission" || state == "outfit" ||
    state == "profile" || state == "options" || state == "graphics" || state == "error";
}

void configureRenderFixture(helion::client::View& view, std::string_view state) {
  view.connected = true;
  view.authenticated = true;
  view.console = false;
  view.commanderName = "ALPHA PILOT";
  view.missionSummary = "ACTIVE / MINE ORE AND RETURN TO STATION";
  view.reputation = {{"authority.kepler", 25}, {"corp.orion", 35}, {"criminal.red_wake", -25}};
  view.ui.connectionStatus = "TLS VERIFIED / COMMAND LINK ONLINE";
  view.ui.graphicsReport = "GRAPHICS requested=3.3-core actual=3.3-core renderer-class=hardware Intel HD 3000";
  view.ui.ownedModules = {"mining-basic", "engine-basic", "hull-standard", "pulse-laser"};
  view.ui.fittedModules = {"mining-basic", "engine-basic", "hull-standard", "pulse-laser"};
  view.ui.missionStage = 1;
  view.ui.missionOreMined = state == "mission";
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
  if (state == "station" || state == "market" || state == "mission" || state == "outfit" ||
      state == "profile" || state == "options" || state == "graphics" || state == "error") {
    view.ui.screen = state == "station" ? helion::client::UiScreen::station :
      state == "market" ? helion::client::UiScreen::market :
      state == "mission" ? helion::client::UiScreen::mission :
      state == "outfit" ? helion::client::UiScreen::outfitting :
      state == "profile" ? helion::client::UiScreen::profile :
      state == "options" ? helion::client::UiScreen::options :
      state == "graphics" ? helion::client::UiScreen::graphics :
      state == "error" ? helion::client::UiScreen::error : helion::client::UiScreen::station;
    view.ui.selected = state == "market" ? 0 : state == "outfit" ? 6 : 0;
    if (state == "error") view.ui.statusMessage = "ERR insufficient-credits / TRANSACTION REJECTED / NO STATE CHANGED";
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
  bool renderCheck=false, graphicsInfo=false, terminal=false;
  helion::graphics::RendererMode rendererMode = helion::graphics::RendererMode::auto_mode;
  std::string host="127.0.0.1", port="4242", caFile, renderPath, renderState="combat";
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
  bool coreProbeAvailable = false;
  if (rendererMode == helion::graphics::RendererMode::auto_mode) {
    auto coreProbe = helion::client::createCoreGraphicsContext("Helion core capability probe", 16, 16, true);
    coreProbeAvailable = coreProbe.result.selected;
    if (coreProbeAvailable) helion::client::destroyGraphicsContext(coreProbe);
    std::cout << "GRAPHICS CORE-PROBE available=" << (coreProbeAvailable ? "yes" : "no")
              << (coreProbe.result.error.empty() ? "" : " error=" + coreProbe.result.error) << '\n';
  }
  const auto selection = helion::graphics::selectRenderer(
    rendererMode, rendererMode == helion::graphics::RendererMode::core ? true : coreProbeAvailable);
  if (!selection.error.empty()) {
    std::cerr << selection.error << '\n';
    SDL_Quit();
    if(fd>=0) close(fd);
    return 1;
  }
  auto graphics = rendererMode == helion::graphics::RendererMode::core
    ? helion::client::createCoreGraphicsContext("Helion / Core diagnostic", renderWidth, renderHeight, renderCheck || graphicsInfo)
    : helion::client::createGraphicsContext("Helion / Kepler Reach", renderWidth, renderHeight, renderCheck || graphicsInfo);
  if(!graphics.result.selected) {
    std::cerr << helion::graphics::diagnosticLine(graphics.result) << '\n';
    helion::client::destroyGraphicsContext(graphics);
    SDL_Quit();
    if(fd>=0) close(fd);
    return 1;
  }
  std::cout << helion::graphics::diagnosticLine(graphics.result) << '\n';
  SDL_Window* window=graphics.window;
  const bool coreMode = rendererMode == helion::graphics::RendererMode::core;
  std::unique_ptr<helion::client::CoreRenderer> coreRenderer;
  if (coreMode) {
    coreRenderer = std::make_unique<helion::client::CoreRenderer>();
    std::string coreError;
    if (!coreRenderer->initialize(coreError)) {
      std::cerr << "CORE-RENDERER initialization failed: " << coreError << '\n';
      coreRenderer.reset();
      helion::client::destroyGraphicsContext(graphics);
      SDL_Quit();
      return 1;
    }
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
      view.console=true; view.authenticated=false; view.typed="/login explorer synthetic-password";
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
  auto queue=[&](const std::string& request) {
    if(!view.connected || !greetingSeen) { view.log.push_back("Command link not ready"); return; }
    if(!helion::protocol::validWireLine(request) || outgoing.size()+request.size()>65536) {
      view.log.push_back("Command queue full or invalid request"); return;
    }
    outgoing+=request+"\n";
  };
  auto queueUi=[&](const std::string& request) {
    if (!outgoing.empty() || view.ui.commandPending) {
      view.ui.statusMessage = "WAIT / COMMAND IN PROGRESS";
      return false;
    }
    if (!view.connected || !greetingSeen || !helion::protocol::validWireLine(request)) return false;
    queue(request);
    view.ui.commandPending = true;
    return true;
  };
  int mouseX = -1, mouseY = -1;
  const auto updateMouseHover = [&](int x, int y) {
    if (!coreMode) return;
    int windowWidth = 0, windowHeight = 0, drawableWidth = 0, drawableHeight = 0;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
    if (windowWidth <= 0 || windowHeight <= 0 || drawableWidth <= 0 || drawableHeight <= 0) return;
    const auto point = helion::client::UiPoint{
      static_cast<float>(x) * drawableWidth / static_cast<float>(windowWidth),
      static_cast<float>(y) * drawableHeight / static_cast<float>(windowHeight)};
    const auto hit = helion::client::hitTestUi(view.ui, point, drawableWidth, drawableHeight);
    view.ui.hoveredControl = hit.id;
    view.ui.hoveredIndex = hit.index;
  };
  const auto activateMouseControl = [&](const helion::client::UiHitResult& hit) {
    if (hit.id == helion::client::UiControlId::none) return;
    view.ui.focusedControl = hit.id;
    if (!hit.enabled) {
      view.ui.statusMessage = "CONTROL DISABLED / WAIT OR MEET STATION REQUIREMENTS";
      return;
    }
    using helion::client::UiControlId;
    using helion::client::UiScreen;
    switch (hit.id) {
      case UiControlId::accountInput:
        view.console = true;
        view.ui.consoleOpen = true;
        return;
      case UiControlId::stationMarket: view.ui.selected = 0; view.ui.screen = UiScreen::market; queueUi("FLIGHT"); return;
      case UiControlId::stationMission: view.ui.screen = UiScreen::mission; queueUi("MISSION"); return;
      case UiControlId::stationOutfitting: view.ui.selected = 0; view.ui.screen = UiScreen::outfitting; queueUi("OUTFIT LIST"); return;
      case UiControlId::stationProfile: view.ui.screen = UiScreen::profile; queueUi("PROFILE"); return;
      case UiControlId::stationGalnet: view.ui.screen = UiScreen::galnet; queueUi("GALNET"); return;
      case UiControlId::stationOptions: view.ui.screen = UiScreen::options; return;
      case UiControlId::stationGraphics: view.ui.screen = UiScreen::graphics; return;
      case UiControlId::stationLaunch:
        if (queueUi("LAUNCH")) view.ui.screen = UiScreen::flight;
        return;
      case UiControlId::stationRepair: queueUi("REPAIR"); return;
      case UiControlId::stationRefuel: queueUi("REFUEL"); return;
      case UiControlId::marketFood:
      case UiControlId::marketParts:
        view.ui.selected = hit.index;
        return;
      case UiControlId::marketQuantityDown:
        view.ui.quantity = std::max(1, view.ui.quantity - 1);
        return;
      case UiControlId::marketQuantityUp:
        view.ui.quantity = std::min(helion::flight::kCargoCapacity, view.ui.quantity + 1);
        return;
      case UiControlId::marketBuy:
      case UiControlId::marketSell:
        queueUi(helion::client::uiCommandFor(hit.id, hit.index, view.ui));
        return;
      case UiControlId::missionAccept:
      case UiControlId::missionTurnIn:
        queueUi(helion::client::uiCommandFor(hit.id, hit.index, view.ui));
        return;
      case UiControlId::outfitRow:
        view.ui.selected = hit.index;
        return;
      case UiControlId::outfitBuy:
      case UiControlId::outfitFit:
      case UiControlId::outfitRemove:
        queueUi(helion::client::uiCommandFor(hit.id, view.ui.selected, view.ui));
        return;
      case UiControlId::galnetEntry:
        view.ui.selected = hit.index - view.ui.scrollOffset;
        return;
      case UiControlId::optionsTelemetry:
        view.showTelemetry = !view.showTelemetry;
        view.ui.telemetryEnabled = view.showTelemetry;
        view.ui.statusMessage = std::string("TELEMETRY / ") + (view.showTelemetry ? "ON" : "OFF");
        return;
      case UiControlId::dismiss:
        view.ui.screen = view.ship.docked ? UiScreen::station : UiScreen::flight;
        view.ui.selected = 0;
        return;
      case UiControlId::none: return;
    }
  };
  double lastTime=SDL_GetTicks64()/1000.0,lastInput=0,lastReply=lastTime;
  helion::flight::State visual=view.ship;
  SDL_StartTextInput();
  while(running) {
    const double now=SDL_GetTicks64()/1000.0,dt=std::min(now-lastTime,0.1); lastTime=now; view.time=now;
    view.ui.connected = view.connected;
    view.ui.authenticated = view.authenticated;
    helion::client::populateUiDerived(view.ui, view.ship);
    SDL_Event event{};
    while(SDL_PollEvent(&event)) {
      if(event.type==SDL_QUIT) running=false;
      if(event.type==SDL_WINDOWEVENT) {
        if(event.window.event==SDL_WINDOWEVENT_FOCUS_LOST) { focused=false; if(view.authenticated) queue("INPUT 0 0 1"); }
        if(event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED) focused=true;
        if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED && mouseX >= 0)
          updateMouseHover(mouseX, mouseY);
      }
      if (coreMode && event.type == SDL_MOUSEMOTION) {
        mouseX = event.motion.x;
        mouseY = event.motion.y;
        updateMouseHover(mouseX, mouseY);
      }
      if (coreMode && event.type == SDL_MOUSEWHEEL && view.ui.screen != helion::client::UiScreen::flight) {
        view.ui.scrollOffset = std::clamp(view.ui.scrollOffset - event.wheel.y, 0,
          helion::client::uiMaxScroll(view.ui));
        if (view.ui.screen == helion::client::UiScreen::galnet)
          view.ui.selected = std::clamp(view.ui.selected, 0, 8);
      }
      if (coreMode && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        mouseX = event.button.x;
        mouseY = event.button.y;
        int windowWidth = 0, windowHeight = 0, drawableWidth = 0, drawableHeight = 0;
        SDL_GetWindowSize(window, &windowWidth, &windowHeight);
        SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
        if (windowWidth > 0 && windowHeight > 0 && drawableWidth > 0 && drawableHeight > 0) {
          const auto point = helion::client::UiPoint{
            static_cast<float>(mouseX) * drawableWidth / static_cast<float>(windowWidth),
            static_cast<float>(mouseY) * drawableHeight / static_cast<float>(windowHeight)};
          const auto hit = helion::client::hitTestUi(view.ui, point, drawableWidth, drawableHeight);
          activateMouseControl(hit);
          updateMouseHover(mouseX, mouseY);
        }
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
      } else if(coreMode && view.authenticated && (view.ui.screen != helion::client::UiScreen::flight ||
                (key >= SDLK_F1 && key <= SDLK_F8) || key == SDLK_o)) {
        const auto open = [&](helion::client::UiScreen screen, const std::string& request = std::string()) {
          view.ui.screen = screen;
          view.ui.selected = 0;
          view.ui.scrollOffset = 0;
          view.ui.focusedControl = helion::client::UiControlId::none;
          if (!request.empty()) queueUi(request);
        };
        if (key==SDLK_F1 && view.ship.docked) open(helion::client::UiScreen::station);
        else if (key==SDLK_F2) open(helion::client::UiScreen::profile, "PROFILE");
        else if (key==SDLK_F3) {
          view.showTelemetry = !view.showTelemetry;
          view.ui.statusMessage = std::string("TELEMETRY / ") + (view.showTelemetry ? "ON" : "OFF");
          view.ui.screen = helion::client::UiScreen::options;
        }
        else if (key==SDLK_F4) open(helion::client::UiScreen::market, "FLIGHT");
        else if (key==SDLK_F5) open(helion::client::UiScreen::mission, "MISSION");
        else if (key==SDLK_F6) open(helion::client::UiScreen::outfitting, "OUTFIT LIST");
        else if (key==SDLK_F7) open(helion::client::UiScreen::galnet, "GALNET");
        else if (key==SDLK_F8) open(helion::client::UiScreen::graphics);
        else if (key==SDLK_o) open(helion::client::UiScreen::options);
        else if (key==SDLK_UP || key==SDLK_DOWN) {
          int maximum = 0;
          if (view.ui.screen == helion::client::UiScreen::station) maximum = 9;
          else if (view.ui.screen == helion::client::UiScreen::market) maximum = 1;
          else if (view.ui.screen == helion::client::UiScreen::outfitting) maximum = static_cast<int>(helion::loadout::kCatalogue.size()) - 1;
          else if (view.ui.screen == helion::client::UiScreen::galnet)
            maximum = std::max(0, std::min(9, static_cast<int>(view.ui.galnet.size()) - view.ui.scrollOffset) - 1);
          view.ui.selected = std::clamp(view.ui.selected + (key==SDLK_DOWN ? 1 : -1), 0, maximum);
        }
        else if ((key==SDLK_PAGEUP || key==SDLK_PAGEDOWN) && view.ui.screen == helion::client::UiScreen::galnet) {
          view.ui.scrollOffset = std::clamp(view.ui.scrollOffset + (key==SDLK_PAGEDOWN ? 1 : -1), 0,
            helion::client::uiMaxScroll(view.ui));
          view.ui.selected = std::clamp(view.ui.selected, 0, 8);
        }
        else if ((key==SDLK_EQUALS || key==SDLK_KP_PLUS) && view.ui.screen == helion::client::UiScreen::market)
          view.ui.quantity = std::min(helion::flight::kCargoCapacity, view.ui.quantity + 1);
        else if ((key==SDLK_MINUS || key==SDLK_KP_MINUS) && view.ui.screen == helion::client::UiScreen::market)
          view.ui.quantity = std::max(1, view.ui.quantity - 1);
        else if (key==SDLK_RETURN) {
          if (view.ui.screen == helion::client::UiScreen::station) {
            switch (view.ui.selected) {
              case 0: open(helion::client::UiScreen::market, "FLIGHT"); break;
              case 1: open(helion::client::UiScreen::mission, "MISSION"); break;
              case 2: open(helion::client::UiScreen::outfitting, "OUTFIT LIST"); break;
              case 3: open(helion::client::UiScreen::profile, "PROFILE"); break;
              case 4: open(helion::client::UiScreen::galnet, "GALNET"); break;
              case 5: open(helion::client::UiScreen::options); break;
              case 6: open(helion::client::UiScreen::graphics); break;
              case 7: if (queueUi("LAUNCH")) view.ui.screen = helion::client::UiScreen::flight; break;
              case 8: queueUi("REPAIR"); break;
              case 9: queueUi("REFUEL"); break;
            }
          } else if (view.ui.screen == helion::client::UiScreen::market) {
            const char* commodity = view.ui.selected == 0 ? "food" : "parts";
            queueUi("BUY " + std::string(commodity) + " " + std::to_string(view.ui.quantity));
          } else if (view.ui.screen == helion::client::UiScreen::mission) {
            if (view.ui.missionStage == 0) queueUi("ACCEPT");
            else if (view.ui.missionStage == 1 && view.ui.missionOreMined) queueUi("TURNIN");
            else view.ui.statusMessage = view.ui.missionStage == 1 ? "MISSION ACTIVE / COMPLETE THE ORE OBJECTIVE" : "MISSION COMPLETE / REWARD ALREADY PAID";
          } else if (view.ui.screen == helion::client::UiScreen::error ||
                     view.ui.screen == helion::client::UiScreen::profile ||
                     view.ui.screen == helion::client::UiScreen::graphics) {
            open(view.ship.docked ? helion::client::UiScreen::station : helion::client::UiScreen::flight);
          } else if (view.ui.screen == helion::client::UiScreen::outfitting) {
            const auto& module = helion::loadout::kCatalogue[static_cast<std::size_t>(view.ui.selected)];
            queueUi((std::find(view.ui.ownedModules.begin(), view.ui.ownedModules.end(), module.id) == view.ui.ownedModules.end() ?
              "OUTFIT BUY " : "OUTFIT FIT ") + std::string(module.id));
          }
        }
        else if (key==SDLK_b && view.ui.screen == helion::client::UiScreen::market) {
          queueUi("BUY " + std::string(view.ui.selected == 0 ? "food" : "parts") + " " + std::to_string(view.ui.quantity));
        }
        else if (key==SDLK_s && view.ui.screen == helion::client::UiScreen::market) {
          queueUi("SELL " + std::string(view.ui.selected == 0 ? "food" : "parts") + " " + std::to_string(view.ui.quantity));
        }
        else if (key==SDLK_b && view.ui.screen == helion::client::UiScreen::outfitting) {
          queueUi("OUTFIT BUY " + std::string(helion::loadout::kCatalogue[static_cast<std::size_t>(view.ui.selected)].id));
        }
        else if (key==SDLK_f && view.ui.screen == helion::client::UiScreen::outfitting) {
          queueUi("OUTFIT FIT " + std::string(helion::loadout::kCatalogue[static_cast<std::size_t>(view.ui.selected)].id));
        }
        else if (key==SDLK_r && view.ui.screen == helion::client::UiScreen::outfitting) {
          queueUi("OUTFIT REMOVE " + std::string(helion::loadout::slotName(helion::loadout::kCatalogue[static_cast<std::size_t>(view.ui.selected)].slot)));
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
    const auto input=helion::flight::controls(keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP],
      keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT], keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT],
      keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN],controls);
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
        }
        if(frame.kind!=helion::protocol::FrameKind::line) { view.connected=false; view.log.push_back("ERR malformed response"); break; }
        updatePresentationMetadata(view, frame.line);
        updateUiFromLine(view, frame.line);
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
            view.authenticated=true; view.console=false; view.ui.consoleOpen=false;
            view.ui.screen = view.ship.docked ? helion::client::UiScreen::station : helion::client::UiScreen::flight;
            view.ui.connectionStatus = "TLS VERIFIED / COMMANDER AUTHENTICATED";
            queue("FLIGHT"); queue("PROFILE"); queue("GALNET"); queue("OUTFIT LIST");
          }
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
    if(!view.connected) { outgoing.clear(); view.thrust=false; }
    if(view.log.size()>64) view.log.erase(view.log.begin(),view.log.end()-64);
    int width,height; SDL_GL_GetDrawableSize(window,&width,&height);
    if(width>0 && height>0) {
      if (coreRenderer) {
        std::string coreError;
        const auto snapshot = helion::client::makePresentationSnapshot(view);
        if (!coreRenderer->render(width, height, snapshot, false, nullptr, coreError)) {
          view.log.push_back("CORE RENDER ERROR / " + coreError);
          running = false;
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
      SDL_GL_SwapWindow(window);
    }
    SDL_Delay(8);
  }
  SDL_StopTextInput(); connection->sendAll("QUIT\n",500); connection->closeNotify();
  shutdown(fd,SHUT_RDWR); close(fd);
  coreRenderer.reset();
  helion::client::destroyGraphicsContext(graphics); SDL_Quit(); return 0;
}
