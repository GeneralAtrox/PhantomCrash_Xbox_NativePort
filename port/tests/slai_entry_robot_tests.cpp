// A port robot in S.L.A.I.'s terms for its entry check (slai_entry_robot.hpp):
// imported parts by the table's facts, Phantom Crash's by the world's entry
// block, on hand-made tables.

#include "test_support.hpp"

#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/slai_entry_robot.hpp"

using namespace phantomcrash;

namespace {

// Phantom Crash parts: body 0, legs 0 (model 1, legs type 7: hover), arm
// weapon 0 (melee), shoulder weapon 0 (missile), CHIP 0 and 5, and an O.C
// module. Imported: body 1 (maker 2), legs 1 (leg type 9), arm weapon 1
// (class 3, aim 9).
struct Tables {
  RumblingRules rules;
  PartTables parts;
  GarageTables garage;
  PartStatsTable stats;
  SlaiRumbleData data;

  Tables() {
    rules.legs_types = {0, 7};
    parts.bodies.assign(2, ArmourRecord{});
    parts.legs.assign(2, ArmourRecord{});
    parts.legs[0].model = 1;
    parts.arm_weapons.assign(2, WeaponRecord{});
    parts.arm_weapons[0].kind = 6;
    parts.shoulder_weapons.assign(1, WeaponRecord{});
    parts.shoulder_weapons[0].kind = 1;
    ModuleRecord module;
    module.active_frames = 600;
    garage.modules = {module};
    data.world = parse_slai_rumble_world(
        DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT).document(slai_rumble_world_file));
    data.table.parts.body_makers = {{1, 2}};
    data.table.parts.leg_types = {{1, 9}};
    data.table.parts.arm_weapons = {{1, {3, 9}}};
  }

  [[nodiscard]] RegulationCheck check() const {
    return {rules, parts, garage, stats};
  }
};

// A ridden hangar holding `ids` in the slots body, legs, right arm, left
// arm, right shoulder, left shoulder and CHIP (-1 for none).
QuestSave riding(const std::array<int, robot_part_slots> &ids, const bool camouflage = false) {
  constexpr std::array kinds{PartKind::body,     PartKind::legs,     PartKind::arm, PartKind::arm,
                             PartKind::shoulder, PartKind::shoulder, PartKind::chip};
  QuestSave quest;
  quest.garage.riding = 0;
  Hangar &hangar = quest.garage.hangars[0];
  hangar.owned = true;
  for (std::size_t slot = 0; slot < ids.size(); ++slot) {
    if (ids[slot] >= 0) {
      quest.garage.stock[static_cast<int>(slot)] = StockItem{kinds[slot], ids[slot], 0, "", 0};
      hangar.parts[slot] = static_cast<int>(slot);
    }
  }
  if (camouflage) {
    quest.garage.stock[10] = StockItem{PartKind::module, 0, 0, "", 0};
    hangar.modules = {10};
  }
  return quest;
}

} // namespace

TEST_CASE("Phantom Crash parts answer S.L.A.I.'s entry check through the world's entry block") {
  const Tables tables;
  const SlaiRobot robot =
      slai_riding_robot(tables.data, tables.check(), riding({0, 0, 0, -1, 0, -1, 0}, true));
  CHECK(robot.ready);
  CHECK(robot.maker == slai_any_maker); // no S.L.A.I. maker
  CHECK(robot.leg_type == 7);           // hover
  CHECK(robot.optical_camouflage);
  REQUIRE(robot.arms[0].has_value());
  CHECK(robot.arms[0]->weapon_class == 2); // melee: close range
  CHECK(!robot.arms[1].has_value());
  REQUIRE(robot.shoulders[0].has_value());
  CHECK(robot.shoulders[0]->aim_kind == 1); // missile: lock-on
  CHECK(robot.chip == tables.data.world.special.e_chips.front());

  const SlaiRobot other =
      slai_riding_robot(tables.data, tables.check(), riding({0, 0, -1, -1, -1, -1, 5}));
  CHECK(other.chip == -1);
  CHECK(!other.optical_camouflage);
}

TEST_CASE("imported S.L.A.I. parts answer with their own values") {
  const Tables tables;
  const SlaiRobot robot =
      slai_riding_robot(tables.data, tables.check(), riding({1, 1, 1, -1, -1, -1, 0}));
  CHECK(robot.maker == 2);
  CHECK(robot.leg_type == 9);
  REQUIRE(robot.arms[0].has_value());
  CHECK(robot.arms[0]->weapon_class == 3);
  CHECK(robot.arms[0]->aim_kind == 9);
}

TEST_CASE("no ridden robot is not ready, and rentals fall back to RUMBLING's") {
  Tables tables;
  CHECK(!slai_riding_robot(tables.data, tables.check(), QuestSave{}).ready);
  tables.rules.rental_robots = {4, 5};
  CHECK((slai_rental_robots(tables.data.world, tables.rules) == std::vector<int>{4, 5}));
  tables.data.world.entry.rental_robots = {7};
  CHECK((slai_rental_robots(tables.data.world, tables.rules) == std::vector<int>{7}));
}
