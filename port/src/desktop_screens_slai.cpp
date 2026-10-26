// S.L.A.I. Rumble's battles (docs/SLAI_RUMBLE_PLAN.md, phase 3): a rumble
// entered on Rumble S.L.A.I. is fought on its city's imported stage, then
// its result fills the save's records and pays the prize.

#include "desktop_game_screens.hpp"
#include "desktop_screens.hpp"
#include "desktop_slai_rumble.hpp"
#include "desktop_training.hpp"
#include "phantomcrash/slai_battle_lineup.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <exception>
#include <random>

namespace phantomcrash {
namespace {

// The match's robots, the player's among them, fight on: the most the
// match takes, so the opponents are one fewer.
int slai_opponents(const SlaiRumbleWorld &world, const SlaiMatch &match) {
  return std::max(slai_match_robots(world, match).second - 1, 1);
}

// The live RUMBLING's navigation grid settings (rumbling.json live.ai),
// so S.L.A.I.'s stages are walked as Phantom Crash's are.
NavTuning slai_route_grid(const DataRoots &roots) {
  try {
    return parse_rumbling_live(roots.document("world/rumbling.json")).ai.navigation;
  } catch (const std::exception &error) {
    SDL_Log("S.L.A.I. route grid: %s; using the default grid", error.what());
    return {};
  }
}

} // namespace

// The match's opponents from the original's rosters (slai_battle_lineup.hpp),
// each on its own robot and fighting with the live RUMBLING's AI
// (rumbling.json live.ai, robot_ai.json); none when the table was converted
// before it carried them, or the AI data does not load.
std::shared_ptr<SlaiBattleLineup> ScreenRunner::make_slai_lineup(const SlaiRumbleData &data,
                                                                 const SlaiMatch &match) {
  if (data.table.opponents.empty()) {
    return nullptr;
  }
  BattleSettings &settings = battle_training_.battle.settings;
  try {
    settings.ai = parse_rumbling_live(roots_.document("world/rumbling.json")).ai;
    settings.ai.original = parse_original_ai(roots_.document("world/robot_ai.json"));
  } catch (const std::exception &error) {
    SDL_Log("S.L.A.I. opponents: %s; training drones stand in", error.what());
    return nullptr;
  }
  settings.ai.area = -1; // no RUMBLING area's exceptions
  const RegulationCheck check = game_->regulation_check();
  const RumblingTables tables{game_->part_tables(),
                              game_->garage_tables(),
                              game_->contests(),
                              check.rules,
                              test_drive_,
                              training_.battle.settings.weapons,
                              parse_battle_hud_style(roots_.document("ui/battle_hud.json"))};
  return std::make_shared<SlaiBattleLineup>(data, match, quest_.slai_rumble, tables,
                                            settings.movement, std::random_device{}());
}

// The ridden robot as TEST DRIVE fits it, against the match's opponents
// (make_slai_lineup), or, without them, training drones, one fewer than the
// match's robots, which come back as they fall. They walk the stage's own
// S.L.A.I. route (its enemy_path, Z/PATH); a stage without one has them
// come in at its spawns and roam between them over the live RUMBLING's
// navigation grid (slai_rumble.json battle). The battle ends when the
// player leaves it or is shot down.
std::unique_ptr<Screen> ScreenRunner::make_slai_battle() {
  const auto *hub = dynamic_cast<const SlaiRumbleScreen *>(screen_.get());
  const SlaiRumbleData *data = game_->slai_rumble();
  if (hub == nullptr || data == nullptr || !hub->entered()) {
    return nullptr;
  }
  slai_match_ = hub->entered();
  const SlaiMatch *match = slai_match(data->table, slai_match_->match);
  const SlaiRegulation *rule = slai_regulation(data->table, match->regulation);
  slai_pay_entry(quest_, data->world, *rule);
  battle_training_ = test_drive_training(training_, test_drive_, quest_.garage,
                                         quest_.garage.riding, game_->part_tables());
  const std::string &stage = slai_match_stage(data->world, slai_match_->city, slai_match_->night);
  if (stages_.contains(stage)) {
    battle_training_.stage = stage;
  } else {
    SDL_Log("S.L.A.I. stage %s is not imported; the battle uses %s", stage.c_str(),
            battle_training_.stage.c_str());
  }
  battle_training_.battle.drones = slai_opponents(data->world, *match);
  const auto chosen = stages_.find(battle_training_.stage);
  if (chosen != stages_.end() && !chosen->second.enemy_path.empty()) {
    battle_training_.battle.enemy_path = chosen->second.enemy_path;
  }
  BattleSettings &settings = battle_training_.battle.settings;
  settings.drone_unstick_seconds = data->world.battle.unstick_seconds;
  settings.drone_hop_seconds = data->world.battle.hop_seconds;
  settings.drone_hops = data->world.battle.hops;
  if (data->world.battle.spawn_route) {
    battle_training_.battle.spawn_route = slai_route_grid(roots_);
    battle_training_.battle.spawn_route_merge = data->world.battle.spawn_merge;
  }
  battle_training_.return_to = slai_result_route;
  slai_lineup_ = make_slai_lineup(*data, *match);
  if (slai_lineup_) {
    battle_training_.battle.lineup = slai_lineup_;
    battle_training_.battle.drones = 0;
  }
  SDL_Log("S.L.A.I. rumble: city=%d day=%d match=%d regulation=%d %s=%d", slai_match_->city,
          slai_match_->slai_day, slai_match_->match, match->regulation,
          slai_lineup_ ? "places" : "drones (stand-in)",
          slai_lineup_ ? slai_lineup_->places() : battle_training_.battle.drones);
  TrainingLesson lesson = test_drive_.lesson;
  lesson.title = rule->name;
  return make_training(battle_training_, lesson);
}

// The opponents the player shot down fill the save's records (the leader's
// clears the match's); the shootdowns are the score the prize is paid on.
// Against the stand-in drones, a player still standing who shot down as
// many robots as the match has opponents beat its leader.
void ScreenRunner::finish_slai_match() {
  const auto *battle = dynamic_cast<const TrainingScreen *>(screen_.get());
  const SlaiRumbleData *data = game_->slai_rumble();
  if (battle == nullptr || data == nullptr || !slai_match_) {
    return;
  }
  const SlaiMatch *match = slai_match(data->table, slai_match_->match);
  const MatchSummary summary = battle->match_summary();
  SlaiMatchResult result;
  result.match = slai_match_->match;
  result.score = summary.shootdowns;
  const bool standing = summary.outcome != MatchOutcome::retire && summary.armour > 0.0F;
  if (slai_lineup_) {
    result.beaten = slai_lineup_->beaten();
  } else if (standing && summary.shootdowns >= slai_opponents(data->world, *match)) {
    result.beaten.emplace_back(match->save_record, slai_leader_flag);
  }
  slai_lineup_.reset();
  const SlaiMatchOutcome outcome = slai_finish_match(quest_, *data, result);
  SDL_Log("S.L.A.I. result: match=%d shootdowns=%d beaten=%zu cleared=%d prize=%lld "
          "stage %d -> %d",
          result.match, result.score, result.beaten.size(), outcome.cleared ? 1 : 0,
          static_cast<long long>(outcome.prize), outcome.stage_before, outcome.stage_after);
  slai_match_.reset();
}

} // namespace phantomcrash
