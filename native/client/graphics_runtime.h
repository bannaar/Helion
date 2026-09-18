#pragma once

#include "client/graphics.h"

#include <SDL2/SDL.h>

namespace helion::client {

struct GraphicsContext {
  SDL_Window* window = nullptr;
  SDL_GLContext context = nullptr;
  helion::graphics::GraphicsContextResult result;
};

GraphicsContext createGraphicsContext(const char* title, int width, int height, bool hidden);
GraphicsContext createCoreGraphicsContext(const char* title, int width, int height, bool hidden);
void destroyGraphicsContext(GraphicsContext& graphicsContext);

} // namespace helion::client
