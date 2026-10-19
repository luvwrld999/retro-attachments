#define RETROATLAS_TEST
#include "../src/main.cpp"
#include <cassert>
int main() {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  char name[] = "test", demo[] = "--demo", flag[] = "--data-dir",
       dir[] = "build/test-userdata";
  char *args[] = {name, demo, flag, dir};
  {
    App app(4, args);
    assert(app.tab == 0 && app.games.size() == 12 && !app.busy);
    app.action("down");
    assert(app.selected == 2);
    app.action("up");
    assert(app.selected == 0);
    app.action("confirm");
    assert(app.detail && app.achievements.size() == 21);
    app.hardcore = true;
    app.filter = 1;
    assert(app.visibleAchievements().size() == 7);
    app.hardcore = false;
    assert(app.visibleAchievements().size() == 14);
    app.filter = 2;
    assert(app.visibleAchievements().size() == 7);
    app.action("back");
    assert(!app.detail);
    app.startEdit(3);
    app.buffer = "sonic";
    app.finishEdit();
    assert(app.visible().size() == 1);
    app.action("confirm");
    assert(app.current.title == "Sonic the Hedgehog");
    app.action("back");
    app.switchTab(2);
    app.selected = 3;
    app.action("confirm");
    assert(app.theme == 3);
    app.switchTab(3);
    app.startEdit(1);
    app.buffer = "tester";
    app.finishEdit();
    assert(app.user == "tester");
    app.startEdit(1);
    app.buffer = "discard";
    app.action("back");
    assert(app.user == "tester");
    app.switchTab(1);
    assert(app.systems.size() == 3);
    app.action("confirm");
    assert(app.catalogue && app.tab == 1);
    app.action("back");
    assert(!app.catalogue);
    app.games.clear();
    app.tab = 0;
    app.action("confirm");
    app.render();
  }
  {
    App app(4, args);
    assert(app.theme == 3 && app.user == "tester");
  }
  curl_global_cleanup();
  puts("PASS: navigation, achievement filters, search, themes, account "
       "editing, persistence, empty-state rendering");
}
