// Port addition: computer robots clear robot wrecks. The original leaves a
// wreck standing until someone shoots it; here a robot with no enemy held
// turns to the nearest wreck in reach and shoots it apart, so the places
// stay clear without wrecks timing out (robot_wrecks.ai_clearing).
#include "phantomcrash/battle.hpp"
#include <algorithm>
#include <cmath>

namespace phantomcrash {
namespace {
float stick(const float error, const float speed, const float seconds) {
  return speed > 0 ? std::clamp(error / (speed * seconds), -1.0F, 1.0F) : 0;
}
} // namespace

bool Battle::wreck_in_sight(const Vec3 from, const std::size_t index) const {
  const RobotWreck &wreck = wrecks_.items()[index];
  const Vec3 to = wreck.centre;
  if (stage_.segment_hit(from, to) || (doors_ && doors_->segment_hit(from, to))) {
    return false;
  }
  if (objects_ && std::ranges::any_of(objects_->objects(), [&](const StageObject &object) {
        return object.solid() && object.collision.segment_hit(from, to).has_value();
      })) {
    return false;
  }
  // The first wreck the line strikes must be this one.
  const auto hit = wrecks_.segment_hit(from, to);
  return !hit || length(*hit - to) <= wreck.radius;
}

void Battle::ai_clear_wreck(Combatant &drone, const float seconds) {
  auto &s = drone.ai;
  const WreckClearing &rules = settings_.robot_wrecks.ai_clearing;
  const auto wrecks = wrecks_.items();
  const int count = static_cast<int>(wrecks.size());
  // A ranged weapon with rounds to spare: a limited magazine keeps its
  // reserve for the robots.
  const auto ranged = [&](const std::size_t slot) {
    const WeaponTuning *weapon = drone.lost[slot] ? nullptr : drone.weapons[slot];
    const Magazine &magazine = drone.magazines[slot];
    return weapon != nullptr && weapon->kind != WeaponKind::melee && !magazine.empty() &&
           (!magazine.limited() || static_cast<float>(magazine.ammo) >
                                       rules.ammo_reserve * static_cast<float>(magazine.max));
  };
  bool armed = false;
  for (std::size_t slot = 0; slot < weapon_slots; ++slot) {
    armed |= ranged(slot);
  }
  // An enemy, a supply box or a jump comes first.
  if (!rules.enabled || !armed || s.target >= 0 || s.pickup >= 0 || s.jump_phase != 0 ||
      s.mode == AiMode::route_special) {
    s.wreck = -1;
    return;
  }
  if (s.wreck >= 0) {
    s.wreck_left -= seconds;
    if (s.wreck >= count || !wrecks[static_cast<std::size_t>(s.wreck)].solid()) {
      ++s.wrecks_cleared; // broken (by this robot or another)
      s.wreck = -1;
    } else if (s.wreck_left <= 0) {
      s.wreck_skipped = s.wreck;
      s.wreck = -1;
    }
  }
  const Vec3 eye = drone.aim_origin();
  if (s.wreck < 0) {
    float nearest = rules.range;
    for (int index = 0; index < count; ++index) {
      const RobotWreck &wreck = wrecks[static_cast<std::size_t>(index)];
      const float distance = length(wreck.centre - eye);
      if (index != s.wreck_skipped && wreck.solid() && distance < nearest &&
          wreck_in_sight(eye, static_cast<std::size_t>(index))) {
        nearest = distance;
        s.wreck = index;
      }
    }
    if (s.wreck < 0) {
      return;
    }
    s.wreck_left = rules.give_up_seconds;
  }
  const RobotWreck &wreck = wrecks[static_cast<std::size_t>(s.wreck)];
  const Vec3 offset = wreck.centre - eye;
  const float distance = length(offset);
  const float flat = std::hypot(offset.x, offset.z);
  const float pitch = std::atan2(offset.y, flat);
  const float yaw = wrap_angle(heading_toward(offset) - drone.mover.heading());
  const auto &motion = drone.mover.tuning();
  s.control = {};
  s.control.reset_twist = true;
  s.control.move[0] = stick(yaw, radians(motion.turn_degrees_per_second), seconds);
  s.control.pitch = stick(pitch - drone.mover.pitch(), motion.pitch_per_second, seconds);
  if (flat > rules.stand_off &&
      std::abs(yaw) < settings_.ai.original->value("route_turn_radians")) {
    s.control.move[1] = 1;
  }
  s.control.jump = (drone.stage_contact & stage_contact::wall) != 0;
  // Fire once the aim, not only the body, is on the wreck.
  const float tolerance = radians(rules.aim_degrees);
  const bool aimed =
      std::abs(wrap_angle(heading_toward(offset) - drone.mover.aim_heading())) < tolerance &&
      std::abs(pitch - drone.mover.pitch()) < tolerance &&
      wreck_in_sight(eye, static_cast<std::size_t>(s.wreck));
  for (std::size_t slot = 0; aimed && slot < weapon_slots; ++slot) {
    if (ranged(slot) && distance <= drone.weapons[slot]->range &&
        distance >= drone.weapons[slot]->min_range) {
      s.fire[slot] = true;
    }
  }
  s.held_triggers = static_cast<int>(std::ranges::count(s.fire, true));
}
} // namespace phantomcrash
