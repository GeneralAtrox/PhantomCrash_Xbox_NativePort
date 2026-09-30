#pragma once

// The live RUMBLING, a port addition (data/world/rumbling.json "live"): each
// area's opponents fight on its stage whether the player is there or not.
// The original only draws the day's roster (rumbling_roster.hpp) and starts
// the battle afresh; here the battle goes on from a NEW GAME or a loaded game
// until the day ends. One shot down leaves and the next is drawn to walk in;
// entering the area joins its battle as it stands. The opponents' state goes
// into the save with the rosters (LiveRobot).

#include "phantomcrash/area_roster.hpp"
#include "phantomcrash/battle.hpp"
#include "phantomcrash/enemy_path.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/rumbling_robot.hpp"
#include "phantomcrash/rumbling_roster.hpp"
#include "phantomcrash/stage_collision.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace phantomcrash {

struct RumblingLive {
  double tick_hz{30.0};        // the areas' battles run at this rate, off screen
  float arrival_seconds{8.0F}; // an opponent shot down is replaced this long after
  // Each area's stage by day and by night (data/world/stages.json names).
  std::array<std::array<std::string, 2>, roster_areas> stages{};
  // Screens the world does not run on: those before a game is under way.
  std::vector<std::string> paused_screens;
  DroneAi ai;
};

// The "live" member of rumbling.json; throws std::runtime_error naming the
// member at fault.
[[nodiscard]] RumblingLive parse_rumbling_live(const json::JsonValue &document);

// An area's stage as its battle needs it.
struct RumblingStage {
  EnemyPath path;
  StageCollision collision;
  std::optional<StageObjects> objects;
};

class RumblingWorld final {
public:
  // `settings` are the battle's (the training ground's, whose weapons the
  // opponents' start from); the tables `check` and `tables` refer to must
  // outlive the world.
  RumblingWorld(RegulationCheck check, RumblingTables tables, BattleSettings settings,
                std::array<RumblingStage, roster_areas> stages, RumblingLive live,
                std::uint32_t seed);

  // Draws the day's rosters when due, takes up rosters it did not write (a
  // new day's, a new or loaded game's), runs each area but a lent one for
  // `seconds` and writes their state back into `quest`.
  void update(QuestSave &quest, double seconds);
  // Area `area`'s battle for the player to join: robot 0 is the player's
  // place, out of the battle until they take it. The world stops running
  // it but still brings in the next opponents, until it is given back.
  // Null when the area has nobody out.
  [[nodiscard]] std::shared_ptr<Battle> lend(int area, QuestSave &quest);
  // The player has left: the opponents they shot down are beaten and their
  // bounties paid.
  std::int64_t give_back(QuestSave &quest);
  [[nodiscard]] bool player_won() const;
  [[nodiscard]] int lent() const noexcept {
    return lent_;
  }
  // Battle robot `index`'s opponent at `area` (roster place index - 1), or null.
  [[nodiscard]] const RumblingRobot *robot(int area, int index) const;
  // Counts each time one of the area's robots becomes someone else.
  [[nodiscard]] int changes(int area) const;
  [[nodiscard]] const RumblingLive &live() const noexcept {
    return live_;
  }

private:
  struct Area {
    std::shared_ptr<Battle> battle;
    AreaRoster roster;
    std::vector<RumblingRobot> robots; // by roster place
    double clock{};
    int changes{};
    std::vector<RosterMember> player_kills; // since it was lent
  };

  void build(int area, const QuestSave &quest);
  // Puts roster place `place`'s robot into robot `place` + 1.
  void fit(int area, std::size_t place, const QuestSave &quest);
  // Stands a new opponent somewhere on the path.
  void scatter(int area, int index);
  void restore(Battle &battle, int index, const LiveRobot &live);
  void step(int area, const QuestSave &quest, float seconds, bool run);
  static void record(const Combatant &robot, LiveRobot &live);
  [[nodiscard]] float random_share();

  RegulationCheck check_;
  RumblingTables tables_;
  BattleSettings settings_;
  std::array<RumblingStage, roster_areas> stages_;
  RumblingLive live_;
  std::minstd_rand random_;
  std::int64_t epoch_{};
  std::array<Area, roster_areas> areas_{};
  int lent_{-1};
};

} // namespace phantomcrash
