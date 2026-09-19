#include "client/text_renderer.h"

#include "client/core_renderer.h"

#include <GL/gl.h>
#include <sstream>

namespace helion::client {
namespace {
constexpr int kAtlasWidth = kFontAtlasColumns * kFontCellWidth;
constexpr int kAtlasHeight = kFontAtlasRows * kFontCellHeight;

std::string shaderLog(CoreFunctions& functions, GLuint shader) {
  GLint length = 0;
  functions.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
  std::string log(static_cast<std::size_t>(length > 1 ? length : 1), '\0');
  GLsizei written = 0;
  functions.glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), &written, log.data());
  log.resize(static_cast<std::size_t>(written));
  return log;
}

std::string programLog(CoreFunctions& functions, GLuint program) {
  GLint length = 0;
  functions.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
  std::string log(static_cast<std::size_t>(length > 1 ? length : 1), '\0');
  GLsizei written = 0;
  functions.glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), &written, log.data());
  log.resize(static_cast<std::size_t>(written));
  return log;
}

bool compile(CoreFunctions& functions, GLenum type, const char* source, GLuint& shader,
             std::string& error) {
  shader = functions.glCreateShader(type);
  if (!shader) { error = "text shader creation failed"; return false; }
  const GLchar* sources[] = {source};
  functions.glShaderSource(shader, 1, sources, nullptr);
  functions.glCompileShader(shader);
  GLint compiled = GL_FALSE;
  functions.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled == GL_TRUE) return true;
  error = "text shader compilation failed: " + shaderLog(functions, shader);
  functions.glDeleteShader(shader);
  shader = 0;
  return false;
}

std::vector<unsigned char> atlasPixels() {
  std::vector<unsigned char> pixels(static_cast<std::size_t>(kAtlasWidth * kAtlasHeight * 4), 0);
  for (int code = 32; code <= 126; ++code) {
    const auto glyph = glyphBitmap(static_cast<char>(code));
    const int cellX = (code - 32) % kFontAtlasColumns;
    const int cellY = (code - 32) / kFontAtlasColumns;
    for (int row = 0; row < 7; ++row) {
      for (int column = 0; column < 5; ++column) {
        if ((glyph.rows[static_cast<std::size_t>(row)] & (1 << (4 - column))) == 0) continue;
        const int px = cellX * kFontCellWidth + column;
        const int py = cellY * kFontCellHeight + row;
        const auto offset = static_cast<std::size_t>((py * kAtlasWidth + px) * 4);
        pixels[offset] = 255;
        pixels[offset + 1] = 255;
        pixels[offset + 2] = 255;
        pixels[offset + 3] = 255;
      }
    }
  }
  return pixels;
}
}

TextRenderer::~TextRenderer() { release(); }

bool TextRenderer::initialize(CoreFunctions& functions, std::string& error) {
  release();
  functions_ = &functions;
  static constexpr const char* vertexSource = R"GLSL(#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;
uniform mat4 uMvp;
out vec2 vUv;
out vec4 vColor;
void main() {
  gl_Position = uMvp * vec4(aPosition, 0.0, 1.0);
  vUv = aUv;
  vColor = aColor;
}
)GLSL";
  static constexpr const char* fragmentSource = R"GLSL(#version 330 core
in vec2 vUv;
in vec4 vColor;
uniform sampler2D uFont;
out vec4 fragColor;
void main() {
  fragColor = vec4(vColor.rgb, vColor.a * texture(uFont, vUv).a);
}
)GLSL";
  GLuint vertexShader = 0;
  GLuint fragmentShader = 0;
  if (!compile(functions, GL_VERTEX_SHADER, vertexSource, vertexShader, error) ||
      !compile(functions, GL_FRAGMENT_SHADER, fragmentSource, fragmentShader, error)) {
    if (fragmentShader) functions.glDeleteShader(fragmentShader);
    release();
    return false;
  }
  program_ = functions.glCreateProgram();
  if (!program_) {
    error = "text program creation failed";
    functions.glDeleteShader(vertexShader);
    functions.glDeleteShader(fragmentShader);
    release();
    return false;
  }
  functions.glAttachShader(program_, vertexShader);
  functions.glAttachShader(program_, fragmentShader);
  functions.glLinkProgram(program_);
  functions.glDeleteShader(vertexShader);
  functions.glDeleteShader(fragmentShader);
  GLint linked = GL_FALSE;
  functions.glGetProgramiv(program_, GL_LINK_STATUS, &linked);
  if (linked != GL_TRUE) {
    error = "text program linking failed: " + programLog(functions, program_);
    release();
    return false;
  }
  mvpLocation_ = functions.glGetUniformLocation(program_, "uMvp");
  samplerLocation_ = functions.glGetUniformLocation(program_, "uFont");
  if (mvpLocation_ < 0 || samplerLocation_ < 0) {
    error = "text shader uniforms were not found";
    release();
    return false;
  }

  functions.glGenVertexArrays(1, &vertexArray_);
  functions.glGenBuffers(1, &vertexBuffer_);
  functions.glBindVertexArray(vertexArray_);
  functions.glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
  functions.glEnableVertexAttribArray(0);
  functions.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
  functions.glEnableVertexAttribArray(1);
  functions.glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
    reinterpret_cast<const void*>(2 * sizeof(float)));
  functions.glEnableVertexAttribArray(2);
  functions.glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
    reinterpret_cast<const void*>(4 * sizeof(float)));
  functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
  functions.glBindVertexArray(0);

  functions.glGenTextures(1, &texture_);
  functions.glActiveTexture(GL_TEXTURE0);
  functions.glBindTexture(GL_TEXTURE_2D, texture_);
  const auto pixels = atlasPixels();
  functions.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kAtlasWidth, kAtlasHeight, 0,
    GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  functions.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  functions.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  functions.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  functions.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  functions.glBindTexture(GL_TEXTURE_2D, 0);
  if (!vertexArray_ || !vertexBuffer_ || !texture_) {
    error = "text GPU resource creation failed";
    release();
    return false;
  }
  initialized_ = true;
  return true;
}

bool TextRenderer::render(const math::Mat4& projection, const std::vector<FontVertex>& vertices,
                          std::size_t glyphs, bool checkErrors, TextRenderStats* stats,
                          std::string& error) {
  if (!initialized_ || !functions_) { error = "text renderer is not initialized"; return false; }
  if (vertices.empty()) {
    if (stats) *stats = {};
    return true;
  }
  const auto checkStage = [&](const char* stage) {
    if (!checkErrors) return true;
    const GLenum glError = glGetError();
    if (glError == GL_NO_ERROR) return true;
    std::ostringstream message;
    message << "text render OpenGL error at " << stage << " 0x" << std::hex << glError;
    error = message.str();
    return false;
  };
  functions_->glUseProgram(program_);
  functions_->glUniformMatrix4fv(mvpLocation_, 1, GL_FALSE, projection.value.data());
  functions_->glUniform1i(samplerLocation_, 0);
  functions_->glActiveTexture(GL_TEXTURE0);
  functions_->glBindTexture(GL_TEXTURE_2D, texture_);
  functions_->glBindVertexArray(vertexArray_);
  functions_->glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
  functions_->glBufferData(GL_ARRAY_BUFFER,
    static_cast<GLsizeiptr>(vertices.size() * sizeof(FontVertex)), vertices.data(), GL_DYNAMIC_DRAW);
  functions_->glEnable(GL_BLEND);
  functions_->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  functions_->glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
  functions_->glDisable(GL_BLEND);
  functions_->glBindBuffer(GL_ARRAY_BUFFER, 0);
  functions_->glBindVertexArray(0);
  functions_->glBindTexture(GL_TEXTURE_2D, 0);
  functions_->glUseProgram(0);
  if (!checkStage("text-draw")) return false;
  if (stats) {
    stats->vertices = vertices.size();
    stats->glyphs = glyphs;
    stats->drawCalls = 1;
    stats->textures = 1;
  }
  return true;
}

void TextRenderer::release() {
  if (!functions_) return;
  if (functions_->glDeleteTextures && texture_) functions_->glDeleteTextures(1, &texture_);
  if (functions_->glDeleteBuffers && vertexBuffer_) functions_->glDeleteBuffers(1, &vertexBuffer_);
  if (functions_->glDeleteVertexArrays && vertexArray_) functions_->glDeleteVertexArrays(1, &vertexArray_);
  if (functions_->glDeleteProgram && program_) functions_->glDeleteProgram(program_);
  texture_ = 0;
  vertexBuffer_ = 0;
  vertexArray_ = 0;
  program_ = 0;
  mvpLocation_ = -1;
  samplerLocation_ = -1;
  initialized_ = false;
}

} // namespace helion::client
