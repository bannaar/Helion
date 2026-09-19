#pragma once

#include "client/font.h"
#include "client/math.h"

#include <cstddef>
#include <string>
#include <vector>

namespace helion::client {

struct CoreFunctions;

struct TextRenderStats {
  std::size_t vertices = 0;
  std::size_t glyphs = 0;
  int drawCalls = 0;
  std::size_t textures = 0;
};

class TextRenderer {
 public:
  TextRenderer() = default;
  ~TextRenderer();

  TextRenderer(const TextRenderer&) = delete;
  TextRenderer& operator=(const TextRenderer&) = delete;

  bool initialize(CoreFunctions& functions, std::string& error);
  bool render(const math::Mat4& projection, const std::vector<FontVertex>& vertices,
              std::size_t glyphs, bool checkErrors, TextRenderStats* stats,
              std::string& error);
  void release();
  bool initialized() const { return initialized_; }

 private:
  CoreFunctions* functions_ = nullptr;
  unsigned int program_ = 0;
  unsigned int vertexArray_ = 0;
  unsigned int vertexBuffer_ = 0;
  unsigned int texture_ = 0;
  int mvpLocation_ = -1;
  int samplerLocation_ = -1;
  bool initialized_ = false;
};

} // namespace helion::client
