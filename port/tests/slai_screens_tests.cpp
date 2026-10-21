// The port's S.L.A.I. screens on hand-made tables: the schedule's S.L.A.I.
// page and its toggle, and Rumble S.L.A.I. (slai_rumble_hub.hpp).

#include "test_support.hpp"

#include "phantomcrash/data_roots.hpp"
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

SlaiRumbleHub hub_with(const SlaiRumbleData &data, const QuestSave &quest) {
  const DataRoots roots = shipped();
  return SlaiRumbleHub(parse_slai_rumble_hub(roots.document("ui/slai_rumble.json")), data, quest,
                       "club_wired", parse_message_box(roots.document("ui/message_box.json")));
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
