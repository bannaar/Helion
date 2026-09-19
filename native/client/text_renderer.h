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
  std::size_t indices = 0;
  std::size_t glyphQuadVertices = 0;
  std::size_t components = 0;
  std::size_t uploadedBytes = 0;
  std::size_t glyphs = 0;
  int drawCalls = 0;
  std::size_t textures = 0;
  int atlasWidth = 0;
  int atlasHeight = 0;
  std::size_t atlasBytes = 0;
};

inline constexpr std::size_t glyphQuadVertexCount(std::size_t glyphs) { return glyphs * 6; }
inline constexpr std::size_t textComponentCount(std::size_t vertices) { return vertices * 8; }
inline constexpr std::size_t textBufferBytes(std::size_t vertices) { return vertices * sizeof(FontVertex); }

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
