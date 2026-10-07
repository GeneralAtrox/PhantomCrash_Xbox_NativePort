#include "phantomcrash/rumbling_world.hpp"
#include <stdexcept>

namespace phantomcrash {

struct RumblingWorld::Checkpoint {
  const RumblingWorld *owner{};
  std::minstd_rand random;
  std::int64_t epoch{};
  std::array<Area, roster_areas> areas;
  std::array<std::shared_ptr<const Battle>, roster_areas> battles;
  int lent{-1};
  bool background{};
  bool deterministic{};
  std::vector<RobotParts> lent_looks;
};

std::shared_ptr<const RumblingWorld::Checkpoint> RumblingWorld::checkpoint() const {
  auto state = std::make_shared<Checkpoint>();
  state->owner = this;
  state->random = random_;
  state->epoch = epoch_;
  state->areas = areas_;
  state->lent = lent_;
  state->background = background_;
  state->deterministic = deterministic_;
  state->lent_looks = lent_looks_;
  for (std::size_t i = 0; i < areas_.size(); ++i) {
    if (areas_[i].battle) {
      state->battles[i] = areas_[i].battle->checkpoint();
    }
  }
  return state;
}

void RumblingWorld::restore_checkpoint(const Checkpoint &state) {
  if (state.owner != this) {
    throw std::runtime_error("Cannot restore a checkpoint into another rumbling world");
  }
  auto areas = state.areas;
  for (std::size_t i = 0; i < areas.size(); ++i) {
    if (state.battles[i]) {
      // Preserve the Battle identity shared with training screens and the match.
      areas[i].battle->restore_checkpoint(*state.battles[i]);
    }
  }
  random_ = state.random;
  epoch_ = state.epoch;
  areas_ = std::move(areas);
  lent_ = state.lent;
  background_ = state.background;
  deterministic_ = state.deterministic;
  lent_looks_ = state.lent_looks;
}

void RumblingWorld::prepare_deterministic() {
  for (auto &area : areas_) {
    if (area.battle) {
      area.battle->prepare_deterministic();
    }
  }
  deterministic_ = true;
}

} // namespace phantomcrash
