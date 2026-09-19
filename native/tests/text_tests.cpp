#include "client/cockpit.h"
#include "client/font.h"
#include "client/text_renderer.h"

#include <cmath>
#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main() {
  using namespace helion::client;
  check(glyphSupported('A') && glyphSupported('z') && glyphSupported('%') && glyphSupported('('),
        "font covers letters and required punctuation");
  check(!glyphSupported('\x01'), "unsupported control character is rejected");
  check(normalizeText("A\x01z", 8) == "A?z", "unsupported characters use bounded fallback");
  check(measureText("AB", 2.0f) == 24.0f, "text measurement uses fixed glyph advance");
  const auto vertices = buildTextVertices("HELION 330", 10, 20, 2.0f, {1, 1, 1, 1}, 32, 200);
  check(!vertices.empty() && vertices.size() % 6 == 0 && vertices.size() <= kMaxTextVertices,
        "glyph quads are bounded triangles");
  check(vertices.size() == visibleGlyphCount("HELION 330", 2.0f, 32, 200) * 6,
        "text uses exactly one six-vertex quad per visible glyph");
  const auto uv = glyphUv('A');
  check(!glyphUv('\n').visible && !glyphUv(' ').visible, "layout-only characters have no atlas access");
  for (int byte = 0; byte <= 255; ++byte) {
    const auto cell = glyphUv(static_cast<char>(byte));
    check(!cell.visible || (cell.u0 >= 0 && cell.u1 <= 1 && cell.v1 >= 0 && cell.v0 <= 1),
          "all byte values produce bounded UVs or no geometry");
  }
  check(uv.visible && uv.u0 < uv.u1 && uv.v0 > uv.v1 && uv.u1 - uv.u0 < 0.06f,
        "glyph UVs stay inside one padded atlas cell");
  check(buildTextVertices("A B", 0, 0, 1, {1, 1, 1, 1}).size() == 12,
        "spaces advance layout without creating geometry");
  check(normalizeText(std::string("A\xE2\x98\x83") + "B", 8) == "A?B",
        "unsupported UTF-8 sequences use one safe fallback glyph");
  check(glyphQuadVertexCount(7) == 42 && textComponentCount(3) == 24 &&
        textBufferBytes(3) == 3 * sizeof(FontVertex),
        "text telemetry units are stable");
  check(buildTextVertices("THIS LINE IS TOO LONG", 0, 0, 2, {1, 1, 1, 1}, 128, 20).size() <
        buildTextVertices("THIS LINE IS TOO LONG", 0, 0, 2, {1, 1, 1, 1}, 128).size(),
        "maximum width truncates glyph generation");

  const auto small = hudLayout(320, 200);
  const auto large = hudLayout(1920, 1200);
  check(small.scale >= 0.25f && small.originX >= -0.01f && small.originY >= -0.01f,
        "small cockpit layout remains visible");
  check(large.scale > small.scale && large.originX >= 0 && large.originY >= 0,
        "large cockpit layout scales and centers");

  PresentationSnapshot snapshot;
  snapshot.reputation["authority.kepler"] = 25;
  snapshot.reputation["corp.orion"] = 75;
  snapshot.reputation["criminal.red_wake"] = -75;
  check(standingText(snapshot, "authority.kepler").find("Friendly") != std::string::npos &&
        standingText(snapshot, "corp.orion").find("Allied") != std::string::npos &&
        standingText(snapshot, "criminal.red_wake").find("Hostile") != std::string::npos,
        "cockpit reputation strings use canonical labels");
  snapshot.recentMessages = {"GALNET / Red Wake activity", "OK MINED cargo=1"};
  const auto cockpit = buildCockpitText(snapshot, 960, 600);
  check(!cockpit.vertices.empty() && cockpit.glyphs > 0,
        "snapshot produces readable cockpit text");
  std::cout << "text and cockpit layout tests passed\n";
  return failures == 0 ? 0 : 1;
}
