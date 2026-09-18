#pragma once

#include <string>
#include <string_view>

namespace helion::graphics {

enum class Profile { unknown, compatibility, core };
enum class RendererClass { unknown, hardware, software };

struct GraphicsRequest {
  int major = 0;
  int minor = 0;
  Profile profile = Profile::unknown;
};

struct GraphicsCapabilities {
  int major = 0;
  int minor = 0;
  Profile profile = Profile::unknown;
  bool complete = false;
  std::string vendor;
  std::string renderer;
  std::string version;
  std::string shadingLanguageVersion;
};

struct GraphicsContextResult {
  bool selected = false;
  bool firstCompatibilityAttemptSucceeded = false;
  bool fallbackTo21 = false;
  GraphicsRequest requested;
  GraphicsCapabilities actual;
  std::string error;
};

bool usableFixedFunctionContext(const GraphicsCapabilities& capabilities);
GraphicsContextResult selectContext(const GraphicsCapabilities& compatibility30,
                                    const GraphicsCapabilities& fallback21);
const char* profileName(Profile profile);
const char* rendererClassName(RendererClass rendererClass);
RendererClass classifyRenderer(std::string_view vendor, std::string_view renderer);
std::string diagnosticLine(const GraphicsContextResult& result);

} // namespace helion::graphics
