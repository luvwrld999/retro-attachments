#pragma once
#include <SDL.h>
#include <stdexcept>

inline SDL_Renderer *createDisplayRenderer(SDL_Window *window) {
#if defined(__PROSPERO__) || defined(RETRO_SURFACE_RENDERER)
  // The PS5 video backend exposes a window surface. Bind the software
  // renderer directly instead of asking SDL to select a window driver.
  SDL_Surface *surface = SDL_GetWindowSurface(window);
  if (!surface) throw std::runtime_error(SDL_GetError());
  SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
#else
  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
      SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
#endif
  if (!renderer) throw std::runtime_error(SDL_GetError());
  return renderer;
}

inline void presentDisplay(SDL_Window *window, SDL_Renderer *renderer) {
  SDL_RenderPresent(renderer);
#if defined(__PROSPERO__) || defined(RETRO_SURFACE_RENDERER)
  if (SDL_UpdateWindowSurface(window) != 0)
    throw std::runtime_error(SDL_GetError());
#else
  (void)window;
#endif
}
