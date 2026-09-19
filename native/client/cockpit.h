#pragma once

#include "client/font.h"
#include "client/presentation.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace helion::client {

struct HudLayout {
  float originX = 0;
  float originY = 0;
  float scale = 1;
  float width = 960;
  float height = 600;
};

struct CockpitTextBatch {
  std::vector<FontVertex> vertices;
  std::size_t glyphs = 0;
};

HudLayout hudLayout(int width, int height);
std::string standingText(const PresentationSnapshot& snapshot, std::string_view id);
std::string targetText(const PresentationSnapshot& snapshot);
CockpitTextBatch buildCockpitText(const PresentationSnapshot& snapshot, int width, int height);

} // namespace helion::client
