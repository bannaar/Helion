#include "client/core_renderer.h"
#include "client/cockpit.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <sstream>

namespace helion::client {
namespace {
constexpr std::array<std::string_view, 35> kRequiredFunctions = {
  "glCreateShader", "glShaderSource", "glCompileShader", "glGetShaderiv",
  "glGetShaderInfoLog", "glDeleteShader", "glCreateProgram", "glAttachShader",
  "glLinkProgram", "glGetProgramiv", "glGetProgramInfoLog", "glUseProgram",
  "glDeleteProgram", "glGenVertexArrays", "glBindVertexArray", "glDeleteVertexArrays",
  "glGenBuffers", "glBindBuffer", "glBufferData", "glDeleteBuffers",
  "glEnableVertexAttribArray", "glVertexAttribPointer", "glGetUniformLocation",
  "glUniformMatrix4fv", "glUniform1i", "glActiveTexture", "glGenTextures",
  "glBindTexture", "glTexImage2D", "glTexParameteri", "glDeleteTextures",
  "glEnable", "glDisable", "glBlendFunc", "glDrawArrays"
};

struct Vertex {
  float x, y, r, g, b;
};
using Vertices = std::vector<Vertex>;

template <typename Function>
Function loadFunction(const char* name) {
  return reinterpret_cast<Function>(SDL_GL_GetProcAddress(name));
}

std::string shaderLog(const CoreFunctions::GetShaderInfoLog& getLog, GLuint object,
                      CoreFunctions::GetShaderiv getValue) {
  GLint length = 0;
  getValue(object, GL_INFO_LOG_LENGTH, &length);
  if (length <= 0) return "no diagnostic log";
  std::string log(static_cast<std::size_t>(length), '\0');
  GLsizei written = 0;
  getLog(object, length, &written, log.data());
  if (written >= 0 && written < static_cast<GLsizei>(log.size())) log.resize(static_cast<std::size_t>(written));
  return log;
}

std::string programLog(const CoreFunctions::GetProgramInfoLog& getLog, GLuint object,
                       CoreFunctions::GetProgramiv getValue) {
  GLint length = 0;
  getValue(object, GL_INFO_LOG_LENGTH, &length);
  if (length <= 0) return "no diagnostic log";
  std::string log(static_cast<std::size_t>(length), '\0');
  GLsizei written = 0;
  getLog(object, length, &written, log.data());
  if (written >= 0 && written < static_cast<GLsizei>(log.size())) log.resize(static_cast<std::size_t>(written));
  return log;
}

void addTriangle(Vertices& vertices, float ax, float ay, float bx, float by, float cx, float cy,
                 const PresentationColor& color) {
  vertices.push_back({ax, ay, color.r, color.g, color.b});
  vertices.push_back({bx, by, color.r, color.g, color.b});
  vertices.push_back({cx, cy, color.r, color.g, color.b});
}

void addQuad(Vertices& vertices, float x, float y, float width, float height,
             const PresentationColor& color) {
  addTriangle(vertices, x, y, x + width, y, x + width, y + height, color);
  addTriangle(vertices, x, y, x + width, y + height, x, y + height, color);
}

void addHudQuad(Vertices& vertices, const HudLayout& layout, float x, float y,
                float width, float height, const PresentationColor& color) {
  addQuad(vertices, layout.originX + x * layout.scale, layout.originY + y * layout.scale,
    width * layout.scale, height * layout.scale, color);
}

void addLine(Vertices& vertices, float ax, float ay, float bx, float by, float thickness,
             const PresentationColor& color) {
  const float dx = bx - ax;
  const float dy = by - ay;
  const float length = std::max(0.001f, std::hypot(dx, dy));
  const float nx = -dy / length * thickness * 0.5f;
  const float ny = dx / length * thickness * 0.5f;
  addTriangle(vertices, ax + nx, ay + ny, bx + nx, by + ny, bx - nx, by - ny, color);
  addTriangle(vertices, ax + nx, ay + ny, bx - nx, by - ny, ax - nx, ay - ny, color);
}

void addHudOutline(Vertices& vertices, const HudLayout& layout, const UiRect& rect,
                   const PresentationColor& color) {
  const float x = layout.originX + rect.x * layout.scale;
  const float y = layout.originY + rect.y * layout.scale;
  const float width = rect.width * layout.scale;
  const float height = rect.height * layout.scale;
  addLine(vertices, x, y, x + width, y, 2.0f, color);
  addLine(vertices, x + width, y, x + width, y + height, 2.0f, color);
  addLine(vertices, x + width, y + height, x, y + height, 2.0f, color);
  addLine(vertices, x, y + height, x, y, 2.0f, color);
}

void addRing(Vertices& vertices, float cx, float cy, float radius, float thickness,
             const PresentationColor& color, int sides = 12) {
  for (int i = 0; i < sides; ++i) {
    const double a = 2.0 * flight::kPi * i / sides;
    const double b = 2.0 * flight::kPi * (i + 1) / sides;
    addLine(vertices, cx + static_cast<float>(std::cos(a) * radius),
      cy + static_cast<float>(std::sin(a) * radius),
      cx + static_cast<float>(std::cos(b) * radius),
      cy + static_cast<float>(std::sin(b) * radius), thickness, color);
  }
}

std::array<float, 2> worldPoint(double cx, double cy, double yaw, float x, float y) {
  const float cosine = static_cast<float>(std::cos(yaw));
  const float sine = static_cast<float>(std::sin(yaw));
  return {static_cast<float>(cx) + x * cosine - y * sine,
          static_cast<float>(cy) + x * sine + y * cosine};
}

void addShip(Vertices& vertices, double x, double y, double yaw, const PresentationColor& color,
             bool destroyed, float size = 1.0f) {
  const auto point = [=](float px, float py) { return worldPoint(x, y, yaw, px * size, py * size); };
  const auto nose = point(0, 24);
  const auto left = point(-16, -13);
  const auto right = point(16, -13);
  const auto center = point(0, -4);
  addTriangle(vertices, nose[0], nose[1], left[0], left[1], center[0], center[1], color);
  addTriangle(vertices, nose[0], nose[1], center[0], center[1], right[0], right[1], color);
  if (destroyed) {
    const auto a = point(-18, -18), b = point(18, 18), c = point(18, -18), d = point(-18, 18);
    addLine(vertices, a[0], a[1], b[0], b[1], 5.0f, {1.0f, 0.20f, 0.20f});
    addLine(vertices, c[0], c[1], d[0], d[1], 5.0f, {1.0f, 0.20f, 0.20f});
  }
}

void addStation(Vertices& vertices, const PresentationStation& station) {
  addRing(vertices, static_cast<float>(station.x), static_cast<float>(station.y), 54.0f, 5.0f, station.color, 12);
  addRing(vertices, static_cast<float>(station.x), static_cast<float>(station.y), 28.0f, 3.0f, {0.35f, 0.48f, 0.55f}, 8);
  for (int i = 0; i < 4; ++i) {
    const double angle = i * flight::kPi / 2.0;
    const float dx = static_cast<float>(std::cos(angle) * 44.0);
    const float dy = static_cast<float>(std::sin(angle) * 44.0);
    addLine(vertices, static_cast<float>(station.x) - dx, static_cast<float>(station.y) - dy,
      static_cast<float>(station.x) + dx, static_cast<float>(station.y) + dy, 8.0f,
      {0.20f, 0.32f, 0.38f});
  }
  addQuad(vertices, static_cast<float>(station.x) - 8.0f, static_cast<float>(station.y) - 8.0f,
    16.0f, 16.0f, {0.25f, 0.88f, 0.76f});
}

void addAsteroid(Vertices& vertices, const PresentationAsteroid& asteroid, int index) {
  const PresentationColor body = asteroid.depleted ? PresentationColor{0.16f, 0.22f, 0.24f} :
    PresentationColor{0.28f, 0.38f, 0.42f};
  constexpr int sides = 8;
  for (int i = 0; i < sides; ++i) {
    const double a = 2.0 * flight::kPi * i / sides + index * 0.37;
    const double b = 2.0 * flight::kPi * (i + 1) / sides + index * 0.37;
    const float ra = static_cast<float>(asteroid.radius * (0.78 + 0.16 * std::sin(index + i * 1.7)));
    const float rb = static_cast<float>(asteroid.radius * (0.78 + 0.16 * std::sin(index + (i + 1) * 1.7)));
    addTriangle(vertices, static_cast<float>(asteroid.x), static_cast<float>(asteroid.y),
      static_cast<float>(asteroid.x + std::cos(a) * ra), static_cast<float>(asteroid.y + std::sin(a) * ra),
      static_cast<float>(asteroid.x + std::cos(b) * rb), static_cast<float>(asteroid.y + std::sin(b) * rb), body);
  }
  if (!asteroid.depleted) addRing(vertices, static_cast<float>(asteroid.x), static_cast<float>(asteroid.y),
    static_cast<float>(asteroid.radius + 7), 2.0f, {0.25f, 0.88f, 0.76f}, 6);
}

void addContact(Vertices& vertices, const PresentationContact& contact) {
  if (contact.classification == PresentationClass::hostile) {
    addTriangle(vertices, static_cast<float>(contact.x), static_cast<float>(contact.y + 20),
      static_cast<float>(contact.x - 17), static_cast<float>(contact.y - 14),
      static_cast<float>(contact.x + 17), static_cast<float>(contact.y - 14), contact.color);
    addRing(vertices, static_cast<float>(contact.x), static_cast<float>(contact.y), 25.0f, 3.0f, contact.color, 6);
  } else if (contact.classification == PresentationClass::hauler) {
    addQuad(vertices, static_cast<float>(contact.x - 18), static_cast<float>(contact.y - 9), 36.0f, 18.0f, contact.color);
    addTriangle(vertices, static_cast<float>(contact.x - 18), static_cast<float>(contact.y - 9),
      static_cast<float>(contact.x - 30), static_cast<float>(contact.y),
      static_cast<float>(contact.x - 18), static_cast<float>(contact.y + 9), {0.75f, 0.48f, 0.20f});
  } else {
    addShip(vertices, contact.x, contact.y, contact.yaw, contact.color, false, 0.65f);
  }
}

void addReticle(Vertices& vertices, float x, float y, float radius, const PresentationColor& color) {
  const float gap = radius * 0.45f;
  addLine(vertices, x - radius, y - radius, x - gap, y - radius, 3.0f, color);
  addLine(vertices, x - radius, y - radius, x - radius, y - gap, 3.0f, color);
  addLine(vertices, x + radius, y - radius, x + gap, y - radius, 3.0f, color);
  addLine(vertices, x + radius, y - radius, x + radius, y - gap, 3.0f, color);
  addLine(vertices, x - radius, y + radius, x - gap, y + radius, 3.0f, color);
  addLine(vertices, x - radius, y + radius, x - radius, y + gap, 3.0f, color);
  addLine(vertices, x + radius, y + radius, x + gap, y + radius, 3.0f, color);
  addLine(vertices, x + radius, y + radius, x + radius, y + gap, 3.0f, color);
}

Vertices staticScene() {
  Vertices vertices;
  vertices.reserve(120);
  const PresentationColor grid{0.035f, 0.09f, 0.12f};
  const PresentationColor axis{0.10f, 0.24f, 0.28f};
  for (int coordinate = -1200; coordinate <= 1200; coordinate += 100) {
    const PresentationColor color = coordinate == 0 ? axis : grid;
    vertices.push_back({-1400.0f, static_cast<float>(coordinate), color.r, color.g, color.b});
    vertices.push_back({1400.0f, static_cast<float>(coordinate), color.r, color.g, color.b});
    vertices.push_back({static_cast<float>(coordinate), -1200.0f, color.r, color.g, color.b});
    vertices.push_back({static_cast<float>(coordinate), 1200.0f, color.r, color.g, color.b});
  }
  return vertices;
}

} // namespace

std::string firstMissingCoreFunction(const std::vector<std::string>& available) {
  for (const auto required : kRequiredFunctions) {
    bool found = false;
    for (const auto& candidate : available)
      if (candidate == required) { found = true; break; }
    if (!found) return std::string(required);
  }
  return {};
}

bool CoreFunctions::load(std::string& error) {
#define HELION_LOAD_CORE(name) \
  name = loadFunction<decltype(name)>(#name); \
  if (!name) { error = "missing OpenGL function " #name; return false; }
  HELION_LOAD_CORE(glCreateShader)
  HELION_LOAD_CORE(glShaderSource)
  HELION_LOAD_CORE(glCompileShader)
  HELION_LOAD_CORE(glGetShaderiv)
  HELION_LOAD_CORE(glGetShaderInfoLog)
  HELION_LOAD_CORE(glDeleteShader)
  HELION_LOAD_CORE(glCreateProgram)
  HELION_LOAD_CORE(glAttachShader)
  HELION_LOAD_CORE(glLinkProgram)
  HELION_LOAD_CORE(glGetProgramiv)
  HELION_LOAD_CORE(glGetProgramInfoLog)
  HELION_LOAD_CORE(glUseProgram)
  HELION_LOAD_CORE(glDeleteProgram)
  HELION_LOAD_CORE(glGenVertexArrays)
  HELION_LOAD_CORE(glBindVertexArray)
  HELION_LOAD_CORE(glDeleteVertexArrays)
  HELION_LOAD_CORE(glGenBuffers)
  HELION_LOAD_CORE(glBindBuffer)
  HELION_LOAD_CORE(glBufferData)
  HELION_LOAD_CORE(glDeleteBuffers)
  HELION_LOAD_CORE(glEnableVertexAttribArray)
  HELION_LOAD_CORE(glVertexAttribPointer)
  HELION_LOAD_CORE(glGetUniformLocation)
  HELION_LOAD_CORE(glUniformMatrix4fv)
  HELION_LOAD_CORE(glUniform1i)
  HELION_LOAD_CORE(glActiveTexture)
  HELION_LOAD_CORE(glGenTextures)
  HELION_LOAD_CORE(glBindTexture)
  HELION_LOAD_CORE(glTexImage2D)
  HELION_LOAD_CORE(glTexParameteri)
  HELION_LOAD_CORE(glDeleteTextures)
  HELION_LOAD_CORE(glEnable)
  HELION_LOAD_CORE(glDisable)
  HELION_LOAD_CORE(glBlendFunc)
  HELION_LOAD_CORE(glDrawArrays)
#undef HELION_LOAD_CORE
  return true;
}

bool CoreFunctions::complete() const {
  return glCreateShader && glShaderSource && glCompileShader && glGetShaderiv &&
    glGetShaderInfoLog && glDeleteShader && glCreateProgram && glAttachShader &&
    glLinkProgram && glGetProgramiv && glGetProgramInfoLog && glUseProgram &&
    glDeleteProgram && glGenVertexArrays && glBindVertexArray && glDeleteVertexArrays &&
    glGenBuffers && glBindBuffer && glBufferData && glDeleteBuffers &&
    glEnableVertexAttribArray && glVertexAttribPointer && glGetUniformLocation &&
    glUniformMatrix4fv && glUniform1i && glActiveTexture && glGenTextures &&
    glBindTexture && glTexImage2D && glTexParameteri && glDeleteTextures &&
    glEnable && glDisable && glBlendFunc && glDrawArrays;
}

CoreRenderer::~CoreRenderer() {
  release();
}

bool CoreRenderer::compileShader(GLenum type, const char* source, GLuint& shader, std::string& error) {
  shader = functions_.glCreateShader(type);
  if (!shader) { error = "shader creation failed"; return false; }
  const GLchar* sources[] = {source};
  functions_.glShaderSource(shader, 1, sources, nullptr);
  functions_.glCompileShader(shader);
  GLint compiled = GL_FALSE;
  functions_.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled == GL_TRUE) return true;
  error = "shader compilation failed: " + shaderLog(functions_.glGetShaderInfoLog, shader,
                                                     functions_.glGetShaderiv);
  functions_.glDeleteShader(shader);
  shader = 0;
  return false;
}

bool CoreRenderer::linkProgram(GLuint vertexShader, GLuint fragmentShader, std::string& error) {
  program_ = functions_.glCreateProgram();
  if (!program_) { error = "program creation failed"; return false; }
  functions_.glAttachShader(program_, vertexShader);
  functions_.glAttachShader(program_, fragmentShader);
  functions_.glLinkProgram(program_);
  GLint linked = GL_FALSE;
  functions_.glGetProgramiv(program_, GL_LINK_STATUS, &linked);
  if (linked == GL_TRUE) return true;
  error = "program linking failed: " + programLog(functions_.glGetProgramInfoLog, program_,
                                                    functions_.glGetProgramiv);
  functions_.glDeleteProgram(program_);
  program_ = 0;
  return false;
}

bool CoreRenderer::setupBuffer(GLuint vertexArray, GLuint vertexBuffer, std::string& error) {
  if (!vertexArray || !vertexBuffer) { error = "core buffer handles are invalid"; return false; }
  functions_.glBindVertexArray(vertexArray);
  functions_.glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  functions_.glEnableVertexAttribArray(0);
  functions_.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
  functions_.glEnableVertexAttribArray(1);
  functions_.glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<const void*>(2 * sizeof(float)));
  functions_.glBindBuffer(GL_ARRAY_BUFFER, 0);
  functions_.glBindVertexArray(0);
  return true;
}

bool CoreRenderer::initialize(std::string& error) {
  release();
  if (!functions_.load(error) || !functions_.complete()) {
    if (error.empty()) error = "incomplete OpenGL function table";
    release();
    return false;
  }
  static constexpr const char* vertexSource = R"GLSL(#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec3 aColor;
uniform mat4 uMvp;
out vec3 vColor;
void main() {
  gl_Position = uMvp * vec4(aPosition, 0.0, 1.0);
  vColor = aColor;
}
)GLSL";
  static constexpr const char* fragmentSource = R"GLSL(#version 330 core
in vec3 vColor;
out vec4 fragColor;
void main() {
  fragColor = vec4(vColor, 1.0);
}
)GLSL";
  GLuint vertexShader = 0;
  GLuint fragmentShader = 0;
  if (!compileShader(GL_VERTEX_SHADER, vertexSource, vertexShader, error) ||
      !compileShader(GL_FRAGMENT_SHADER, fragmentSource, fragmentShader, error)) {
    if (fragmentShader) functions_.glDeleteShader(fragmentShader);
    release();
    return false;
  }
  if (!linkProgram(vertexShader, fragmentShader, error)) {
    functions_.glDeleteShader(vertexShader);
    functions_.glDeleteShader(fragmentShader);
    release();
    return false;
  }
  functions_.glDeleteShader(vertexShader);
  functions_.glDeleteShader(fragmentShader);
  functions_.glGenVertexArrays(1, &staticVertexArray_);
  functions_.glGenBuffers(1, &staticVertexBuffer_);
  functions_.glGenVertexArrays(1, &dynamicVertexArray_);
  functions_.glGenBuffers(1, &dynamicVertexBuffer_);
  if (!setupBuffer(staticVertexArray_, staticVertexBuffer_, error) ||
      !setupBuffer(dynamicVertexArray_, dynamicVertexBuffer_, error)) {
    release();
    return false;
  }
  const auto staticVertices = staticScene();
  staticVertexCount_ = staticVertices.size();
  functions_.glBindVertexArray(staticVertexArray_);
  functions_.glBindBuffer(GL_ARRAY_BUFFER, staticVertexBuffer_);
  functions_.glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(staticVertices.size() * sizeof(Vertex)),
    staticVertices.data(), GL_STATIC_DRAW);
  functions_.glBindBuffer(GL_ARRAY_BUFFER, 0);
  functions_.glBindVertexArray(0);
  mvpLocation_ = functions_.glGetUniformLocation(program_, "uMvp");
  if (mvpLocation_ < 0) {
    error = "core shader uniform uMvp was not found";
    release();
    return false;
  }
  if (!textRenderer_.initialize(functions_, error)) {
    release();
    return false;
  }
  initialized_ = true;
  return true;
}

bool CoreRenderer::render(int width, int height, const PresentationSnapshot& snapshot,
                          bool checkErrors, CoreRenderStats* stats, std::string& error) {
  if (!initialized_) { error = "core renderer is not initialized"; return false; }
  const auto checkStage = [&](const char* stage) {
    if (!checkErrors) return true;
    const GLenum glError = glGetError();
    if (glError == GL_NO_ERROR) return true;
    std::ostringstream message;
    message << "core render OpenGL error at " << stage << " 0x" << std::hex << glError;
    error = message.str();
    return false;
  };
  const auto started = std::chrono::steady_clock::now();
  const auto camera = cameraFrame(width, height);
  const auto projection = math::orthographic(-camera.halfWidth, camera.halfWidth,
    -camera.halfHeight, camera.halfHeight, -1.0f, 1.0f);
  const auto view = math::multiply(math::rotationZ(static_cast<float>(-snapshot.player.yaw)),
    math::translation(static_cast<float>(-snapshot.player.x), static_cast<float>(-snapshot.player.y)));
  const auto worldMvp = math::multiply(projection, view);
  Vertices dynamic;
  dynamic.reserve(4096);
  for (const auto& station : snapshot.stations) addStation(dynamic, station);
  for (std::size_t i = 0; i < snapshot.asteroids.size(); ++i) addAsteroid(dynamic, snapshot.asteroids[i], static_cast<int>(i));
  for (const auto& contact : snapshot.contacts) {
    addContact(dynamic, contact);
    if (contact.selected) addReticle(dynamic, static_cast<float>(contact.x), static_cast<float>(contact.y), 42.0f, contact.color);
  }
  const auto nearestRock = snapshot.asteroids.empty() ? PresentationAsteroid{} :
    snapshot.asteroids[static_cast<std::size_t>(flight::nearestRock(snapshot.player)) % snapshot.asteroids.size()];
  if (snapshot.miningActive) addLine(dynamic, static_cast<float>(snapshot.player.x), static_cast<float>(snapshot.player.y),
    static_cast<float>(nearestRock.x), static_cast<float>(nearestRock.y), 5.0f, {0.25f, 0.88f, 0.76f});
  const auto targetIt = std::find_if(snapshot.contacts.begin(), snapshot.contacts.end(),
    [](const auto& contact) { return contact.selected && contact.hostile; });
  if (snapshot.weaponFired && targetIt != snapshot.contacts.end())
    addLine(dynamic, static_cast<float>(snapshot.player.x), static_cast<float>(snapshot.player.y),
      static_cast<float>(targetIt->x), static_cast<float>(targetIt->y), 7.0f, {1.0f, 0.32f, 0.65f});
  addShip(dynamic, snapshot.player.x, snapshot.player.y, snapshot.player.yaw,
    {0.30f, 0.86f, 0.95f}, snapshot.player.destroyed, 1.0f);
  if (dynamic.size() > kMaxCoreDynamicVertices) {
    dynamic.resize(kMaxCoreDynamicVertices - kMaxCoreDynamicVertices % 3);
  }

  Vertices hud;
  hud.reserve(1200);
  const PresentationColor panel{0.04f, 0.10f, 0.14f};
  const PresentationColor grid{0.12f, 0.22f, 0.26f};
  const PresentationColor teal{0.25f, 0.88f, 0.76f};
  const PresentationColor amber{1.0f, 0.67f, 0.29f};
  const PresentationColor red{1.0f, 0.24f, 0.24f};
  const float uiWidth = static_cast<float>(std::max(width, 1));
  const float uiHeight = static_cast<float>(std::max(height, 1));
  const auto layout = hudLayout(width, height);
  const auto uiBuildStarted = std::chrono::steady_clock::now();
  if (snapshot.ui.screen != UiScreen::flight) {
    addHudQuad(hud, layout, 8, 8, 944, 584, panel);
    addHudQuad(hud, layout, 8, 8, 944, 86, {0.06f, 0.15f, 0.18f});
    addHudQuad(hud, layout, 20, 100, 920, 1, grid);
  } else {
    addHudQuad(hud, layout, 8, 8, 944, 72, panel);
    addHudQuad(hud, layout, 8, 92, 340, 142, panel);
    addHudQuad(hud, layout, 352, 92, 600, 142, panel);
    addHudQuad(hud, layout, 8, 372, 944, 210, panel);
  }
  {
    for (const auto& control : buildUiControls(snapshot.ui)) {
      const bool hovered = control.id == snapshot.ui.hoveredControl &&
        control.index == snapshot.ui.hoveredIndex;
      const bool selected = control.id != UiControlId::accountInput &&
        ((control.id == UiControlId::galnetEntry && control.index - snapshot.ui.scrollOffset == snapshot.ui.selected) ||
         (control.id != UiControlId::galnetEntry && control.index == snapshot.ui.selected));
      if (hovered || selected) addHudOutline(hud, layout, control.rect,
        hovered ? PresentationColor{1.0f, 0.67f, 0.29f} : PresentationColor{0.25f, 0.88f, 0.76f});
      else if (!control.enabled && control.id != UiControlId::accountInput)
        addHudOutline(hud, layout, control.rect, PresentationColor{0.18f, 0.25f, 0.28f});
    }
  }
  if (snapshot.ui.screen == UiScreen::flight) {
  addHudQuad(hud, layout, 220, 120, 110, 10, grid);
  addHudQuad(hud, layout, 220, 120, 110 * clampHudValue(snapshot.player.hull, snapshot.player.maxHull),
    10, snapshot.player.hull <= snapshot.player.maxHull / 4 ? amber : teal);
  addHudQuad(hud, layout, 220, 145, 110, 10, grid);
  addHudQuad(hud, layout, 220, 145, 110 * clampHudValue(snapshot.player.fuel, snapshot.player.maxFuel), 10, teal);
  addHudQuad(hud, layout, 220, 170, 110, 10, grid);
  addHudQuad(hud, layout, 220, 170, 110 * clampHudValue(flight::cargoUsed(snapshot.player), flight::kCargoCapacity), 10, amber);
  addHudQuad(hud, layout, 820, 120, 110, 8, grid);
  addHudQuad(hud, layout, 820, 120, 110 * clampHudValue(snapshot.player.weaponCooldown, 1.0), 8, red);
  addHudQuad(hud, layout, 820, 145, 22, 22, snapshot.player.docked ? amber : grid);
  if (snapshot.miningActive) addHudQuad(hud, layout, 854, 145, 22, 22, teal);
  if (snapshot.weaponFired) addHudQuad(hud, layout, 888, 145, 22, 22, {1.0f, 0.32f, 0.65f});
  const auto target = targetIt == snapshot.contacts.end() ? nullptr : &*targetIt;
  const auto indicator = targetIndicator(snapshot.player, target);
  if (indicator.visible) {
    const float cx = uiWidth * 0.5f + indicator.directionX * std::min(uiWidth, uiHeight) * 0.33f;
    const float cy = uiHeight * 0.5f - indicator.directionY * std::min(uiWidth, uiHeight) * 0.33f;
    addReticle(hud, cx, cy, 18.0f, target->color);
    addQuad(hud, uiWidth * 0.5f - 70 * layout.scale, uiHeight - 30 * layout.scale,
      140 * layout.scale, 6 * layout.scale, grid);
    addQuad(hud, uiWidth * 0.5f - 70 * layout.scale, uiHeight - 30 * layout.scale,
      140.0f * layout.scale * std::min(1.0f, indicator.distance / 520.0f), 6 * layout.scale, target->color);
  }
  if (snapshot.incomingDamage) addQuad(hud, 4, 4, uiWidth - 8, 4, red);
  if (snapshot.player.destroyed) {
    addQuad(hud, uiWidth * 0.5f - 90 * layout.scale, 28 * layout.scale,
      180 * layout.scale, 8 * layout.scale, red);
    addQuad(hud, uiWidth * 0.5f - 90 * layout.scale, 40 * layout.scale,
      180 * layout.scale, 8 * layout.scale, red);
  }
  }
  const auto uiBuildFinished = std::chrono::steady_clock::now();
  const auto textBuildStarted = std::chrono::steady_clock::now();
  const auto cockpitText = buildCockpitText(snapshot, width, height);
  const auto textBuildFinished = std::chrono::steady_clock::now();
  const auto buildFinished = std::chrono::steady_clock::now();
  const auto gpuStarted = buildFinished;

  glViewport(0, 0, std::max(width, 1), std::max(height, 1));
  glClearColor(0.008f, 0.015f, 0.045f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  if (!checkStage("clear")) return false;
  functions_.glUseProgram(program_);
  if (!checkStage("use-program")) return false;
  functions_.glUniformMatrix4fv(mvpLocation_, 1, GL_FALSE, worldMvp.value.data());
  functions_.glBindVertexArray(staticVertexArray_);
  functions_.glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(staticVertexCount_));
  if (!checkStage("static-draw")) return false;
  functions_.glBindVertexArray(0);
  functions_.glBindVertexArray(dynamicVertexArray_);
  functions_.glBindBuffer(GL_ARRAY_BUFFER, dynamicVertexBuffer_);
  functions_.glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(dynamic.size() * sizeof(Vertex)),
    dynamic.data(), GL_DYNAMIC_DRAW);
  if (!checkStage("dynamic-upload")) return false;
  functions_.glBindBuffer(GL_ARRAY_BUFFER, 0);
  functions_.glUniformMatrix4fv(mvpLocation_, 1, GL_FALSE, worldMvp.value.data());
  functions_.glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(dynamic.size()));
  if (!checkStage("dynamic-draw")) return false;
  functions_.glBindVertexArray(0);

  const auto hudMvp = math::orthographic(0, uiWidth, uiHeight, 0, -1, 1);
  functions_.glUniformMatrix4fv(mvpLocation_, 1, GL_FALSE, hudMvp.value.data());
  functions_.glBindVertexArray(dynamicVertexArray_);
  functions_.glBindBuffer(GL_ARRAY_BUFFER, dynamicVertexBuffer_);
  functions_.glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(hud.size() * sizeof(Vertex)),
    hud.data(), GL_DYNAMIC_DRAW);
  if (!checkStage("hud-upload")) return false;
  functions_.glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(hud.size()));
  if (!checkStage("hud-draw")) return false;
  functions_.glBindVertexArray(0);
  functions_.glUseProgram(0);
  if (!checkStage("unbind")) return false;
  TextRenderStats textStats;
  const auto textMvp = math::orthographic(0, uiWidth, uiHeight, 0, -1, 1);
  if (!textRenderer_.render(textMvp, cockpitText.vertices, cockpitText.glyphs,
                            checkErrors, &textStats, error)) return false;
  if (!checkStage("text")) return false;
  if (stats) {
    stats->drawCalls = 3 + textStats.drawCalls;
    stats->staticVertices = staticVertexCount_;
    stats->worldVertices = dynamic.size();
    stats->hudVertices = hud.size();
    stats->textVertices = textStats.vertices;
    stats->textIndices = textStats.indices;
    stats->textGlyphVertices = textStats.glyphQuadVertices;
    stats->textComponents = textStats.components;
    stats->textBytes = textStats.uploadedBytes;
    stats->textDrawCalls = textStats.drawCalls;
    stats->dynamicVertices = dynamic.size() + hud.size();
    stats->glyphs = textStats.glyphs;
    stats->textures = textStats.textures;
    stats->atlasWidth = textStats.atlasWidth;
    stats->atlasHeight = textStats.atlasHeight;
    stats->atlasBytes = textStats.atlasBytes;
    stats->cpuBuildMilliseconds = std::chrono::duration<double, std::milli>(buildFinished - started).count();
    stats->cpuUiBuildMilliseconds = std::chrono::duration<double, std::milli>(uiBuildFinished - uiBuildStarted).count();
    stats->cpuTextBuildMilliseconds = std::chrono::duration<double, std::milli>(textBuildFinished - textBuildStarted).count();
    stats->renderMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - gpuStarted).count();
    stats->frameMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
  }
  return true;
}

void CoreRenderer::release() {
  textRenderer_.release();
  if (functions_.glDeleteBuffers && staticVertexBuffer_) functions_.glDeleteBuffers(1, &staticVertexBuffer_);
  if (functions_.glDeleteBuffers && dynamicVertexBuffer_) functions_.glDeleteBuffers(1, &dynamicVertexBuffer_);
  if (functions_.glDeleteVertexArrays && staticVertexArray_) functions_.glDeleteVertexArrays(1, &staticVertexArray_);
  if (functions_.glDeleteVertexArrays && dynamicVertexArray_) functions_.glDeleteVertexArrays(1, &dynamicVertexArray_);
  if (functions_.glDeleteProgram && program_) functions_.glDeleteProgram(program_);
  staticVertexBuffer_ = 0;
  dynamicVertexBuffer_ = 0;
  staticVertexArray_ = 0;
  dynamicVertexArray_ = 0;
  program_ = 0;
  mvpLocation_ = -1;
  staticVertexCount_ = 0;
  initialized_ = false;
}

} // namespace helion::client
