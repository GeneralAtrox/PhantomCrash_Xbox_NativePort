// The port's S.L.A.I. screens on hand-made tables: the schedule's S.L.A.I.
// page and its toggle, and Rumble S.L.A.I. (slai_rumble_hub.hpp).

#include "test_support.hpp"

#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/rumbling_rules.hpp"
#include "phantomcrash/schedule.hpp"
#include "phantomcrash/slai_rumble_hub.hpp"

#include <string>

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

InputFrame press(const Buttons buttons) {
  InputFrame input;
  input.pressed = buttons;
  input.held = buttons;
  return input;
}

constexpr double tick = 1.0 / 60.0;

template <class Screen> void settle(Screen &screen, const double seconds = 1.0) {
  for (double time = 0.0; time < seconds; time += tick) {
    screen.update(tick, InputFrame{});
  }
}

template <class Screen> void tap(Screen &screen, const Buttons buttons) {
  screen.update(tick, press(buttons));
  settle(screen, 0.3);
}

Schedule schedule_with(const SlaiRumbleData *slai, const QuestSave &quest = {}) {
  const DataRoots roots = shipped();
  return Schedule(parse_schedule(roots.document("ui/schedule.json")), ContestTables{}, quest,
                  "garage", "club_wired", parse_message_box(roots.document("ui/message_box.json")),
                  slai);
}

// The ridden robot as slai_entry_robot reads it: ready when one is ridden,
// on legs, with no maker of S.L.A.I.'s.
SlaiRobot robot_of(const QuestSave &quest) {
  SlaiRobot robot;
  robot.ready = riding_ready(quest);
  robot.leg_type = 0;
  return robot;
}

SlaiRumbleHub hub_with(const SlaiRumbleData &data, const QuestSave &quest,
                       const std::optional<SlaiRobot> &robot = std::nullopt,
                       const bool rentals = false,
                       const std::string &file = "ui/slai_rumble.json") {
  const DataRoots roots = shipped();
  return SlaiRumbleHub(parse_slai_rumble_hub(roots.document(file)), data, quest,
                       robot.value_or(robot_of(quest)), rentals, "club_wired",
                       parse_message_box(roots.document("ui/message_box.json")));
}

// Confirms the first row's rumble twice: its INFORMATION, then entry.
template <class Hub> bool try_first(Hub &hub) {
  settle(hub);
  tap(hub, hub.script().confirm_buttons);
  settle(hub);
  tap(hub, hub.script().confirm_buttons);
  return hub.entered().has_value();
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

} // namespace

TEST_CASE("the schedule offers its S.L.A.I. toggle only with S.L.A.I.'s data") {
  const Schedule plain = schedule_with(nullptr);
  CHECK(!plain.toggle_offered());
  CHECK(plain.button_row() == 3);

  const SlaiRumbleData data = hand_made();
  Schedule schedule = schedule_with(&data);
  REQUIRE(schedule.toggle_offered());
  settle(schedule);
  const Buttons cancel = schedule.script().cancel_buttons;
  const Buttons confirm = schedule.script().confirm_buttons;
  tap(schedule, cancel);
  CHECK(schedule.row() == schedule.button_row());
  CHECK(schedule.button() == Schedule::exit_button);
  // The buttons stand SKIP TO DAY, the toggle, EXIT from the left.
  tap(schedule, schedule.script().left);
  CHECK(schedule.button() == Schedule::toggle_button);
  tap(schedule, confirm);
  CHECK(schedule.slai_page());
  CHECK(schedule.rows() == 7);
  CHECK(schedule.row() == schedule.button_row());
  CHECK(schedule.button() == Schedule::toggle_button);
  CHECK(!schedule.done());
  tap(schedule, confirm);
  CHECK(!schedule.slai_page());
  CHECK(schedule.rows() == 3);
}

TEST_CASE("the S.L.A.I. page shows the rota's cities in the table's display order") {
  const SlaiRumbleData data = hand_made();
  const Schedule schedule = schedule_with(&data);
  CHECK(schedule.slai_city(0) == 2);
  CHECK(schedule.slai_city(1) == 6);
  // Day 0 opens New York, Las Vegas and Tokyo; day 1 none of them.
  const auto new_york = schedule.slai_match_at(0, 0);
  REQUIRE(new_york);
  CHECK(new_york->match == 2);
  CHECK(schedule.slai_match_at(0, 2));
  CHECK(!schedule.slai_match_at(0, 3));
  CHECK(!schedule.slai_match_at(1, 0));
  CHECK(schedule.slai_match_at(1, 3));
}

TEST_CASE("Rumble S.L.A.I. lists today's cities and leaves by EXIT") {
  const SlaiRumbleData data = hand_made();
  SlaiRumbleHub hub = hub_with(data, QuestSave{});
  REQUIRE(hub.matches().size() == 3);
  CHECK(hub.matches()[0].city == 2);
  CHECK(hub.matches()[2].city == 0);
  CHECK(hub.exit_row() == 3);
  // Each city has its Wired sheet's emblem and DAY and NIGHT labels.
  REQUIRE(hub.script().city_art.size() == 7U);
  CHECK(hub.script().city_art[2].image == "textures/slai/wired/new_york.png");
  CHECK(hub.script().city_art[2].night[3] <= hub.script().city_art[2].day[1]);
  settle(hub);
  tap(hub, hub.script().down);
  CHECK(hub.row() == 1);
  tap(hub, hub.script().cancel_buttons);
  CHECK(hub.row() == hub.exit_row());
  CHECK(hub.selected() == nullptr);
  tap(hub, hub.script().confirm_buttons);
  settle(hub);
  CHECK(hub.done());
  CHECK(hub.next() == "club_wired");
  CHECK(!hub.entered());
}

TEST_CASE("Battle School lists each lesson once and Rumble S.L.A.I. leaves them out") {
  SlaiRumbleData data = hand_made();
  // Tokyo's match is the tutorial (record 24), and Las Vegas holds it too.
  data.table.matches[0].save_record = 24;
  data.table.city_days[6] = {{0, slai_no_match}};
  const SlaiRumbleHub school =
      hub_with(data, QuestSave{}, std::nullopt, false, "ui/slai_battle_school.json");
  REQUIRE(school.matches().size() == 1);
  CHECK(school.matches()[0].match == 0);
  CHECK(school.matches()[0].city == 6); // the first in display order
  CHECK(school.script().exit_to == "town_map");
  const SlaiRumbleHub rumble = hub_with(data, QuestSave{});
  REQUIRE(rumble.matches().size() == 1);
  CHECK(rumble.matches()[0].city == 2);
}

TEST_CASE("before the tutorial, Rumble S.L.A.I. sends the player to Battle School") {
  SlaiRumbleData data = hand_made();
  // Stage 0: every open city's rumble is the tutorial (record 24).
  data.table.matches[0].save_record = 24;
  for (const int city : {2, 6, 0}) {
    data.table.city_days[static_cast<std::size_t>(city)] = {{0, slai_no_match}};
  }
  const SlaiRumbleHub rumble = hub_with(data, QuestSave{});
  CHECK(rumble.matches().empty());
  CHECK(rumble.only_excluded());
  CHECK(!rumble.script().excluded.empty());
  const SlaiRumbleHub school =
      hub_with(data, QuestSave{}, std::nullopt, false, "ui/slai_battle_school.json");
  CHECK(!school.only_excluded());
  // The tutorial's regulations are named for it, not the disc's "Dummy".
  const auto &names = data.world.regulation_names;
  REQUIRE(names.size() == 2);
  CHECK(names[0].first == 0);
  CHECK(names[1].first == 1);
  CHECK(names[0].second.starts_with("TUTORIAL"));
}

TEST_CASE("an optional tutorial opens stage 1's rumbles and stays at Battle School") {
  SlaiRumbleData data = hand_made();
  data.world.ladder.tutorial_required = false;
  // Stage 0 is Tokyo's tutorial (record 24); stage 1 a ranker rumble each.
  data.table.stages = 2;
  data.table.matches[0].save_record = 24;
  for (int city = 0; city < 7; ++city) {
    SlaiMatch match = data.table.matches[static_cast<std::size_t>(city)];
    match.id = 7 + city;
    match.save_record = data.world.ladder.ranker_records[0] + city;
    data.table.matches.push_back(match);
    data.table.city_days[static_cast<std::size_t>(city)].push_back({7 + city, slai_no_match});
  }
  QuestSave quest;
  const SlaiRumbleHub rumble = hub_with(data, quest);
  REQUIRE(rumble.matches().size() == 3);
  CHECK(rumble.matches()[0].match == 9); // New York's, at stage 1
  const SlaiRumbleHub school =
      hub_with(data, quest, std::nullopt, false, "ui/slai_battle_school.json");
  REQUIRE(school.matches().size() == 1);
  CHECK(school.matches()[0].match == 0);
  // A battle moves the stored stage on as stage 1's.
  CHECK(slai_next_stage(quest.slai_rumble, data.world.ladder) == 1);
  quest.slai_rumble.records[24].cleared = true;
  CHECK(hub_with(data, quest, std::nullopt, false, "ui/slai_battle_school.json").matches().empty());
  // The original's rule keeps every rumble the tutorial until it is cleared.
  data.world.ladder.tutorial_required = true;
  CHECK(slai_ladder_stage(SlaiRumbleSave{}, data.world.ladder) == 0);
}

TEST_CASE("Rumble S.L.A.I. refuses entry short of money or without a ready robot") {
  const SlaiRumbleData data = hand_made();
  const std::int64_t needed = slai_entry_requirement(
      data.world, *slai_regulation(data.table, data.table.matches[2].regulation));
  for (const QuestSave &quest : {riding_quest(needed - 1), QuestSave{}}) {
    QuestSave held = quest;
    held.money = quest.garage.riding < 0 ? needed : quest.money;
    SlaiRumbleHub hub = hub_with(data, held);
    settle(hub);
    tap(hub, hub.script().confirm_buttons);
    REQUIRE(hub.popup().is_open());
    settle(hub);
    tap(hub, hub.script().confirm_buttons);
    CHECK(hub.box().is_open());
    CHECK(!hub.entered());
    settle(hub);
    CHECK(!hub.done());
  }
}

TEST_CASE("Rumble S.L.A.I. enters the rumble under the cursor and leaves for its battle") {
  const SlaiRumbleData data = hand_made();
  SlaiRumbleHub hub = hub_with(data, riding_quest(1'000'000));
  settle(hub);
  tap(hub, hub.script().down);
  tap(hub, hub.script().confirm_buttons);
  settle(hub);
  tap(hub, hub.script().confirm_buttons);
  const auto entered = hub.entered();
  REQUIRE(entered);
  CHECK(entered->city == 6);
  settle(hub);
  CHECK(hub.done());
  CHECK(hub.next() == "slai_battle");
}

TEST_CASE("Rumble S.L.A.I. refuses a robot the regulation's maker or special rule shuts out") {
  SlaiRumbleData data = hand_made();
  SlaiRegulation &rule = data.table.regulations[24]; // New York's, the first row
  rule.maker = 2;
  const QuestSave quest = riding_quest(1'000'000);
  SlaiRumbleHub other = hub_with(data, quest);
  CHECK(!try_first(other));
  CHECK(other.box().is_open());
  SlaiRobot hartman = robot_of(quest);
  hartman.maker = 2;
  SlaiRumbleHub fitting = hub_with(data, quest, hartman);
  CHECK(try_first(fitting));

  rule.maker = slai_any_maker;
  rule.special = SlaiSpecialRule::shoulder_weapons_only;
  SlaiRobot armed = robot_of(quest);
  armed.arms[0] = SlaiWeapon{};
  SlaiRumbleHub refused = hub_with(data, quest, armed);
  CHECK(!try_first(refused));
  CHECK(refused.box().is_open());
}

TEST_CASE("a rental-only S.L.A.I. rumble lends a robot, given the money and a rental") {
  SlaiRumbleData data = hand_made();
  data.table.regulations[24].special = SlaiSpecialRule::rental_only;
  QuestSave walking;
  walking.money = 1'000'000; // no robot ridden
  SlaiRumbleHub lent = hub_with(data, walking, std::nullopt, true);
  CHECK(try_first(lent));
  SlaiRumbleHub none = hub_with(data, walking, std::nullopt, false);
  CHECK(!try_first(none));
  CHECK(none.box().is_open());
  walking.money = 0;
  SlaiRumbleHub poor = hub_with(data, walking, std::nullopt, true);
  CHECK(!try_first(poor));
}
