#pragma once

#include "client/math.h"

#include <SDL2/SDL.h>
#include <GL/gl.h>

#include <string>
#include <string_view>
#include <vector>

namespace helion::client {

inline constexpr std::string_view kCoreShaderVersion = "330 core";

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
  DrawArrays glDrawArrays = nullptr;

  bool load(std::string& error);
  bool complete() const;
};

std::string firstMissingCoreFunction(const std::vector<std::string>& available);

class CoreRenderer {
 public:
  CoreRenderer() = default;
  ~CoreRenderer();

  CoreRenderer(const CoreRenderer&) = delete;
  CoreRenderer& operator=(const CoreRenderer&) = delete;

  bool initialize(std::string& error);
  bool render(int width, int height, bool checkErrors, std::string& error);
  void release();
  bool initialized() const { return initialized_; }

 private:
  bool compileShader(GLenum type, const char* source, GLuint& shader, std::string& error);
  bool linkProgram(GLuint vertexShader, GLuint fragmentShader, std::string& error);

  CoreFunctions functions_;
  GLuint program_ = 0;
  GLuint vertexArray_ = 0;
  GLuint vertexBuffer_ = 0;
  GLint mvpLocation_ = -1;
  bool initialized_ = false;
};

} // namespace helion::client
