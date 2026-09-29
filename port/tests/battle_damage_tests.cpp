// The damage rules (0x0988e1): hits from behind, dodges, criticals that
// knock off weapons, knockback, and a homing missile's rounds one at a time.

#include "test_support.hpp"

#include <cmath>

#include "phantomcrash/battle.hpp"
#include "phantomcrash/json.hpp"

using namespace phantomcrash;

namespace {

constexpr float tick = 1.0F / 60.0F;
constexpr float full = 1000.0F;

BattleSettings damage_settings() {
  return parse_battle_settings(json::parse(R"({
    "arena": {"half_size": 100},
    "weapons": {
      "handgun": {"kind": "gun", "damage": 40, "cooldown": 5, "speed": 300},
      "breaker": {"kind": "gun", "damage": 40, "cooldown": 5, "speed": 300,
                  "knockdown_percent": 200},
      "pusher": {"kind": "gun", "damage": 1, "cooldown": 5, "speed": 300, "knockback": 30},
      "salvo": {"kind": "missile", "damage": 10, "cooldown": 5, "speed": 60, "rounds": 4,
                "turn_degrees_per_second": 360}
    }
  })"));
}

// A battle with the player at the origin, facing -z, firing `weapon`, and
// a target 30 ahead facing `heading`.
struct Range {
  Battle battle{damage_settings(), EnemyPath{}};
  int target{};

  Range(const std::string &weapon, const float heading) {
    battle.add_robot("player", 0, {}, 0.0F, {weapon, "", "", ""}, full);
    target = battle.add_robot("target", 1, Vec3{0.0F, 0.0F, -30.0F}, heading,
                              {"handgun", "handgun", "salvo", "salvo"}, full);
  }

  [[nodiscard]] const Combatant &victim() const {
    return battle.robots()[static_cast<std::size_t>(target)];
  }

  // Fires once and runs until the target is hit or a second has passed.
  void shoot() {
    FireControl fire;
    fire.fire[0] = true;
    battle.tick({}, fire, tick);
    for (int i = 0; i < 60 && victim().hp == full && victim().invulnerable == 0.0F; ++i) {
      battle.tick({}, {}, tick);
    }
  }
};

int lost_weapons(const Combatant &robot) {
  int count = 0;
  for (const bool lost : robot.lost) {
    count += lost ? 1 : 0;
  }
  return count;
}

} // namespace

TEST_CASE("a shot from behind does three times the damage") {
  Range facing("handgun", pi);
  facing.shoot();
  CHECK(std::fabs(facing.victim().hp - (full - 40.0F)) < 1e-3F);
  Range turned("handgun", 0.0F); // facing away, the way the shot flies
  turned.shoot();
  CHECK(std::fabs(turned.victim().hp - (full - 120.0F)) < 1e-3F);
}

TEST_CASE("a dodge misses and leaves the robot untouchable for a while") {
  Range range("handgun", pi);
  CombatStats nimble;
  nimble.dodge_chance = 1.0F;
  range.battle.set_combat(range.target, nimble);
  range.shoot();
  CHECK(range.victim().hp == full);
  CHECK(range.victim().invulnerable > 0.4F);
  for (int i = 0; i < 40; ++i) {
    range.battle.tick({}, {}, tick);
  }
  CHECK(range.victim().invulnerable == 0.0F);
}

TEST_CASE("a critical hit doubles the damage and may knock off a weapon") {
  Range range("breaker", pi);
  CombatStats sure;
  sure.critical_percent = 100.0F;
  range.battle.set_combat(0, sure);
  range.shoot();
  CHECK(std::fabs(range.victim().hp - (full - 80.0F)) < 1e-3F);
  CHECK(lost_weapons(range.victim()) == 1);

  Range resisted("breaker", pi);
  resisted.battle.set_combat(0, sure);
  CombatStats sturdy;
  sturdy.knockdown_resistance = 200.0F;
  resisted.battle.set_combat(resisted.target, sturdy);
  resisted.shoot();
  CHECK(lost_weapons(resisted.victim()) == 0);
}

TEST_CASE("knockback pushes the target away, less for resistant legs") {
  Range range("pusher", pi);
  range.shoot();
  CHECK(range.victim().push.z < -1.0F);
  for (int i = 0; i < 60; ++i) {
    range.battle.tick({}, {}, tick);
  }
  const float pushed = -30.0F - range.victim().mover.position().z;
  CHECK(pushed > 0.5F);
  CHECK(length(range.victim().push) < 0.1F); // it fades

  Range heavy("pusher", pi);
  CombatStats planted;
  planted.knockback_resistance = 100.0F;
  heavy.battle.set_combat(heavy.target, planted);
  heavy.shoot();
  CHECK(length(heavy.victim().push) == 0.0F);
}

TEST_CASE("a homing missile's rounds leave one every 10 frames") {
  Battle battle(damage_settings(), EnemyPath{});
  battle.add_robot("player", 0, {}, 0.0F, {"salvo", "", "", ""}, full);
  FireControl fire;
  fire.fire[0] = true;
  battle.tick({}, fire, tick);
  CHECK(battle.armory().projectiles().size() == 1);
  for (int i = 0; i < 5; ++i) {
    battle.tick({}, {}, tick);
  }
  CHECK(battle.armory().projectiles().size() == 1);
  for (int i = 0; i < 10; ++i) {
    battle.tick({}, {}, tick);
  }
  CHECK(battle.armory().projectiles().size() == 2);
  for (int i = 0; i < 25; ++i) {
    battle.tick({}, {}, tick);
  }
  CHECK(battle.armory().projectiles().size() == 4);
}

TEST_CASE("a stunned robot reels the way the hit pushes it") {
  Range facing("handgun", pi); // shot in the face: pushed back
  facing.shoot();
  CHECK(facing.victim().mover.mode() == MoveMode::hit);
  CHECK(facing.victim().mover.motion() == robot_motion::hit_back);
  Range turned("handgun", 0.0F); // shot in the back: pushed forward
  turned.shoot();
  CHECK(turned.victim().mover.motion() == robot_motion::hit_forward);
  Range side("handgun", pi * 0.5F); // the shot crosses it
  side.shoot();
  const int motion = side.victim().mover.motion();
  CHECK((motion == robot_motion::hit_left || motion == robot_motion::hit_right));
}
