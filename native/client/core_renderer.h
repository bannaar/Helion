#pragma once

#include "client/math.h"
#include "client/presentation.h"
#include "client/text_renderer.h"

#include <SDL2/SDL.h>
#include <GL/gl.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace helion::client {

inline constexpr std::string_view kCoreShaderVersion = "330 core";
inline constexpr std::size_t kMaxCoreDynamicVertices = 12000;

struct CoreFunctions {
  using CreateShader = GLuint (*)(GLenum);
  using ShaderSource = void (*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
  using CompileShader = void (*)(GLuint);
  using GetShaderiv = void (*)(GLuint, GLenum, GLint*);
  using GetShaderInfoLog = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);
  using DeleteShader = void (*)(GLuint);
  using CreateProgram = GLuint (*)();
  using AttachShader = void (*)(GLuint, GLuint);
  using LinkProgram = void (*)(GLuint);
  using GetProgramiv = void (*)(GLuint, GLenum, GLint*);
  using GetProgramInfoLog = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);
  using UseProgram = void (*)(GLuint);
  using DeleteProgram = void (*)(GLuint);
  using GenVertexArrays = void (*)(GLsizei, GLuint*);
  using BindVertexArray = void (*)(GLuint);
  using DeleteVertexArrays = void (*)(GLsizei, const GLuint*);
  using GenBuffers = void (*)(GLsizei, GLuint*);
  using BindBuffer = void (*)(GLenum, GLuint);
  using BufferData = void (*)(GLenum, GLsizeiptr, const void*, GLenum);
  using DeleteBuffers = void (*)(GLsizei, const GLuint*);
  using EnableVertexAttribArray = void (*)(GLuint);
  using VertexAttribPointer = void (*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
  using GetUniformLocation = GLint (*)(GLuint, const GLchar*);
  using UniformMatrix4fv = void (*)(GLint, GLsizei, GLboolean, const GLfloat*);
  using Uniform1i = void (*)(GLint, GLint);
  using ActiveTexture = void (*)(GLenum);
  using GenTextures = void (*)(GLsizei, GLuint*);
  using BindTexture = void (*)(GLenum, GLuint);
  using TexImage2D = void (*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
  using TexParameteri = void (*)(GLenum, GLenum, GLint);
  using DeleteTextures = void (*)(GLsizei, const GLuint*);
  using Enable = void (*)(GLenum);
  using Disable = void (*)(GLenum);
  using BlendFunc = void (*)(GLenum, GLenum);
  using DrawArrays = void (*)(GLenum, GLint, GLsizei);

  CreateShader glCreateShader = nullptr;
  ShaderSource glShaderSource = nullptr;
  CompileShader glCompileShader = nullptr;
  GetShaderiv glGetShaderiv = nullptr;
  GetShaderInfoLog glGetShaderInfoLog = nullptr;
  DeleteShader glDeleteShader = nullptr;
  CreateProgram glCreateProgram = nullptr;
  AttachShader glAttachShader = nullptr;
  LinkProgram glLinkProgram = nullptr;
  GetProgramiv glGetProgramiv = nullptr;
  GetProgramInfoLog glGetProgramInfoLog = nullptr;
  UseProgram glUseProgram = nullptr;
  DeleteProgram glDeleteProgram = nullptr;
  GenVertexArrays glGenVertexArrays = nullptr;
  BindVertexArray glBindVertexArray = nullptr;
  DeleteVertexArrays glDeleteVertexArrays = nullptr;
  GenBuffers glGenBuffers = nullptr;
  BindBuffer glBindBuffer = nullptr;
  BufferData glBufferData = nullptr;
  DeleteBuffers glDeleteBuffers = nullptr;
  EnableVertexAttribArray glEnableVertexAttribArray = nullptr;
  VertexAttribPointer glVertexAttribPointer = nullptr;
  GetUniformLocation glGetUniformLocation = nullptr;
  UniformMatrix4fv glUniformMatrix4fv = nullptr;
  Uniform1i glUniform1i = nullptr;
  ActiveTexture glActiveTexture = nullptr;
  GenTextures glGenTextures = nullptr;
  BindTexture glBindTexture = nullptr;
  TexImage2D glTexImage2D = nullptr;
  TexParameteri glTexParameteri = nullptr;
  DeleteTextures glDeleteTextures = nullptr;
  Enable glEnable = nullptr;
  Disable glDisable = nullptr;
  BlendFunc glBlendFunc = nullptr;
  DrawArrays glDrawArrays = nullptr;

  bool load(std::string& error);
  bool complete() const;
};

std::string firstMissingCoreFunction(const std::vector<std::string>& available);

struct CoreRenderStats {
  int drawCalls = 0;
  std::size_t staticVertices = 0;
  std::size_t worldVertices = 0;
  std::size_t hudVertices = 0;
  std::size_t textVertices = 0;
  std::size_t dynamicVertices = 0;
  std::size_t glyphs = 0;
  std::size_t textures = 0;
  double frameMilliseconds = 0;
};

class CoreRenderer {
 public:
  CoreRenderer() = default;
  ~CoreRenderer();

  CoreRenderer(const CoreRenderer&) = delete;
  CoreRenderer& operator=(const CoreRenderer&) = delete;

  bool initialize(std::string& error);
  bool render(int width, int height, const PresentationSnapshot& snapshot,
              bool checkErrors, CoreRenderStats* stats, std::string& error);
  void release();
  bool initialized() const { return initialized_; }

 private:
  bool compileShader(GLenum type, const char* source, GLuint& shader, std::string& error);
  bool linkProgram(GLuint vertexShader, GLuint fragmentShader, std::string& error);
  bool setupBuffer(GLuint vertexArray, GLuint vertexBuffer, std::string& error);

  CoreFunctions functions_;
  GLuint program_ = 0;
  GLuint staticVertexArray_ = 0;
  GLuint staticVertexBuffer_ = 0;
  GLuint dynamicVertexArray_ = 0;
  GLuint dynamicVertexBuffer_ = 0;
  GLint mvpLocation_ = -1;
  std::size_t staticVertexCount_ = 0;
  TextRenderer textRenderer_;
  bool initialized_ = false;
};

} // namespace helion::client
