#pragma once

#include "phantomcrash/garage.hpp"
#include "phantomcrash/garage_tables.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/robot_parts.hpp"
#include "phantomcrash/rumbling_entry.hpp"
#include "phantomcrash/test_drive.hpp"
#include "phantomcrash/training_ground.hpp"

#include <string>

namespace phantomcrash {

// The battle a RUMBLING entry leads to (data/world/rumbling.json battle).
// Until the area stages and the contest's roster are ported it runs as the
// training ground's FreePlay with the entered robot as the player's.
struct RumblingBattle {
  std::string return_to{"club_wired"};
  TrainingLesson lesson{"RUMBLING", 3, 1};
};

// Throws std::runtime_error naming the member at fault.
[[nodiscard]] RumblingBattle parse_rumbling_battle(const json::JsonValue &document);

// The training script with the entry's robot as the player's: the ridden
// hangar's, or for a rental its SV record fitted into a hangar of its own.
[[nodiscard]] TrainingScript rumbling_training(const TrainingScript &base,
                                               const TestDriveScript &drive,
                                               const RumblingBattle &battle,
                                               const RumblingEntry &entry, const Garage &garage,
                                               const PartTables &parts, const GarageTables &tables);

} // namespace phantomcrash
