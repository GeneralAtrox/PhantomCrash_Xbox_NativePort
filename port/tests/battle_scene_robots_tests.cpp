// Computer robots' entrances (robot state 1) and the boss's (state 5): the
// quicker walk out of a gate, the waits for busy doors and for the other
// robots' scenes, the doors they open, the boss's shot and the entry shield;
// and their exits.

#include "test_support.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "phantomcrash/battle_chatter.hpp"
#include "phantomcrash/battle_scene.hpp"
#include "phantomcrash/json.hpp"

using namespace phantomcrash;

namespace {

constexpr float tick = 1.0F / 30.0F;
const Vec3 first_gate{0.0F, 0.0F, 50.0F};
const Vec3 second_gate{0.0F, 0.0F, -50.0F};

MotionChannel constant(const float value) {
  MotionChannel channel;
  channel.values = {value};
  return channel;
}

// Every entrance walks 15 units forward between frames 119 and 359 of 540;
// gate 0 faces -z and gate 1 +z.
BattleSceneTables test_tables() {
  SceneTrack entrance;
  entrance.frames = 540.0F;
  entrance.relative = true;
  entrance.camera.roll = constant(0.0F);
  entrance.camera.fov = constant(40.0F);
  entrance.camera.eye = {constant(0.0F), constant(5.0F), constant(10.0F)};
  entrance.camera.target = {constant(0.0F), constant(2.0F), constant(0.0F)};
  MotionChannel forward;
  forward.kind = ChannelKind::linear;
  forward.keys = {119.0F, 359.0F};
  forward.values = {0.0F, 0.0F, -15.0F};
  entrance.walk = std::array<MotionChannel, 3>{constant(0.0F), constant(0.0F), forward};
  BattleSceneTables tables;
  for (int state = scene_state::entrance_first; state <= scene_state::boss; ++state) {
    tables.states[state] = entrance;
  }
  tables.gates[SceneTuning{}.stage] = {{first_gate, 0.0F}, {second_gate, pi}};
  return tables;
}

// The player at the arena's centre and two drones.
struct TestBattle {
  Battle battle{parse_battle_settings(json::parse(R"({"arena": {"half_size": 100}})")),
                EnemyPath{}};
  TestBattle() {
    battle.add_robot("player", 0, {}, 0.0F, {}, 100.0F);
    battle.add_drone("drone1", 100.0F);
    battle.add_drone("drone2", 100.0F);
  }
};

// Gate 1's door, without walls.
StageDoors test_doors() {
  DoorFrame door;
  door.gate = 1;
  return StageDoors({door}, DoorTuning{});
}

struct Heard {
  std::vector<std::pair<int, int>> lines; // robot, chatter event
  std::vector<std::string> music;
};

void step(BattleScene &scene, Battle &battle, Heard *heard = nullptr, const int ticks = 1) {
  for (int index = 0; index < ticks; ++index) {
    scene.prepare(battle, tick);
    battle.tick({}, {}, tick);
    scene.observe(battle);
    const auto music = scene.take_music();
    if (heard == nullptr) {
      continue;
    }
    for (const BattleEvent &event : battle.events()) {
      if (event.kind == BattleEventKind::chatter) {
        heard->lines.emplace_back(event.robot, event.chatter);
      }
    }
    if (music) {
      heard->music.push_back(*music);
    }
  }
}

bool near(const Vec3 a, const Vec3 b) {
  return length(a - b) < 0.5F;
}

} // namespace

TEST_CASE("a computer robot uses the player entrance timing and opens its door") {
  BattleScene scene(test_tables(), SceneTuning{}, 1);
  TestBattle test;
  Battle &battle = test.battle;
  battle.set_doors(test_doors());
  CHECK(scene.start_entrance(battle));
  CHECK(!scene.start_robot_entrance(battle, 0)); // the player's is its own
  CHECK(scene.start_robot_entrance(battle, 1));
  CHECK(scene.robot_state(1) == scene_state::computer);
  CHECK(near(battle.robots()[1].mover.position(), second_gate));
  Heard heard;
  step(scene, battle, &heard, 29);
  CHECK(battle.doors()->idle(1));
  step(scene, battle, &heard, 2); // 60 frames: 480 left
  CHECK(!battle.doors()->idle(1));
  step(scene, battle, &heard, 238);
  CHECK(scene.robot_state(1) == scene_state::computer);
  step(scene, battle, &heard, 2); // the complete 540-frame sequence
  CHECK(scene.robot_state(1) == scene_state::none);
  CHECK(battle.robots()[1].mover.mode() != MoveMode::scripted);
  CHECK(near(battle.robots()[1].mover.position(), second_gate + Vec3{0.0F, 0.0F, 15.0F}));
  CHECK(!scene.active()); // both entrances have the same duration
  CHECK(heard.lines.empty());
  CHECK(heard.music.empty());
}

TEST_CASE("a robot is protected through entry and for five seconds afterwards") {
  SceneTuning tuning;
  BattleScene scene(test_tables(), tuning, 1);
  TestBattle test;
  Battle &battle = test.battle;
  CHECK(scene.start_robot_entrance(battle, 1));
  step(scene, battle, nullptr, 30);
  const Combatant &robot = battle.robots()[1];
  CHECK(robot.shield > 0.0F);
  CHECK(robot.shield_glow > 0.0F);
  CHECK(robot.shield_glow <= EntryShield{}.peak);
  // Protection refreshes throughout the complete entrance (ticks of 1/30 s).
  step(scene, battle, nullptr, 90);
  CHECK(robot.shield > 0.0F);
  step(scene, battle, nullptr, 305);
  CHECK(robot.shield == 0.0F);
  CHECK(robot.shield_glow == 0.0F);
}

TEST_CASE("a computer robot back in a gate walks in and leaves once its door has shut") {
  BattleSceneTables tables = test_tables();
  SceneTrack exit;
  exit.frames = 240.0F;
  tables.states[scene_state::exit] = exit;
  BattleScene scene(std::move(tables), SceneTuning{}, 1);
  TestBattle test;
  Battle &battle = test.battle;
  battle.set_doors(test_doors());
  CHECK(scene.start_robot_entrance(battle, 1));
  step(scene, battle, nullptr, 271); // the full entrance has finished
  REQUIRE(scene.robot_state(1) == scene_state::none);
  battle.place(1, second_gate + Vec3{0.0F, 0.0F, 3.0F}, 0.0F);
  Heard heard;
  step(scene, battle, &heard);
  CHECK(scene.robot_state(1) == scene_state::exit);
  CHECK(std::ranges::find(heard.lines, std::pair{1, chatter_event::left}) != heard.lines.end());
  CHECK(!scene.active()); // no bars or camera of its own
  const Combatant &robot = battle.robots()[1];
  step(scene, battle, nullptr, 100);
  CHECK(robot.shield > 0.0F);
  battle.doors()->open(1); // its door is busy as the walk ends: it waits in the gate
  step(scene, battle, nullptr, 25);
  CHECK(robot.alive);
  CHECK(near(robot.mover.position(), second_gate));
  for (int index = 0; index < 400 && robot.alive; ++index) {
    step(scene, battle);
  }
  CHECK(!robot.alive);
  CHECK(battle.doors()->idle(1));
  // A drone comes back as a shot-down one would.
  for (int index = 0; index < 200 && !robot.alive; ++index) {
    step(scene, battle);
  }
  CHECK(robot.alive);
}
