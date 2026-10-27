// S.L.A.I.'s story counts: the nights, the sponsors after a battle and the
// completion score (slai_story_rules.hpp).

#include "test_support.hpp"

#include "phantomcrash/slai_story_rules.hpp"

using namespace phantomcrash;

namespace {

// The shipped rules, cut down (tables/slai_talk.json).
SlaiStoryRules small_rules() {
  return slai_story_rules_from_json(json::parse(R"({
    "sponsor_rules": {"offer_flags": [107], "offer_score_above": 14,
      "regulation_makers": {"6": 1, "10": 0}, "brand_regulations": [11],
      "brand_makers": {"2": 0, "5": 1}, "keep_score_below": 10, "streak_limit": 2,
      "sponsor_brands": {"0": [2], "1": [5, 1]}, "head_to_head_matches": [2]},
    "completion_score": {"divisor": 10, "parts": [
      {"source": "records", "first": 48, "count": 4, "points": 400},
      {"source": "music", "count": 90, "points": 480},
      {"source": "sponsors", "count": 5, "points": 500}]},
    "overnight_flags": [[27, 28]],
    "defeat_milestones": {"defeated_over_99": {"above": 99, "superseded_by": 131, "marks": 130}}
  })"));
}

SlaiBattleOutcome battle(int regulation, int score, bool survived, int brand) {
  SlaiBattleOutcome outcome;
  outcome.match = 10;
  outcome.regulation = regulation;
  outcome.score = score;
  outcome.shootdowns = score / 10;
  outcome.survived = survived;
  outcome.brand = brand;
  return outcome;
}

} // namespace

TEST_CASE("S.L.A.I. story counts round-trip through the save") {
  SlaiStorySave save;
  save.flags[0] = true;
  save.flags[255] = true;
  save.days = 12;
  save.nights_seen = 40;
  save.defeated = 600;
  save.sponsor = 3;
  save.previous_sponsor = 1;
  save.sponsors_had = 0b1010;
  save.shot_down_streak = 1;
  save.dismissal = 2;
  save.survived = true;
  CHECK(slai_story_from_json(slai_story_to_json(save)) == save);
  CHECK(slai_story_from_json(json::parse(R"({"flags": [3, 999, -1]})")).flags[3]);
  CHECK(slai_story_from_json(json::parse(R"({"sponsor": 9})")).sponsor == slai_no_sponsor);
  CHECK(slai_story_from_json(json::parse("null")) == SlaiStorySave{});
}

TEST_CASE("The story's days follow the port's nights and set the overnight flags") {
  const auto rules = small_rules();
  SlaiStorySave save;
  slai_story_nights(save, rules, 5); // the first look only notes the count
  CHECK(save.days == 0);
  CHECK(save.nights_seen == 5);
  slai_set_story_flag(save, 27);
  slai_story_nights(save, rules, 7);
  CHECK(save.days == 2);
  CHECK(slai_story_flag(save, 28));
  CHECK(rules.milestones.at("defeated_over_99").marks == 130);
}

TEST_CASE("A good score in a maker's rumble brings its offer; the sponsor keeps its brands") {
  const auto rules = small_rules();
  SlaiStorySave save;
  slai_story_after_battle(save, rules, battle(6, 20, true, 1));
  CHECK(save.sponsor == 1);
  CHECK(save.previous_sponsor == slai_no_sponsor);
  CHECK(save.sponsors_had == 0b10);
  CHECK(save.defeated == 2);
  // A body of another brand: the sponsor leaves (reason 1).
  slai_story_after_battle(save, rules, battle(3, 12, true, 2));
  CHECK(save.sponsor == slai_no_sponsor);
  CHECK(save.previous_sponsor == 1);
  CHECK(save.dismissal == 1);
  // A body-type rumble's offer follows the body's brand.
  slai_story_after_battle(save, rules, battle(11, 15, true, 2));
  CHECK(save.sponsor == 0);
  CHECK(save.sponsors_had == 0b11);
}

TEST_CASE("A sponsor leaves on a low score or after being shot down twice") {
  const auto rules = small_rules();
  SlaiStorySave save;
  save.sponsor = 0;
  slai_story_after_battle(save, rules, battle(3, 9, true, 2));
  CHECK(save.sponsor == slai_no_sponsor);
  CHECK(save.dismissal == 3);
  save.sponsor = 0;
  slai_story_after_battle(save, rules, battle(3, 12, false, 2));
  CHECK(save.sponsor == 0);
  CHECK(save.shot_down_streak == 1);
  slai_story_after_battle(save, rules, battle(3, 12, false, 2));
  CHECK(save.sponsor == slai_no_sponsor);
  CHECK(save.dismissal == 2);
  // A head-to-head match only checks the body.
  save.sponsor = 0;
  save.shot_down_streak = 0;
  auto duel = battle(3, 0, false, 2);
  duel.match = 2;
  slai_story_after_battle(save, rules, duel);
  CHECK(save.sponsor == 0);
  CHECK(save.shot_down_streak == 0);
  // An excluded battle leaves the sponsors alone.
  auto excluded = battle(3, 0, false, 7);
  excluded.excluded = true;
  slai_story_after_battle(save, rules, excluded);
  CHECK(save.sponsor == 0);
}

TEST_CASE("The completion score scales each part and rounds half up") {
  const auto rules = small_rules();
  SlaiStorySave save;
  SlaiRumbleSave rumble;
  rumble.records[48].cleared = true;
  rumble.records[51].cleared = true;
  rumble.records[52].cleared = true; // past the part's records
  SlaiCollections collections;
  collections.music = 45;
  save.sponsors_had = 0b111;
  // (200 + 240 + 300) / 10
  CHECK(slai_completion_score(rules, save, rumble, collections) == 74);
  collections.music = 46; // 245.33: 745 rounds up to 75
  CHECK(slai_completion_score(rules, save, rumble, collections) == 75);
}
