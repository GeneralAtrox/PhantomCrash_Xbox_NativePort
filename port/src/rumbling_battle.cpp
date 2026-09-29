#include "phantomcrash/rumbling_battle.hpp"

#include "phantomcrash/json_fields.hpp"

#include <stdexcept>

namespace phantomcrash {

RumblingBattle parse_rumbling_battle(const json::JsonValue &document) {
  try {
    RumblingBattle battle;
    const auto &value = json::required(document, "battle");
    battle.return_to = value.string_or("return_to", battle.return_to);
    if (const json::JsonValue *lesson = value.find("lesson")) {
      battle.lesson.title = lesson->string_or("title", battle.lesson.title);
      battle.lesson.mode = static_cast<int>(lesson->number_or("mode", battle.lesson.mode));
    }
    return battle;
  } catch (const std::exception &error) {
    throw std::runtime_error(std::string("rumbling.json: ") + error.what());
  }
}

TrainingScript rumbling_training(const TrainingScript &base, const TestDriveScript &drive,
                                 const RumblingBattle &battle, const RumblingEntry &entry,
                                 const Garage &garage, const PartTables &parts,
                                 const GarageTables &tables) {
  TrainingScript script;
  const RobotRecord *rental = entry.rental ? tables.robot(entry.rental_robot) : nullptr;
  if (rental != nullptr) {
    // The rental is nobody's: a garage of its own holds it.
    Garage lent;
    install_robot(lent, 0, *rental, parts, tables);
    script = test_drive_training(base, drive, lent, 0, parts);
  } else {
    script = test_drive_training(base, drive, garage, garage.riding, parts);
  }
  script.return_to = battle.return_to;
  return script;
}

} // namespace phantomcrash
