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
#include "client/render.h"
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
  std::cout<<"Commands: /create, /login, /chat, /profile, /mission, /accept, /turnin, /upgrade engine|hull, /contacts, /buy, /sell, /launch, /input, /flight, /mine, /dock, /quit\n";
  helion::protocol::LineDecoder decoder;
  bool greeted=false, running=true, stdinClosed=false, quitQueued=false;
  std::string outgoing, typed;
  while(running) {
    pollfd ready[2]={{fd,static_cast<short>(POLLIN|(outgoing.empty()?0:POLLOUT)),0},
      {stdinClosed?-1:STDIN_FILENO,POLLIN,0}};
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
} // namespace

int main(int argc,char** argv) {
  std::signal(SIGPIPE, SIG_IGN);
  bool renderCheck=false, terminal=false;
  std::string host="127.0.0.1", port="4242", caFile, renderPath;
  int positional=0;
  try {
    for(int i=1;i<argc;++i) {
      const std::string arg=argv[i];
      if(arg=="--terminal") terminal=true;
      else if(arg=="--ca" && i+1<argc) caFile=argv[++i];
      else if(arg=="--render-check" && i+1<argc) { renderCheck=true; renderPath=argv[++i]; }
      else if(arg.rfind("--",0)==0) throw std::runtime_error("unknown or incomplete option");
      else if(positional++==0) host=arg;
      else if(positional==2) port=arg;
      else throw std::runtime_error("too many arguments");
    }
  } catch(const std::exception& error) {
    std::cerr<<error.what()<<"\nUsage: helion_client [host] [port] [--ca certificate.pem] [--terminal]\n"; return 2;
  }
  const int fd=renderCheck?-1:connectTo(host,port);
  if(!renderCheck && fd<0) { std::cerr<<"Unable to connect to "<<host<<':'<<port<<'\n'; return 1; }
  helion::tls::Context tlsContext(nullptr,SSL_CTX_free);
  std::unique_ptr<helion::tls::Connection> connection;
  if(!renderCheck) {
    try {
      tlsContext=helion::tls::clientContext(caFile);
      connection=std::make_unique<helion::tls::Connection>(tlsContext.get(),fd);
      connection->handshake(false,host);
    } catch(const std::exception& error) {
      std::cerr<<error.what()<<'\n'; close(fd); return 1;
    }
  }
  if(terminal) {
    if(!connection) return 2;
    const int result=terminalClient(fd,*connection);
    connection->closeNotify(); shutdown(fd,SHUT_RDWR); close(fd); return result;
  }
  if(SDL_Init(SDL_INIT_VIDEO)!=0) { std::cerr<<SDL_GetError()<<'\n'; if(fd>=0) close(fd); return 1; }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
  SDL_Window* window=SDL_CreateWindow("Helion / Kepler Reach",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
    960,600,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|(renderCheck?SDL_WINDOW_HIDDEN:0));
  SDL_GLContext context=window?SDL_GL_CreateContext(window):nullptr;
  if(!context) { std::cerr<<SDL_GetError()<<'\n'; if(window) SDL_DestroyWindow(window); SDL_Quit(); if(fd>=0)close(fd); return 1; }
  SDL_SetWindowMinimumSize(window,960,600); SDL_GL_SetSwapInterval(1);
  helion::client::View view; view.connected=!renderCheck;
  view.log={"TLS verified / Connected to "+host+":"+port};
  if(renderCheck) {
    view.connected=true; view.authenticated=true; view.console=false;
    helion::flight::launch(view.ship); view.ship.y=205; view.ship.cargo=3;
    view.time=2; view.beamUntil=2.3; view.log={"OK MINED cargo=3"};
    helion::client::render(960,600,view);
    bool saved=saveFrame(renderPath,960,600);
    view.console=true; view.authenticated=false; view.typed="/login explorer synthetic-password";
    helion::client::render(960,600,view);
    saved=saveFrame(renderPath+".console.bmp",960,600)&&saved;
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit(); return saved?0:1;
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
  double lastTime=SDL_GetTicks64()/1000.0,lastInput=0,lastReply=lastTime;
  helion::flight::State visual=view.ship;
  SDL_StartTextInput();
  while(running) {
    const double now=SDL_GetTicks64()/1000.0,dt=std::min(now-lastTime,0.1); lastTime=now; view.time=now;
    SDL_Event event{};
    while(SDL_PollEvent(&event)) {
      if(event.type==SDL_QUIT) running=false;
      if(event.type==SDL_WINDOWEVENT) {
        if(event.window.event==SDL_WINDOWEVENT_FOCUS_LOST) { focused=false; if(view.authenticated) queue("INPUT 0 0 1"); }
        if(event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED) focused=true;
      }
      if(view.console && event.type==SDL_TEXTINPUT && view.typed.size()+std::strlen(event.text.text)<=helion::protocol::kMaxLine)
        view.typed+=event.text.text;
      if(event.type!=SDL_KEYDOWN || event.key.repeat) continue;
      const auto key=event.key.keysym.sym;
      if(key==SDLK_ESCAPE) { view.console=!view.console; view.typed.clear(); }
      else if(key==SDLK_RETURN) {
        if(!view.console) { view.console=true; view.typed.clear(); }
        else if(!view.typed.empty()) {
          const auto request=commandLine(view.typed);
          if(request=="QUIT") running=false;
          else if(helion::protocol::parseRequest(request).command==helion::protocol::Command::invalid)
            view.log.push_back("Unknown or malformed command");
          else { queue(request); view.log.push_back("> "+helion::client::redactCommand(view.typed)); }
          view.typed.clear();
        } else if(view.authenticated) view.console=false;
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
        if(key==SDLK_b) { view.console=true; view.typed="BUY food 1"; }
        if(key==SDLK_F2) { view.console=true; view.typed="PROFILE"; }
        if(key==SDLK_F3) { view.showTelemetry=!view.showTelemetry; }
      }
    }
    const Uint8* keys=SDL_GetKeyboardState(nullptr);
    const bool controls=focused && !view.console && view.authenticated && view.connected;
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
        if(frame.line.rfind("FLIGHT ",0)==0) {
          if(!helion::flight::readSnapshot(frame.line,view.ship,view.credits,view.experience)) {
            view.connected=false; view.log.push_back("ERR malformed flight state"); break;
          }
          if(view.ship.docked) visual=view.ship;
        } else {
          helion::flight::Contact contact;
          if (helion::flight::readContact(frame.line, contact)) {
            view.contacts.push_back(std::move(contact));
          } else if (frame.line == "CONTACTS END") {
            // Contact frames are replaced atomically once the stream terminates.
            view.log.push_back("CONTACTS / "+std::to_string(view.contacts.size())+" IN SECTOR");
          }
          if(frame.line.rfind("OK LOGIN",0)==0 || frame.line.rfind("OK CREATED",0)==0) {
            view.authenticated=true; view.console=false; queue("FLIGHT");
          }
          if(frame.line.rfind("OK MINED",0)==0) view.beamUntil=now+0.45;
          if(frame.line.rfind("STATE ",0)!=0) view.log.push_back(std::move(frame.line));
        }
      }
    }
    if(view.connected && now-lastReply>10) { view.connected=false; view.log.push_back("ERR command link timed out"); }
    if(!view.connected) { outgoing.clear(); view.thrust=false; }
    if(view.log.size()>64) view.log.erase(view.log.begin(),view.log.end()-64);
    const double alpha=1-std::exp(-18*dt);
    visual.x+=(view.ship.x-visual.x)*alpha; visual.y+=(view.ship.y-visual.y)*alpha;
    visual.yaw+=std::remainder(view.ship.yaw-visual.yaw,2*helion::flight::kPi)*alpha;
    const auto authoritative=view.ship;
    view.ship.x=visual.x; view.ship.y=visual.y; view.ship.yaw=visual.yaw;
    int width,height; SDL_GL_GetDrawableSize(window,&width,&height);
    if(width>0 && height>0) { helion::client::render(width,height,view); SDL_GL_SwapWindow(window); }
    view.ship=authoritative; SDL_Delay(8);
  }
  SDL_StopTextInput(); connection->sendAll("QUIT\n",500); connection->closeNotify();
  shutdown(fd,SHUT_RDWR); close(fd);
  SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit(); return 0;
}
