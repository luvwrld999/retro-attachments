#pragma once
#include "json.hpp"
#include <algorithm>
#include <string>
#include <vector>
using Json = nlohmann::json;
inline std::string str(const Json &j, const char *k, std::string d = "") {
  auto i = j.find(k);
  return i != j.end() && i->is_string() ? i->get<std::string>() : d;
}
inline int num(const Json &j, const char *k, int d = 0) {
  auto i = j.find(k);
  if (i == j.end() || i->is_null())
    return d;
  if (i->is_number_integer())
    return i->get<int>();
  if (i->is_string()) {
    try {
      return std::stoi(i->get<std::string>());
    } catch (...) {
    }
  }
  return d;
}
struct Game {
  int id = 0, total = 0, earned = 0, hard = 0;
  bool progress = false;
  std::string title, console, icon;
};
struct Achievement {
  int id = 0, points = 0, order = 0;
  std::string title, description, badge, date, hardDate;
  bool earned(bool hardcore) const {
    return hardcore ? !hardDate.empty() : !date.empty() || !hardDate.empty();
  }
};
inline Game gameFrom(const Json &j, bool progress) {
  Game g;
  g.id = num(j, "GameID", num(j, "ID"));
  g.title = str(j, "Title", "Untitled");
  g.console = str(j, "ConsoleName");
  g.icon = str(j, "ImageIcon");
  g.total = num(j, "MaxPossible", num(j, "NumAchievements"));
  g.earned = num(j, "NumAwarded");
  g.hard = num(j, "NumAwardedHardcore");
  g.progress = progress;
  return g;
}
inline std::vector<Achievement> achievementsFrom(const Json &j) {
  std::vector<Achievement> out;
  auto i = j.find("Achievements");
  if (i == j.end() || (!i->is_object() && !i->is_array()))
    return out;
  for (auto &a : *i) {
    Achievement x;
    x.id = num(a, "ID");
    x.points = num(a, "Points");
    x.order = num(a, "DisplayOrder");
    x.title = str(a, "Title");
    x.description = str(a, "Description");
    x.badge = str(a, "BadgeName");
    x.date = str(a, "DateEarned");
    x.hardDate = str(a, "DateEarnedHardcore");
    out.push_back(x);
  }
  std::stable_sort(out.begin(), out.end(),
                   [](auto &a, auto &b) { return a.order < b.order; });
  return out;
}
inline std::string lower(std::string s) {
  for (auto &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
