#pragma once

#include "phantomcrash/garage.hpp"
#include "phantomcrash/garage_tables.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/robot_parts.hpp"
#include "phantomcrash/test_drive.hpp"
#include "phantomcrash/training_ground.hpp"

#include <string>

namespace phantomcrash {

// The battle the VS mode leads to (data/world/vs_battle.json). Until the
// area stages and the versus rules are ported it runs as the training
// ground's FreePlay split into a screen for each player, with the first
// player's Scoobee as the player's robot.
struct VsBattle {
  std::string return_to{"vs_mode"};
  TrainingLesson lesson{"VS MODE", 3, 2};
};

// Throws std::runtime_error naming the member at fault.
[[nodiscard]] VsBattle parse_vs_battle(const json::JsonValue &document);

// The lesson for `players` players: a screen each.
[[nodiscard]] TrainingLesson vs_lesson(const VsBattle &battle, int players);

// The training script with `robot` fitted into a garage of its own as the
// player's.
[[nodiscard]] TrainingScript vs_training(const TrainingScript &base, const TestDriveScript &drive,
                                         const VsBattle &battle, const RobotRecord &robot,
                                         const PartTables &parts, const GarageTables &tables);

} // namespace phantomcrash
