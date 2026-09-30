// The runner's 3D battles: the training ground's lessons, TEST DRIVE, a
// RUMBLING entry's battle and the VS mode's, all on the training ground for
// now.

#include "desktop_game_screens.hpp"
#include "desktop_screens.hpp"
#include "desktop_training.hpp"
#include "desktop_training_robots.hpp"
#include "desktop_robot_preview.hpp"
#include "desktop_world_renderer.hpp"
#include "phantomcrash/robot_library.hpp"

#include <algorithm>
#include <utility>

namespace phantomcrash {

bool ScreenRunner::mouse_aim() const {
  return screen_ && screen_->mouse_aim();
}

std::unique_ptr<Screen> ScreenRunner::make_battle(const std::string &name) {
  if (const auto lesson = training_.lessons.find(name); lesson != training_.lessons.end()) {
    return make_training(training_, lesson->second);
  }
  if (name == "test_drive") {
    battle_training_ = test_drive_training(training_, test_drive_, quest_.garage,
                                           quest_.garage.riding, game_->part_tables());
    return make_training(battle_training_, test_drive_.lesson);
  }
  if (name == "rumbling_battle" || name == rumbling_battle_.quick_rental.screen) {
    std::optional<RumblingEntry> quick;
    if (name == rumbling_battle_.quick_rental.screen) {
      quick = prepare_quick_rental(rumbling_battle_.quick_rental, game_->contests(),
                                   game_->regulation_check().rules, game_->garage_tables(), quest_);
      if (!quick) {
        SDL_Log("Quick rental unavailable: check the preset, converted tables and entry fee");
        return game_->make("rumbling", "", place_before_, quest_);
      }
    }
    // The RUMBLING screen that entered is still the one showing.
    const RumblingEntry *entry = quick ? &*quick : screen_ ? screen_->rumbling_entry() : nullptr;
    if (entry == nullptr) {
      return nullptr;
    }
    active_match_ = *entry;
    battle_training_ =
        rumbling_training(training_, test_drive_, rumbling_battle_, *entry, quest_.garage,
                          game_->part_tables(), game_->garage_tables());
    RumblingWorld *live = rumbling_world();
    if (live == nullptr) {
      return make_training(battle_training_, rumbling_battle_.lesson, {}, -1, true);
    }
    // A direct Level Select entry has not passed through the menu's roster update.
    if (quick) {
      live->update(quest_, 0.0);
    }
    join_rumbling(*entry);
    return make_training(battle_training_, rumbling_battle_.lesson, live->lend(entry->area, quest_),
                         entry->area, true);
  }
  if (name == "vs_battle") {
    // The first entered player's Scoobee rides; a screen for each player.
    const VsSession &session = game_->vs_session();
    const auto rider = std::ranges::find_if(
        session.robots, [](const std::optional<RobotRecord> &robot) { return robot.has_value(); });
    if (rider == session.robots.end()) {
      return nullptr;
    }
    battle_training_ = vs_training(training_, test_drive_, vs_battle_, **rider,
                                   game_->part_tables(), game_->garage_tables());
    const auto players = static_cast<int>(std::ranges::count(session.joined, true));
    return make_training(battle_training_, vs_lesson(vs_battle_, players));
  }
  return nullptr;
}

PreviewWorld ScreenRunner::world() {
  if (!world_) {
    world_ = std::make_unique<WorldRenderer>(textures_.renderer(), roots_);
    robots_ = std::make_unique<RobotLibrary>(roots_);
    for (const std::string &error : robots_->errors()) {
      SDL_Log("%s", error.c_str());
    }
  }
  return {world_.get(), robots_.get()};
}

std::unique_ptr<Screen> ScreenRunner::make_training(const TrainingScript &script,
                                                    const TrainingLesson &lesson,
                                                    std::shared_ptr<Battle> live,
                                                    const int live_area,
                                                    const bool match_results) {
  static_cast<void>(world());
  const auto stage = stages_.find(script.stage);
  staged_training_ = script;
  if (stage != stages_.end()) {
    apply_stage(staged_training_.battle.scenes, stage->second);
  }
  const TrainingContext context{roots_,
                                world_.get(),
                                robots_.get(),
                                staged_training_,
                                stage == stages_.end() ? nullptr : &stage->second,
                                talk_,
                                talk_tuning_,
                                quest_.player_name,
                                options_.controls.standard(),
                                quest_.day,
                                quest_.money,
                                std::move(live),
                                rumbling_world_.get(),
                                live_area,
                                match_results};
  return std::make_unique<TrainingScreen>(textures_, font_, context, lesson);
}

std::vector<ScreenRunner::Level> ScreenRunner::levels() const {
  std::vector<std::pair<int, Level>> found; // FreePlay (mode 3) first
  for (const auto &[screen, lesson] : training_.lessons) {
    found.push_back({(lesson.mode + 1) % 4, {"Training Ground: " + lesson.title, screen}});
  }
  std::ranges::sort(found, {}, &std::pair<int, Level>::first);
  std::vector<Level> out;
  for (auto &entry : found) {
    out.push_back(std::move(entry.second));
  }
  const QuickRental &rental = rumbling_battle_.quick_rental;
  if (!rental.screen.empty()) {
    out.push_back({rental.label, rental.screen});
  }
  return out;
}

} // namespace phantomcrash
