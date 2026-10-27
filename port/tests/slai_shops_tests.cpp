// S.L.A.I.'s shops' stock (phantomcrash/slai_shops.hpp) on a small table, and
// the parts shop keeping to its stock's records.

#include "test_support.hpp"

#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/module_shop.hpp"
#include "phantomcrash/slai_shops.hpp"

using namespace phantomcrash;

namespace {

// Two S.L.A.I. days of two cities; city-day rows 0..3, the junk shop's
// ten-day cycle shows day 1 in city 0 and day 2 in city 1.
SlaiShops table() {
  return parse_slai_shops(json::parse(R"({
    "days": [
      {"dealers": [-1, -1, -1, 5, 6], "city_days": [0, 1]},
      {"dealers": [-1, -1, -1, 7, 8], "city_days": [2, 3]}
    ],
    "city_days": [
      {"chips": 10, "modules": [0, 1, 2, 3, -1, 5, 6], "used_svs": 20, "junk": 0},
      {"chips": 11, "modules": [7, 8, 9, 10, -1, 12, 13], "used_svs": 21, "junk": -1},
      {"chips": 12, "modules": [14, 15, 16, 17, -1, 19, 20], "used_svs": 22, "junk": -1},
      {"chips": 13, "modules": [21, 22, 23, 24, -1, 26, 27], "used_svs": 23, "junk": 1}
    ],
    "junk": {
      "cycle": [1, 2],
      "rows": [
        {"modules": [30, 31, 32, 33, -1, 35, 36], "sv_row": 2},
        {"modules": [40, 41, 42, 43, -1, 45, 46], "sv_row": 0}
      ],
      "sv_lineups": [50, 51, 52],
      "locked_sv_rows": {"2": 1},
      "unlock_stage": 4
    },
    "dealers": {"dealer_omsk": {"shop": 3, "profile": ["OMSK", "line"]}},
    "slai_stock": {"body": [16, 26], "chip": [96, 481]}
  })"));
}

SlaiRota rota() {
  SlaiRota out;
  out.days = {{{0, 0}, {1, 0}}, {{1, 1}}};
  return out;
}

} // namespace

TEST_CASE("S.L.A.I.'s shop table parses") {
  const auto shops = table();
  REQUIRE(shops.city_days.size() == 4);
  CHECK(shops.city_days[1].chips == 11);
  CHECK(shops.city_days[1].modules[5] == 12);
  CHECK(shops.city_days[3].used_svs == 23);
  CHECK(shops.junk_of_city_day[3] == 1);
  CHECK(shops.dealers.at("dealer_omsk").shop == 3);
  CHECK(shops.dealers.at("dealer_omsk").profile.size() == 2);
  CHECK(shops.stock.at("body").holds(16));
  CHECK(!shops.stock.at("body").holds(26));
  CHECK(shop_stock_named("slai_junk") == ShopStock::slai_junk);
  CHECK(!shop_stock_named("pc"));
  CHECK_THROWS(parse_slai_shops(json::parse(
      R"({"days": [], "city_days": [{"modules": [1, 2]}], "junk": {"cycle": [], "rows": [],
          "sv_lineups": []}})")));
}

TEST_CASE("S.L.A.I.'s shops stand in the player's city, else the day's first") {
  const auto open = rota();
  const auto here = slai_shop_place(open, 0, 1);
  REQUIRE(here.has_value());
  CHECK(here->city == 1);
  const auto elsewhere = slai_shop_place(open, 1, 0);
  REQUIRE(elsewhere.has_value());
  CHECK(elsewhere->city == 1);
  CHECK(elsewhere->slai_day == 1);
}

TEST_CASE("S.L.A.I.'s lineups follow the place's day and city") {
  const auto shops = table();
  const auto *row = slai_shop_lineups(shops, {1, 1});
  REQUIRE(row != nullptr);
  CHECK(row->chips == 13);
  CHECK(slai_shop_lineups(shops, {1, 5}) == nullptr);
  CHECK(slai_dealer_lineup(shops, 1, 4) == 8);
  CHECK(slai_dealer_lineup(shops, 0, 3) == 5);
  CHECK(slai_dealer_lineup(shops, 9, 3) == -1);
}

TEST_CASE("S.L.A.I.'s junk shop shows its cycle day and holds back the best SVs") {
  const auto shops = table();
  // City 0 shows day 1 of each ten (S.L.A.I. day 0): city-day row 0, junk row 0.
  const auto *first = slai_junk_row(shops, {0, 1});
  REQUIRE(first != nullptr);
  CHECK(first->modules[0] == 30);
  // Its SV row 2 shows row 1's until the stage ladder reaches 4.
  CHECK(slai_junk_svs(shops, *first, 0) == 51);
  CHECK(slai_junk_svs(shops, *first, 4) == 52);
  // City 1 shows day 2 (S.L.A.I. day 1): row 3, junk row 1.
  const auto *second = slai_junk_row(shops, {1, 0});
  REQUIRE(second != nullptr);
  CHECK(second->modules[0] == 40);
  CHECK(slai_junk_svs(shops, *second, 0) == 50);
}

TEST_CASE("a parts shop sells only the records its stock holds") {
  const auto roots = DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT);
  PartTables parts;
  parts.bodies.assign(3, ArmourRecord{});
  for (ArmourRecord &record : parts.bodies) {
    record.name = "BODY";
  }
  GarageTables garage;
  garage.module_lineups = {{{0, 0}, {1, 0}, {2, 0}}};
  const auto table = parse_part_stats(roots.document("ui/part_stats.json"));
  const auto body = static_cast<std::size_t>(GoodsKind::body);
  const auto offered = [&](const int from, const int until) {
    auto script = parse_module_shop(roots.document("ui/module_shop.json"), ModuleShopMode::buy);
    script.stock_from[body] = from;
    script.stock_until[body] = until;
    ModuleShop logic(std::move(script), QuestSave{}, parts, garage, table, {0});
    for (int frame = 0; frame < 120; ++frame) {
      logic.update(1.0 / 60.0, InputFrame{});
    }
    const Buttons confirm = logic.script().confirm_buttons;
    logic.update(1.0 / 60.0, InputFrame{confirm, confirm, 1});
    for (int frame = 0; frame < 40; ++frame) {
      logic.update(1.0 / 60.0, InputFrame{});
    }
    return logic.goods().size();
  };
  CHECK(offered(0, ModuleShopScript::no_stock_limit()[body]) == 3);
  CHECK(offered(0, 2) == 2); // Phantom Crash's, before S.L.A.I.'s
  CHECK(offered(2, 3) == 1); // S.L.A.I.'s
}
