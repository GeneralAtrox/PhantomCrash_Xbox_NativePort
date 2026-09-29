#include "phantomcrash/rumbling_battle.hpp"

#include "phantomcrash/json_fields.hpp"

#include <algorithm>
#include <stdexcept>

namespace phantomcrash {

RumblingBattle parse_rumbling_battle(const json::JsonValue &document) {
  try {
    RumblingBattle battle;
    const auto &value = json::required(document, "battle");
    battle.return_to = value.string_or("return_to", battle.return_to);
    if (const auto *quick = value.find("quick_rental")) {
      battle.quick_rental = {json::required(*quick, "label").string(),
                             json::required(*quick, "screen").string(),
                             static_cast<int>(json::required(*quick, "day").number()),
                             static_cast<int>(json::required(*quick, "area").number()),
                             static_cast<int>(json::required(*quick, "gate").number()),
                             static_cast<int>(json::required(*quick, "robot").number())};
    }
    if (const json::JsonValue *lesson = value.find("lesson")) {
      battle.lesson.title = lesson->string_or("title", battle.lesson.title);
      battle.lesson.mode = static_cast<int>(lesson->number_or("mode", battle.lesson.mode));
    }
    return battle;
  } catch (const std::exception &error) {
    throw std::runtime_error(std::string("rumbling.json: ") + error.what());
  }
}

std::optional<RumblingEntry> prepare_quick_rental(const QuickRental &preset,
                                                  const ContestTables &contests,
                                                  const RumblingRules &rules,
                                                  const GarageTables &garage, QuestSave &quest) {
  const RobotRecord *robot = garage.robot(preset.robot);
  if (preset.day < 0 || preset.area < 0 || preset.area >= calendar_venues || preset.gate < 0 ||
      robot == nullptr || !robot->complete() ||
      std::ranges::find(rules.rental_robots, preset.robot) == rules.rental_robots.end()) {
    return std::nullopt;
  }
  const int held = contest_on(contests, preset.day, preset.area, quest.cleared);
  const Contest *contest = contests.contest(held);
  const ContestClass *category = contest ? contests.contest_class(contest->contest_class) : nullptr;
  if (category == nullptr || !rental_allowed(rules, contest->contest_class) || category->fee < 0 ||
      quest.money < category->fee) {
    return std::nullopt;
  }
  quest.day = preset.day;
  quest.money -= category->fee;
  quest.rosters = {};
  quest.live_epoch = 0;
  return RumblingEntry{preset.area, held, preset.gate, true, preset.robot};
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
