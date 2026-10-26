#pragma once

// S.L.A.I.'s rumble line-up as the battle fields it (battle_lineup.hpp):
// the original's draw (slai_lineup.hpp), each opponent on its own robot
// with the world's chip for its level and its AI from its level and
// behaviour (slai_rumble.json "opponents").

#include "phantomcrash/battle_lineup.hpp"
#include "phantomcrash/slai_lineup.hpp"

#include <random>
#include <utility>
#include <vector>

namespace phantomcrash {

// The chip row for an opponent's level, or none.
[[nodiscard]] const SlaiOpponentChip *slai_opponent_chip(const SlaiOpponentRules &rules, int level);
// The AI profile for an opponent's behaviour value.
[[nodiscard]] int slai_opponent_profile(const SlaiOpponentRules &rules, int behaviour);
// The robot record an opponent fights on: its own, with the world's chip,
// paint and sticker.
[[nodiscard]] RobotRecord slai_opponent_robot(const SlaiOpponentRules &rules,
                                              const SlaiOpponent &opponent);

class SlaiBattleLineup final : public BattleLineup {
public:
  // `data`, `match` and the tables `tables` refers to (but its rules,
  // copied) outlive the line-up.
  // `story_flags` as slai_candidates takes them.
  SlaiBattleLineup(const SlaiRumbleData &data, const SlaiMatch &match, const SlaiRumbleSave &save,
                   std::vector<int> story_flags, const RumblingTables &tables,
                   const MovementTuning &movement, unsigned int seed);

  [[nodiscard]] int places() const override {
    return slai_battle_places;
  }
  [[nodiscard]] std::optional<LineupRobot> enter(int place) override;
  void leave(int place, bool by_player) override;
  [[nodiscard]] float arrival_seconds() const override {
    return data_.world.opponents.arrival_seconds;
  }

  // The (record, flag) pairs the player beat (SlaiMatchResult::beaten).
  [[nodiscard]] const std::vector<std::pair<int, int>> &beaten() const noexcept {
    return draw_.beaten();
  }

private:
  const SlaiRumbleData &data_;
  SlaiLineupDraw draw_;
  RumblingRules rules_;
  RumblingTables tables_;
  MovementTuning movement_;
  std::mt19937 random_;
};

} // namespace phantomcrash
