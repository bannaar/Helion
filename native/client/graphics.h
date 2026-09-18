#pragma once

#include <string>
#include <string_view>

namespace helion::graphics {

enum class Profile { unknown, compatibility, core };
enum class RendererClass { unknown, hardware, software };
enum class RendererMode { auto_mode, legacy, core };

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

struct RendererSelection {
  RendererMode requested = RendererMode::auto_mode;
  bool useCore = false;
  bool coreAvailable = false;
  std::string error;
};

bool usableFixedFunctionContext(const GraphicsCapabilities& capabilities);
GraphicsContextResult selectContext(const GraphicsCapabilities& compatibility30,
                                    const GraphicsCapabilities& fallback21);
const char* profileName(Profile profile);
const char* rendererClassName(RendererClass rendererClass);
RendererClass classifyRenderer(std::string_view vendor, std::string_view renderer);
std::string diagnosticLine(const GraphicsContextResult& result);
bool parseRendererMode(std::string_view value, RendererMode& mode);
const char* rendererModeName(RendererMode mode);
RendererSelection selectRenderer(RendererMode requested, bool coreAvailable);
bool usableCoreContext(const GraphicsCapabilities& capabilities);

} // namespace helion::graphics
