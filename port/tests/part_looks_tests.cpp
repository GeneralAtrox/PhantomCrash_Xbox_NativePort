// Per-part appearance (a port addition): the meshes a part may wear, the
// look kept with the stock, and turning it in ASSEMBLE and BUY MODULE.

#include "test_support.hpp"

#include "phantomcrash/assemble.hpp"
#include "phantomcrash/battle_weapons.hpp"
#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/garage.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/module_shop.hpp"
#include "phantomcrash/part_looks.hpp"

#include <string>
#include <vector>

using namespace phantomcrash;

namespace {

constexpr double tick = 1.0 / 60.0;

// Bodies 0 and 2 draw the same model (2's type is a copy for paints), body
// 1 another, body 3 none. Arms 0 and 1 are cannons of class 0 with their own
// models, arm 2 a cannon of class 1 and arm 3 a melee weapon of class 0.
RobotCatalogue catalogue() {
  RobotCatalogue robots;
  robots.body_parts = {0, 1, 2, -1};
  robots.body_types.resize(3);
  robots.body_types[0].model = 10;
  robots.body_types[1].model = 11;
  robots.body_types[2].model = 10;
  robots.arm_parts = {0, 1, 2, 3};
  robots.arm_types.resize(4);
  for (std::size_t type = 0; type < robots.arm_types.size(); ++type) {
    const int model = 20 + 2 * static_cast<int>(type);
    robots.arm_types[type].models = {model, model + 1};
  }
  robots.arm_types[2].arm_class = 1;
  return robots;
}

PartTables tables() {
  PartTables parts;
  parts.bodies.assign(4, ArmourRecord{});
  for (std::size_t index = 0; index < parts.bodies.size(); ++index) {
    parts.bodies[index].name = "BODY-" + std::to_string(index);
    parts.bodies[index].maker = 2;
    parts.bodies[index].price.base = 1000.0F;
  }
  parts.arm_weapons.assign(4, WeaponRecord{});
  parts.arm_weapons[3].kind = hud_kind::melee;
  return parts;
}

GarageTables garage_tables() {
  GarageTables garage;
  garage.modules.assign(46, ModuleRecord{});
  garage.module_lineups = {{{0, 1}, {1, -1}}};
  return garage;
}

PartStatsTable shipped_table() {
  return parse_part_stats(
      DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT).document("ui/part_stats.json"));
}

template <typename Logic> void settle(Logic &logic, const double seconds = 1.5) {
  for (double time = 0.0; time < seconds; time += tick) {
    logic.update(tick, InputFrame{});
  }
}

template <typename Logic> void tap(Logic &logic, const Buttons buttons) {
  logic.update(tick, InputFrame{buttons, buttons, 1});
  settle(logic, 0.6);
}

struct Bench {
  RobotCatalogue robots = catalogue();
  PartTables parts = tables();
  GarageTables garage = garage_tables();
  PartStatsTable table = shipped_table();
  [[nodiscard]] PartLooks looks(const PartLookRules &rules = {}) const {
    return PartLooks(robots, parts, rules);
  }
};

} // namespace

TEST_CASE("a part's looks: its own mesh first, one part for each other mesh") {
  const Bench bench;
  const PartLooks looks = bench.looks();
  CHECK((looks.choices(PartKind::body, 0) == std::vector<int>{0, 1}));
  CHECK((looks.choices(PartKind::body, 1) == std::vector<int>{1, 0}));
  // Body 2 draws body 0's mesh, so it offers only body 1's.
  CHECK((looks.choices(PartKind::body, 2) == std::vector<int>{2, 1}));
  CHECK(!looks.offers(PartKind::body, 3));
  CHECK(!looks.offers(PartKind::chip, 0));
  CHECK(!looks.offers(PartKind::body, 9));
}

TEST_CASE("a weapon's looks keep its kind and an arm's class unless the rules allow") {
  const Bench bench;
  const PartLooks kept = bench.looks();
  CHECK((kept.choices(PartKind::arm, 0) == std::vector<int>{0, 1}));
  CHECK(!kept.offers(PartKind::arm, 2));
  CHECK(!kept.offers(PartKind::arm, 3));
  const PartLooks free = bench.looks({true, false, false});
  CHECK((free.choices(PartKind::arm, 0) == std::vector<int>{0, 1, 2, 3}));
  CHECK(!bench.looks({false, true, true}).offers(PartKind::body, 0));
}

TEST_CASE("stepping a look wraps round to the part's own mesh") {
  const PartLooks looks = Bench{}.looks({true, false, false});
  CHECK(looks.step(PartKind::arm, 0, -1, 1) == 1);
  CHECK(looks.step(PartKind::arm, 0, 1, 1) == 2);
  CHECK(looks.step(PartKind::arm, 0, 3, 1) == -1);
  CHECK(looks.step(PartKind::arm, 0, -1, -1) == 3);
  // A look no longer offered starts again from the own mesh.
  CHECK(looks.step(PartKind::arm, 0, 7, 1) == 1);
  CHECK(looks.step(PartKind::body, 3, -1, 1) == -1);
}

TEST_CASE("the stock keeps a look, saved only when one is chosen") {
  Garage garage;
  const int worn = garage.add_stock({PartKind::body, 0, 0, "", -1, 1});
  const int plain = garage.add_stock({PartKind::legs, 0, 0, "", -1});
  const std::string text = json::write(garage_to_json(garage));
  CHECK(text.find("\"look\"") != std::string::npos);
  CHECK(text.find("\"look\"") == text.rfind("\"look\""));
  const Garage back = garage_from_json(garage_to_json(garage));
  CHECK(back.stock.at(worn).look == 1);
  CHECK(back.stock.at(plain).look == -1);
}

TEST_CASE("a part wears its look, or its own mesh when the look has none") {
  const RobotCatalogue robots = catalogue();
  CHECK(worn_part({PartKind::body, 0, 0, "", -1}) == 0);
  CHECK(worn_part({PartKind::body, 0, 0, "", -1, 1}, &robots) == 1);
  CHECK(worn_part({PartKind::body, 0, 0, "", -1, 3}, &robots) == 0);
  CHECK(worn_part({PartKind::body, 0, 0, "", -1, 3}) == 3);
}

TEST_CASE("ASSEMBLE: left and right turn the chosen part's look, which the robot tries on") {
  const Bench bench;
  QuestSave quest;
  quest.garage = new_game_garage(garage_new_game_hangars);
  quest.garage.add_stock({PartKind::body, 0, 0, "", -1});
  quest.garage.add_stock({PartKind::body, 1, 0, "", -1});
  Assemble logic(
      parse_assemble(DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT).document("ui/assemble.json")),
      quest, bench.parts, bench.garage, bench.table, 0);
  logic.set_looks(bench.looks());
  settle(logic);
  const AssembleScript &script = logic.script();
  REQUIRE(script.look_arrows.has_value());
  tap(logic, script.confirm_buttons);
  REQUIRE(logic.view() == AssembleView::list);
  const ShopGoods *line = logic.chosen();
  REQUIRE(line != nullptr);
  REQUIRE(logic.offers_look(*line));
  const int entry = line->stock;
  const int own = line->goods.id;
  CHECK(logic.shown_hangar().part(PartSlot::body) == -1);
  tap(logic, script.right);
  CHECK(logic.view() == AssembleView::list);
  CHECK(logic.quest().garage.stock.at(entry).look == 1 - own);
  // Tried on, not fitted.
  CHECK(logic.shown_hangar().part(PartSlot::body) == entry);
  CHECK(logic.fitted().part(PartSlot::body) == -1);
  tap(logic, script.left);
  CHECK(logic.quest().garage.stock.at(entry).look == -1);
  tap(logic, script.cancel_buttons);
  CHECK(logic.shown_hangar().part(PartSlot::body) == -1);
}

TEST_CASE("BUY MODULE: the look shown is the look bought; selling offers none") {
  const Bench bench;
  QuestSave quest;
  quest.money = 10000;
  const auto script = [](const ModuleShopMode mode) {
    return parse_module_shop(
        DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT).document("ui/module_shop.json"), mode);
  };
  ModuleShop logic(script(ModuleShopMode::buy), quest, bench.parts, bench.garage, bench.table, {0});
  logic.set_looks(bench.looks());
  settle(logic, 2.0);
  const ModuleShopScript &buy = logic.script();
  tap(logic, buy.confirm_buttons);
  REQUIRE(logic.view() == ModuleShopView::list);
  tap(logic, buy.down);
  const ShopGoods *line = logic.chosen();
  REQUIRE(line != nullptr);
  REQUIRE(line->goods.id == 1);
  REQUIRE(logic.offers_look(*line));
  CHECK(logic.shown_part(*line) == 1);
  tap(logic, buy.right);
  CHECK(logic.shown_part(*logic.chosen()) == 0);
  tap(logic, buy.right);
  CHECK(logic.shown_part(*logic.chosen()) == 1);
  tap(logic, buy.left);
  CHECK(logic.shown_part(*logic.chosen()) == 0);
  tap(logic, buy.confirm_buttons);
  REQUIRE(logic.view() == ModuleShopView::question);
  tap(logic, buy.left);
  tap(logic, buy.confirm_buttons);
  REQUIRE(logic.message() == "thanks");
  REQUIRE(logic.quest().garage.stock.size() == 1);
  CHECK(logic.quest().garage.stock.begin()->second.id == 1);
  CHECK(logic.quest().garage.stock.begin()->second.look == 0);

  QuestSave owned;
  owned.garage.add_stock({PartKind::body, 1, 0, "BODY-1", -1});
  ModuleShop selling(script(ModuleShopMode::sell), owned, bench.parts, bench.garage, bench.table);
  selling.set_looks(bench.looks());
  settle(selling, 2.0);
  tap(selling, selling.script().confirm_buttons);
  REQUIRE(selling.view() == ModuleShopView::list);
  REQUIRE(selling.chosen() != nullptr);
  CHECK(!selling.offers_look(*selling.chosen()));
}
