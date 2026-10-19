#include "model.hpp"
#include <cassert>
#include <iostream>
int main() {
  auto g = gameFrom(
      Json::parse(
          R"({"GameID":12,"Title":"Test","MaxPossible":40,"NumAwarded":12,"NumAwardedHardcore":4,"ImageIcon":null})"),
      true);
  assert(g.id == 12 && g.total == 40 && g.earned == 12 && g.hard == 4 &&
         g.icon.empty());
  auto list = achievementsFrom(Json::parse(
      R"({"Achievements":{"2":{"ID":2,"Points":"10","DisplayOrder":2,"DateEarned":"2026-10-01","DateEarnedHardcore":null},"1":{"ID":1,"DisplayOrder":1,"DateEarnedHardcore":"2026-10-02"},"3":{"ID":3,"DisplayOrder":3}}})"));
  assert(list.size() == 3 && list[0].id == 1 && list[0].earned(true) &&
         list[0].earned(false));
  assert(list[1].earned(false) && !list[1].earned(true) &&
         list[1].points == 10);
  assert(!list[2].earned(false));
  assert(achievementsFrom(Json::object()).empty());
  assert(num(Json{{"x", "bad"}}, "x", 7) == 7);
  std::cout << "PASS: game progress, null/string values, achievement ordering, "
               "Hardcore and Softcore unlock semantics\n";
}
