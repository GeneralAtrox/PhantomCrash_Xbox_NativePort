// A battle's line-up on the training screen (training_battle.hpp lineup,
// battle_lineup.hpp): each place is a battle robot seated with the robot
// and AI the line-up gives it, drawn as itself. One shot down leaves its
// place, which asks the line-up again once its death motion is over and
// the line-up's arrival time has passed; an empty place keeps asking at
// that interval.

#include "desktop_training.hpp"

#include "phantomcrash/battle_lineup.hpp"

#include <SDL3/SDL.h>

#include <exception>
#include <string>

namespace phantomcrash {

struct TrainingLineup {
  struct Place {
    int robot{-1}; // its battle robot
    bool seated{}; // a line-up robot stands or is falling there
    float wait{};  // seconds until an empty place asks again
    RobotParts look;
    MovementTuning movement;
  };
  std::shared_ptr<BattleLineup> source;
  std::vector<Place> places; // place 1 first
};

namespace {

// A robot without armour records still takes a few hits (as RUMBLING's).
constexpr float missing_armour = 1000.0F;

void seat(Battle &battle, const int index, const LineupRobot &entry) {
  const RumblingRobot &robot = entry.robot;
  std::array<std::string, weapon_slots> weapons{};
  for (std::size_t slot = 0; slot < weapon_slots; ++slot) {
    if (robot.weapons[slot]) {
      weapons[slot] = battle.add_weapon(*robot.weapons[slot]).name;
    }
  }
  // A team each: everyone out is against everyone else.
  battle.set_identity(index, robot.name, index,
                      robot.armour > 0.0F ? robot.armour : missing_armour);
  auto &brain = battle.robot(index).ai;
  brain.ability = entry.ability;
  brain.profile = entry.profile;
  brain.reset();
  battle.arm(index, weapons);
  battle.set_tag(index, robot.tag);
  battle.robot(index).marker_colour = entry.marker;
  battle.set_combat(index, robot.combat);
  battle.set_lock_seconds(index, robot.lock_seconds);
  battle.set_sight(index, robot.sight);
  battle.set_movement(index, robot.movement);
  battle.set_auto_respawn(index, false);
  battle.set_boss(index, false);
}

// Asks the line-up for place `place`'s robot; true when one is seated.
bool fill(Battle &battle, TrainingLineup &lineup, const std::size_t place) {
  TrainingLineup::Place &at = lineup.places[place];
  const auto entry = lineup.source->enter(static_cast<int>(place) + 1);
  if (!entry) {
    at.seated = false;
    at.wait = lineup.source->arrival_seconds();
    return false;
  }
  seat(battle, at.robot, *entry);
  SDL_Log("Line-up place %zu: %s (%s), ability %d", place + 1, entry->robot.name.c_str(),
          entry->robot.tag.c_str(), entry->ability);
  at.seated = true;
  at.look = entry->robot.look;
  at.movement = entry->robot.movement;
  return true;
}

} // namespace

void TrainingScreen::add_lineup() {
  const auto &source = script_.battle.lineup;
  if (!source || live_world_ != nullptr || vs_ || coop_match_ || online()) {
    return;
  }
  lineup_ = std::make_shared<TrainingLineup>();
  lineup_->source = source;
  for (int place = 1; place <= source->places(); ++place) {
    TrainingLineup::Place &at = lineup_->places.emplace_back();
    at.robot = battle_->add_drone("lineup" + std::to_string(place), missing_armour);
    if (!fill(*battle_, *lineup_, lineup_->places.size() - 1)) {
      battle_->take_out(at.robot);
    }
  }
}

// Draws each place as its robot, after build_battle's drone looks and
// movement.
void TrainingScreen::show_lineup() {
  if (!lineup_) {
    return;
  }
  const float fallback = script_.battle.settings.robot_radius;
  for (const TrainingLineup::Place &at : lineup_->places) {
    const auto index = static_cast<std::size_t>(at.robot);
    if (view_ && index < view_->size()) {
      try {
        view_->replace(index, at.look);
      } catch (const std::exception &error) {
        SDL_Log("Line-up robot %d: %s", at.robot, error.what());
        view_->replace(index, RobotParts{}); // a part out of range: a stand-in
      }
      battle_->set_radius(at.robot, view_->radius(index, fallback));
    }
    battle_->set_movement(at.robot, at.movement);
  }
}

void TrainingScreen::follow_lineup(const float seconds) {
  if (!lineup_) {
    return;
  }
  for (std::size_t place = 0; place < lineup_->places.size(); ++place) {
    TrainingLineup::Place &at = lineup_->places[place];
    const Combatant &robot = battle_->robot(at.robot);
    if (at.seated) {
      if (!robot.alive) {
        lineup_->source->leave(static_cast<int>(place) + 1,
                               robot.dying && robot.shot_by == player_index_);
        at.seated = false;
        at.wait = lineup_->source->arrival_seconds();
      }
      continue;
    }
    at.wait -= seconds;
    if (at.wait > 0.0F || robot.dying || !fill(*battle_, *lineup_, place)) {
      continue;
    }
    battle_->bring_in(at.robot);
    if (view_) {
      try {
        view_->replace(static_cast<std::size_t>(at.robot), at.look);
      } catch (const std::exception &) {
        view_->replace(static_cast<std::size_t>(at.robot), RobotParts{});
      }
      battle_->set_radius(at.robot, view_->radius(static_cast<std::size_t>(at.robot),
                                                  script_.battle.settings.robot_radius));
    }
  }
}

} // namespace phantomcrash
