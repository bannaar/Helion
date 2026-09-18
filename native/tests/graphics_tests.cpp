#include "client/graphics.h"

#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
helion::graphics::GraphicsCapabilities caps(int major, int minor, helion::graphics::Profile profile) {
  return {major, minor, profile, true, "Intel", "Mesa Intel HD Graphics 3000", "OpenGL", "GLSL"};
}
}

int main() {
  using namespace helion::graphics;
  const auto preferred = selectContext(caps(3, 3, Profile::compatibility), caps(2, 1, Profile::compatibility));
  check(preferred.selected && preferred.firstCompatibilityAttemptSucceeded && !preferred.fallbackTo21 &&
        preferred.actual.major == 3 && preferred.actual.profile == Profile::compatibility,
        "prefers usable compatibility context");
  const auto tooOld = selectContext(caps(2, 1, Profile::compatibility), caps(2, 1, Profile::compatibility));
  check(tooOld.selected && tooOld.fallbackTo21 && !tooOld.firstCompatibilityAttemptSucceeded,
        "does not mislabel a 2.1 result as a successful 3.0 attempt");
  const auto fallback = selectContext({}, caps(2, 1, Profile::compatibility));
  check(fallback.selected && fallback.fallbackTo21 && !fallback.firstCompatibilityAttemptSucceeded &&
        fallback.actual.major == 2 && fallback.actual.minor == 1,
        "falls back to OpenGL 2.1");
  const auto core = selectContext(caps(3, 3, Profile::core), caps(2, 1, Profile::compatibility));
  check(core.selected && core.fallbackTo21 && core.actual.profile == Profile::compatibility,
        "rejects core context for fixed-function renderer");
  check(!selectContext({}, {}).selected, "rejects incomplete context results");
  check(profileName(Profile::compatibility) == std::string("compatibility") &&
        profileName(Profile::core) == std::string("core"), "profile labels are stable");
  check(classifyRenderer("Intel", "Mesa Intel(R) HD Graphics 3000") == RendererClass::hardware,
        "Intel renderer classified as hardware");
  check(classifyRenderer("Mesa", "llvmpipe (LLVM)") == RendererClass::software,
        "llvmpipe classified as software");
  check(classifyRenderer("Unknown", "Mystery") == RendererClass::unknown,
        "unknown renderer is not labeled hardware");
  check(diagnosticLine(preferred) ==
        "GRAPHICS requested=3.0-compatibility actual=3.3-compatibility first-compat=yes fallback-21=no renderer-class=hardware vendor=Intel renderer=Mesa Intel HD Graphics 3000 version=OpenGL glsl=GLSL",
        "diagnostic serialization is stable");
  return failures == 0 ? 0 : 1;
}
