#define RETRO_SURFACE_RENDERER
#include "display.hpp"
#include <cassert>
#include <cstring>
int main() {
  assert(SDL_Init(SDL_INIT_VIDEO)==0);
  SDL_Window *w=SDL_CreateWindow("surface test",0,0,64,64,0);
  assert(w);
  SDL_Renderer *r=createDisplayRenderer(w);
  SDL_RendererInfo info{}; assert(SDL_GetRendererInfo(r,&info)==0);
  assert(info.flags & SDL_RENDERER_SOFTWARE);
  assert(SDL_SetRenderDrawColor(r,25,75,145,255)==0);
  assert(SDL_RenderClear(r)==0);
  presentDisplay(w,r);
  SDL_Surface *s=SDL_GetWindowSurface(w); assert(s);
  assert(SDL_LockSurface(s)==0);
  Uint32 pixel=0; std::memcpy(&pixel,s->pixels,s->format->BytesPerPixel);
  Uint8 red,green,blue; SDL_GetRGB(pixel,s->format,&red,&green,&blue);
  SDL_UnlockSurface(s);
  assert(red==25 && green==75 && blue==145);
  SDL_DestroyRenderer(r); SDL_DestroyWindow(w); SDL_Quit();
  puts("PASS: direct surface renderer presents expected pixels");
}
