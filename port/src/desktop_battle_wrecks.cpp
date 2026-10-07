#include "desktop_battle_view.hpp"
#include "desktop_wreck_collision.hpp"
#include "phantomcrash/money.hpp"

namespace phantomcrash {

void BattleView::sync_wrecks(Battle &battle) {
  if (wreck_battle_ != &battle || wreck_generation_ != battle.restore_generation()) {
    clear_wrecks();
    wreck_battle_ = &battle;
    wreck_generation_ = battle.restore_generation();
  }
  const auto &wrecks = battle.wrecks().items();
  while (wreck_items_.size() < wrecks.size()) {
    const std::size_t index = wreck_items_.size();
    const auto &wreck = wrecks[index];
    auto &items = wreck_items_.emplace_back();
    wreck_solid_.push_back(false);
    RobotVisual visual(world_, wreck.assembly);
    visual.add_items(wreck.pose, wreck.root, items, &wreck.shoulders, wreck.lost);
    if (!battle.wrecks().simulation_collision()) {
      battle.wrecks().set_collision(index, posed_collision(world_, items));
    }
    SDL_Log("Robot wreck: created=%zu parts=%zu triangles=%zu burn_left=%.3f reward=%d NY", index,
            items.size(), wreck.collision ? wreck.collision->walls().size() : 0, wreck.burn_left,
            battle.settings().robot_wrecks.reward_ny);
  }
  for (const auto &event : battle.events()) {
    if (event.kind == BattleEventKind::wreck_destroyed) {
      SDL_Log("Robot wreck: destroyed by=%d reward=%.0f NY (no shootdown)", event.robot,
              event.amount * ny_per_stored_unit);
    }
  }
  for (std::size_t i = 0; i < wrecks.size(); ++i) {
    if (wrecks[i].solid() && !wreck_solid_[i]) {
      SDL_Log("Robot wreck: solid=%zu fire extinguished, persistent collision enabled", i);
      wreck_solid_[i] = true;
    }
    if (wrecks[i].removed) {
      wreck_items_[i].clear();
    }
  }
}

void BattleView::clear_wrecks() {
  wreck_items_.clear();
  wreck_solid_.clear();
  wreck_battle_ = nullptr;
  wreck_generation_ = 0;
}

void BattleView::add_wreck_items(const Battle &battle, std::vector<WorldItem> &items) const {
  const auto &wrecks = battle.wrecks().items();
  for (std::size_t i = 0; i < wreck_items_.size() && i < wrecks.size(); ++i) {
    if (!wrecks[i].removed) {
      items.insert(items.end(), wreck_items_[i].begin(), wreck_items_[i].end());
    }
  }
}
} // namespace phantomcrash
