#include "client/render.h"
#include "client/presentation.h"
#include "shared/organizations.h"
#include <SDL2/SDL_opengl.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace helion::client {
namespace {
struct Color { float r, g, b; };
constexpr Color ink{0.025f, 0.042f, 0.064f}, panel{0.047f, 0.075f, 0.105f},
  line{0.14f, 0.23f, 0.28f}, white{0.83f, 0.89f, 0.9f}, muted{0.42f, 0.57f, 0.61f},
  teal{0.25f, 0.88f, 0.76f}, amber{1.0f, 0.67f, 0.29f}, hostileColor{1.0f, 0.32f, 0.32f};
void color(Color c) { glColor3f(c.r, c.g, c.b); }
void rect(float x, float y, float w, float h, Color c) {
  color(c); glBegin(GL_QUADS);
  glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h); glEnd();
}
void triangle(float ax, float ay, float bx, float by, float cx, float cy, Color c) {
  color(c); glBegin(GL_TRIANGLES); glVertex2f(ax,ay); glVertex2f(bx,by); glVertex2f(cx,cy); glEnd();
}
void ring(double x, double y, double radius, Color c, int sides = 64) {
  color(c); glBegin(GL_LINE_LOOP);
  for (int i=0; i<sides; ++i) {
    const double a = 2*flight::kPi*i/sides;
    glVertex2d(x+std::cos(a)*radius,y+std::sin(a)*radius);
  }
  glEnd();
}
// Original 5x7 cockpit glyphs, rows encoded as five-bit masks.
std::array<unsigned char,7> glyph(char ch) {
  switch (static_cast<char>(std::toupper(static_cast<unsigned char>(ch)))) {
    case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14}; case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31}; case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,15}; case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14}; case 'J': return {7,2,2,2,18,18,12};
    case 'K': return {17,18,20,24,20,18,17}; case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,25,21,19,19,17};
    case 'O': return {14,17,17,17,17,17,14}; case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13}; case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30}; case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14}; case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10}; case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4}; case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,1,14};
    case '/': return {1,1,2,4,8,16,16}; case '-': return {0,0,0,31,0,0,0};
    case '.': return {0,0,0,0,0,12,12}; case ':': return {0,12,12,0,12,12,0};
    case '=': return {0,0,31,0,31,0,0}; case '>': return {16,8,4,2,4,8,16};
    case '<': return {1,2,4,8,4,2,1}; case '[': return {14,8,8,8,8,8,14};
    case ']': return {14,2,2,2,2,2,14}; case '_': return {0,0,0,0,0,0,31};
    case '*': return {0,21,14,31,14,21,0}; case '+': return {0,4,4,31,4,4,0};
    case '|': return {4,4,4,4,4,4,4}; case ' ': return {};
    default: return {14,17,1,2,4,0,4};
  }
}
void text(float x, float y, const std::string& value, Color c = white, float scale = 2) {
  color(c); glBegin(GL_QUADS);
  for (char ch : value) {
    const auto rows = glyph(ch);
    for (int row=0; row<7; ++row) for (int col=0; col<5; ++col) if (rows[row] & (1 << (4-col))) {
      const float px=x+col*scale, py=y+row*scale;
      glVertex2f(px,py); glVertex2f(px+scale,py); glVertex2f(px+scale,py+scale); glVertex2f(px,py+scale);
    }
    x += 6*scale;
  }
  glEnd();
}
void rockAsset(const flight::Rock& rock, int seed) {
  std::array<std::array<float,2>,10> vertices{};
  for (int i=0; i<10; ++i) {
    const double a = i*2*flight::kPi/10 + seed;
    const double r = rock.radius*(0.8+0.2*std::sin(i*7.1+seed));
    vertices[i] = {static_cast<float>(rock.x+std::cos(a)*r),static_cast<float>(rock.y+std::sin(a)*r)};
  }
  for (int i=0; i<10; ++i) {
    const auto& a=vertices[i]; const auto& b=vertices[(i+1)%10];
    const float shade = 0.23f+0.035f*(i%5);
    triangle(static_cast<float>(rock.x-5),static_cast<float>(rock.y+6),a[0],a[1],b[0],b[1],{shade,shade*1.04f,shade*1.1f});
  }
  for (int i=0; i<3; ++i) {
    const float x=static_cast<float>(rock.x+i*9-10), y=static_cast<float>(rock.y+(i%2)*10-4);
    triangle(x,y,x+6,y+2,x+2,y+10,teal);
  }
}
void stationAsset(double time) {
  ring(0,0,flight::kDockRange,{0.13f,0.28f,0.29f});
  glPushMatrix(); glRotated(time*3,0,0,1);
  for (int i=0;i<4;++i) {
    glPushMatrix(); glRotated(i*90,0,0,1);
    rect(-7,15,14,46,line); rect(-25,41,50,19,{0.22f,0.31f,0.37f});
    rect(-23,43,46,3,muted); rect(-20,49,40,7,panel);
    for (int j=0;j<5;++j) rect(-18+j*8,50,3,4,teal);
    glPopMatrix();
  }
  ring(0,0,48,muted,12); ring(0,0,52,line,12);
  for (int i=0;i<8;++i) {
    double a=i*flight::kPi/4,b=(i+1)*flight::kPi/4;
    triangle(0,0,static_cast<float>(std::cos(a)*24),static_cast<float>(std::sin(a)*24),
      static_cast<float>(std::cos(b)*24),static_cast<float>(std::sin(b)*24),i%2 ? muted : line);
  }
  rect(-9,-13,18,26,ink); rect(-6,-10,12,20,teal);
  glPopMatrix();
}
void shipAsset(const View& v) {
  glPushMatrix(); glTranslated(v.ship.x,v.ship.y,0); glRotated(v.ship.yaw*180/flight::kPi,0,0,1);
  if (v.thrust && !v.ship.docked && !v.console) {
    float tail = static_cast<float>(20+5*std::sin(v.time*43));
    triangle(-5,-10,5,-10,0,-tail,teal);
    triangle(-2,-10,2,-10,0,-tail+4,white);
  }
  triangle(0,22,-18,-13,0,-6,{0.39f,0.5f,0.56f});
  triangle(0,22,18,-13,0,-6,{0.69f,0.76f,0.77f});
  triangle(-18,-13,-7,5,-9,-11,line); triangle(18,-13,7,5,9,-11,line);
  triangle(0,12,-4,1,4,1,ink); triangle(0,10,-2,3,2,3,teal);
  rect(-16,-11,3,3,amber); rect(13,-11,3,3,teal);
  glPopMatrix();
}
} // namespace

std::string redactCommand(const std::string& command) {
  std::string result = command;
  std::string normalized=command;
  std::transform(normalized.begin(),normalized.end(),normalized.begin(),[](unsigned char c){return std::toupper(c);});
  const auto start=normalized.find_first_not_of(" /\t");
  if (start==std::string::npos) return result;
  const auto end=normalized.find_first_of(" \t",start);
  const auto verb=normalized.substr(start,end-start);
  if (verb!="LOGIN" && verb!="CREATE") return result;
  auto user=normalized.find_first_not_of(" \t",end);
  if (user==std::string::npos) return result;
  auto userEnd=normalized.find_first_of(" \t",user);
  auto pass=normalized.find_first_not_of(" \t",userEnd);
  if (pass==std::string::npos) return result;
  auto passEnd=normalized.find_first_of(" \t",pass);
  result.replace(pass,passEnd==std::string::npos ? result.size()-pass : passEnd-pass,"********");
  return result;
}

void render(int width, int height, const View& v) {
  const double scale=std::min(width/960.0,height/600.0);
  const int drawWidth=static_cast<int>(960*scale),drawHeight=static_cast<int>(600*scale);
  glViewport((width-drawWidth)/2,(height-drawHeight)/2,drawWidth,drawHeight);
  width=960; height=600;
  glDisable(GL_DEPTH_TEST);
  glClearColor(ink.r,ink.g,ink.b,1); glClear(GL_COLOR_BUFFER_BIT);
  const double aspect=static_cast<double>(width)/std::max(height,1);
  glMatrixMode(GL_PROJECTION); glLoadIdentity();
  glOrtho(-340*aspect,340*aspect,-340,340,-1,1);
  glMatrixMode(GL_MODELVIEW); glLoadIdentity();
  // Slow parallax stars stay deterministic across sessions.
  glPointSize(2); color(muted); glBegin(GL_POINTS);
  for(int i=0;i<240;++i) {
    double x=std::fmod(i*173.71-v.ship.x*0.18+10000,1600)-800;
    double y=std::fmod(i*91.37-v.ship.y*0.18+10000,900)-450;
    glVertex2d(x,y);
  }
  glEnd();
  glTranslated(-v.ship.x,-v.ship.y,0);
  stationAsset(v.time);
  for(std::size_t i=0;i<flight::kRocks.size();++i) rockAsset(flight::kRocks[i],static_cast<int>(i));
  const auto& target=flight::kRocks[flight::nearestRock(v.ship)];
  ring(target.x,target.y,target.radius+10,teal,6);
  if (v.time<v.beamUntil) {
    glLineWidth(3); color(teal); glBegin(GL_LINES);
    glVertex2d(v.ship.x,v.ship.y); glVertex2d(target.x,target.y); glEnd(); glLineWidth(1);
    for(int i=0;i<8;++i) {
      const double a=i*flight::kPi/4;
      double r=(0.45-(v.beamUntil-v.time))*70;
      ring(target.x+std::cos(a)*r,target.y+std::sin(a)*r,2,amber,4);
    }
  }
  shipAsset(v);
  for (const auto& contact : v.contacts) {
    if (contact.hostile) {
      ring(contact.x, contact.y, contact.id == v.targetId ? 23 : 18, hostileColor, 6);
      triangle(contact.x, contact.y + 14, contact.x - 9, contact.y - 8,
               contact.x + 9, contact.y - 8, hostileColor);
    } else if (contact.id == "HAULER-7") {
      ring(contact.x, contact.y, 18, amber, 6);
      triangle(contact.x, contact.y + 14, contact.x - 9, contact.y - 8,
               contact.x + 9, contact.y - 8, amber);
    } else {
      ring(contact.x, contact.y, 14, teal, 6);
      triangle(contact.x, contact.y + 11, contact.x - 7, contact.y - 7,
               contact.x + 7, contact.y - 7, muted);
    }
  }
  // HUD uses a fixed logical canvas, letterboxed to preserve readability.
  const float uiWidth=static_cast<float>(960), uiHeight=static_cast<float>(height)*960/width;
  glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0,uiWidth,uiHeight,0,-1,1);
  glMatrixMode(GL_MODELVIEW); glLoadIdentity();
  rect(0,0,960,74,panel); rect(0,73,960,1,line);
  text(24,18,"HELION",white,3); text(24,48,"KEPLER REACH / MINING SECTOR",muted,1.5f);
  text(400,20,v.connected ? (v.authenticated ? "COMMAND LINK / ONLINE" : "COMMAND LINK / SIGN IN") : "COMMAND LINK / OFFLINE",
       v.connected ? teal : amber,1.5f);
  text(400,46,"SIDEWINDER / "+std::string(v.ship.docked ? "DOCKED" : "FREE FLIGHT"),white,1.5f);
  const auto kepler = v.reputation.find("authority.kepler");
  const auto orion = v.reputation.find("corp.orion");
  const auto redWake = v.reputation.find("criminal.red_wake");
  text(400,62,"KEPLER "+std::string(kepler == v.reputation.end() ? "NEUTRAL" : helion::organizations::standingLabel(kepler->second))+
       " / ORION "+std::string(orion == v.reputation.end() ? "NEUTRAL" : helion::organizations::standingLabel(orion->second))+
       " / RED WAKE "+std::string(redWake == v.reputation.end() ? "NEUTRAL" : helion::organizations::standingLabel(redWake->second)), muted, 1.1f);
  text(758,18,std::to_string(v.credits)+" CR",amber,2);
  text(758,46,std::to_string(v.experience)+" XP",muted,1.5f);
  rect(20,92,254,107,panel); text(34,106,"FLIGHT CONTRACT",teal,1.5f);
  text(34,132,v.ship.docked ? "01 / LAUNCH FROM KEPLER" : v.ship.cargo==0 ? "02 / EXTRACT ORE" : "03 / RETURN AND SELL",white,1.5f);
  text(34,155,"ORE VALUE / 60 CR PER UNIT",muted,1.5f);
  const int displayedFuel=std::max(0,static_cast<int>(std::ceil(v.ship.fuel)));
  const int displayedMaxFuel=std::max(1,static_cast<int>(std::ceil(v.ship.maxFuel)));
  text(34,178,"FUEL / "+std::to_string(displayedFuel)+" OF "+std::to_string(displayedMaxFuel),
       v.ship.fuel < v.ship.maxFuel*0.2 ? amber : muted,1.5f);
  // Sector radar and a station vector always remain visible when flying away.
  if (v.showTelemetry) rect(788,92,152,152,panel);
  if (v.showTelemetry) text(800,103,"SECTOR SCAN",muted,1.5f);
  if (v.showTelemetry) ring(864,180,49,line);
  if (v.showTelemetry) rect(861,177,6,6,amber);
  if (v.showTelemetry) for(const auto& r:flight::kRocks) rect(863+static_cast<float>(r.x)*0.1f,179-static_cast<float>(r.y)*0.1f,3,3,muted);
  if (v.showTelemetry) for(const auto& contact: v.contacts) {
    const Color contactColor = contact.hostile ? hostileColor : amber;
    rect(863+static_cast<float>(contact.x)*0.1f,179-static_cast<float>(contact.y)*0.1f,
      contact.id == v.targetId ? 5 : 3, contact.id == v.targetId ? 5 : 3, contactColor);
  }
  const float radarX=864+static_cast<float>(v.ship.x)*0.1f,radarY=180-static_cast<float>(v.ship.y)*0.1f;
  if (v.showTelemetry) rect(std::clamp(radarX,800.f,928.f)-2,std::clamp(radarY,126.f,230.f)-2,4,4,teal);
  if (v.showTelemetry) text(800,228,"BASE "+std::to_string(static_cast<int>(std::hypot(v.ship.x,v.ship.y)))+" M",amber,1.5f);
  const float bottom=uiHeight-106;
  rect(0,bottom,960,106,panel); rect(0,bottom,960,1,line);
  if (v.showTelemetry) text(24,bottom+16,"SPEED",muted,1.5f);
  if (v.showTelemetry) text(24,bottom+38,std::to_string(static_cast<int>(flight::speed(v.ship)))+" M/S",white,2);
  text(185,bottom+16,"CARGO / "+std::to_string(v.ship.cargo)+" OF 8",muted,1.5f);
  for(int i=0;i<8;++i) rect(185+i*19,bottom+38,14,14,i<v.ship.cargo ? teal : line);
  text(24,bottom+58,"HULL",muted,1.5f);
  const int hullBars=std::clamp(v.ship.hull*8/std::max(v.ship.maxHull,1),0,8);
  for(int i=0;i<8;++i) rect(24+i*15,bottom+78,11,10,i<hullBars ? (hullBars<=2 ? amber : teal) : line);
  text(24,bottom+96,std::to_string(v.ship.hull)+" / "+std::to_string(v.ship.maxHull),hullBars<=2 ? amber : white,1.5f);
  const double range=std::hypot(v.ship.x-target.x,v.ship.y-target.y);
  text(392,bottom+16,"ORE TARGET / "+std::to_string(static_cast<int>(range))+" M",muted,1.5f);
  const auto hostileTarget = std::find_if(v.contacts.begin(), v.contacts.end(),
    [&v](const auto& contact) { return contact.hostile && contact.id == v.targetId; });
  const bool hasTarget = hostileTarget != v.contacts.end();
  const int targetRange = hasTarget ? static_cast<int>(std::hypot(v.ship.x-hostileTarget->x,v.ship.y-hostileTarget->y)) : 0;
  if (hasTarget) {
    std::string affiliation = hostileTarget->affiliationName;
    std::replace(affiliation.begin(), affiliation.end(), '_', ' ');
    text(392,bottom+16,"HOSTILE / "+hostileTarget->id,hostileColor,1.5f);
    if (!affiliation.empty()) text(392,bottom+29,"AFFILIATION / "+affiliation,hostileColor,1.1f);
    text(392,bottom+38,"HULL "+std::to_string(hostileTarget->hull)+" / "+std::to_string(hostileTarget->maxHull)+
      "  RANGE "+std::to_string(targetRange)+" M",white,1.5f);
  } else {
    text(392,bottom+16,"HOSTILE / NO CONTACT",muted,1.5f);
  }
  std::string prompt=v.ship.destroyed ? "SHIP DISABLED / [R] RECOVER AT SAFE STATION" : v.ship.docked ? (v.ship.hull<v.ship.maxHull ? "[R] REPAIR HULL" :
    v.ship.fuel<v.ship.maxFuel ? "[T] REFUEL / [L] LAUNCH" : "[L] LAUNCH") :
    hasTarget ? (v.ship.weaponCooldown>0 ? "LASER RECHARGING" : "[SPACE] FIRE AT TARGET") :
    v.ship.cargo==8 ? "HOLD FULL / RETURN TO BASE" :
    range>flight::kMineRange ? "APPROACH TO 85 M" : flight::speed(v.ship)>flight::kWorkSpeed ? "[S] BRAKE TO MINE" :
    v.ship.cooldown>0 ? "EXTRACTOR RECHARGING" : "[E] EXTRACT ORE";
  if (!v.ship.docked && std::hypot(v.ship.x,v.ship.y)<=flight::kDockRange)
    prompt=flight::speed(v.ship)>flight::kWorkSpeed ? "[S] BRAKE TO DOCK" : "[F] DOCK AND SELL ORE";
  text(392,bottom+39,prompt,teal,1.5f);
  text(185,bottom+77,"W THRUST   A/D TURN   S BRAKE   E MINE   SPACE FIRE   F DOCK   L LAUNCH",muted,1.5f);
  if (!v.log.empty() && !v.console) {
    rect(20,bottom-37,740,26,panel); text(30,bottom-30,v.log.back().substr(0,78),amber,1.5f);
  }
  if (v.console) {
    const float top=std::max(210.f,bottom-260);
    rect(20,top,740,bottom-top-10,panel); rect(20,top,3,bottom-top-10,teal);
    text(38,top+14,v.authenticated ? "COMMS CONSOLE / ESC TO FLY" : "COMMANDER ACCESS",teal,2);
    text(38,top+42,"/CREATE USER PASSWORD DISPLAY  OR  /LOGIN USER PASSWORD",muted,1.5f);
    text(38,top+63,"NEW PASSWORD: 12-128 BYTES / ENTER SENDS",muted,1.5f);
    int rows=std::max(0,static_cast<int>((bottom-top-130)/18));
    auto start=v.log.size()>static_cast<std::size_t>(rows) ? v.log.size()-rows : 0;
    float y=top+88;
    for(auto i=start;i<v.log.size();++i,y+=18) text(38,y,v.log[i].substr(0,76),white,1.5f);
    rect(34,bottom-43,710,25,ink);
    std::string typed=redactCommand(v.typed);
    if (typed.size()>73) typed=typed.substr(typed.size()-73);
    text(42,bottom-36,"> "+typed+(static_cast<int>(v.time*2)%2==0 ? "_" : ""),teal,1.5f);
  }
}

void render(int width, int height, const View& view, const PresentationSnapshot& snapshot) {
  View presentationView = view;
  presentationView.ship = snapshot.player;
  presentationView.credits = snapshot.credits;
  presentationView.experience = snapshot.experience;
  presentationView.targetId = snapshot.selectedTarget;
  presentationView.beamUntil = snapshot.miningActive ? snapshot.time + 0.1 : snapshot.time - 1.0;
  presentationView.weaponUntil = snapshot.weaponFired ? snapshot.time + 0.1 : snapshot.time - 1.0;
  presentationView.damageUntil = snapshot.incomingDamage ? snapshot.time + 0.1 : snapshot.time - 1.0;
  presentationView.contacts.clear();
  for (const auto& contact : snapshot.contacts)
    presentationView.contacts.push_back({contact.id, contact.kind, contact.x, contact.y, contact.yaw,
      contact.docked, contact.hostile, contact.hull, contact.maxHull,
      contact.affiliationId, contact.affiliationName});
  render(width, height, presentationView);
}
} // namespace helion::client
