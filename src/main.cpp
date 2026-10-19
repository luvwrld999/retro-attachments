#include "display.hpp"
#include "api.hpp"
#include "render.hpp"
#include <deque>
#include <functional>
#include <memory>
#include <set>
#include <sstream>
#include <unistd.h>

struct Result {
  int kind = 0;
  std::vector<Game> games;
  Json json;
  std::string error, path, bytes;
};
struct Job {
  int kind;
  std::string arg;
  Api api;
};
struct Worker {
  SDL_mutex *mutex = SDL_CreateMutex();
  SDL_cond *cond = SDL_CreateCond();
  SDL_Thread *thread = nullptr;
  std::deque<Job> jobs;
  std::deque<Result> results;
  std::atomic<bool> stop{false};
  Worker() {
    thread = SDL_CreateThread(run, "Retro Attachments network", this);
    if (!thread)
      throw std::runtime_error("Cannot start network worker");
  }
  ~Worker() {
    stop = true;
    SDL_LockMutex(mutex);
    SDL_CondSignal(cond);
    SDL_UnlockMutex(mutex);
    SDL_WaitThread(thread, nullptr);
    SDL_DestroyCond(cond);
    SDL_DestroyMutex(mutex);
  }
  void add(Job j) {
    SDL_LockMutex(mutex);
    if (j.kind != 4) {
      jobs.erase(std::remove_if(jobs.begin(), jobs.end(),
                                [](auto &q) { return q.kind == 4; }),
                 jobs.end());
      jobs.push_front(std::move(j));
    } else
      jobs.push_back(std::move(j));
    SDL_CondSignal(cond);
    SDL_UnlockMutex(mutex);
  }
  bool poll(Result &r) {
    SDL_LockMutex(mutex);
    bool ok = !results.empty();
    if (ok) {
      r = std::move(results.front());
      results.pop_front();
    }
    SDL_UnlockMutex(mutex);
    return ok;
  }
  static int run(void *ptr) {
    auto &w = *static_cast<Worker *>(ptr);
    while (!w.stop) {
      SDL_LockMutex(w.mutex);
      while (w.jobs.empty() && !w.stop)
        SDL_CondWait(w.cond, w.mutex);
      if (w.stop) {
        SDL_UnlockMutex(w.mutex);
        break;
      }
      Job j = std::move(w.jobs.front());
      w.jobs.pop_front();
      SDL_UnlockMutex(w.mutex);
      Result r;
      r.kind = j.kind;
      r.path = j.arg;
      j.api.stop = &w.stop;
      try {
        if (j.kind == 1)
          r.games = j.api.library();
        if (j.kind == 2)
          r.json =
              j.api.get("GetConsoleIDs", "&a=1&g=1", "systems.json", 86400);
        if (j.kind == 3) {
          auto rows = j.api.get("GetGameList", "&i=" + j.arg + "&f=1&h=0",
                                "console-" + j.arg + ".json", 86400);
          if (!rows.is_array())
            throw std::runtime_error("Unexpected catalogue response");
          for (auto &a : rows)
            r.games.push_back(gameFrom(a, false));
        }
        if (j.kind == 5)
          r.json = j.api.get("GetGameInfoAndUserProgress",
                             "&g=" + j.arg + "&u=" + Api::encode(j.api.user));
        if (j.kind == 4)
          r.bytes = j.api.fetch("https://media.retroachievements.org" + j.arg);
      } catch (const std::exception &e) {
        r.error = e.what();
      }
      SDL_LockMutex(w.mutex);
      w.results.push_back(std::move(r));
      SDL_UnlockMutex(w.mutex);
      if (j.kind != 4)
        SDL_Delay(1100);
    }
    return 0;
  }
};

struct App {
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  std::unique_ptr<Painter> p;
  std::unique_ptr<Worker> worker;
  std::vector<SDL_GameController *> pads;
  std::map<std::string, SDL_Texture *> images;
  std::set<std::string> requested;
  std::vector<Game> games;
  std::vector<Achievement> achievements;
  Json systems = Json::array();
  Game current;
  std::string dir, user, key, search,
      status = "Enter your RetroAchievements account in Settings.", systemName;
  int theme = 0, tab = 3, selected = 0, systemSelected = 0, filter = 0;
  bool hardcore = true, busy = false, running = true, demo = false,
       detail = false, catalogue = false, sidebar = false;
  int edit = 0, keySelected = 0;
  std::string buffer;
  Uint32 lastAxis = 0;
  std::string screenshot;
  int screenshotScreen = 0;
  int frames = 0;
  int previewTheme = -1;
  Theme t() const { return themes[theme]; }
  Api api() {
    Api a;
    a.key = key;
    a.user = user;
    a.dir = dir;
    a.ca = dir + "/cacert.pem";
    return a;
  }
  App(int argc, char **argv) {
#ifdef __PROSPERO__
    dir = "/data/retroatlas";
#else
    dir = "userdata";
#endif
    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--demo")
        demo = true;
      else if (arg == "--screenshot" && i + 1 < argc)
        screenshot = argv[++i];
      else if (arg == "--detail")
        screenshotScreen = 1;
      else if (arg == "--theme" && i + 1 < argc)
        previewTheme = std::clamp(std::atoi(argv[++i]), 0, 3);
      else if (arg == "--themes")
        screenshotScreen = 2;
      else if (arg == "--data-dir" && i + 1 < argc)
        dir = argv[++i];
    }
#ifdef __PROSPERO__
    SDL_setenv("SDL_VIDEODRIVER", "ps5", 1);
#endif
    startupTrace("2: initializing video and controller");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER))
      throw std::runtime_error(SDL_GetError());
    startupTrace("3: video and controller initialized");
    mkdir(dir.c_str(), 0700);
    {
      std::ofstream ca(dir + "/cacert.pem", std::ios::binary);
      ca.write((const char *)ca_data, sizeof(ca_data) - 1);
      if (!ca)
        status =
            "Storage unavailable: cannot save settings or initialize HTTPS.";
    }
    try {
      std::ifstream f(dir + "/settings.json");
      if (f) {
        Json j;
        f >> j;
        user = str(j, "username");
        key = str(j, "api_key");
        theme = std::clamp(num(j, "theme"), 0, 3);
        hardcore = j.value("hardcore", true);
      }
    } catch (...) {
      status = "Settings unreadable. Please re-enter your account.";
    }
    if (previewTheme >= 0)
      theme = previewTheme;
    window = SDL_CreateWindow("Retro Attachments", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, 1280, 720,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window)
      throw std::runtime_error(SDL_GetError());
    renderer = createDisplayRenderer(window);
    SDL_RenderSetLogicalSize(renderer, 1280, 720);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    startupTrace("7: building font textures");
    p = std::make_unique<Painter>(renderer);
    startupTrace("8: font ready; starting worker");
    worker = std::make_unique<Worker>();
    startupTrace("9: worker ready");
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
      connect(i);
    if (demo) {
      loadDemo();
      if (screenshotScreen == 1) {
        detail = true;
        demoAchievements();
      }
      if (screenshotScreen == 2) {
        tab = 2;
        selected = 0;
      }
    } else if (!key.empty() && !user.empty()) {
      tab = 0;
      load(1);
    }
  }
  ~App() {
    worker.reset();
    for (auto *c : pads)
      SDL_GameControllerClose(c);
    for (auto &[_, tex] : images)
      SDL_DestroyTexture(tex);
    p.reset();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
  }
  void connect(int i) {
    if (SDL_IsGameController(i)) {
      auto *pad = SDL_GameControllerOpen(i);
      if (pad)
        pads.push_back(pad);
    }
  }
  void save() {
    Json j = {{"username", user},
              {"api_key", key},
              {"theme", theme},
              {"hardcore", hardcore}};
    std::string path = dir + "/settings.json";
    std::ofstream f(path + ".tmp");
    chmod((path + ".tmp").c_str(), 0600);
    f << j.dump(2);
    f.close();
    if (!f || std::rename((path + ".tmp").c_str(), path.c_str()) != 0)
      status = "Unable to save settings. Check storage permissions.";
    else
      status = "Settings saved on this device.";
  }
  void load(int kind, std::string arg = "") {
    if (busy)
      return;
    if (key.empty() || user.empty()) {
      status = "Set a username and web API key in Settings first.";
      return;
    }
    busy = true;
    status = "Connecting to RetroAchievements...";
    requested.clear();
    worker->add({kind, arg, api()});
  }
  void loadDemo() {
    tab = 0;
    status = "DEMO DATA - Sample interface, not your account.";
    games.clear();
    for (int i = 0; i < 12; ++i) {
      Game g;
      g.id = i + 1;
      const char *titles[] = {
          "Chrono Trigger",      "Super Mario World",
          "Sonic the Hedgehog",  "Castlevania: Symphony of the Night",
          "The Legend of Zelda", "Final Fantasy VI",
          "Metroid Fusion",      "Mega Man X",
          "Donkey Kong Country", "Pokemon Emerald",
          "Super Metroid",       "EarthBound"};
      g.title = titles[i];
      g.console = i % 3 == 0 ? "Super Nintendo" : "Retro collection";
      g.total = 40 + i * 3;
      g.earned = 12 + i;
      g.hard = 8 + i;
      g.progress = true;
      games.push_back(g);
    }
    current = games[0];
  }
  void demoAchievements() {
    achievements.clear();
    const char *names[] = {"A new beginning",   "Beyond the horizon",
                           "No stone unturned", "The perfect encounter",
                           "A hero's journey",  "Against all odds",
                           "One more adventure"};
    for (int i = 0; i < 21; ++i) {
      Achievement a;
      a.id = i;
      a.title = names[i % 7];
      a.description = "Explore every corner and complete the challenge. Sample "
                      "achievement for interface preview.";
      a.points = 5 * (1 + i % 5);
      if (i % 3 == 0)
        a.date = a.hardDate = "2026-10-18 12:30:00";
      else if (i % 3 == 1)
        a.date = "2026-10-17 18:15:00";
      achievements.push_back(a);
    }
    current.total = 21;
    current.earned = 14;
    current.hard = 7;
    current.progress = true;
  }
  std::vector<int> visible() {
    std::vector<int> v;
    for (size_t i = 0; i < games.size(); ++i)
      if (search.empty() ||
          lower(games[i].title).find(lower(search)) != std::string::npos)
        v.push_back(i);
    return v;
  }
  std::vector<int> visibleAchievements() {
    std::vector<int> v;
    for (size_t i = 0; i < achievements.size(); ++i) {
      bool e = achievements[i].earned(hardcore);
      if (filter == 0 || (filter == 1 && e) || (filter == 2 && !e))
        v.push_back(i);
    }
    return v;
  }
  SDL_Texture *artwork(std::string path) {
    if (path.empty() || demo)
      return nullptr;
    if (path.rfind("/Images/", 0) != 0 && path.rfind("/Badge/", 0) != 0)
      return nullptr;
    if (path.find("..") != std::string::npos ||
        path.find('?') != std::string::npos)
      return nullptr;
    auto it = images.find(path);
    if (it != images.end())
      return it->second;
    if (!busy && requested.size() < 48 && requested.insert(path).second)
      worker->add({4, path, api()});
    return nullptr;
  }
  void poll() {
    Result r;
    while (worker->poll(r)) {
      if (r.kind == 4) {
        requested.erase(r.path);
        if (r.error.empty()) {
          if (images.size() >= 96) {
            auto i = images.begin();
            SDL_DestroyTexture(i->second);
            images.erase(i);
          }
          images[r.path] = p->texture(r.bytes);
        } else
          images[r.path] = nullptr;
        continue;
      }
      busy = false;
      if (!r.error.empty()) {
        status = r.error;
        continue;
      }
      status = "Connected as " + user + " - " +
               (hardcore ? "Hardcore" : "Softcore / any unlock");
      selected = 0;
      if (r.kind == 1) {
        games = std::move(r.games);
        catalogue = false;
        search.clear();
      }
      if (r.kind == 2) {
        if (r.json.is_array())
          systems = std::move(r.json);
        else
          status = "Unexpected systems response.";
        catalogue = false;
      }
      if (r.kind == 3) {
        games = std::move(r.games);
        catalogue = true;
        search.clear();
      }
      if (r.kind == 5) {
        achievements = achievementsFrom(r.json);
        current.title = str(r.json, "Title", current.title);
        current.console = str(r.json, "ConsoleName", current.console);
        current.icon = str(r.json, "ImageIcon", current.icon);
        current.total = num(r.json, "NumAchievements");
        current.earned = num(r.json, "NumAwardedToUser");
        current.hard = num(r.json, "NumAwardedToUserHardcore");
        current.progress = true;
        detail = true;
        filter = 0;
      }
    }
  }
  void switchTab(int next) {
    if (busy)
      return;
    tab = (next + 4) % 4;
    selected = 0;
    detail = false;
    search.clear();
    sidebar = false;
    catalogue = false;
    if (tab == 0) {
      if (demo)
        loadDemo();
      else {
        games.clear();
        load(1);
      }
    }
    if (tab == 1) {
      games.clear();
      if (demo) {
        systems = Json::array({{{"ID", 3}, {"Name", "Super Nintendo"}},
                               {{"ID", 1}, {"Name", "Mega Drive / Genesis"}},
                               {{"ID", 12}, {"Name", "PlayStation"}}});
      } else if (systems.empty())
        load(2);
    }
  }
  int count() {
    if (detail)
      return visibleAchievements().size();
    if (tab == 0 || (tab == 1 && catalogue))
      return visible().size();
    if (tab == 1)
      return systems.size();
    if (tab == 2)
      return 4;
    return 6;
  }
  void startEdit(int field) {
    edit = field;
    keySelected = 0;
    buffer = field == 1 ? user : field == 2 ? key : search;
    SDL_StartTextInput();
  }
  void finishEdit() {
    if (edit == 1)
      user = buffer;
    if (edit == 2)
      key = buffer;
    if (edit == 3) {
      search = buffer;
      selected = 0;
    } else
      save();
    edit = 0;
    SDL_StopTextInput();
  }
  std::vector<std::string> keys() {
    std::vector<std::string> out;
    std::string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_- .";
    for (char c : chars)
      out.emplace_back(1, c);
    out.push_back("DEL");
    out.push_back("SAVE");
    return out;
  }
  void action(std::string a) {
    if (edit) {
      auto ks = keys();
      if (a == "back") {
        edit = 0;
        SDL_StopTextInput();
      } else if (a == "left")
        keySelected = std::max(0, keySelected - 1);
      else if (a == "right")
        keySelected = std::min((int)ks.size() - 1, keySelected + 1);
      else if (a == "up")
        keySelected = std::max(0, keySelected - 10);
      else if (a == "down")
        keySelected = std::min((int)ks.size() - 1, keySelected + 10);
      else if (a == "confirm") {
        if (ks[keySelected] == "SAVE")
          finishEdit();
        else if (ks[keySelected] == "DEL") {
          if (!buffer.empty())
            buffer.pop_back();
        } else if (buffer.size() < 128)
          buffer += ks[keySelected];
      }
      return;
    }
    if (a == "quit") {
      running = false;
      return;
    }
    if (a == "next") {
      switchTab(tab + 1);
      return;
    }
    if (a == "prev") {
      switchTab(tab - 1);
      return;
    }
    if (a == "mode") {
      hardcore = !hardcore;
      selected = 0;
      save();
      return;
    }
    if (a == "back") {
      if (detail) {
        detail = false;
        selected = 0;
      } else if (tab == 1 && catalogue) {
        catalogue = false;
        selected = systemSelected;
      } else
        sidebar = !sidebar;
      return;
    }
    if (a == "search" && !busy && !detail &&
        (tab == 0 || (tab == 1 && catalogue))) {
      startEdit(3);
      return;
    }
    if (a == "refresh" && !busy && !demo) {
      if (detail)
        load(5, std::to_string(current.id));
      else if (tab == 0)
        load(1);
      return;
    }
    if (busy)
      return;
    if (sidebar) {
      if (a == "up")
        tab = (tab + 3) % 4;
      if (a == "down")
        tab = (tab + 1) % 4;
      if (a == "confirm" || a == "right")
        switchTab(tab);
      return;
    }
    int n = count();
    int stride = (!detail && (tab == 0 || (tab == 1 && catalogue)))
                     ? 2
                     : (!detail && tab == 2 ? 2 : 1);
    if (a == "down")
      selected = std::min(std::max(0, n - 1), selected + stride);
    if (a == "up")
      selected = std::max(0, selected - stride);
    if (a == "right")
      selected = std::min(std::max(0, n - 1), selected + 1);
    if (a == "left")
      selected = std::max(0, selected - 1);
    if (a == "filter" && detail) {
      filter = (filter + 1) % 3;
      selected = 0;
    }
    if (a != "confirm")
      return;
    if (detail)
      return;
    if (tab == 0 || (tab == 1 && catalogue)) {
      auto v = visible();
      if (v.empty())
        return;
      current = games[v[selected]];
      if (demo) {
        demoAchievements();
        detail = true;
        selected = 0;
      } else
        load(5, std::to_string(current.id));
    } else if (tab == 1 && !systems.empty()) {
      systemSelected = selected;
      systemName = str(systems[selected], "Name");
      if (demo) {
        loadDemo();
        tab = 1;
        catalogue = true;
      } else
        load(3, std::to_string(num(systems[selected], "ID")));
    } else if (tab == 2) {
      theme = selected;
      save();
    } else if (tab == 3) {
      if (selected == 0)
        startEdit(1);
      if (selected == 1)
        startEdit(2);
      if (selected == 2) {
        demo = false;
        switchTab(0);
      }
      if (selected == 3) {
        hardcore = !hardcore;
        save();
      }
      if (selected == 4) {
        demo = true;
        loadDemo();
      }
      if (selected == 5)
        running = false;
    }
  }
  void events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
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
      if (e.type == SDL_TEXTINPUT && edit &&
          buffer.size() + strlen(e.text.text) <= 128)
        buffer += e.text.text;
      if (e.type == SDL_KEYDOWN) {
        auto k = e.key.keysym.sym;
        if (edit) {
          if (k == SDLK_BACKSPACE && !buffer.empty())
            buffer.pop_back();
          if (k == SDLK_RETURN) {
            finishEdit();
            continue;
          }
        }
        if (k == SDLK_ESCAPE)
          action("back");
        if (k == SDLK_RETURN && !edit)
          action("confirm");
        if (k == SDLK_UP)
          action("up");
        if (k == SDLK_DOWN)
          action("down");
        if (k == SDLK_LEFT)
          action("left");
        if (k == SDLK_RIGHT)
          action("right");
        if (!edit) {
          if (k == SDLK_TAB)
            action((e.key.keysym.mod & KMOD_SHIFT) ? "prev" : "next");
          if (k == SDLK_f)
            action("filter");
          if (k == SDLK_h)
            action("mode");
          if (k == SDLK_s)
            action("search");
          if (k == SDLK_r)
            action("refresh");
          if (k == SDLK_F10)
            action("quit");
        }
      }
      if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (e.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_A:
          action("confirm");
          break;
        case SDL_CONTROLLER_BUTTON_B:
          action("back");
          break;
        case SDL_CONTROLLER_BUTTON_X:
          action("filter");
          break;
        case SDL_CONTROLLER_BUTTON_Y:
          action("search");
          break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
          action("prev");
          break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
          action("next");
          break;
        case SDL_CONTROLLER_BUTTON_BACK:
          action("mode");
          break;
        case SDL_CONTROLLER_BUTTON_START:
          action("refresh");
          break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
          action("up");
          break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
          action("down");
          break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
          action("left");
          break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
          action("right");
          break;
        }
      }
      if (e.type == SDL_CONTROLLERAXISMOTION &&
          SDL_GetTicks() - lastAxis > 180) {
        if (std::abs(e.caxis.value) > 18000) {
          if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
            action(e.caxis.value < 0 ? "left" : "right");
          if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
            action(e.caxis.value < 0 ? "up" : "down");
          lastAxis = SDL_GetTicks();
        }
      }
    }
  }
  void card(int x, int y, int w, int h, bool active) {
    auto th = t();
    p->box(x, y + 5, w, h, {0, 0, 0, 65}, 14);
    if (active && !sidebar) {
      p->box(x - 4, y - 4, w + 8, h + 8, Painter::mix(th.bg, th.accent, .15f),
             17);
      p->box(x - 2, y - 2, w + 4, h + 4, th.accent, 15);
    }
    p->box(x, y, w, h,
           active ? Painter::mix(th.card, th.accent, .07f) : th.card, 13);
  }
  void hint(const std::string &button, const std::string &text, int x) {
    auto th = t();
    int w = button.size() > 2 ? 44 : 28;
    p->box(x, 670, w, 28, th.card, 7);
    p->text(button, x + 7, 674, 16, th.accent, w - 8);
    p->text(text, x + w + 9, 674, 17, th.text);
  }
  void base() {
    auto th = t();
    p->box(0, 0, 1280, 720, th.bg);
    if (theme != 1)
      p->gradient(216, 0, 1064, 150, Painter::mix(th.bg, th.accent, .085f),
                  th.bg);
    p->box(0, 0, 216, 720, th.panel);
    p->box(215, 0, 1, 720, Painter::mix(th.panel, th.accent, .15f));
    p->box(28, 35, 46, 46, Painter::mix(th.panel, th.accent, .15f), 13);
    p->text("RA", 36, 43, 26, th.accent);
    p->text("RETRO", 86, 35, 22, th.text);
    p->text("ATTACHMENTS", 86, 61, 14, th.muted);
    p->text("YOUR NEXT UNLOCK", 28, 116, 13, th.muted);
    const char *tabs[] = {"Collection", "Explore", "Appearance", "Settings"};
    for (int i = 0; i < 4; ++i) {
      int y = 177 + i * 64;
      if (tab == i) {
        p->box(17, y - 7, 182, 50, Painter::mix(th.panel, th.accent, .12f), 10);
        p->box(17, y + 6, 3, 24, th.accent, 1);
        if (sidebar)
          p->outline(17, y - 7, 182, 50, th.accent);
      }
      Color c = tab == i ? th.accent : th.muted;
      if (i == 0) {
        for (int k = 0; k < 4; ++k)
          p->box(32 + (k % 2) * 9, y + 11 + (k / 2) * 9, 6, 6, c, 1);
      }
      if (i == 1) {
        p->ring(40, y + 19, 9, 1, c, c, 2);
        p->box(38, y + 16, 5, 5, c, 1);
      }
      if (i == 2) {
        p->box(31, y + 11, 6, 16, c, 2);
        p->box(40, y + 11, 6, 16, Painter::mix(c, th.panel, .4f), 2);
        p->box(49, y + 11, 3, 16, c, 1);
      }
      if (i == 3) {
        for (int k = 0; k < 3; ++k) {
          p->box(31, y + 12 + k * 7, 20, 2, c);
          p->box(34 + (k % 2) * 9, y + 10 + k * 7, 4, 6, c, 1);
        }
      }
      p->text(tabs[i], 67, y + 6, 22, tab == i ? th.text : th.muted);
    }
    p->box(22, 551, 172, 94, th.card, 12);
    p->pill(demo ? "DEMO DATA" : "PLAYER PROFILE", 34, 564, th.accent, th.panel,
            12);
    p->label(user.empty() ? "Welcome, player" : user, 35, 602, 20, th.text,
             146);
    p->text("PS5 / 0.1.3 alpha", 28, 675, 14, th.muted);
    p->text(detail     ? "YOUR NEXT MILESTONE"
            : tab == 0 ? "RETROACHIEVEMENTS"
            : tab == 1 ? "THE GAME CATALOGUE"
            : tab == 2 ? "PERSONALIZE YOUR SPACE"
                       : "MAKE YOURSELF AT HOME",
            254, 29, 13, th.accent);
    p->label(detail     ? current.title
             : tab == 0 ? "Your retro collection."
             : tab == 1 ? (catalogue ? systemName : "Find your next adventure.")
             : tab == 2 ? "Choose your atmosphere."
                        : "Ready, player?",
             254, 56, 37, th.text, 974);
    p->text(detail     ? current.console
            : tab == 0 ? "Small victories. Legendary journeys."
            : tab == 1
                ? "Browse systems, discover games, track every achievement."
            : tab == 2 ? "Four looks. The same collection. Always yours."
                       : "Connect your account and pick up where you left off.",
            254, 108, 18, th.muted, 700);
    p->pill(hardcore ? "HARDCORE" : "ANY UNLOCK", 1081, 104, th.accent, th.card,
            13);
    p->box(254, 625, 974, 1, th.card);
    p->box(255, 642, 5, 5, busy ? th.accent : th.muted, 2);
    p->label(status, 270, 631, 16, th.muted, 958);
    hint("+", "Navigate", 254);
    if (!detail)
      hint("X", "Select", 411);
    if (detail) {
      hint("[]", "Filter", 557);
      hint("TP", "Mode", 699);
      hint("O", "Back", 835);
    } else {
      hint("L/R", "Tabs", 557);
      hint("O", "Back", 713);
      if (tab == 0 || (tab == 1 && catalogue))
        hint("/\\", "Search", 851);
    }
  }
  void drawGames() {
    auto th = t();
    auto v = visible();
    int total = 0, earned = 0, complete = 0;
    for (auto &g : games) {
      total += g.total;
      int n = hardcore ? g.hard : g.earned;
      earned += n;
      if (g.progress && g.total > 0 && n >= g.total)
        ++complete;
    }
    bool progressView = tab == 0;
    p->box(254, 151, 974, 120, th.panel, 15);
    p->box(254, 172, 3, 76, th.accent, 1);
    p->text(progressView ? "YOUR COLLECTION AT A GLANCE"
                         : "A WORLD OF ACHIEVEMENTS",
            278, 166, 13, th.accent);
    p->text(progressView ? std::to_string(earned) : std::to_string(total), 278,
            189, 42, th.text);
    p->text(progressView ? "achievements earned" : "achievements to discover",
            278, 239, 16, th.muted);
    p->box(597, 174, 1, 74, th.card);
    p->text(std::to_string(games.size()), 625, 181, 34, th.text);
    p->text("GAMES", 626, 232, 13, th.muted);
    p->box(765, 174, 1, 74, th.card);
    p->text(progressView ? std::to_string(complete) : "24h", 792, 181, 34,
            th.text);
    p->text(progressView ? "FULL SETS EARNED" : "CATALOGUE CACHE", 792, 232, 13,
            th.muted);
    float rate =
        progressView && total ? std::clamp(float(earned) / total, 0.f, 1.f) : 0;
    p->ring(1147, 210, 38, rate, th.accent, th.card, 5);
    auto percent = progressView ? std::to_string(int(rate * 100)) + "%" : "RA";
    p->text(percent, 1147 - p->width(percent, 22) / 2, 196, 22, th.text);
    p->label(search.empty() ? "YOUR GAMES" : "RESULTS FOR  " + search, 254, 289,
             15, th.muted, 740);
    if (v.empty()) {
      p->box(254, 333, 974, 256, th.panel, 14);
      p->cartridge(290, 390, 100, 112, th.accent, th.panel, 0);
      p->text(busy ? "Gathering your collection..."
                   : "Your next adventure starts here.",
              423, 390, 29, th.text, 755);
      p->wrap(search.empty() ? "Connect your account in Settings, or explore a "
                               "console to find a game."
                             : "No games match this search. Press Triangle to "
                               "try another title.",
              423, 440, 21, th.muted, 720, 3);
      return;
    }
    selected = std::clamp(selected, 0, (int)v.size() - 1);
    int start = (selected / 4) * 4;
    p->text(std::to_string(start + 1) + " - " +
                std::to_string(std::min(start + 4, (int)v.size())) + "  /  " +
                std::to_string(v.size()),
            1090, 289, 15, th.muted, 138);
    const Color accents[] = {
        {116, 204, 255}, {196, 168, 255}, {247, 195, 120}, {116, 224, 193}};
    for (int j = 0; j < 4 && start + j < (int)v.size(); ++j) {
      auto &g = games[v[start + j]];
      int x = 254 + j % 2 * 498, y = 327 + j / 2 * 144;
      bool active = selected == start + j;
      auto accent = Painter::mix(th.accent, accents[std::abs(g.id) % 4],
                                 theme == 1 ? 0.f : .38f);
      card(x, y, 474, 126, active);
      auto *img = artwork(g.icon);
      if (img) {
        p->box(x + 14, y + 14, 96, 98, th.panel, 9);
        p->image(img, x + 19, y + 20, 86, 86);
      } else
        p->cartridge(x + 14, y + 14, 96, 98, accent, th.panel, std::abs(g.id));
      p->label(g.console, x + 126, y + 12, 13, accent, 330);
      p->wrap(g.title, x + 126, y + 31, 22, th.text, 330, 2);
      int n = hardcore ? g.hard : g.earned;
      float fraction =
          g.progress && g.total ? std::clamp(float(n) / g.total, 0.f, 1.f) : 0;
      std::string value =
          g.progress
              ? std::to_string(n) + " / " + std::to_string(g.total) + " earned"
              : std::to_string(g.total) + " achievements";
      p->text(value, x + 126, y + 91, 15, th.muted, 245);
      p->text(g.progress ? std::to_string(int(fraction * 100)) + "%" : "OPEN",
              x + 411, y + 91, 15, active ? th.accent : th.muted, 49);
      p->bar(x + 126, y + 115, 332, fraction, accent, th.panel);
    }
    p->text("Select a game to explore its achievement set", 254, 604, 13,
            th.muted);
  }
  void drawSystems() {
    auto th = t();
    int n = systems.size();
    if (!n) {
      p->box(254, 170, 974, 405, th.panel, 14);
      p->text(busy ? "Loading systems..." : "A whole world of retro awaits.",
              286, 285, 31, th.text);
      p->text("Connect an account in Settings to browse the catalogue.", 286,
              340, 21, th.muted);
      return;
    }
    selected = std::clamp(selected, 0, n - 1);
    int start = (selected / 6) * 6;
    for (int i = start; i < std::min(n, start + 6); ++i) {
      int y = 158 + (i - start) * 74;
      card(254, y, 974, 62, selected == i);
      p->box(267, y + 10, 42, 42, th.panel, 9);
      p->text(std::to_string(i + 1), 280, y + 18, 18, th.accent);
      p->label(str(systems[i], "Name"), 330, y + 15, 25, th.text, 660);
      p->pill("EXPLORE  >", 1090, y + 16, th.accent, th.panel, 13);
    }
    p->text(std::to_string(n) + " systems to explore", 254, 606, 13, th.muted);
  }
  void drawDetail() {
    auto th = t();
    auto v = visibleAchievements();
    int earned = hardcore ? current.hard : current.earned;
    float fraction =
        current.total ? std::clamp(float(earned) / current.total, 0.f, 1.f) : 0;
    p->box(254, 151, 974, 82, th.panel, 13);
    p->ring(300, 192, 27, fraction, th.accent, th.card, 4);
    p->text(std::to_string(int(fraction * 100)) + "%", 280, 180, 17, th.text,
            43);
    p->text(std::to_string(earned) + " / " + std::to_string(current.total), 348,
            160, 30, th.text);
    p->text("ACHIEVEMENTS EARNED", 349, 202, 13, th.muted);
    p->bar(676, 190, 520, fraction, th.accent, th.card);
    const char *filters[] = {"All achievements", "Earned", "Still to unlock"};
    for (int i = 0; i < 3; ++i) {
      int x = 254 + i * 190;
      p->pill(filters[i], x, 248, filter == i ? th.bg : th.muted,
              filter == i ? th.accent : th.panel, 16);
    }
    if (v.empty()) {
      p->text("No achievements match this filter.", 278, 357, 27, th.muted);
      return;
    }
    selected = std::clamp(selected, 0, (int)v.size() - 1);
    int start = (selected / 4) * 4;
    p->text(std::to_string(selected + 1) + " / " + std::to_string(v.size()),
            1143, 252, 16, th.muted);
    for (int i = start; i < std::min((int)v.size(), start + 4); ++i) {
      auto &a = achievements[v[i]];
      int y = 297 + (i - start) * 78;
      bool e = a.earned(hardcore);
      card(254, y, 584, 68, selected == i);
      p->box(267, y + 10, 48, 48, th.panel, 9);
      auto *img = artwork(a.badge.empty() ? ""
                                          : "/Badge/" + a.badge +
                                                (e ? "" : "_lock") + ".png");
      if (img)
        p->image(img, 270, y + 13, 42, 42);
      else {
        p->ring(291, y + 34, 14, e ? 1 : 0, th.accent, th.muted, 2);
        p->text(e ? "+" : "-", 284, y + 22, 20, e ? th.accent : th.muted);
      }
      p->label(a.title, 330, y + 9, 22, th.text, 400);
      p->text(e ? "UNLOCKED" : "LOCKED", 330, y + 40, 12,
              e ? th.accent : th.muted);
      p->text(std::to_string(a.points) + " pts", 756, y + 25, 17, th.accent,
              70);
    }
    auto &a = achievements[v[selected]];
    p->box(860, 297, 368, 302, th.panel, 14);
    p->text("THE CHALLENGE", 882, 313, 12, th.accent);
    p->wrap(a.title, 882, 341, 25, th.text, 322, 2);
    p->wrap(a.description, 882, 416, 19, th.muted, 322, 4);
    p->box(882, 529, 324, 1, th.card);
    std::string date =
        hardcore ? a.hardDate : (!a.date.empty() ? a.date : a.hardDate);
    p->text(date.empty() ? "Waiting for your next unlock"
                         : "Earned  " + date.substr(0, 10),
            882, 541, 17, th.accent, 322);
    p->text(std::to_string(a.points) + " points", 882, 570, 16, th.muted);
  }
  void drawThemes() {
    const char *descriptions[] = {
        "Cool tones for late-night adventures.",
        "Pure black. Crisp contrast. No distractions.",
        "A little nostalgia in every shade of green.",
        "Violet skies and a brighter next unlock."};
    for (int i = 0; i < 4; ++i) {
      int x = 254 + i % 2 * 498, y = 157 + i / 2 * 230;
      auto th = themes[i];
      card(x, y, 474, 209, selected == i);
      p->box(x + 12, y + 12, 450, 119, th.bg, 10);
      p->box(x + 12, y + 12, 69, 119, th.panel, 10);
      p->box(x + 27, y + 28, 25, 25, th.accent, 7);
      p->text("RA", x + 31, y + 32, 13, th.bg);
      for (int n = 0; n < 3; ++n)
        p->box(x + 27, y + 68 + n * 15, 38, n == 0 ? 5 : 3,
               n == 0 ? th.accent : th.card, 1);
      p->box(x + 96, y + 28, 200, 6, th.text, 2);
      p->box(x + 96, y + 44, 128, 4, th.muted, 1);
      p->ring(x + 426, y + 44, 16, .7f, th.accent, th.card, 3);
      for (int j = 0; j < 3; ++j) {
        p->box(x + 96 + j * 118, y + 69, 106, 46, th.card, 6);
        p->box(x + 105 + j * 118, y + 78, 22, 22,
               Painter::mix(th.card, th.accent, .5f), 4);
        p->bar(x + 135 + j * 118, y + 94, 54, .65f, th.accent, th.panel);
      }
      if (i == 2)
        for (int line = 0; line < 100; line += 5)
          p->box(x + 83, y + 18 + line, 372, 1, {0, 0, 0, 42});
      p->text(th.name, x + 19, y + 144, 26, t().text);
      p->pill(theme == i ? "ACTIVE" : "APPLY", x + 369, y + 146,
              theme == i ? t().bg : t().accent,
              theme == i ? t().accent : t().panel, 12);
      p->text(descriptions[i], x + 19, y + 184, 15, t().muted, 435);
    }
  }
  void drawSettings() {
    const std::string labels[] = {"RetroAchievements username",
                                  "Web API key",
                                  "Connect my collection",
                                  hardcore ? "Achievement mode: Hardcore"
                                           : "Achievement mode: Any unlock",
                                  "Explore the demo",
                                  "Exit Retro Attachments"};
    for (int i = 0; i < 6; ++i) {
      int y = 159 + i * 68;
      card(254, y, 974, 56, selected == i);
      p->text("0" + std::to_string(i + 1), 274, y + 17, 15, t().accent);
      p->label(labels[i], 319, y + 13, 23, t().text, 490);
      if (i < 2)
        p->label(i == 0 ? (user.empty() ? "Not connected" : user)
                        : (key.empty() ? "Add your web API key"
                                       : "Configured / hidden"),
                 853, y + 18, 17, t().muted, 347);
      else
        p->text(">", 1189, y + 16, 21, t().accent);
    }
    p->text("Your web API key is available at retroachievements.org/settings",
            254, 585, 17, t().muted, 974);
    p->text("Account details and preferences stay on this device.", 254, 608,
            13, t().muted);
  }
  void drawKeyboard() {
    auto th = t();
    p->box(0, 0, 1280, 720, {0, 0, 0, 210});
    p->box(200, 80, 880, 560, th.panel, 16);
    p->text(edit == 1   ? "Enter username"
            : edit == 2 ? "Enter web API key"
                        : "Search games",
            232, 100, 30, th.text);
    p->box(232, 151, 816, 52, th.bg, 6);
    std::string shown = edit == 2 ? std::string(buffer.size(), '*') : buffer;
    if (shown.size() > 55)
      shown = shown.substr(shown.size() - 55);
    p->text(shown + "_", 248, 164, 24, th.accent, 780);
    auto ks = keys();
    for (size_t i = 0; i < ks.size(); ++i) {
      int x = 232 + (i % 10) * 81, y = 227 + (i / 10) * 49;
      p->box(x, y, 73, 41, keySelected == (int)i ? th.accent : th.card, 5);
      p->text(ks[i], x + 12, y + 6, 21,
              keySelected == (int)i ? th.bg : th.text);
    }
    p->text("D-pad + Cross to type   |   SAVE to apply   |   Circle to cancel",
            232, 594, 18, th.muted);
  }
  void render() {
    if (frames == 0) startupTrace("10: drawing first frame");
    base();
    if (detail)
      drawDetail();
    else if (tab == 0 || (tab == 1 && catalogue))
      drawGames();
    else if (tab == 1)
      drawSystems();
    else if (tab == 2)
      drawThemes();
    else
      drawSettings();
    if (busy) {
      int x = 254 + (SDL_GetTicks() / 10) % 800;
      p->box(x, 618, 140, 3, t().accent);
    }
    if (edit)
      drawKeyboard();
    if (!screenshot.empty() && frames == 3) {
      auto *surface = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32,
                                                     SDL_PIXELFORMAT_RGBA32);
      SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                           surface->pixels, surface->pitch);
      SDL_SaveBMP(surface, screenshot.c_str());
      SDL_FreeSurface(surface);
      running = false;
    }
    if (frames == 0) startupTrace("11: submitting first frame");
    presentDisplay(window, renderer);
    if (frames == 0) startupTrace("12: first frame submitted");
    ++frames;
  }
  void run() {
    while (running) {
      events();
      poll();
      render();
      SDL_Delay(16);
    }
  }
};
#ifndef RETROATLAS_TEST
int main(int argc, char **argv) {
  startupTrace("1: entered app main; initializing network library");
  curl_global_init(CURL_GLOBAL_DEFAULT);
  try {
    App app(argc, argv);
    app.run();
  } catch (const std::exception &e) {
    startupTrace(e.what());
    SDL_Quit();
    fprintf(stderr, "Retro Attachments: %s\n", e.what());
    curl_global_cleanup();
    return 1;
  }
  curl_global_cleanup();
  return 0;
}

#endif
