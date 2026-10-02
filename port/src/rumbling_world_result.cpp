// The battle behind the match's results: the original's RESULT (0x094fee)
// starts a fresh battle task (0x04a6a0) and stands the contest's robots up in
// it (0x045a68, 0x0456b7), while the area's own battle waits.

#include "phantomcrash/rumbling_world.hpp"

#include <algorithm>

namespace phantomcrash {

std::shared_ptr<Battle> RumblingWorld::stand_up(const int area, const QuestSave &quest) {
  if (area < 0 || area >= roster_areas) {
    return nullptr;
  }
  const Area &source = areas_[static_cast<std::size_t>(area)];
  if (source.roster.members.empty()) {
    return nullptr;
  }
  const RumblingStage &stage = stages_[static_cast<std::size_t>(area)];
  auto fresh =
      std::make_shared<Battle>(settings_, stage.path, static_cast<std::uint32_t>(random_()));
  Battle &battle = *fresh;
  battle.set_stage(stage.collision);
  if (stage.objects) {
    battle.set_objects(*stage.objects);
  }
  // Robot 0 is the player's place, as in the area's battle, so the robots
  // keep the places the screen draws them by.
  battle.add_robot("player", 0, {}, 0.0F, {}, 1.0F);
  battle.take_out(0);
  const std::size_t count =
      std::min(source.roster.members.size(), static_cast<std::size_t>(result_robots));
  for (std::size_t place = 0; place < count; ++place) {
    static_cast<void>(battle.add_drone("opponent", 1.0F));
    static_cast<void>(seat(battle, area, place, quest));
    scatter(battle, area, static_cast<int>(place) + 1);
  }
  return fresh;
}

} // namespace phantomcrash
