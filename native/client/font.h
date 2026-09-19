#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace helion::client {

inline constexpr std::size_t kMaxTextCharacters = 512;
inline constexpr std::size_t kMaxTextVertices = 120000;
inline constexpr int kFontCellWidth = 6;
inline constexpr int kFontCellHeight = 8;
inline constexpr int kFontAtlasColumns = 16;
inline constexpr int kFontAtlasRows = 6;

struct GlyphBitmap {
  std::array<unsigned char, 7> rows{};
  bool supported = false;
};

struct FontColor {
  float r = 1.0f;
  float g = 1.0f;
  float b = 1.0f;
  float a = 1.0f;
};

struct FontVertex {
  float x = 0;
  float y = 0;
  float u = 0;
  float v = 0;
  float r = 1.0f;
  float g = 1.0f;
  float b = 1.0f;
  float a = 1.0f;
};

GlyphBitmap glyphBitmap(char character);
bool glyphSupported(char character);
std::string normalizeText(std::string_view value, std::size_t maxCharacters = kMaxTextCharacters);
float measureText(std::string_view value, float scale = 1.0f,
                 std::size_t maxCharacters = kMaxTextCharacters);
std::vector<FontVertex> buildTextVertices(std::string_view value, float x, float y,
                                           float scale, FontColor color,
                                           std::size_t maxCharacters = kMaxTextCharacters,
                                           float maxWidth = 0.0f);

} // namespace helion::client
