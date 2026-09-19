#include "client/font.h"

#include <algorithm>
#include <cctype>

namespace helion::client {
namespace {
std::array<unsigned char, 7> rowsFor(char character) {
  switch (static_cast<char>(std::toupper(static_cast<unsigned char>(character)))) {
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
    case '%': return {25,25,2,4,8,19,19}; case '(': return {2,4,8,8,8,4,2};
    case ')': return {8,4,2,2,2,4,8}; case '!': return {4,4,4,4,4,0,4};
    case ',': return {0,0,0,0,0,4,8}; case '?': return {14,17,1,2,4,0,4};
    case '|': return {4,4,4,4,4,4,4}; case ' ': return {0,0,0,0,0,0,0};
    default: return {14,17,1,2,4,0,4};
  }
}

bool isSupported(char character) {
  const auto upper = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
  if (character == ' ' || character == '\n') return true;
  if (upper >= 'A' && upper <= 'Z') return true;
  if (character >= '0' && character <= '9') return true;
  return std::string_view("/-.:=<>[]_*+%()!,?|").find(character) != std::string_view::npos;
}
}

GlyphBitmap glyphBitmap(char character) {
  if (character == '\n') return {{}, true};
  return {rowsFor(character), isSupported(character)};
}

bool glyphSupported(char character) { return glyphBitmap(character).supported; }

std::string normalizeText(std::string_view value, std::size_t maxCharacters) {
  std::string result;
  result.reserve(std::min(value.size(), maxCharacters));
  for (const char character : value) {
    if (result.size() >= maxCharacters) break;
    result.push_back(glyphSupported(character) ? character : '?');
  }
  return result;
}

float measureText(std::string_view value, float scale, std::size_t maxCharacters) {
  if (scale <= 0) return 0.0f;
  const auto normalized = normalizeText(value, maxCharacters);
  float width = 0.0f;
  float lineWidth = 0.0f;
  for (const char character : normalized) {
    if (character == '\n') {
      width = std::max(width, lineWidth);
      lineWidth = 0.0f;
    } else lineWidth += kFontCellWidth * scale;
  }
  return std::max(width, lineWidth);
}

std::vector<FontVertex> buildTextVertices(std::string_view value, float x, float y,
                                           float scale, FontColor color,
                                           std::size_t maxCharacters, float maxWidth) {
  std::vector<FontVertex> vertices;
  if (scale <= 0) return vertices;
  const auto normalized = normalizeText(value, std::min(maxCharacters, kMaxTextCharacters));
  vertices.reserve(std::min(kMaxTextVertices, normalized.size() * 30));
  const float startX = x;
  for (const char character : normalized) {
    if (character == '\n') {
      x = startX;
      y += kFontCellHeight * scale;
      continue;
    }
    if (maxWidth > 0 && x + kFontCellWidth * scale > startX + maxWidth) break;
    const auto glyph = glyphBitmap(character);
    const int code = static_cast<unsigned char>(character);
    const int cellX = (code - 32) % kFontAtlasColumns;
    const int cellY = (code - 32) / kFontAtlasColumns;
    for (int row = 0; row < 7; ++row) {
      for (int column = 0; column < 5; ++column) {
        if ((glyph.rows[static_cast<std::size_t>(row)] & (1 << (4 - column))) == 0) continue;
        const float px = x + column * scale;
        const float py = y + row * scale;
        const float u0 = static_cast<float>(cellX * kFontCellWidth + column + 0.25f) / (kFontAtlasColumns * kFontCellWidth);
        const float v0 = static_cast<float>(cellY * kFontCellHeight + row + 0.25f) / (kFontAtlasRows * kFontCellHeight);
        const float u1 = static_cast<float>(cellX * kFontCellWidth + column + 0.75f) / (kFontAtlasColumns * kFontCellWidth);
        const float v1 = static_cast<float>(cellY * kFontCellHeight + row + 0.75f) / (kFontAtlasRows * kFontCellHeight);
        vertices.push_back({px, py, u0, v0, color.r, color.g, color.b, color.a});
        vertices.push_back({px + scale, py, u1, v0, color.r, color.g, color.b, color.a});
        vertices.push_back({px + scale, py + scale, u1, v1, color.r, color.g, color.b, color.a});
        vertices.push_back({px, py, u0, v0, color.r, color.g, color.b, color.a});
        vertices.push_back({px + scale, py + scale, u1, v1, color.r, color.g, color.b, color.a});
        vertices.push_back({px, py + scale, u0, v1, color.r, color.g, color.b, color.a});
        if (vertices.size() >= kMaxTextVertices) {
          vertices.resize(kMaxTextVertices - kMaxTextVertices % 6);
          return vertices;
        }
      }
    }
    x += kFontCellWidth * scale;
  }
  return vertices;
}

} // namespace helion::client
