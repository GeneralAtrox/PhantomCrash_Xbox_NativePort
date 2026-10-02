// Original NPC callback dispatcher and perception (0x057c71 / 0x05b9c1).
#include "phantomcrash/battle.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace phantomcrash {
DroneAi parse_drone_ai(const json::JsonValue &value) {
  DroneAi ai;
  ai.fight = value.boolean_or("fight", false);
  ai.trace = value.boolean_or("trace", false);
  return ai;
}

void Battle::ai_sense(Combatant &drone, const float seconds) {
  auto &s = drone.ai;
  const auto &data = *settings_.ai.original;
  const auto &level = data.levels[static_cast<std::size_t>(s.ability)];
  s.detect_left -= seconds;
  if (s.detect_left <= 0) {
    s.cloak_detection =
        ai_chance(random_, static_cast<float>(s.ability + 1), data.value("cloak_detect_scale"));
    s.detect_left = data.value("cloak_detect_seconds");
  }
  const auto eligible = [&](const Combatant &other) {
    return &other != &drone && other.alive && other.team != drone.team &&
           other.mover.mode() != MoveMode::scripted;
  };
  const auto sees = [&](const Combatant &other) {
    if (!eligible(other)) {
      return false;
    }
    const Vec3 offset = other.centre(settings_.robot_height) - drone.aim_origin();
    const float distance = length(offset);
    if (distance > data.value("acquire_range")) {
      return false;
    }
    const float yaw = std::abs(wrap_angle(heading_toward(offset) - drone.mover.aim_heading()));
    const float pitch = std::atan2(offset.y, std::hypot(offset.x, offset.z));
    if (yaw > level.sight_yaw || std::abs(pitch - drone.mover.pitch()) > level.sight_pitch) {
      return false;
    }
    if (other.camouflaged && !s.cloak_detection) {
      const bool firing = std::ranges::any_of(other.cooldown, [](float v) { return v > 0; });
      if ((distance > data.value("cloak_proximity") && !firing) ||
          !ai_chance(random_, static_cast<float>(s.ability), data.value("cloak_detect_scale"))) {
        return false;
      }
    }
    return clear_target_sight(drone.aim_origin(), other.centre(settings_.robot_height));
  };
  s.visible = false;
  if (s.target >= 0 && s.target < static_cast<int>(robots_.size())) {
    const auto &old = robots_[static_cast<std::size_t>(s.target)];
    if (!eligible(old)) {
      s.target = -1;
    } else {
      s.visible = sees(old);
    }
  } else {
    s.target = -1;
  }
  // Keep the sensed target, then check the player and the closest visible rival.
  if (!s.visible) {
    int candidate = -1;
    float nearest = data.value("acquire_range");
    for (std::size_t i = 0; i < robots_.size(); ++i) {
      const float distance = length(robots_[i].mover.position() - drone.mover.position());
      if (distance < nearest && sees(robots_[i])) {
        candidate = static_cast<int>(i);
        nearest = distance;
        if (i == 0) {
          break;
        }
      }
    }
    if (candidate >= 0) {
      if (s.target != candidate) {
        ++s.detections;
      }
      s.target = candidate;
      s.visible = true;
    }
  }
  if (s.visible) {
    s.memory = level.memory_seconds;
    s.last_seen = robots_[static_cast<std::size_t>(s.target)].centre(settings_.robot_height);
    s.search = -1;
  } else {
    s.memory -= seconds;
    if (s.memory <= 0) {
      s.target = -1;
    }
  }
  // Damage can provoke a turn/search, but never grants a lock through scenery.
  if (drone.hp < s.previous_hp && !s.visible &&
      ai_chance(random_, static_cast<float>(s.ability), data.value("cloak_detect_scale"))) {
    int attacker = -1;
    float most = 0;
    for (std::size_t i = 0; i < drone.damage_from.size() && i < robots_.size(); ++i) {
      if (drone.damage_from[i] > most && eligible(robots_[i])) {
        attacker = static_cast<int>(i);
        most = drone.damage_from[i];
      }
    }
    if (attacker >= 0) {
      s.target = attacker;
      s.last_seen = robots_[static_cast<std::size_t>(attacker)].centre(settings_.robot_height);
      s.memory = level.memory_seconds;
      ai_enter(s, AiMode::engage, data, random_);
    }
  }
}

RobotControl Battle::ai_control(Combatant &drone, const float seconds) {
  auto &s = drone.ai;
  const auto &data = *settings_.ai.original;
  const float step = 1.0F / data.value("tick_hz");
  if (!s.initialized) {
    s.ability = std::clamp(s.ability, 0, 99);
    if (s.profile < 0 || s.profile >= static_cast<int>(data.profiles.size())) {
      throw std::runtime_error("NPC has an invalid original AI profile");
    }
    s.initialized = true;
    s.previous_hp = drone.hp;
    s.last_position = drone.mover.position();
    ai_enter(s, AiMode::route, data, random_);
    s.camouflage_wait =
        data.levels[static_cast<std::size_t>(s.ability)].camouflage_interval * ai_random(random_);
    if (settings_.ai.trace) {
      std::clog << "Original AI: entrant=" << s.entrant << " ability=" << s.ability
                << " profile=" << s.profile << " name=" << drone.name << '\n';
    }
  }
  // Analogue controls persist between AI updates; pressed buttons are pulses.
  s.control.dodge = 0;
  s.camouflage_toggle = false;
  s.accumulator += seconds;
  while (s.accumulator + 0.000001F >= step) {
    s.accumulator -= step;
    ++s.ticks;
    s.guard -= step;
    s.state_left -= step;
    s.action_left -= step;
    ai_sense(drone, step);
    bool armed = false;
    for (std::size_t slot = 0; slot < weapon_slots; ++slot) {
      armed |= drone.weapons[slot] && !drone.lost[slot] &&
               (!drone.magazines[slot].empty() || drone.magazines[slot].recharging());
    }
    if (s.pickup < 0) {
      ai_decide(s, data, drone.hp, drone.max_hp, armed, random_);
    }
    s.previous_hp = drone.hp;
    if (s.pickup < 0) {
      ai_move(drone, step);
    }
    ai_pickup(drone, step);
    ai_reactions(drone, step);
    ai_weapons(drone, step);
    s.diagnostics_left -= step;
    if (settings_.ai.trace && s.diagnostics_left <= 0) {
      s.diagnostics_left = data.value("trace_interval");
      std::clog << "Original AI: entrant=" << s.entrant << " ticks=" << s.ticks
                << " state=" << static_cast<int>(s.mode) << " target=" << s.target
                << " visible=" << s.visible << " changes=" << s.transitions << " hp=" << drone.hp
                << " requests=" << s.shots << " attacks=" << s.attacks << " dodges=" << s.dodges
                << " pickups=" << s.pickup_selections << " recoveries=" << s.recoveries << '\n';
    }
  }
  return s.control;
}
} // namespace phantomcrash
