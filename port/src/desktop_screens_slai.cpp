// S.L.A.I. Rumble's battles (docs/SLAI_RUMBLE_PLAN.md, phase 3): a rumble
// entered on Rumble S.L.A.I. is fought on its city's imported stage, then
// its result fills the save's records and pays the prize.

#include "desktop_game_screens.hpp"
#include "desktop_screens.hpp"
#include "desktop_slai_rumble.hpp"
#include "desktop_training.hpp"

#include <SDL3/SDL.h>

#include <algorithm>

namespace phantomcrash {
namespace {

// The match's robots, the player's among them, fight on: the most the
// match takes, so the opponents are one fewer.
int slai_opponents(const SlaiRumbleWorld &world, const SlaiMatch &match) {
  return std::max(slai_match_robots(world, match).second - 1, 1);
}

} // namespace

// A stand-in until S.L.A.I.'s opponent rosters are decoded (phase 1b): the
// ridden robot as TEST DRIVE fits it, against training drones, one fewer
// than the match's robots, which come back as they fall. The battle ends
// when the player leaves it or is shot down.
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
  battle_training_.return_to = slai_result_route;
  SDL_Log("S.L.A.I. rumble: city=%d day=%d match=%d regulation=%d drones=%d (stand-in)",
          slai_match_->city, slai_match_->slai_day, slai_match_->match, match->regulation,
          battle_training_.battle.drones);
  TrainingLesson lesson = test_drive_.lesson;
  lesson.title = rule->name;
  return make_training(battle_training_, lesson);
}

// The stand-in's result: a player still standing who shot down as many
// robots as the match has opponents beat its leader, which clears the
// match's record; the shootdowns are the score the prize is paid on.
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
  if (standing && summary.shootdowns >= slai_opponents(data->world, *match)) {
    result.beaten.emplace_back(match->save_record, 0);
  }
  const SlaiMatchOutcome outcome = slai_finish_match(quest_, *data, result);
  SDL_Log("S.L.A.I. result: match=%d shootdowns=%d cleared=%d prize=%lld stage %d -> %d",
          result.match, result.score, outcome.cleared ? 1 : 0,
          static_cast<long long>(outcome.prize), outcome.stage_before, outcome.stage_after);
  slai_match_.reset();
}

} // namespace phantomcrash
