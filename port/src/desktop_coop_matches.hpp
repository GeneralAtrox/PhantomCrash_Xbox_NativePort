#pragma once

#include "phantomcrash/battle_scene.hpp"
#include "phantomcrash/rumbling_world.hpp"
#include <map>
#include <tuple>

namespace phantomcrash {

struct CoopMatch {
  struct Participant {
    int actor{-1};
    RobotParts look;
    std::int64_t bounty{};
    std::vector<int> beaten;
    bool won{};
  };
  int day{}, area{}, contest{};
  std::shared_ptr<RumblingWorld> world;
  std::shared_ptr<Battle> battle;
  QuestSave simulation_save;
  std::unique_ptr<BattleScene> entrances;
  std::array<Participant, 4> players{};
  int look_revision{};
  double elapsed{};
  CoopOptions options;
  void collect_rewards();
  int join(int slot, RobotParts look);
  std::int64_t leave(int slot, QuestSave &save);
  void tick(float seconds);
  [[nodiscard]] bool empty() const;
  [[nodiscard]] RobotParts look(int actor) const;
};

class CoopMatches {
public:
  using Key = std::tuple<int, int, int>;
  std::map<Key, std::shared_ptr<CoopMatch>> matches;
  void tick(float seconds);
};

} // namespace phantomcrash
