// S.L.A.I. Rumble against the installed tables/slai_rumble.json, with
// expected values from campaign.py run on the disc
// (port/tests/slai_rumble_expected.json, slai/rumble_fixture.py).

#include "test_support.hpp"

#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/slai_rumble_rules.hpp"

#include <filesystem>

using namespace phantomcrash;

namespace {

SlaiRumbleData installed() {
  const auto roots = DataRoots::configured(PHANTOMCRASH_SOURCE_ROOT, true);
  auto data = load_slai_rumble(roots);
  if (!data) {
    throw std::runtime_error("tables/slai_rumble.json is not installed; register the S.L.A.I. "
                             "assets again (register-slai-assets)");
  }
  return std::move(*data);
}

json::JsonValue expected() {
  return json::read_file(std::filesystem::path(PHANTOMCRASH_SOURCE_ROOT) / "port" / "tests" /
                         "slai_rumble_expected.json");
}

int integer(const json::JsonValue &value) {
  return static_cast<int>(value.number());
}

} // namespace

TEST_CASE("S.L.A.I. Rumble's rota lookup matches campaign.py for sample days and every slot") {
  const auto data = installed();
  const auto fixture = expected();
  const auto &days = fixture.find("days")->array();
  const auto &lookups = fixture.find("lookups")->array();
  REQUIRE(days.size() == lookups.size());
  int checked = 0;
  for (std::size_t d = 0; d < days.size(); ++d) {
    const int day = integer(days[d]);
    const auto &cities = lookups[d].array();
    REQUIRE(cities.size() == data.table.cities.size());
    for (std::size_t city = 0; city < cities.size(); ++city) {
      const auto &stages = cities[city].array();
      for (std::size_t stage = 0; stage < stages.size(); ++stage) {
        for (int slot = 0; slot < 2; ++slot) {
          const auto &by_finals = stages[stage].array()[static_cast<std::size_t>(slot)].array();
          for (std::size_t finals = 0; finals < by_finals.size(); ++finals) {
            CHECK(slai_scheduled_match(data.table, day, static_cast<int>(city),
                                       static_cast<int>(stage), slot == 1,
                                       static_cast<int>(finals)) == integer(by_finals[finals]));
            ++checked;
          }
        }
      }
    }
  }
  CHECK(checked == 10 * 7 * 5 * 2 * 4);
}

TEST_CASE("S.L.A.I. Rumble's loaded rows match campaign.py's decoders") {
  const auto data = installed();
  const auto fixture = expected();
  CHECK(data.table.schedule_days == 100);
  CHECK(data.table.stages == 5);
  CHECK(data.table.regulations.size() == 32);
  CHECK(data.table.matches.size() == 157);
  CHECK(data.table.city_days.size() == 700);
  CHECK((data.table.display_order == std::vector<int>{2, 6, 0, 4, 5, 1, 3}));
  for (const auto &row : fixture.find("matches")->array()) {
    const auto *match = slai_match(data.table, integer(*row.find("id")));
    REQUIRE(match != nullptr);
    CHECK(match->regulation == integer(*row.find("regulation")));
    CHECK(match->night == row.find("night")->boolean());
    const auto &robots = row.find("robots")->array();
    CHECK(match->robots_min == integer(robots[0]));
    CHECK(match->robots_max == integer(robots[1]));
    CHECK(match->save_record == integer(*row.find("save_record")));
    CHECK(match->save_record_b == integer(*row.find("save_record_b")));
  }
  for (const auto &row : fixture.find("regulations")->array()) {
    const auto *rule = slai_regulation(data.table, integer(*row.find("id")));
    REQUIRE(rule != nullptr);
    CHECK(rule->entry_requirement == integer(*row.find("entry_requirement")));
    const auto &maker = *row.find("maker");
    CHECK(rule->maker == (maker.is_null() ? slai_any_maker : integer(maker)));
    const auto &legs = row.find("leg_classes")->array();
    REQUIRE(rule->leg_classes.size() == legs.size());
    for (std::size_t k = 0; k < legs.size(); ++k) {
      CHECK(rule->leg_classes[k] == legs[k].string());
    }
    CHECK((rule->special == SlaiSpecialRule::none) == row.find("special")->is_null());
    const int param = integer(*row.find("param_20"));
    CHECK(rule->target == (param == 0xffff ? slai_no_target : param));
    CHECK(rule->prize == integer(*row.find("prize")));
  }
}

TEST_CASE("S.L.A.I. Rumble's stage ladder matches campaign.py's next_stage") {
  const auto ladder = installed().world.ladder;
  for (const auto &row : expected().find("ladder")->array()) {
    const auto &values = row.find("case")->array();
    SlaiRumbleSave save;
    save.stage = integer(values[0]);
    const auto clear = [&](const std::array<int, 2> &range, int count) {
      for (int record = range[0]; record < range[0] + count; ++record) {
        slai_record_beat(save, record, slai_leader_flag);
      }
    };
    clear(ladder.ranker_records, integer(values[1]));
    clear(ladder.regional_records, integer(values[2]));
    clear(ladder.finals_records, integer(values[3]));
    if (values[4].boolean()) {
      slai_record_beat(save, ladder.tutorial_record, slai_leader_flag);
    }
    CHECK(slai_next_stage(save, ladder) == integer(*row.find("next")));
  }
}

TEST_CASE("every city holds one rumble a day once the ranker stage opens") {
  const auto data = installed();
  for (int day = 0; day < data.table.schedule_days; ++day) {
    for (int city = 0; city < static_cast<int>(data.table.cities.size()); ++city) {
      const int held = (slai_scheduled_match(data.table, day, city, 1, false, 0) >= 0 ? 1 : 0) +
                       (slai_scheduled_match(data.table, day, city, 1, true, 0) >= 0 ? 1 : 0);
      CHECK(held == 1);
    }
  }
  // The tutorial record is match 0's, and the finals' records are the
  // ladder's: the world data names the table's records.
  const auto &ladder = data.world.ladder;
  CHECK(slai_match(data.table, 0)->save_record == ladder.tutorial_record);
  CHECK(slai_match(data.table, 2)->save_record == ladder.finals_records[0]);
  CHECK(slai_match(data.table, 4)->save_record == ladder.finals_records[1] - 1);
  CHECK(data.table.finals.size() == static_cast<std::size_t>(ladder.finals_needed));
  for (const auto &match : data.table.matches) {
    const auto robots = slai_match_robots(data.world, match);
    CHECK(robots.first <= robots.second);
    CHECK(robots.second <= data.world.balance.robots_max);
  }
}
