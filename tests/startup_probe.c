/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
struct notification { char reserved[45]; char message[3075]; };
int sceKernelSendNotificationRequest(int, void *, size_t, int);
static void report(const char *message) {
  struct notification n = {0};
  snprintf(n.message, sizeof(n.message), "Retro Attachments probe: %s", message);
  sceKernelSendNotificationRequest(0, &n, sizeof(n), 0);
  fprintf(stderr, "%s\n", n.message); fflush(stderr);
  FILE *f = fopen("/data/retro-attachments-probe.log", "a");
  if (f) { fprintf(f, "%s\n", n.message); fclose(f); }
}
int main(void) {
  report("1: entered main");
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)) { report(SDL_GetError()); return 1; }
  report("2: video initialized");
  SDL_Window *w = SDL_CreateWindow("Retro Attachments startup probe", 0, 0, 1920, 1080, SDL_WINDOW_SHOWN);
  if (!w) { report(SDL_GetError()); SDL_Quit(); return 2; }
  SDL_Renderer *r = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE);
  if (!r) { report(SDL_GetError()); SDL_DestroyWindow(w); SDL_Quit(); return 3; }
  report("3: renderer ready");
  SDL_SetRenderDrawColor(r, 25, 75, 145, 255);
  SDL_RenderClear(r); SDL_RenderPresent(r);
  report("4: frame submitted; blue screen for 10 seconds");
  SDL_Delay(10000);
  SDL_DestroyRenderer(r); SDL_DestroyWindow(w); SDL_Quit();
  report("5: probe finished");
  return 0;
}
