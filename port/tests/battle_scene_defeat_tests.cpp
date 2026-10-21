// A boss's defeat (robot states 0x13-0x17): the battle waits its frames
// and its line, then a ranker's ends and another boss's goes on, the
// players spared meanwhile.

#include "test_support.hpp"

#include "phantomcrash/battle_scene.hpp"
#include "phantomcrash/json.hpp"

using namespace phantomcrash;

namespace {

constexpr float tick = 1.0F / 30.0F; // two of the scene's 60 Hz frames

struct TestBattle {
  Battle battle{parse_battle_settings(json::parse(R"({"arena": {"half_size": 100}})")),
                EnemyPath{}};
  TestBattle() {
    battle.add_robot("player", 0, {}, 0.0F, {}, 100.0F);
  }
};

void step(BattleScene &scene, Battle &battle, const int ticks) {
  for (int index = 0; index < ticks; ++index) {
    scene.prepare(battle, tick);
  }
}

} // namespace

TEST_CASE("a ranker's defeat waits its frames and its line, then ends the battle") {
  TestBattle test;
  BattleScene scene(BattleSceneTables{}, SceneTuning{});
  scene.start_defeat(0, 180.0F, true);
  CHECK(scene.scripted());
  CHECK(scene.defeat_holds());
  scene.set_defeat_line(true);
  step(scene, test.battle, 90);
  CHECK(scene.letterbox() == 1.0F);
  CHECK(scene.defeat_holds()); // the line is still being spoken
  scene.set_defeat_line(false);
  step(scene, test.battle, 1);
  CHECK(scene.defeat_over());
  CHECK(!scene.defeat_holds());
  CHECK(scene.scripted()); // the controls stay out until the battle ends
}

TEST_CASE("another boss's defeat gives the battle back") {
  TestBattle test;
  BattleScene scene(BattleSceneTables{}, SceneTuning{});
  scene.start_defeat(0, 180.0F, false);
  step(scene, test.battle, 89);
  CHECK(scene.defeat_robot() == 0);
  step(scene, test.battle, 1);
  CHECK(scene.defeat_robot() == -1);
  CHECK(!scene.scripted());
  CHECK(!scene.defeat_over());
  step(scene, test.battle, 16);
  CHECK(scene.letterbox() == 0.0F);
}

TEST_CASE("the players take no hits while another boss's defeat holds their controls") {
  TestBattle test;
  BattleScene scene(BattleSceneTables{}, SceneTuning{});
  scene.start_defeat(0, 180.0F, false);
  step(scene, test.battle, 1);
  CHECK(test.battle.players_spared());
  step(scene, test.battle, 89);
  CHECK(scene.defeat_robot() == -1);
  CHECK(!test.battle.players_spared());

  // A ranker's ends the battle and spares nobody; nor does any with the
  // sparing off.
  TestBattle ranker;
  BattleScene ends(BattleSceneTables{}, SceneTuning{});
  ends.start_defeat(0, 180.0F, true);
  step(ends, ranker.battle, 1);
  CHECK(!ranker.battle.players_spared());
  SceneTuning off;
  off.boss_spares_players = false;
  TestBattle unspared;
  BattleScene plain(BattleSceneTables{}, off);
  plain.start_defeat(0, 180.0F, false);
  step(plain, unspared.battle, 1);
  CHECK(!unspared.battle.players_spared());
}
