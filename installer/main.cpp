// SPDX-License-Identifier: GPL-3.0-or-later
#include "installer_bundle.hpp"
#include "render.hpp"
#include <atomic>
#include <cstdio>
#include <memory>

struct Installer {
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  std::unique_ptr<Painter> painter;
  std::vector<SDL_GameController *> pads;
  SDL_Thread *thread = nullptr;
  std::atomic<int> progress{0},
      state{0}; // 0 confirmation, 1 working, 2 installed, 3 failed
  std::string root, result, screenshot;
  bool running = true;
  Installer(int argc, char **argv) {
#ifdef __PROSPERO__
    root = "/data";
#endif
    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--screenshot" && i + 1 < argc)
        screenshot = argv[++i];
#ifndef __PROSPERO__
      if (arg == "--test-root" && i + 1 < argc)
        root = argv[++i];
#endif
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) !=
        0)
      throw std::runtime_error(SDL_GetError());
    window =
        SDL_CreateWindow("Retro Attachments Installer", SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN);
    if (!window)
      throw std::runtime_error(SDL_GetError());
    renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
      renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer)
      throw std::runtime_error(SDL_GetError());
    SDL_RenderSetLogicalSize(renderer, 1280, 720);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    painter = std::make_unique<Painter>(renderer);
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
      connect(i);
  }
  ~Installer() {
    if (thread)
      SDL_WaitThread(thread, nullptr);
    for (auto p : pads)
      SDL_GameControllerClose(p);
    painter.reset();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
  }
  void connect(int i) {
    if (SDL_IsGameController(i)) {
      auto p = SDL_GameControllerOpen(i);
      if (p)
        pads.push_back(p);
    }
  }
  static int work(void *ptr) {
    auto &app = *static_cast<Installer *>(ptr);
    try {
      auto out = install::run(app.root, installer_files,
                              {installer_blob, installer_blob_size},
                              [&](int n) { app.progress = n; });
      app.result = "Files installed. Exit this installer and let "
                   "ShadowMountPlus scan /data/homebrew.";
      if (!out.backup.empty())
        app.result += " Previous app preserved at " + out.backup + ".";
      app.state = 2;
    } catch (const std::exception &e) {
      app.result = e.what();
      app.state = 3;
    }
    return 0;
  }
  void start() {
    if (state != 0 || root.empty())
      return;
    state = 1;
    thread = SDL_CreateThread(work, "Retro Attachments install", this);
    if (!thread) {
      result = "Cannot start installation thread. Nothing was installed.";
      state = 3;
    }
  }
  void draw() {
    auto &p = *painter;
    auto t = themes[0];
    p.gradient(0, 0, 1280, 720, Painter::mix(t.bg, t.accent, .09f), t.bg);
    p.box(74, 72, 1132, 586, {0, 0, 0, 70}, 18);
    p.box(74, 65, 1132, 586, t.panel, 18);
    int s = state.load();
    p.pill("01  REVIEW", 112, 188, s == 0 ? t.accent : t.muted, t.card, 12);
    p.pill("02  VERIFY", 274, 188, s == 1 && progress < 85 ? t.accent : t.muted,
           t.card, 12);
    p.pill("03  INSTALL", 436, 188,
           s == 2 || (s == 1 && progress >= 85) ? t.accent : t.muted, t.card,
           12);
    p.box(112, 103, 70, 70, t.accent, 12);
    p.text("RA", 124, 113, 40, t.bg);
    p.text("RETRO ATTACHMENTS / INSTALLER", 211, 106, 18, t.accent);
    p.text("Your collection, on the home screen.", 211, 135, 31, t.text, 935);
    p.text(s == 0   ? "Install Retro Attachments"
           : s == 1 ? "Installing Retro Attachments..."
           : s == 2 ? "App files installed"
                    : "Installation stopped",
           112, 224, 36, t.text);
    if (s == 0) {
      p.wrap("Adds a Retro Attachments home-screen launcher using "
             "ShadowMountPlus. "
             "The launcher starts the included app through your local ELF "
             "loader on port 9021.",
             112, 284, 24, t.muted, 1040, 3);
      p.text("Destination: /data/homebrew/PPSA99841", 112, 392, 22, t.accent);
      p.text("Close Retro Attachments first. Existing app backed up; settings "
             "preserved.",
             112, 437, 21, t.text);
      p.text("Hardware-test build: PS5 firmware 13.60 has not been verified.",
             112, 482, 20, t.muted);
    }
    if (s == 1) {
      p.text("Verifying files and preparing the app folder.", 112, 293, 25,
             t.muted);
      p.bar(112, 386, 1040, progress / 100.f, t.accent, t.card);
      p.text(std::to_string(progress.load()) + "%", 112, 412, 25, t.accent);
      p.text("Keep the console on until this operation finishes.", 112, 470, 22,
             t.muted);
    }
    if (s >= 2)
      p.wrap(result, 112, 291, 24, s == 2 ? t.accent : t.muted, 1040, 7);
    p.box(112, 551, 1040, 1, t.card);
    p.text(s == 0   ? (root.empty()
                           ? "DESKTOP PREVIEW ONLY    |    Escape to close"
                           : "Cross / Enter  Install     Circle / Escape  Cancel")
           : s == 1 ? "Writing local app files..."
                    : "Circle / Escape  Close installer",
           112, 581, 23, t.text);
    if (!screenshot.empty()) {
      auto *surface = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32,
                                                     SDL_PIXELFORMAT_RGBA32);
      if (surface) {
        SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                             surface->pixels, surface->pitch);
        SDL_SaveBMP(surface, screenshot.c_str());
        SDL_FreeSurface(surface);
      }
      running = false;
    }
    SDL_RenderPresent(renderer);
  }
  void run() {
    while (running) {
      SDL_Event e;
      while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT && state != 1)
          running = false;
        if (e.type == SDL_CONTROLLERDEVICEADDED)
          connect(e.cdevice.which);
        if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
          for (auto it = pads.begin(); it != pads.end();)
            if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(*it)) ==
                e.cdevice.which) {
              SDL_GameControllerClose(*it);
              it = pads.erase(it);
            } else
              ++it;
        }
        if (e.type == SDL_KEYDOWN) {
          if (e.key.keysym.sym == SDLK_RETURN)
            start();
          if (e.key.keysym.sym == SDLK_ESCAPE && state != 1)
            running = false;
        }
        if (e.type == SDL_CONTROLLERBUTTONDOWN) {
          if (e.cbutton.button == SDL_CONTROLLER_BUTTON_A)
            start();
          if (e.cbutton.button == SDL_CONTROLLER_BUTTON_B && state != 1)
            running = false;
        }
      }
      draw();
      SDL_Delay(16);
    }
  }
};
int main(int argc, char **argv) {
  try {
    Installer app(argc, argv);
    app.run();
  } catch (const std::exception &e) {
    fprintf(stderr, "Retro Attachments installer: %s\n", e.what());
    return 1;
  }
  return 0;
}
