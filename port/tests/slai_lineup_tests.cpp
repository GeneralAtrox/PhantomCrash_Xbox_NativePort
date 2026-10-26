// S.L.A.I.'s line-ups against the installed tables/slai_rumble.json, with
// expected values from opponent_rules.py run on the disc
// (port/tests/slai_lineup_expected.json, slai/lineup_fixture.py), and the
// world's opponent settings.

#include "test_support.hpp"

#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/slai_battle_lineup.hpp"
#include "phantomcrash/slai_lineup.hpp"

#include <array>
#include <filesystem>

using namespace phantomcrash;

namespace {

SlaiRumbleData installed() {
  const auto roots = DataRoots::configured(PHANTOMCRASH_SOURCE_ROOT, true);
  auto data = load_slai_rumble(roots);
  if (!data || data->table.opponents.empty()) {
    throw std::runtime_error("tables/slai_rumble.json has no opponents; register the S.L.A.I. "
                             "assets again (register-slai-assets)");
  }
  return std::move(*data);
}

int integer(const json::JsonValue &value) {
  return static_cast<int>(value.number());
}

SlaiRumbleSave save_of(const json::JsonValue &row) {
  const auto &state = row.find("state")->array();
  SlaiRumbleRecord record;
  record.cleared = integer(state[0]) == 1;
  for (std::size_t flag = 0; flag < slai_record_flags; ++flag) {
    record.beaten[flag] = integer(state[flag + 1]) == 1;
  }
  SlaiRumbleSave save;
  for (const auto &id : row.find("records")->array()) {
    save.records[integer(id)] = record;
  }
  return save;
}

} // namespace

TEST_CASE("S.L.A.I.'s candidate lists and line-ups match opponent_rules.py") {
  const auto data = installed();
  const auto fixture = json::read_file(std::filesystem::path(PHANTOMCRASH_SOURCE_ROOT) / "port" /
                                       "tests" / "slai_lineup_expected.json");
  int cases = 0;
  for (const auto &row : fixture.find("cases")->array()) {
    const SlaiMatch *match = slai_match(data.table, integer(*row.find("match")));
    REQUIRE(match != nullptr);
    const SlaiRumbleSave save = save_of(row);
    const SlaiCandidates found = slai_candidates(data, *match, save);
    CHECK(static_cast<int>(found.status) == integer(*row.find("status")));
    const auto &lists = row.find("lists")->array();
    REQUIRE(lists.size() == slai_lists);
    for (std::size_t list = 0; list < slai_lists; ++list) {
      const auto &want = lists[list].array();
      const auto &got = found.lists[list];
      REQUIRE(got.size() == want.size());
      for (std::size_t k = 0; k < want.size(); ++k) {
        const auto &entry = want[k].array();
        CHECK((got[k] == SlaiCandidate{integer(entry[0]), integer(entry[1]), integer(entry[2])}));
      }
    }
    for (const auto &draw_row : row.find("draws")->array()) {
      const auto &values = draw_row.find("values")->array();
      std::size_t used = 0;
      // Past the scripted values reads 0 and counts on, failing the check below.
      const auto share = [&] {
        const std::size_t at = used++;
        return at < values.size() ? static_cast<float>(values[at].number()) : 0.0F;
      };
      SlaiLineupDraw draw(data, *match, save);
      std::vector<std::array<int, 5>> lineup;
      for (int place = 1; place <= slai_battle_places; ++place) {
        if (const auto drawn = draw.draw(place, share)) {
          lineup.push_back({place, static_cast<int>(drawn->list), drawn->candidate.opponent,
                            drawn->candidate.record, drawn->candidate.flag});
        }
      }
      const auto &want = draw_row.find("lineup")->array();
      REQUIRE(lineup.size() == want.size());
      for (std::size_t k = 0; k < want.size(); ++k) {
        for (std::size_t field = 0; field < 5; ++field) {
          CHECK(lineup[k][field] == integer(want[k].array()[field]));
        }
      }
      CHECK(used == static_cast<std::size_t>(integer(*draw_row.find("used"))));
    }
    ++cases;
  }
  CHECK(cases == 48);
}

TEST_CASE("an S.L.A.I. place shot down draws again and records the beaten flag") {
  const auto data = installed();
  const SlaiMatch *match = slai_match(data.table, 0);
  REQUIRE(match != nullptr);
  SlaiLineupDraw draw(data, *match, {});
  const auto first = [] { return 0.0F; };
  const auto drawn = draw.draw(1, first);
  REQUIRE(drawn.has_value());
  CHECK(!draw.draw(1, first).has_value()); // the place is taken
  draw.leave(1, true);
  REQUIRE(draw.beaten().size() == 1);
  CHECK(draw.beaten()[0] == std::make_pair(drawn->candidate.record, drawn->candidate.flag));
  const auto next = draw.draw(1, first);
  REQUIRE(next.has_value());
  CHECK(next->candidate.flag != drawn->candidate.flag); // now beaten, behind the unbeaten
  draw.leave(1, false);
  CHECK(draw.beaten().size() == 1); // not the player's shootdown
}

TEST_CASE("S.L.A.I. opponents take the chip their level reaches and their behaviour's profile") {
  SlaiOpponentRules rules;
  rules.chips = {{0, 95, 0}, {12, 94, 0}, {25, 93, 0}, {40, 92, 0}, {55, 91, 0}};
  rules.profiles = {{3, 7}};
  CHECK(slai_opponent_chip(rules, 1)->chip == 95);
  CHECK(slai_opponent_chip(rules, 12)->chip == 94);
  CHECK(slai_opponent_chip(rules, 39)->chip == 93);
  CHECK(slai_opponent_chip(rules, 65)->chip == 91);
  CHECK(slai_opponent_profile(rules, 3) == 7);
  CHECK(slai_opponent_profile(rules, 0) == 4);
  SlaiOpponent opponent;
  opponent.level = 30;
  opponent.robot.paint = 2;
  rules.emblem = 7;
  const RobotRecord robot = slai_opponent_robot(rules, opponent);
  REQUIRE(robot.part(PartSlot::chip).has_value());
  CHECK(robot.part(PartSlot::chip)->id == 93);
  CHECK(robot.paint == 2);
  CHECK(robot.emblem == 7);
}

TEST_CASE("the shipped S.L.A.I. world fields opponents as the original") {
  const auto data = installed();
  const SlaiOpponentRules &rules = data.world.opponents;
  CHECK((rules.always_eligible_makers == std::vector<int>{8, 9}));
  CHECK(rules.story_flags.empty());
  CHECK(rules.leader_session);
  CHECK(!rules.leader_counter);
  CHECK(rules.chips.size() == 5);
  CHECK(rules.arrival_seconds > 0.0F);
  CHECK(!rules.pool_flags);
}

TEST_CASE("the save's story flags decide S.L.A.I.'s substitutions as the world's do") {
  SlaiRumbleData data = installed();
  const SlaiOpponentTable &table = data.table.opponents;
  REQUIRE(!table.substitutions.empty());
  // Row 0 stands in while flag 0x16 is clear (0x256cc0).
  const SlaiSubstitution &first = table.substitutions[0];
  REQUIRE(!first.ids.empty());
  CHECK(slai_substitute(table, first.ids[0], {}) == first.replacement);
  CHECK(slai_substitute(table, first.ids[0], {0x16}) == first.ids[0]);
  const std::vector<int> story{0x16, 0x24, 0x25};
  for (const int id : {0, 9, 42, 126, 130}) {
    const SlaiMatch *match = slai_match(data.table, id);
    REQUIRE(match != nullptr);
    const SlaiCandidates by_save = slai_candidates(data, *match, {}, story);
    data.world.opponents.story_flags = story;
    const SlaiCandidates by_world = slai_candidates(data, *match, {});
    data.world.opponents.story_flags.clear();
    for (std::size_t list = 0; list < slai_lists; ++list) {
      CHECK(by_save.lists[list] == by_world.lists[list]);
    }
  }
}
