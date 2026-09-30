#include "test_support.hpp"
#include "phantomcrash/battle_monitor.hpp"
#include "phantomcrash/training_ground.hpp"

#include <set>

using namespace phantomcrash;

TEST_CASE("battle monitors cycle living humans and AI without immediate repeats") {
  Battle battle({}, {});
  for (int i = 0; i < 4; ++i) {
    battle.add_robot("fighter", i, {static_cast<float>(i * 10), 0, 0}, 0, {}, 100);
  }
  battle.robot(3).alive = false;
  BattleMonitorSettings settings;
  settings.switch_seconds = 2;
  BattleMonitor monitor(settings, 37);
  monitor.tick(battle, 0);
  std::set<int> seen;
  for (int i = 0; i < 50; ++i) {
    const int prior = monitor.subject();
    CHECK(prior >= 0 && prior < 3);
    seen.insert(prior);
    monitor.tick(battle, 1);
    CHECK(monitor.subject() == prior);
    monitor.tick(battle, 1);
    CHECK(monitor.subject() != prior);
  }
  CHECK(seen.size() == 3);
}

TEST_CASE("battle monitors cut immediately on death and recover when fighters return") {
  Battle battle({}, {});
  battle.add_robot("player", 0, {}, 0, {}, 100);
  battle.add_robot("AI", 1, {20, 0, 0}, 0, {}, 100);
  BattleMonitor monitor({}, 5);
  monitor.tick(battle, 0);
  const int dead = monitor.subject();
  battle.robot(dead).alive = false;
  monitor.tick(battle, 0);
  CHECK(monitor.subject() == 1 - dead);
  monitor.tick(battle, 20);
  CHECK(monitor.subject() == 1 - dead);
  battle.robot(1 - dead).alive = false;
  monitor.tick(battle, 0);
  CHECK(monitor.subject() == -1);
  battle.robot(dead).alive = true;
  monitor.tick(battle, 0);
  CHECK(monitor.subject() == dead);
}

TEST_CASE("battle monitor timing uses simulation seconds independently of draws") {
  Battle battle({}, {});
  battle.add_robot("player", 0, {}, 0, {}, 100);
  battle.add_robot("AI", 1, {20, 0, 0}, 0, {}, 100);
  BattleMonitor a({}, 17), b({}, 17);
  a.tick(battle, 0);
  b.tick(battle, 0);
  for (int i = 0; i < 80; ++i) {
    a.tick(battle, 0.125F);
    b.tick(battle, 0.0625F);
    b.tick(battle, 0.0625F);
    CHECK(a.subject() == b.subject());
  }
}

TEST_CASE("stage monitor settings bind named surfaces independently of sky") {
  const auto stages = parse_stages(json::parse(R"({
    "monitor": {"switch_seconds": 5, "aspect": 1.5, "camera": {"distance": 20}},
    "stages": {"arena": {"sky_nodes": ["sky"], "monitor_nodes": ["gate", "ship"]}}
  })"));
  const auto &stage = stages.at("arena");
  CHECK(stage.monitor_nodes.size() == 2);
  CHECK(stage.sky_nodes.size() == 1);
  CHECK(stage.monitor.switch_seconds == 5);
  CHECK(stage.monitor.aspect == 1.5F);
  CHECK(stage.monitor.camera.distance == 20);
  CHECK_THROWS(parse_battle_monitor(json::parse(R"({"switch_seconds": 0})")));
}
