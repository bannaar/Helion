#include "client/graphics_runtime.h"

#include <GL/gl.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace helion::client {
namespace {
using helion::graphics::GraphicsCapabilities;
using helion::graphics::GraphicsRequest;
using helion::graphics::Profile;

GraphicsCapabilities queryCapabilities() {
  GraphicsCapabilities result;
  const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  const auto* glsl = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
  if (vendor) result.vendor = vendor;
  if (renderer) result.renderer = renderer;
  if (version) result.version = version;
  if (glsl) result.shadingLanguageVersion = glsl;

  SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &result.major);
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &result.minor);
  int profile = 0;
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile);
  if (profile == SDL_GL_CONTEXT_PROFILE_CORE) result.profile = Profile::core;
  else if (profile == SDL_GL_CONTEXT_PROFILE_COMPATIBILITY || result.major < 3)
    result.profile = Profile::compatibility;
  else result.profile = Profile::unknown;

  if ((result.major == 0 || result.minor == 0) && !result.version.empty())
    std::sscanf(result.version.c_str(), "%d.%d", &result.major, &result.minor);
  result.complete = !result.vendor.empty() && !result.renderer.empty() &&
    !result.version.empty() && result.major > 0;
  return result;
}

GraphicsCapabilities attempt(SDL_Window*& window, SDL_GLContext& context,
                              const char* title, int width, int height, bool hidden,
                              const GraphicsRequest& request) {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, request.major);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, request.minor);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                      request.profile == Profile::core ? SDL_GL_CONTEXT_PROFILE_CORE : SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  const Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | (hidden ? SDL_WINDOW_HIDDEN : 0);
  window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
  if (!window) return {};
  context = SDL_GL_CreateContext(window);
  if (!context) {
    SDL_DestroyWindow(window);
    window = nullptr;
    return {};
  }
  return queryCapabilities();
}
}

GraphicsContext createGraphicsContext(const char* title, int width, int height, bool hidden) {
  GraphicsContext result;
  const GraphicsRequest compatibility30{3, 0, Profile::compatibility};
  const GraphicsRequest fallback21{2, 1, Profile::compatibility};
  SDL_Window* firstWindow = nullptr;
  SDL_GLContext firstContext = nullptr;
  const auto first = attempt(firstWindow, firstContext, title, width, height, hidden, compatibility30);
  if (helion::graphics::usableFixedFunctionContext(first)) {
    result.window = firstWindow;
    result.context = firstContext;
    result.result = helion::graphics::selectContext(first, {});
    return result;
  }
  if (firstContext) SDL_GL_DeleteContext(firstContext);
  if (firstWindow) SDL_DestroyWindow(firstWindow);

  SDL_Window* fallbackWindow = nullptr;
  SDL_GLContext fallbackContext = nullptr;
  const auto fallback = attempt(fallbackWindow, fallbackContext, title, width, height, hidden, fallback21);
  result.result = helion::graphics::selectContext(first, fallback);
  if (!result.result.selected) {
    if (fallbackContext) SDL_GL_DeleteContext(fallbackContext);
    if (fallbackWindow) SDL_DestroyWindow(fallbackWindow);
    return result;
  }
  result.window = fallbackWindow;
  result.context = fallbackContext;
  return result;
}

void destroyGraphicsContext(GraphicsContext& graphicsContext) {
  if (graphicsContext.context) SDL_GL_DeleteContext(graphicsContext.context);
  if (graphicsContext.window) SDL_DestroyWindow(graphicsContext.window);
  graphicsContext.context = nullptr;
  graphicsContext.window = nullptr;
}

} // namespace helion::client
