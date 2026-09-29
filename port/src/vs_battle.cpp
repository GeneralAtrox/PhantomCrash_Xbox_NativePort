#include "phantomcrash/vs_battle.hpp"

#include "phantomcrash/json_fields.hpp"

#include <algorithm>
#include <stdexcept>

namespace phantomcrash {

VsBattle parse_vs_battle(const json::JsonValue &document) {
  try {
    VsBattle battle;
    const auto &value = json::required(document, "battle");
    battle.return_to = value.string_or("return_to", battle.return_to);
    if (const json::JsonValue *lesson = value.find("lesson")) {
      battle.lesson.title = lesson->string_or("title", battle.lesson.title);
      battle.lesson.mode = static_cast<int>(lesson->number_or("mode", battle.lesson.mode));
    }
    return battle;
  } catch (const std::exception &error) {
    throw std::runtime_error(std::string("vs_battle.json: ") + error.what());
  }
}

TrainingLesson vs_lesson(const VsBattle &battle, const int players) {
  TrainingLesson lesson = battle.lesson;
  lesson.screens = std::clamp(players, 1, 4);
  return lesson;
}

TrainingScript vs_training(const TrainingScript &base, const TestDriveScript &drive,
                           const VsBattle &battle, const RobotRecord &robot,
                           const PartTables &parts, const GarageTables &tables) {
  // The Scoobee is nobody's garage's: a garage of its own holds it.
  Garage lent;
  install_robot(lent, 0, robot, parts, tables);
  TrainingScript script = test_drive_training(base, drive, lent, 0, parts);
  script.return_to = battle.return_to;
  return script;
}

} // namespace phantomcrash
