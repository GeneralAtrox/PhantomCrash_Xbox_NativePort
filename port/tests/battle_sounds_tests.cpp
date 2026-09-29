// The battle's sounds (battle_sounds.hpp): the weapons' rules, the robots'
// own sounds and their placing.

#include "test_support.hpp"

#include "phantomcrash/battle_sounds.hpp"
#include "phantomcrash/json.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace phantomcrash;

namespace {

constexpr float tick = 1.0F / 60.0F;

BattleSoundTable test_table() {
  return parse_battle_sounds(json::parse(R"({
    "robot": {
      "footstep": {"player": "p_step", "computer": "c_step"},
      "boost_start": {"player": "p_boost_start"},
      "boost": {"player": "p_boost"},
      "shot_down": {"sound": "down"}
    },
    "footstep_seconds": 0.25,
    "fire": [
      {"style": 0, "mount": "shoulder", "models": [1, 11], "player": "p_shoulder", "computer": "c_shoulder"},
      {"style": 2, "hud_kind": 3, "player": "p_grenade"},
      {"style": 2, "player": "p_missile"},
      {"style": 0, "player": "p_gun"}
    ],
    "fire_fallback": {"player": "p_plain", "computer": "c_plain"},
    "impact": [{"hud_kind": 5}, {"sound": "impact"}]
  })"));
}

BattleSettings test_settings() {
  return parse_battle_settings(json::parse(R"({
    "arena": {"half_size": 100},
    "weapons": {"handgun": {"kind": "gun", "damage": 40, "cooldown": 0.1, "speed": 300}}
  })"));
}

std::vector<std::string> names(const std::vector<PlacedCue> &cues, SoundCue::Action action) {
  std::vector<std::string> out;
  for (const PlacedCue &placed : cues) {
    if (placed.cue.action == action) {
      out.push_back(placed.cue.name);
    }
  }
  return out;
}

bool has(const std::vector<std::string> &list, const std::string &name) {
  return std::ranges::find(list, name) != list.end();
}

} // namespace

TEST_CASE("a weapon's fire sound is the first rule its record matches") {
  const BattleSoundTable table = test_table();
  WeaponTuning weapon;
  weapon.fire_sound = 0;
  weapon.model = 11;
  const int shoulder = static_cast<int>(WeaponSlot::left_shoulder);
  const int arm = static_cast<int>(WeaponSlot::right_arm);
  CHECK(weapon_sound(table.fire, weapon, shoulder)->player == "p_shoulder");
  CHECK(weapon_sound(table.fire, weapon, arm)->player == "p_gun"); // the mount differs
  weapon.fire_sound = 2;
  weapon.hud_kind = hud_kind::grenade;
  CHECK(weapon_sound(table.fire, weapon, arm)->player == "p_grenade");
  weapon.hud_kind = hud_kind::missile;
  CHECK(weapon_sound(table.fire, weapon, arm)->player == "p_missile");
  weapon.fire_sound = 5;
  CHECK(weapon_sound(table.fire, weapon, arm) == nullptr);
  weapon.hud_kind = hud_kind::beam;
  CHECK(weapon_sound(table.impact, weapon, -1)->player.empty()); // a beam lands silently
}

TEST_CASE("a battle sounds its shots where they leave and land, and a walk's steps") {
  Battle battle(test_settings(), EnemyPath{});
  battle.add_robot("player", 0, {}, 0.0F, {"handgun", "", "", ""}, 100.0F);
  const int target = battle.add_robot("target", 1, {0.0F, 0.0F, -30.0F}, 0.0F, {}, 1000.0F);
  BattleSounds sounds(test_table());
  FireControl fire;
  fire.fire[0] = true;
  std::vector<PlacedCue> heard;
  for (int i = 0; i < 20; ++i) {
    battle.tick({}, fire, tick);
    sounds.observe(battle, tick);
    std::ranges::move(sounds.take(), std::back_inserter(heard));
  }
  const auto played = names(heard, SoundCue::Action::play);
  CHECK(has(played, "p_plain")); // a weapon without a record: the fallback, the player's side
  CHECK(has(played, "impact"));
  const auto shot = std::ranges::find(heard, std::string("p_plain"),
                                      [](const PlacedCue &placed) { return placed.cue.name; });
  CHECK(length(shot->at - battle.muzzle(battle.player(), WeaponSlot::right_arm)) < 1.0F);
  CHECK(battle.robots()[static_cast<std::size_t>(target)].hp < 1000.0F);

  heard.clear();
  RobotControl walk;
  walk.move = {0.0F, 0.7F}; // at the dash's threshold: still a walk
  for (int i = 0; i < 120; ++i) { // two seconds at 0.7 of the walk: 5.6 steps
    battle.tick(walk, {}, tick);
    sounds.observe(battle, tick);
    std::ranges::move(sounds.take(), std::back_inserter(heard));
  }
  CHECK(battle.player().mover.mode() == MoveMode::walk);
  const auto steps = std::ranges::count(names(heard, SoundCue::Action::play), "p_step");
  CHECK(steps >= 5);
  CHECK(steps <= 6);
}

TEST_CASE("a dash starts its sound, loops another with the robot and stops it") {
  Battle battle(test_settings(), EnemyPath{});
  battle.add_robot("player", 0, {}, 0.0F, {}, 100.0F);
  BattleSounds sounds(test_table());
  RobotControl dash;
  dash.move = {0.0F, 1.0F};
  dash.dash = true;
  battle.tick(dash, {}, tick);
  sounds.observe(battle, tick);
  std::vector<PlacedCue> heard = sounds.take();
  REQUIRE(battle.player().mover.mode() == MoveMode::dash);
  CHECK(has(names(heard, SoundCue::Action::play), "p_boost_start"));
  const auto loop = std::ranges::find(heard, std::string("p_boost"),
                                      [](const PlacedCue &placed) { return placed.cue.name; });
  REQUIRE(loop != heard.end());
  CHECK(loop->cue.loop);
  const std::uint64_t key = loop->cue.key;
  CHECK(key != 0);
  dash.dash = false;
  battle.tick(dash, {}, tick);
  sounds.observe(battle, tick);
  heard = sounds.take();
  CHECK(std::ranges::any_of(heard, [&](const PlacedCue &placed) {
    return placed.cue.action == SoundCue::Action::move && placed.cue.key == key;
  }));
  battle.tick({}, {}, tick);
  sounds.observe(battle, tick);
  heard = sounds.take();
  CHECK(std::ranges::any_of(heard, [&](const PlacedCue &placed) {
    return placed.cue.action == SoundCue::Action::stop && placed.cue.key == key;
  }));
}

TEST_CASE("placed cues fade with the camera's distance; the split screen hears them whole") {
  std::vector<PlacedCue> cues{{SoundCue{SoundCue::Action::play, "far"}, {0.0F, 0.0F, -200.0F}}};
  const SoundListener listener;
  CHECK(place_cues(SoundFalloff{}, &listener, cues).front().mix.gain < 0.1F);
  CHECK(place_cues(SoundFalloff{}, nullptr, cues).front().mix.gain == 1.0F);
}
