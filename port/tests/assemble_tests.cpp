// The hangar's ASSEMBLE: fitting parts by the robot's maker, weapons by
// side, modules by sockets, dismantling, and the screen's routes.

#include "test_support.hpp"

#include "phantomcrash/assemble.hpp"
#include "phantomcrash/data_roots.hpp"

#include <algorithm>
#include <string>

using namespace phantomcrash;

namespace {

constexpr double tick = 1.0 / 60.0;

AssembleScript shipped_script() {
  return parse_assemble(DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT).document("ui/assemble.json"));
}

PartStatsTable shipped_table() {
  return parse_part_stats(
      DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT).document("ui/part_stats.json"));
}

// Body 0 (maker 2, four sockets), body 1 (maker 2, two sockets), body 2
// (maker 3, which cannot be painted); legs of makers 2 and 1; a CHIP; an
// arm weapon of maker 2.
PartTables tables() {
  PartTables parts;
  parts.bodies.assign(3, ArmourRecord{});
  parts.bodies[0].maker = 2;
  parts.bodies[0].sockets.base = 4.0F;
  parts.bodies[1].maker = 2;
  parts.bodies[1].sockets.base = 2.0F;
  parts.bodies[2].maker = 3;
  parts.legs.assign(2, ArmourRecord{});
  parts.legs[0].maker = 2;
  parts.legs[0].capacity.base = 5000.0F;
  parts.legs[1].maker = 1;
  parts.chips.assign(1, ChipRecord{});
  parts.arm_weapons.assign(1, WeaponRecord{});
  parts.arm_weapons[0].maker = 2;
  const auto name = [](auto &records, const char *prefix) {
    for (std::size_t index = 0; index < records.size(); ++index) {
      records[index].name = prefix + std::to_string(index);
    }
  };
  name(parts.bodies, "BODY-");
  name(parts.legs, "LEGS-");
  name(parts.chips, "CHIP-");
  name(parts.arm_weapons, "ARM-");
  return parts;
}

// Module 0 takes three sockets.
GarageTables garage_tables() {
  GarageTables garage;
  garage.modules.assign(1, ModuleRecord{});
  garage.modules[0].name = "ARMOR";
  garage.modules[0].sockets = 3;
  return garage;
}

struct Stock {
  int body{}, small_body{}, odd_body{}, legs{}, odd_legs{}, chip{}, arm{}, module_a{}, module_b{};
};

// Everything loose in the stock; hangars 0 to 2 are owned and empty.
QuestSave owned(Stock &stock) {
  QuestSave quest;
  quest.garage = new_game_garage(garage_new_game_hangars);
  Garage &garage = quest.garage;
  stock.body = garage.add_stock({PartKind::body, 0, 0, "", -1});
  stock.small_body = garage.add_stock({PartKind::body, 1, 0, "", -1});
  stock.odd_body = garage.add_stock({PartKind::body, 2, 0, "", -1});
  stock.legs = garage.add_stock({PartKind::legs, 0, 0, "", -1});
  stock.odd_legs = garage.add_stock({PartKind::legs, 1, 0, "", -1});
  stock.chip = garage.add_stock({PartKind::chip, 0, 0, "", -1});
  stock.arm = garage.add_stock({PartKind::arm, 0, 0, "", -1});
  stock.module_a = garage.add_stock({PartKind::module, 0, 0, "", -1});
  stock.module_b = garage.add_stock({PartKind::module, 0, 0, "", -1});
  return quest;
}

struct Bench {
  PartTables parts = tables();
  GarageTables garage = garage_tables();
  PartStatsTable table = shipped_table();
  Stock stock;
  QuestSave quest = owned(stock);
  FittingRules rules{true, {3, 4, 5, 6}};

  [[nodiscard]] ShopGoods line(const GoodsKind kind, const int entry) const {
    for (const ShopGoods &goods : goods_owned(table, parts, garage, quest.garage, kind, 1.0)) {
      if (goods.stock == entry) {
        return goods;
      }
    }
    return {};
  }
  [[nodiscard]] bool can(const GoodsKind kind, const int entry, const int side = 0) const {
    return can_fit(parts, garage, quest.garage, 0, line(kind, entry), side, rules);
  }
  FitResult put(const GoodsKind kind, const int entry, const int side = 0) {
    return fit(parts, garage, quest.garage, 0, line(kind, entry), side);
  }
  [[nodiscard]] const Hangar &bay() const {
    return quest.garage.hangars[0];
  }
};

void settle(Assemble &logic, const double seconds = 1.5) {
  for (double time = 0.0; time < seconds; time += tick) {
    logic.update(tick, InputFrame{});
  }
}

void tap(Assemble &logic, const Buttons buttons, const int times = 1) {
  for (int count = 0; count < times; ++count) {
    logic.update(tick, InputFrame{buttons, buttons, 1});
    settle(logic, 0.5);
  }
}

} // namespace

TEST_CASE("assemble.json reads the grid, the panel, the list and the routes") {
  const AssembleScript script = shipped_script();
  CHECK(script.scene == 4);
  CHECK(script.cells.size() == 12);
  CHECK(script.grid_rows == 2);
  CHECK(script.exit_to == "hangar");
  REQUIRE(script.cell_for(AssembleItem::arm) != nullptr);
  CHECK(script.cell_for(AssembleItem::arm)->sides.has_value());
  CHECK(!script.cell_for(AssembleItem::body)->sides.has_value());
  CHECK(script.routes.at(AssembleItem::name) == "name_entry");
  CHECK(script.routes.at(AssembleItem::spec) == "sv_spec");
  CHECK(script.messages.count("modules_off") == 1);
}

TEST_CASE("a body, legs and CHIP of the robot's maker make it complete") {
  Bench bench;
  CHECK(bench.can(GoodsKind::legs, bench.stock.odd_legs)); // nothing fitted yet
  bench.put(GoodsKind::body, bench.stock.body);
  CHECK(robot_maker(bench.parts, bench.quest.garage, bench.bay()) == 2);
  CHECK(!bench.can(GoodsKind::legs, bench.stock.odd_legs));
  CHECK(bench.can(GoodsKind::legs, bench.stock.legs));
  bench.put(GoodsKind::legs, bench.stock.legs);
  bench.put(GoodsKind::chip, bench.stock.chip);
  CHECK(bench.bay().complete());
  CHECK(bench.quest.garage.stock.at(bench.stock.body).hangar == 0);
  // Fitting it again takes it off.
  const FitResult off = bench.put(GoodsKind::chip, bench.stock.chip);
  CHECK(off.removed);
  CHECK(!bench.bay().complete());
}

TEST_CASE("a weapon goes on the side chosen and is greyed out on the other") {
  Bench bench;
  bench.put(GoodsKind::body, bench.stock.body);
  bench.put(GoodsKind::arm, bench.stock.arm, 0);
  CHECK(bench.bay().part(PartSlot::right_arm) == bench.stock.arm);
  CHECK(bench.bay().part(PartSlot::left_arm) == -1);
  CHECK(bench.can(GoodsKind::arm, bench.stock.arm, 0));
  CHECK(!bench.can(GoodsKind::arm, bench.stock.arm, 1));
}

TEST_CASE("modules need free sockets; a smaller body takes them off") {
  Bench bench;
  bench.put(GoodsKind::body, bench.stock.body);
  CHECK(free_sockets(bench.parts, bench.garage, bench.quest.garage, bench.bay()) == 4);
  bench.put(GoodsKind::module, bench.stock.module_a);
  CHECK(bench.bay().modules.size() == 1);
  CHECK(free_sockets(bench.parts, bench.garage, bench.quest.garage, bench.bay()) == 1);
  CHECK(!bench.can(GoodsKind::module, bench.stock.module_b));
  const FitResult swapped = bench.put(GoodsKind::body, bench.stock.small_body);
  CHECK(swapped.modules_off == 1);
  CHECK(bench.bay().modules.empty());
  CHECK(bench.bay().part(PartSlot::body) == bench.stock.small_body);
  CHECK(bench.quest.garage.stock.at(bench.stock.body).hangar == -1);
}

TEST_CASE("dismantling clears the name and the ride; some makers cannot be painted") {
  Bench bench;
  bench.put(GoodsKind::body, bench.stock.body);
  bench.put(GoodsKind::legs, bench.stock.legs);
  bench.put(GoodsKind::chip, bench.stock.chip);
  Garage &garage = bench.quest.garage;
  garage.hangars[0].name = "ALPHA";
  garage.riding = 0;
  bench.quest.riding = "ALPHA";
  dismantle(garage, 0);
  settle_hangar(bench.parts, bench.rules, garage, 0, bench.quest.riding);
  CHECK(bench.bay().empty());
  CHECK(bench.bay().name.empty());
  CHECK(bench.quest.riding.empty());
  CHECK(garage.stock.at(bench.stock.body).hangar == -1);

  garage.hangars[0].paint = 5;
  bench.put(GoodsKind::body, bench.stock.odd_body);
  settle_hangar(bench.parts, bench.rules, garage, 0, bench.quest.riding);
  CHECK(bench.bay().paint == 0);
  Assemble logic(shipped_script(), bench.quest, bench.parts, bench.garage, bench.table, 0);
  CHECK(!logic.can_choose(AssembleItem::paint));
}

TEST_CASE("the grid fits a body through the list and details; cancel goes to EXIT") {
  Bench bench;
  Assemble logic(shipped_script(), bench.quest, bench.parts, bench.garage, bench.table, 0);
  settle(logic);
  const AssembleScript &script = logic.script();
  CHECK(logic.cell().item == AssembleItem::body);
  CHECK(!logic.can_choose(AssembleItem::name));
  tap(logic, script.confirm_buttons);
  REQUIRE(logic.view() == AssembleView::list);
  CHECK(logic.goods_list().goods().size() == 3);
  tap(logic, script.confirm_buttons);
  REQUIRE(logic.view() == AssembleView::details);
  tap(logic, script.confirm_buttons);
  CHECK(logic.view() == AssembleView::list);
  CHECK(logic.fitted().part(PartSlot::body) != -1);
  tap(logic, script.cancel_buttons);
  CHECK(logic.view() == AssembleView::grid);
  tap(logic, script.cancel_buttons);
  CHECK(logic.cell().item == AssembleItem::exit);
  tap(logic, script.confirm_buttons);
  settle(logic, 2.0);
  CHECK(logic.done());
  CHECK(logic.next() == "hangar");
}

TEST_CASE("ARM asks for a side first; NAME leaves for NAME ENTRY with the hangar") {
  Bench bench;
  bench.put(GoodsKind::body, bench.stock.body);
  bench.put(GoodsKind::legs, bench.stock.legs);
  bench.put(GoodsKind::chip, bench.stock.chip);
  Assemble logic(shipped_script(), bench.quest, bench.parts, bench.garage, bench.table, 0);
  settle(logic);
  const AssembleScript &script = logic.script();
  tap(logic, script.right);
  CHECK(logic.cell().item == AssembleItem::arm);
  tap(logic, script.confirm_buttons);
  REQUIRE(logic.view() == AssembleView::sides);
  tap(logic, script.left);
  CHECK(logic.side() == 1);
  tap(logic, script.cancel_buttons);
  CHECK(logic.view() == AssembleView::grid);
  tap(logic, script.right, 3);
  CHECK(logic.cell().item == AssembleItem::name);
  tap(logic, script.confirm_buttons);
  settle(logic, 2.0);
  CHECK(logic.done());
  CHECK(logic.next() == "name_entry");
  CHECK(logic.naming_hangar() == 0);
}
