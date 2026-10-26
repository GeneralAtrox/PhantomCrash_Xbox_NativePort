// S.L.A.I. Rumble's rules on hand-made tables, its world data and its save
// block. The disc-backed checks are in slai_rumble_table_tests.cpp.

#include "test_support.hpp"

#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/sleep.hpp"
#include "phantomcrash/slai_rumble_rules.hpp"

using namespace phantomcrash;

namespace {

// The rota of tests/test_slai_rumble_table.py's _synthetic(): every day
// the same, with the finals stage holding final ids to replace.
SlaiRumbleTable synthetic_table() {
  SlaiRumbleTable table;
  table.cities = {"Tokyo", "Stuttgart", "New York", "London", "Hong Kong", "Cairo", "Las Vegas"};
  table.schedule_days = 100;
  table.stages = 5;
  table.finals_stage = 3;
  constexpr std::array<int, 7> finals_stage{2, 6, 3, 5, 40, 41, slai_no_match};
  for (int city = 0; city < 7; ++city) {
    auto &stages = table.city_days.emplace_back();
    stages.push_back({10 + city, slai_no_match});
    stages.push_back({20 + city, slai_no_match});
    stages.push_back({30, slai_no_match});
    stages.push_back(city == 0
                         ? std::array<int, 2>{slai_no_match, 40}
                         : std::array<int, 2>{finals_stage[static_cast<std::size_t>(city)], 6});
    stages.push_back({50, slai_no_match});
    table.finals_fallback.push_back({70 + city, 80 + city});
  }
  table.day_rows.assign(100, {0, 1, 2, 3, 4, 5, 6});
  table.finals = {
      {"3rd place", 0, {2, 5}, false, {{{6, 5, 3, 2}, std::nullopt}}},
      {"2nd place", 3, {3, 6}, false, {{{6, 3}, std::nullopt}}},
      {"final",
       2,
       {4, slai_no_match},
       true,
       {{{6, 3}, std::array<int, 2>{0x86, 0x88}}, {{5, 2}, std::array<int, 2>{0x85, 0x87}}}},
  };
  return table;
}

SlaiRumbleWorld world_data() {
  return parse_slai_rumble_world(
      DataRoots::standard(PHANTOMCRASH_SOURCE_ROOT).document(slai_rumble_world_file));
}

SlaiRegulation regulation(const SlaiSpecialRule special) {
  SlaiRegulation rule;
  rule.entry_requirement = 2000;
  rule.leg_classes = {"any"};
  rule.special = special;
  rule.target = 30;
  rule.prize = 3500;
  return rule;
}

SlaiRobot ready_robot() {
  SlaiRobot robot;
  robot.ready = true;
  robot.maker = 1;
  robot.leg_type = 4;
  robot.arms[0] = SlaiWeapon{2, 1};
  robot.shoulders[1] = SlaiWeapon{3, 9};
  robot.chip = 5;
  return robot;
}

} // namespace

TEST_CASE("S.L.A.I. Rumble's finals replace the rota's finals-stage matches as 0x257070 does") {
  const auto table = synthetic_table();
  const auto at = [&](int finals, int city, bool night) {
    return slai_scheduled_match(table, 0, city, 3, night, finals);
  };
  // campaign.scheduled_match on the same rota gives these.
  CHECK(at(0, 0, false) == slai_no_match);
  CHECK(at(0, 0, true) == 5);
  CHECK(at(0, 1, false) == 71);
  CHECK(at(0, 1, true) == 81);
  CHECK(at(0, 2, false) == 72);
  CHECK(at(0, 3, false) == 73);
  CHECK(at(0, 4, false) == 40);
  CHECK(at(0, 5, false) == 41);
  CHECK(at(0, 6, false) == slai_no_match);
  CHECK(at(1, 3, false) == 3);
  CHECK(at(1, 3, true) == 6);
  CHECK(at(1, 1, false) == 71);
  CHECK(at(1, 2, false) == 72);
  CHECK(at(1, 2, true) == 82);
  CHECK(at(2, 2, false) == 4);
  CHECK(at(2, 2, true) == slai_no_match);
  CHECK(at(2, 1, false) == 134);
  CHECK(at(2, 1, true) == 136);
  CHECK(at(2, 3, false) == 133);
  CHECK(at(2, 4, false) == 40);
  CHECK(at(2, 0, true) == 40);
  CHECK(at(2, 0, false) == slai_no_match);
  CHECK(at(3, 1, false) == 6);
  // Other stages read the rota as it is; the day wraps at schedule_days.
  CHECK(slai_scheduled_match(table, 0, 4, 1, false, 0) == 24);
  CHECK(slai_scheduled_match(table, 163, 4, 1, false, 2) == 24);
  CHECK(slai_scheduled_match(table, 0, 7, 1, false, 0) == slai_no_match);
  CHECK(slai_scheduled_match(table, -1, 0, 1, false, 0) == slai_no_match);
  SlaiRumbleData data;
  data.table = table;
  data.rota.days = {{{2, 0}, {1, 7}, {3, 0}}, {{6, 5}, {0, 5}, {4, 5}}};
  SlaiRumbleSave save;
  save.city = 1;
  save.stage = 3;
  save.finals_cleared = 2;
  CHECK((slai_todays_matches(data, 0, save) == std::array<int, 2>{134, 136}));
  CHECK((slai_todays_matches(data, 1, save) == std::array<int, 2>{slai_no_match, slai_no_match}));
  CHECK((slai_todays_matches(data, 2, save) == std::array<int, 2>{134, 136}));
}

TEST_CASE("S.L.A.I. Rumble's world data names a stage pair for each city") {
  const auto world = world_data();
  REQUIRE(world.cities.size() == 7);
  CHECK(slai_match_stage(world, 0, false) == "slai_day_tokyo2");
  CHECK(slai_match_stage(world, 5, true) == "slai_night_egypt");
  // No NIGHT_LASVEGAS.BG: the day scene with the night lighting.
  CHECK(slai_match_stage(world, 6, true) == "slai_day_lasvegas");
  CHECK(world.cities[6].night_lighting == "lasvegas_night");
  CHECK(world.makers.size() == 5);
  CHECK(world.makers[3] == "KHT");
  CHECK(world.status_rules.size() == 32);
  CHECK(world.ladder.rankers_needed == 56);
  CHECK(world.balance.prize_scale == 1.0);
  CHECK(!world.balance.charge_entry);
  // The stand-in opponents keep to what they can walk and get unstuck (port
  // choices).
  CHECK(world.battle.spawn_route);
  CHECK(world.battle.spawn_merge > 0.0F);
  CHECK(world.battle.unstick_seconds > 0.0F);
  CHECK(world.battle.hops > 0);
}

TEST_CASE("the S.L.A.I. entry check follows 0x259a40's order and rules") {
  const auto world = world_data();
  auto rule = regulation(SlaiSpecialRule::none);
  const auto robot = ready_robot();
  CHECK(slai_entry_check(world, rule, 2000, robot) == SlaiEntry::allowed);
  CHECK(slai_entry_check(world, rule, 1999, robot) == SlaiEntry::money);
  auto unready = robot;
  unready.ready = false;
  CHECK(slai_entry_check(world, rule, 2000, unready) == SlaiEntry::not_ready);
  rule.maker = 3;
  CHECK(slai_entry_check(world, rule, 2000, robot) == SlaiEntry::maker);
  rule.maker = 1;
  CHECK(slai_entry_check(world, rule, 2000, robot) == SlaiEntry::allowed);
  rule.leg_classes = {"legged"};
  CHECK(slai_entry_check(world, rule, 2000, robot) == SlaiEntry::legs);
  rule.leg_classes = {"wheeled"};
  CHECK(slai_entry_check(world, rule, 2000, robot) == SlaiEntry::allowed);
  rule.leg_classes.clear();
  CHECK(slai_entry_check(world, rule, 2000, robot) == SlaiEntry::allowed);

  const auto special = [&](SlaiSpecialRule kind, const SlaiRobot &with) {
    return slai_entry_check(world, regulation(kind), 2000, with) == SlaiEntry::allowed;
  };
  CHECK(!special(SlaiSpecialRule::rental_only, robot));
  auto camouflaged = robot;
  camouflaged.optical_camouflage = true;
  CHECK(special(SlaiSpecialRule::no_optical_camouflage, robot));
  CHECK(!special(SlaiSpecialRule::no_optical_camouflage, camouflaged));
  CHECK(!special(SlaiSpecialRule::shoulder_weapons_only, robot));
  CHECK(!special(SlaiSpecialRule::arm_weapons_only, robot));
  auto shoulders = robot;
  shoulders.arms = {};
  CHECK(special(SlaiSpecialRule::shoulder_weapons_only, shoulders));
  CHECK(special(SlaiSpecialRule::grenade_only, shoulders));
  CHECK(!special(SlaiSpecialRule::close_range_only, robot)); // the shoulder weapon is class 3
  auto arms = robot;
  arms.shoulders = {};
  CHECK(special(SlaiSpecialRule::arm_weapons_only, arms));
  CHECK(special(SlaiSpecialRule::close_range_only, arms));
  CHECK(special(SlaiSpecialRule::lock_on_only, robot)); // aim kinds 1 and 9
  CHECK(!special(SlaiSpecialRule::e_chip_only, robot));
  auto e_chip = robot;
  e_chip.chip = 1;
  CHECK(special(SlaiSpecialRule::e_chip_only, e_chip));
}

TEST_CASE("S.L.A.I. records set flags, clear on the leader and give the manager's status") {
  const auto world = world_data();
  SlaiRumbleSave save;
  CHECK(slai_record_beat(save, 0x30, 1));
  CHECK(!slai_record_beat(save, 0x30, 1));
  CHECK(!slai_record_beat(save, 0x30, 15));
  CHECK(!slai_record_beat(save, -1, 1));
  const auto &record = save.records.at(0x30);
  CHECK(record.beaten[0]);
  CHECK(!record.cleared);
  // Class D (23) waits for all 14 members; the tutorial (0) for 5; Class
  // Free (22) and the head-to-heads have their leader open from the start.
  CHECK(slai_record_status(world, 23, record) == SlaiRecordStatus::open_members);
  for (int flag = 2; flag <= 14; ++flag) {
    slai_record_beat(save, 0x30, flag);
  }
  CHECK(slai_record_status(world, 23, save.records.at(0x30)) == SlaiRecordStatus::members_beaten);
  SlaiRumbleRecord five;
  for (std::size_t flag = 0; flag < 5; ++flag) {
    five.beaten[flag] = true;
  }
  CHECK(slai_record_status(world, 0, five) == SlaiRecordStatus::members_beaten);
  CHECK(slai_record_status(world, 22, {}) == SlaiRecordStatus::leader_open);
  CHECK(slai_record_status(world, 27, {}) == SlaiRecordStatus::leader_open);
  CHECK(slai_record_status(world, 30, {}) == SlaiRecordStatus::members_beaten);
  CHECK(slai_record_beat(save, 0x30, slai_leader_flag));
  CHECK(slai_record_status(world, 23, save.records.at(0x30)) == SlaiRecordStatus::leader_open);
  CHECK(slai_record_cleared(save, 0x30));
}

TEST_CASE("S.L.A.I.'s stage ladder moves one step after a battle") {
  const auto ladder = world_data().ladder;
  SlaiRumbleSave save;
  CHECK(slai_next_stage(save, ladder) == 0);
  slai_record_beat(save, ladder.tutorial_record, slai_leader_flag);
  CHECK(slai_next_stage(save, ladder) == 1);
  save.stage = 1;
  for (int record = 0x30; record < 0x67; ++record) {
    slai_record_beat(save, record, slai_leader_flag);
  }
  CHECK(slai_next_stage(save, ladder) == 1); // 55 of 56
  slai_record_beat(save, 0x67, slai_leader_flag);
  CHECK(slai_next_stage(save, ladder) == 2);
  // Every regional and final cleared still moves only one step.
  for (int record = 0x68; record < 0x6f; ++record) {
    slai_record_beat(save, record, slai_leader_flag);
  }
  for (int record = 0x1a; record < 0x1d; ++record) {
    slai_record_beat(save, record, slai_leader_flag);
  }
  CHECK(slai_next_stage(save, ladder) == 2);
  save.stage = 3;
  CHECK(slai_next_stage(save, ladder) == 4);
  save.stage = 4;
  CHECK(slai_next_stage(save, ladder) == 4);
}

TEST_CASE("a S.L.A.I. match's result fills its records, pays its prize and moves the ladder") {
  SlaiRumbleData data{synthetic_table(), world_data()};
  data.table.regulations = {regulation(SlaiSpecialRule::none)};
  data.table.regulations[0].target = 1;
  data.table.regulations[0].prize = 25000;
  data.table.matches = {{0, 0, false, 1, 2, -1, 0x1c, -1}};
  QuestSave quest;
  quest.money = 1000;
  quest.slai_rumble.stage = 3;
  slai_record_beat(quest.slai_rumble, 0x1a, slai_leader_flag);
  slai_record_beat(quest.slai_rumble, 0x1b, slai_leader_flag);
  const auto lost = slai_finish_match(quest, data, {0, 0, {}});
  CHECK(lost.prize == 0);
  CHECK(!lost.cleared);
  CHECK(quest.money == 1000);
  CHECK(quest.slai_rumble.finals_cleared == 2);
  const auto won = slai_finish_match(quest, data, {0, 1, {{0x1c, slai_leader_flag}}});
  CHECK(won.prize == 25000);
  CHECK(won.cleared);
  CHECK(won.stage_before == 3);
  CHECK(won.stage_after == 4);
  CHECK(quest.money == 26000);
  CHECK(quest.slai_rumble.finals_cleared == 3);
  data.world.balance.prize_scale = 0.5;
  quest.money = money_limit - 100;
  const auto again = slai_finish_match(quest, data, {0, 3, {}});
  CHECK(again.prize == 12500);
  CHECK(!again.cleared);
  CHECK(quest.money == money_limit);
}

TEST_CASE("S.L.A.I.'s entry requirement is held, and taken only when charge_entry is set") {
  auto world = world_data();
  const auto rule = regulation(SlaiSpecialRule::none);
  QuestSave quest;
  quest.money = 1500;
  CHECK(!slai_pay_entry(quest, world, rule));
  quest.money = 2500;
  CHECK(slai_pay_entry(quest, world, rule));
  CHECK(quest.money == 2500);
  world.balance.charge_entry = true;
  world.balance.entry_scale = 0.5;
  CHECK(slai_entry_requirement(world, rule) == 1000);
  CHECK(slai_pay_entry(quest, world, rule));
  CHECK(quest.money == 1500);
  CHECK((slai_match_robots(world, {0, 0, false, 3, 9, -1, -1, -1}) == std::pair<int, int>{3, 6}));
}

TEST_CASE("the S.L.A.I. Rumble save block round-trips, and older saves load it empty") {
  QuestSave quest;
  quest.slai_rumble.stage = 2;
  quest.slai_rumble.city = 6;
  quest.slai_rumble.finals_cleared = 1;
  slai_record_beat(quest.slai_rumble, 0x31, 3);
  slai_record_beat(quest.slai_rumble, 0x18, slai_leader_flag);
  const auto copy = quest_from_json(quest_to_json(quest));
  CHECK(copy.slai_rumble == quest.slai_rumble);
  CHECK(copy == quest);
  const auto older = quest_from_json(json::parse(R"({"day":12,"money":3400,"cleared":true})"));
  CHECK(older.day == 12);
  CHECK(older.slai_rumble == SlaiRumbleSave{});
  const auto odd = slai_rumble_from_json(json::parse(
      R"({"day":42,"stage":-3,"records":[{"id":5,"beaten":[0,1,15]},{"id":6},{"cleared":true}]})"));
  CHECK(odd.stage == 0);
  REQUIRE(odd.records.size() == 1);
  CHECK(odd.records.at(5).beaten[0]);
}

TEST_CASE("S.L.A.I. Rumble's rota follows the calendar's day") {
  SlaiRumbleData data;
  data.table = synthetic_table();
  data.rota.days = {{{2, 0}, {1, 7}, {3, 0}}, {{6, 5}, {0, 5}, {4, 5}}};
  SlaiRumbleSave save;
  save.stage = 1;
  const auto today = slai_day_matches(data, 0, save);
  REQUIRE(today.size() == 3);
  CHECK(today[1].city == 1);
  CHECK(today[1].slai_day == 7);
  CHECK(today[1].match == 21);
  CHECK(!today[1].night);
  // A city with no day match shows its night one; a closed city none.
  save.stage = 3;
  save.finals_cleared = 2;
  const auto tokyo = slai_city_match(data, 1, 0, save);
  REQUIRE(tokyo);
  CHECK(tokyo->match == 40);
  CHECK(tokyo->night);
  CHECK(!slai_city_match(data, 1, 1, save));
  CHECK(slai_rota_source(data.rota, 3, 4) == 5);
  CHECK(slai_rota_day(SlaiRota{}, 4).empty());
  CHECK_THROWS(parse_slai_rota(json::parse(R"({"days":[[{"city":7,"slai_day":0}]]})"), 7, 100));
  CHECK_THROWS(parse_slai_rota(json::parse(R"({"days":[[{"city":1,"slai_day":100}]]})"), 7, 100));
}

TEST_CASE("the committed rota opens three cities a day, none two days running") {
  const auto roots = DataRoots::standard(PHANTOMCRASH_SOURCE_ROOT);
  const auto rota = parse_slai_rota(roots.document(slai_rota_file), 7, 100);
  REQUIRE(rota.days.size() == 60);
  for (std::size_t day = 0; day < rota.days.size(); ++day) {
    const auto &today = rota.days[day];
    const auto &next = rota.days[(day + 1) % rota.days.size()];
    REQUIRE(today.size() == 3);
    for (const auto &entry : today) {
      for (const auto &later : next) {
        CHECK(entry.city != later.city);
      }
    }
  }
}
