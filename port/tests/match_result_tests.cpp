#include "test_support.hpp"
#include "phantomcrash/match_result.hpp"
#include "phantomcrash/robot_parts.hpp"
#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/sleep.hpp"

using namespace phantomcrash;

TEST_CASE("result replay has eight views and exits through next day to the garage") {
  const auto roots = DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT);
  const auto result = roots.document("ui/match_result.json");
  CHECK(result.string_or("exit_to", "") == "sleep");
  const auto &views = result.find("replay_settings")->find("views")->array();
  CHECK(views.size() == 8);
  CHECK(views[6].string() == "Free view");
  CHECK(views[7].string() == "Player View");
  const auto scenes = roots.document("story/scenes.json");
  CHECK(scenes.find("scenes")->find("match_result")->number() == 30);
  const auto sleep = parse_sleep_script(roots.document("ui/sleep.json"));
  CHECK(sleep.sound == "sleep");
  CHECK(sleep.exit_to == "garage");
}

namespace {
PartTables prices() {
  PartTables parts;
  parts.arm_weapons.resize(1);
  parts.arm_weapons[0].price.base = 2000;
  parts.bodies.resize(1);
  parts.bodies[0].price.base = 1200;
  parts.legs.resize(1);
  parts.legs[0].price.base = 1000;
  return parts;
}
MatchSummary entered() {
  MatchSummary match;
  match.outcome = MatchOutcome::retire;
  match.rental = true;
  match.robot.parts[static_cast<int>(PartSlot::body)] = RobotPart{0, 0};
  match.robot.parts[static_cast<int>(PartSlot::legs)] = RobotPart{0, 0};
  match.robot.parts[static_cast<int>(PartSlot::right_arm)] = RobotPart{0, 0};
  return match;
}
} // namespace

TEST_CASE("retirement receipt halves rental prize and charges equipped parts once") {
  const auto receipt = match_receipt(entered(), 20000, 18, prices(), {}, {});
  CHECK(receipt.prize == 9);
  CHECK(receipt.repairs[0] == 420);
  CHECK(receipt.repairs[1] == 0);
  CHECK(receipt.repairs[2] == 0);
  CHECK(receipt.repairs[3] == 0);
  CHECK(receipt.repairs[4] == 462);
  CHECK(receipt.repair_total == 882);
  CHECK(receipt.total_money == 19127);
  QuestSave quest;
  settle_match(receipt, quest);
  CHECK(quest.money == 19127);
  CHECK(quest.records.at("rumblings") == 1);
  CHECK(quest.records.at("total_prize") == 9);
  // Computing/rendering another receipt is pure: the save does not change.
  const auto again = match_receipt(entered(), 20000, 18, prices(), {}, {});
  CHECK(again.total_money == quest.money);
  CHECK(quest.records.at("rumblings") == 1);
}

TEST_CASE("surviving exit repairs only lost weapons and keeps the full owned prize") {
  auto match = entered();
  match.outcome = MatchOutcome::leave;
  match.rental = false;
  auto receipt = match_receipt(match, 20000, 19, prices(), {}, {});
  CHECK(receipt.total_money == 20019);
  CHECK(receipt.repair_total == 0);
  match.lost[0] = true;
  receipt = match_receipt(match, 20000, 19, prices(), {}, {});
  CHECK(receipt.repairs[0] == 420);
  CHECK(receipt.repairs[4] == 0);
  CHECK(receipt.total_money == 19599);
  QuestSave quest;
  settle_match(receipt, quest);
  CHECK(quest.records.at("returns") == 1);
  CHECK(quest.records.at("weapons_destroyed") == 1);
}

TEST_CASE("receipt caps the wallet and rounds rental awards in stored tens of NY") {
  auto match = entered();
  CHECK(match_receipt(match, 1, 19, prices(), {}, {}).total_money == 0);
  CHECK(match_receipt(match, 1, 19, prices(), {}, {}).prize == 9);
  match.outcome = MatchOutcome::win;
  CHECK(match_receipt(match, money_limit - 1, 19, prices(), {}, {}).total_money == money_limit);
}

TEST_CASE("repair combines base and current level prices before truncating") {
  auto parts = prices();
  parts.arm_weapons[0].price.top = 1000;
  auto match = entered();
  match.robot.parts[static_cast<int>(PartSlot::right_arm)]->level = 99;
  const auto receipt = match_receipt(match, 20000, 0, parts, {}, {});
  CHECK(receipt.repairs[0] == 430);
  const auto rules = parse_match_result_rules(
      *DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT).document("ui/match_result.json").find("rules"));
  CHECK(rules.base_repair_share == 0.2);
  CHECK(rules.level_repair_share == 0.01);
  CHECK(rules.rental_prize_divisor == 2);
}
