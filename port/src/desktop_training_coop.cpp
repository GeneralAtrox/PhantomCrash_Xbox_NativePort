#include "desktop_training.hpp"
#include "desktop_coop_matches.hpp"

namespace phantomcrash {

void TrainingScreen::sync_coop_views() {
  if (!view_ || (coop_revision_ == coop_match_->look_revision &&
                 live_changes_ == coop_match_->world->changes(live_area_))) {
    return;
  }
  coop_revision_ = coop_match_->look_revision;
  live_changes_ = coop_match_->world->changes(live_area_);
  for (std::size_t index = 0; index < battle_->robots().size(); ++index) {
    const auto look = coop_match_->look(static_cast<int>(index));
    if (index >= coop_looks_.size()) {
      coop_looks_.push_back(look);
      view_->add(look);
    } else if (coop_looks_[index] != look) {
      coop_looks_[index] = look;
      view_->replace(index, look);
    }
    battle_->set_radius(static_cast<int>(index),
                        view_->radius(index, script_.battle.settings.robot_radius));
  }
  view_->bind(*battle_);
}

bool TrainingScreen::coop_contest_won() const {
  return coop_match_ && coop_match_->players[coop_slot_].won;
}

std::int64_t TrainingScreen::settle_coop(QuestSave &save) {
  return coop_match_ ? coop_match_->leave(coop_slot_, save) : 0;
}

} // namespace phantomcrash
