// S.L.A.I.'s area select (slai_area_select.hpp) on hand-made tables: the
// cities' plates and pictures, CLOSED for the rota's closed cities, and
// entering a rumble through RUMBLING's sub-menu and gates.

#include "test_support.hpp"

#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/rumbling_rules.hpp"
#include "phantomcrash/slai_area_select.hpp"

#include <stdexcept>
#include <string>
#include <utility>

using namespace phantomcrash;

namespace {

DataRoots shipped() {
  return DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT);
}

// Seven cities, each holding one day match (its own id) at stage 0 every
// S.L.A.I. day; the calendar's day 0 opens New York, Las Vegas and Tokyo.
SlaiRumbleData hand_made() {
  SlaiRumbleData data;
  SlaiRumbleTable &table = data.table;
  table.cities = {"Tokyo", "Stuttgart", "New York", "London", "Hong Kong", "Cairo", "Las Vegas"};
  table.display_order = {2, 6, 0, 4, 5, 1, 3};
  table.schedule_days = 100;
  table.stages = 1;
  table.finals_stage = 99;
  for (int id = 0; id < 28; ++id) {
    SlaiRegulation rule;
    rule.id = id;
    rule.name = "Rule " + std::to_string(id);
    rule.entry_requirement = 2000;
    rule.target = 20;
    rule.prize = 3500;
    table.regulations.push_back(rule);
  }
  for (int city = 0; city < 7; ++city) {
    SlaiMatch match;
    match.id = city;
    match.regulation = 22 + city % 6;
    match.robots_min = 4;
    match.robots_max = 6;
    match.save_record = city;
    table.matches.push_back(match);
    table.city_days.push_back({{city, slai_no_match}});
  }
  table.day_rows.assign(100, {0, 1, 2, 3, 4, 5, 6});
  data.world = parse_slai_rumble_world(shipped().document(slai_rumble_world_file));
  data.world.ladder.tutorial_required = true; // the table holds stage 0 only
  data.rota.days = {{{2, 0}, {6, 0}, {0, 0}}, {{4, 1}, {5, 1}, {1, 1}}};
  return data;
}

// Each city's day and night stage with four gates on its map.
SlaiAreaMaps maps_of(const SlaiRumbleData &data) {
  SlaiAreaMaps maps;
  for (int city = 0; city < 7; ++city) {
    for (const bool night : {false, true}) {
      RumblingArea area;
      area.gates = {{100, 100}, {200, 100}, {100, 200}, {200, 200}};
      maps[slai_match_stage(data.world, city, night)] = area;
    }
  }
  return maps;
}

QuestSave riding_quest(const std::int64_t money) {
  QuestSave quest;
  quest.money = money;
  quest.garage.riding = 0;
  Hangar &hangar = quest.garage.hangars[0];
  hangar.owned = true;
  for (const PartSlot slot : {PartSlot::body, PartSlot::legs, PartSlot::chip}) {
    hangar.parts[static_cast<std::size_t>(slot)] = 0;
  }
  return quest;
}

SlaiAreaSelect select_with(const SlaiRumbleData &data, const QuestSave &quest,
                           const SlaiAreaMaps &maps) {
  auto script = load_slai_area_select(shipped(), "ui/slai_area_select.json");
  if (!script) {
    throw std::runtime_error("ui/slai_area_select.json is missing");
  }
  SlaiRobot robot;
  robot.ready = riding_ready(quest);
  robot.leg_type = 0;
  return SlaiAreaSelect(std::move(*script), data, maps, quest, robot, false);
}

InputFrame press(const Buttons buttons) {
  InputFrame input;
  input.pressed = buttons;
  input.held = buttons;
  return input;
}

constexpr double tick = 1.0 / 60.0;

void settle(SlaiAreaSelect &select, const double seconds = 1.0) {
  for (double time = 0.0; time < seconds; time += tick) {
    select.update(tick, InputFrame{});
  }
}

void tap(SlaiAreaSelect &select, const Buttons buttons) {
  select.update(tick, press(buttons));
  settle(select, 0.6);
}

} // namespace

TEST_CASE(
    "S.L.A.I.'s area select has a plate for each city and CLOSED for the rota's closed ones") {
  const SlaiRumbleData data = hand_made();
  const SlaiAreaMaps maps = maps_of(data);
  SlaiAreaSelect select = select_with(data, QuestSave{}, maps);
  const RumblingScript &rumbling = select.rumbling();
  REQUIRE(rumbling.areas.size() == 8U);
  CHECK(rumbling.areas.back().exit);
  // The table's display order: New York, Las Vegas, Tokyo, Hong Kong, ...
  CHECK(select.city(0) == 2);
  CHECK(select.city(1) == 6);
  CHECK(select.city(3) == 4);
  CHECK(rumbling.areas[0].item.id == "slai_area_plate_2");
  // Two rows: four plates above, three and EXIT below.
  CHECK(rumbling.areas[0].item.position[1] < rumbling.areas[4].item.position[1]);
  CHECK(rumbling.areas[4].item.position[1] == rumbling.areas[7].item.position[1]);
  // Day 0 opens New York, Las Vegas and Tokyo; Hong Kong is closed.
  CHECK(select.held(0));
  CHECK(select.held(2));
  CHECK(!select.held(3));
  CHECK(rumbling.areas[0].gates.size() == 4U);
  CHECK(rumbling.areas[3].gates.empty());
  // The picture frame is drawn smaller to make room for the upper row.
  const RumblingAreaSelect &style = rumbling.area_select;
  REQUIRE(style.picture_frame.size);
  CHECK(style.picture_frame.position[1] + (*style.picture_frame.size)[1] <=
        rumbling.areas[0].item.position[1]);
  CHECK(rumbling.areas[0].picture.image == "textures/slai/wired/new_york.png");
}

TEST_CASE("S.L.A.I.'s area select refuses a closed city and one short of money") {
  const SlaiRumbleData data = hand_made();
  const SlaiAreaMaps maps = maps_of(data);
  SlaiAreaSelect select = select_with(data, riding_quest(0), maps);
  settle(select);
  tap(select, select.rumbling().confirm_buttons);
  CHECK(select.view() == RumblingView::notice);
  CHECK(select.message() == "slai");
  tap(select, select.rumbling().confirm_buttons);
  settle(select);
  CHECK(select.view() == RumblingView::areas);
  for (int step = 0; step < 3; ++step) {
    tap(select, select.rumbling().right);
  }
  REQUIRE(select.area() == 3);
  tap(select, select.rumbling().confirm_buttons);
  CHECK(select.view() == RumblingView::areas);
  CHECK(!select.in_menu());
}

TEST_CASE("S.L.A.I.'s area select enters through the sub-menu at the chosen gate") {
  const SlaiRumbleData data = hand_made();
  const SlaiAreaMaps maps = maps_of(data);
  SlaiAreaSelect select = select_with(data, riding_quest(1'000'000), maps);
  settle(select);
  tap(select, select.rumbling().right);
  REQUIRE(select.area() == 1);
  tap(select, select.rumbling().confirm_buttons);
  settle(select);
  REQUIRE(select.view() == RumblingView::menu);
  CHECK(select.can_choose(RumblingAction::entry));
  tap(select, select.rumbling().confirm_buttons);
  REQUIRE(select.view() == RumblingView::gates);
  tap(select, select.rumbling().right);
  CHECK(select.gate() == 1);
  tap(select, select.rumbling().confirm_buttons);
  settle(select);
  const auto &entered = select.entry();
  REQUIRE(entered);
  CHECK(entered->match.city == 6);
  CHECK(entered->gate == 1);
  CHECK(select.done());
  CHECK(select.next() == "slai_battle");
}

TEST_CASE("S.L.A.I.'s area select leaves by EXIT for Club Wired") {
  const SlaiRumbleData data = hand_made();
  SlaiAreaSelect select = select_with(data, QuestSave{}, maps_of(data));
  settle(select);
  tap(select, select.rumbling().cancel_buttons);
  CHECK(select.area() == 7);
  tap(select, select.rumbling().confirm_buttons);
  settle(select);
  CHECK(select.done());
  CHECK(select.next() == "club_wired");
  CHECK(!select.entry());
}
