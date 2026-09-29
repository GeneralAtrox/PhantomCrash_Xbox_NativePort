#pragma once

#include "phantomcrash/garage.hpp"
#include "phantomcrash/garage_tables.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/robot_parts.hpp"
#include "phantomcrash/rumbling_entry.hpp"
#include "phantomcrash/rumbling_rules.hpp"
#include "phantomcrash/settings.hpp"
#include "phantomcrash/test_drive.hpp"
#include "phantomcrash/training_ground.hpp"

#include <optional>
#include <string>

namespace phantomcrash {

// An editable testing destination, entered with the current quest's inventory.
struct QuickRental {
  std::string label;
  std::string screen;
  int day{};
  int area{};
  int gate{};
  int robot{-1};
};

// The battle a RUMBLING entry leads to (data/world/rumbling.json battle).
// Until the area stages and the contest's roster are ported it runs as the
// training ground's FreePlay with the entered robot as the player's.
struct RumblingBattle {
  std::string return_to{"club_wired"};
  TrainingLesson lesson{"RUMBLING", 3, 1};
  QuickRental quick_rental;
};

// Throws std::runtime_error naming the member at fault.
[[nodiscard]] RumblingBattle parse_rumbling_battle(const json::JsonValue &document);

// Restarts the preset's calendar day and opponents, pays the normal entry fee,
// and borrows its robot without adding anything to the player's garage.
// An invalid or unaffordable entry leaves the quest unchanged.
[[nodiscard]] std::optional<RumblingEntry>
prepare_quick_rental(const QuickRental &preset, const ContestTables &contests,
                     const RumblingRules &rules, const GarageTables &garage, QuestSave &quest);

// The training script with the entry's robot as the player's: the ridden
// hangar's, or for a rental its SV record fitted into a hangar of its own.
[[nodiscard]] TrainingScript rumbling_training(const TrainingScript &base,
                                               const TestDriveScript &drive,
                                               const RumblingBattle &battle,
                                               const RumblingEntry &entry, const Garage &garage,
                                               const PartTables &parts, const GarageTables &tables);

} // namespace phantomcrash
