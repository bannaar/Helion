#include "client/core_renderer.h"

#include <array>
#include <sstream>

namespace helion::client {
namespace {
constexpr std::array<std::string_view, 25> kRequiredFunctions = {
  "glCreateShader", "glShaderSource", "glCompileShader", "glGetShaderiv",
  "glGetShaderInfoLog", "glDeleteShader", "glCreateProgram", "glAttachShader",
  "glLinkProgram", "glGetProgramiv", "glGetProgramInfoLog", "glUseProgram",
  "glDeleteProgram", "glGenVertexArrays", "glBindVertexArray", "glDeleteVertexArrays",
  "glGenBuffers", "glBindBuffer", "glBufferData", "glDeleteBuffers",
  "glEnableVertexAttribArray", "glVertexAttribPointer", "glGetUniformLocation",
  "glUniformMatrix4fv", "glDrawArrays"
};

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
    glUniformMatrix4fv && glDrawArrays;
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

  constexpr std::array<float, 13 * 5> vertices = {
    -0.95f, 0.0f, 0.10f, 0.20f, 0.32f,  0.95f, 0.0f, 0.10f, 0.20f, 0.32f,
    0.0f, -0.65f, 0.10f, 0.20f, 0.32f,  0.0f, 0.65f, 0.10f, 0.20f, 0.32f,
    0.30f, 0.12f, 0.22f, 0.86f, 0.95f,  0.08f, -0.13f, 0.16f, 0.52f, 0.82f,
    0.52f, -0.13f, 0.16f, 0.52f, 0.82f,
    -0.68f, 0.30f, 0.86f, 0.64f, 0.24f,  -0.55f, 0.42f, 0.98f, 0.80f, 0.30f,
    -0.42f, 0.30f, 0.86f, 0.64f, 0.24f,  -0.42f, 0.30f, 0.86f, 0.64f, 0.24f,
    -0.55f, 0.18f, 0.98f, 0.80f, 0.30f,  -0.68f, 0.30f, 0.86f, 0.64f, 0.24f
  };
  functions_.glGenVertexArrays(1, &vertexArray_);
  functions_.glGenBuffers(1, &vertexBuffer_);
  if (!vertexArray_ || !vertexBuffer_) {
    error = "core scene buffer creation failed";
    release();
    return false;
  }
  functions_.glBindVertexArray(vertexArray_);
  functions_.glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
  functions_.glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(vertices)), vertices.data(), GL_STATIC_DRAW);
  functions_.glEnableVertexAttribArray(0);
  functions_.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
  functions_.glEnableVertexAttribArray(1);
  functions_.glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<const void*>(2 * sizeof(float)));
  functions_.glBindBuffer(GL_ARRAY_BUFFER, 0);
  functions_.glBindVertexArray(0);
  mvpLocation_ = functions_.glGetUniformLocation(program_, "uMvp");
  if (mvpLocation_ < 0) {
    error = "core shader uniform uMvp was not found";
    release();
    return false;
  }
  initialized_ = true;
  return true;
}

bool CoreRenderer::render(int width, int height, bool checkErrors, std::string& error) {
  if (!initialized_) { error = "core renderer is not initialized"; return false; }
  const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.6f;
  const auto projection = math::orthographic(-0.78f * aspect, 0.78f * aspect, -0.78f, 0.78f, -1.0f, 1.0f);
  const auto model = math::multiply(math::translation(0.03f, 0.01f), math::rotationZ(0.05f));
  const auto mvp = math::multiply(projection, model);
  glViewport(0, 0, width, height);
  glClearColor(0.015f, 0.025f, 0.07f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  functions_.glUseProgram(program_);
  functions_.glBindVertexArray(vertexArray_);
  functions_.glUniformMatrix4fv(mvpLocation_, 1, GL_FALSE, mvp.value.data());
  functions_.glDrawArrays(GL_LINES, 0, 4);
  functions_.glDrawArrays(GL_TRIANGLES, 4, 3);
  functions_.glDrawArrays(GL_TRIANGLES, 7, 6);
  functions_.glBindVertexArray(0);
  functions_.glUseProgram(0);
  if (checkErrors) {
    const GLenum glError = glGetError();
    if (glError != GL_NO_ERROR) {
      std::ostringstream message;
      message << "core render OpenGL error 0x" << std::hex << glError;
      error = message.str();
      return false;
    }
  }
  return true;
}

void CoreRenderer::release() {
  if (functions_.glDeleteBuffers && vertexBuffer_) functions_.glDeleteBuffers(1, &vertexBuffer_);
  if (functions_.glDeleteVertexArrays && vertexArray_) functions_.glDeleteVertexArrays(1, &vertexArray_);
  if (functions_.glDeleteProgram && program_) functions_.glDeleteProgram(program_);
  vertexBuffer_ = 0;
  vertexArray_ = 0;
  program_ = 0;
  mvpLocation_ = -1;
  initialized_ = false;
}

} // namespace helion::client
