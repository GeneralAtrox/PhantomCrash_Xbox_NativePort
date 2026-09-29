// Battle: drones that fight (DroneAi). A port addition for the live
// RUMBLING, where the opponents fight each other whether the player is
// there or not; the training ground's drones only walk their path.
#include "phantomcrash/battle.hpp"

#include <algorithm>
#include <cmath>

namespace phantomcrash {
namespace {

float number(const json::JsonValue &value, const char *key, const float fallback) {
  return static_cast<float>(value.number_or(key, fallback));
}

float flat_distance(const Vec3 a, const Vec3 b) {
  Vec3 offset = b - a;
  offset.y = 0.0F;
  return length(offset);
}

} // namespace

DroneAi parse_drone_ai(const json::JsonValue &value) {
  DroneAi ai;
  ai.fight = value.boolean_or("fight", ai.fight);
  ai.engage_range = number(value, "engage_range", ai.engage_range);
  ai.keep_distance = number(value, "keep_distance", ai.keep_distance);
  ai.distance_slack = number(value, "distance_slack", ai.distance_slack);
  ai.walk = number(value, "walk", ai.walk);
  ai.turn_gain = number(value, "turn_gain", ai.turn_gain);
  ai.face_degrees = number(value, "face_degrees", ai.face_degrees);
  return ai;
}

int Battle::ai_target(const Combatant &drone) const {
  const Vec3 at = drone.mover.position();
  int best = -1;
  float best_distance = settings_.ai.engage_range;
  for (std::size_t index = 0; index < robots_.size(); ++index) {
    const Combatant &other = robots_[index];
    if (&other == &drone || !other.alive || other.team == drone.team || other.camouflaged) {
      continue;
    }
    const float distance = flat_distance(at, other.mover.position());
    if (distance < best_distance) {
      best_distance = distance;
      best = static_cast<int>(index);
    }
  }
  return best;
}

// Turns to face the target and holds keep_distance from it; a shielded
// robot coming in is left alone until its shield is down, as the shots
// would do nothing.
RobotControl Battle::ai_control(Combatant &drone, const int target) {
  const DroneAi &ai = settings_.ai;
  RobotControl control;
  const Vec3 at = drone.mover.position();
  const Vec3 there = robots_[static_cast<std::size_t>(target)].mover.position();
  Vec3 offset = there - at;
  offset.y = 0.0F;
  const float distance = length(offset);
  if (distance < 1e-3F) {
    return control;
  }
  const float error = wrap_angle(heading_toward(offset) - drone.mover.heading());
  control.move[0] = std::clamp(error * ai.turn_gain, -1.0F, 1.0F);
  if (std::abs(error) < radians(ai.face_degrees)) {
    if (distance > ai.keep_distance + ai.distance_slack) {
      control.move[1] = ai.walk;
    } else if (distance < ai.keep_distance - ai.distance_slack) {
      control.move[1] = -ai.walk;
    }
  }
  return control;
}

// Every weapon with a lock fires; the locks are last tick's.
FireControl Battle::ai_fire(const Combatant &drone) {
  FireControl fire;
  for (std::size_t slot = 0; slot < weapon_slots; ++slot) {
    fire.fire[slot] = drone.weapons[slot] != nullptr && !drone.lost[slot] && drone.locks[slot] >= 0;
  }
  return fire;
}

void Battle::fire_drones(const float seconds) {
  for (std::size_t index = 1; index < robots_.size(); ++index) {
    const Combatant &robot = robots_[index];
    if (robot.drone && robot.alive && !robot.frozen && robot.mover.mode() != MoveMode::scripted) {
      fire_weapons(static_cast<int>(index), ai_fire(robot), seconds);
    }
  }
}

} // namespace phantomcrash
