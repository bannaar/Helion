#include "client/graphics.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace helion::graphics {
namespace {
bool atLeast(int major, int minor, int requiredMajor, int requiredMinor) {
  return major > requiredMajor || (major == requiredMajor && minor >= requiredMinor);
}

bool shadingLanguageAtLeast(std::string_view version, int requiredMajor, int requiredMinor) {
  int major = 0;
  int minor = 0;
  const std::string value(version);
  if (std::sscanf(value.c_str(), "%d.%d", &major, &minor) != 2) return false;
  return atLeast(major, minor, requiredMajor, requiredMinor);
}

std::string lower(std::string_view value) {
  std::string result(value);
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return result;
}
}

bool usableFixedFunctionContext(const GraphicsCapabilities& capabilities) {
  return capabilities.complete && capabilities.profile != Profile::core &&
    atLeast(capabilities.major, capabilities.minor, 2, 1);
}

GraphicsContextResult selectContext(const GraphicsCapabilities& compatibility30,
                                    const GraphicsCapabilities& fallback21) {
  GraphicsContextResult result;
  result.requested = {3, 0, Profile::compatibility};
  const bool usableCompatibility30 = compatibility30.complete && compatibility30.profile == Profile::compatibility &&
    atLeast(compatibility30.major, compatibility30.minor, 3, 0);
  if (usableCompatibility30) {
    result.selected = true;
    result.firstCompatibilityAttemptSucceeded = true;
    result.actual = compatibility30;
    return result;
  }
  result.requested = {2, 1, Profile::compatibility};
  result.fallbackTo21 = true;
  if (usableFixedFunctionContext(fallback21)) {
    result.selected = true;
    result.actual = fallback21;
    return result;
  }
  result.error = "no usable fixed-function OpenGL context";
  return result;
}

const char* profileName(Profile profile) {
  switch (profile) {
    case Profile::compatibility: return "compatibility";
    case Profile::core: return "core";
    case Profile::unknown: return "unknown";
  }
  return "unknown";
}

const char* rendererClassName(RendererClass rendererClass) {
  switch (rendererClass) {
    case RendererClass::hardware: return "hardware";
    case RendererClass::software: return "software";
    case RendererClass::unknown: return "unknown";
  }
  return "unknown";
}

RendererClass classifyRenderer(std::string_view vendor, std::string_view renderer) {
  const std::string combined = lower(std::string(vendor) + " " + std::string(renderer));
  for (const auto* marker : {"llvmpipe", "softpipe", "software rasterizer", "swrast"})
    if (combined.find(marker) != std::string::npos) return RendererClass::software;
  if (combined.find("intel") != std::string::npos || combined.find("nvidia") != std::string::npos ||
      combined.find("amd") != std::string::npos || combined.find("radeon") != std::string::npos)
    return RendererClass::hardware;
  return RendererClass::unknown;
}

std::string diagnosticLine(const GraphicsContextResult& result) {
  const auto& actual = result.actual;
  const auto rendererClass = classifyRenderer(actual.vendor, actual.renderer);
  std::string line = "GRAPHICS requested=" + std::to_string(result.requested.major) + "." +
    std::to_string(result.requested.minor) + "-" + profileName(result.requested.profile) +
    " actual=" + std::to_string(actual.major) + "." + std::to_string(actual.minor) +
    "-" + profileName(actual.profile) +
    " first-compat=" + (result.firstCompatibilityAttemptSucceeded ? "yes" : "no") +
    " fallback-21=" + (result.fallbackTo21 ? "yes" : "no") +
    " renderer-class=" + rendererClassName(rendererClass) +
    " vendor=" + (actual.vendor.empty() ? "unknown" : actual.vendor) +
    " renderer=" + (actual.renderer.empty() ? "unknown" : actual.renderer) +
    " version=" + (actual.version.empty() ? "unknown" : actual.version) +
    " glsl=" + (actual.shadingLanguageVersion.empty() ? "unavailable" : actual.shadingLanguageVersion);
  if (!result.selected) line += " error=" + result.error;
  return line;
}

bool parseRendererMode(std::string_view value, RendererMode& mode) {
  if (value == "auto") mode = RendererMode::auto_mode;
  else if (value == "legacy") mode = RendererMode::legacy;
  else if (value == "core") mode = RendererMode::core;
  else return false;
  return true;
}

const char* rendererModeName(RendererMode mode) {
  switch (mode) {
    case RendererMode::auto_mode: return "auto";
    case RendererMode::legacy: return "legacy";
    case RendererMode::core: return "core";
  }
  return "unknown";
}

bool usableCoreContext(const GraphicsCapabilities& capabilities) {
  return capabilities.complete && capabilities.profile == Profile::core &&
    atLeast(capabilities.major, capabilities.minor, 3, 3) &&
    shadingLanguageAtLeast(capabilities.shadingLanguageVersion, 3, 30);
}

RendererSelection selectRenderer(RendererMode requested, bool coreAvailable) {
  RendererSelection result;
  result.requested = requested;
  result.coreAvailable = coreAvailable;
  if (requested == RendererMode::core) {
    if (!coreAvailable) result.error = "OpenGL 3.3 core renderer is unavailable";
    else result.useCore = true;
    return result;
  }
  // Auto deliberately remains legacy until the core renderer reaches gameplay parity.
  result.useCore = false;
  return result;
}

} // namespace helion::graphics
