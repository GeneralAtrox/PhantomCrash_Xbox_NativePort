#pragma once

// Opponents a training-screen battle brings in place by place, each with its
// own robot and AI, a place emptied by a shootdown filled again after a
// while (S.L.A.I.'s rumbles: slai_battle_lineup.hpp).

#include "phantomcrash/rumbling_robot.hpp"

#include <optional>

namespace phantomcrash {

struct LineupRobot {
  RumblingRobot robot;
  int ability{}; // the AI's level (battle_ai.hpp)
  int profile{}; // and profile (contests.hpp Entrant)
  int marker{};  // the HUD marker's colour (battle_hud.json, as RosterSource)
};

class BattleLineup {
public:
  virtual ~BattleLineup() = default;

  // Battle robots 1..places() are the line-up's (0 is the player's).
  [[nodiscard]] virtual int places() const = 0;
  // The robot entering `place`, or none: the place stays empty for now.
  [[nodiscard]] virtual std::optional<LineupRobot> enter(int place) = 0;
  // `place`'s robot is out of the battle; `by_player` when the player shot
  // it down.
  virtual void leave(int place, bool by_player) = 0;
  // How long an emptied place waits before enter() is asked again.
  [[nodiscard]] virtual float arrival_seconds() const = 0;
};

} // namespace phantomcrash
