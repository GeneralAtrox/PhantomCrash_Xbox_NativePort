#include "desktop_training.hpp"

namespace phantomcrash {

void TrainingScreen::update_result(const double seconds) {
  if (!in_scene()) {
    play_seconds_ += seconds;
  }
  done_ = done_ || (free_play_ && screens_ == 1 && battle_ && !battle_->player().alive) ||
          (scene_ && scene_->scene().finished()) ||
          (!free_play_ && tutorial_.finished() && !message_);
  if (!match_results_ || !battle_) {
    return;
  }
  for (const BattleEvent &event : battle_->events()) {
    if (event.robot == 0 && event.kind == BattleEventKind::carrier_down) {
      extra_prize_ += static_cast<std::int64_t>(event.amount);
    }
  }
  if (!battle_->player().alive) {
    outcome_ = MatchOutcome::retire;
    done_ = true;
  } else if (live_world_ && live_world_->player_won()) {
    outcome_ = MatchOutcome::win;
    done_ = true;
  }
}

MatchSummary TrainingScreen::match_summary() const {
  MatchSummary result;
  result.outcome = outcome_;
  result.shootdowns = battle_->player().kills;
  result.play_seconds = play_seconds_;
  result.lost = battle_->player().lost;
  const auto look = script_.robots.find("player");
  if (look != script_.robots.end()) {
    result.robot.parts[static_cast<int>(PartSlot::body)] =
        RobotPart{look->second.body, script_.battle.player.body_level};
    result.robot.parts[static_cast<int>(PartSlot::legs)] =
        RobotPart{look->second.legs, script_.battle.player.legs_level};
  }
  for (std::size_t slot = 0; slot < weapon_slots; ++slot) {
    const WeaponTuning *weapon = battle_->player().weapons[slot];
    if (weapon && weapon->part) {
      result.robot.parts[static_cast<int>(PartSlot::right_arm) + slot] =
          RobotPart{weapon->part->index, weapon->part->level};
    }
  }
  return result;
}

WorldFrame TrainingScreen::result_background() {
  // Own the final poses and effects; returning the live battle cannot change them.
  return battle_frame(camera_, 640, 480, -1);
}

} // namespace phantomcrash
