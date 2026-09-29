// Battle: locking on, firing, and what the hits do.
#include "phantomcrash/battle.hpp"
#include "phantomcrash/battle_chatter.hpp"

#include <algorithm>
#include <cmath>

namespace phantomcrash {
namespace {

// A homing missile's rounds leave 10 frames apart (0x09b4ed).
constexpr float missile_round_seconds = 10.0F / part_frames_per_second;

// Missiles and grenades wait for the fire control's lock; guns and melee
// weapons use the target at once.
bool needs_lock_time(const WeaponTuning &weapon) {
  return weapon.kind == WeaponKind::missile || weapon.kind == WeaponKind::grenade;
}

// A trigger pull fires the weapon's rounds, each spending one round of
// ammunition, but a machine gun spends one a pull (0x09b4ed). Returns how
// many it fires: no more than are left.
int spend_rounds(const WeaponTuning &weapon, Magazine &magazine) {
  if (magazine.recharging()) {
    magazine.take(); // a beam's charge: one a pull
    return std::max(weapon.rounds, 1);
  }
  const int rounds = std::max(weapon.rounds, 1);
  if (weapon.hud_kind == hud_kind::machine_gun || !magazine.limited()) {
    magazine.take();
    return rounds;
  }
  const int fired = std::min(rounds, magazine.ammo);
  for (int round = 0; round < fired; ++round) {
    magazine.take();
  }
  return fired;
}

bool fires_one_at_a_time(const WeaponTuning &weapon) {
  return weapon.kind == WeaponKind::missile && weapon.homing;
}

// A robot's weapon on `slot`, or null when it has none or lost it.
const WeaponTuning *weapon_on(const Combatant &robot, const std::size_t slot) {
  return robot.lost[slot] ? nullptr : robot.weapons[slot];
}

} // namespace

std::vector<Target> Battle::targets() const {
  std::vector<Target> out;
  out.reserve(robots_.size());
  for (const Combatant &robot : robots_) {
    out.push_back({robot.centre(settings_.robot_height), robot.radius, robot.team, robot.alive});
  }
  add_supply_targets(out);
  return out;
}

int Battle::find_lock(const Combatant &robot, const WeaponTuning &weapon) const {
  const Vec3 from = robot.aim_origin();
  const Vec3 facing = heading_forward(robot.mover.aim_heading());
  int best = -1;
  float best_angle = radians(weapon.lock_degrees);
  for (std::size_t index = 0; index < robots_.size(); ++index) {
    const Combatant &other = robots_[index];
    if (!other.alive || other.team == robot.team || other.camouflaged) {
      continue;
    }
    Vec3 offset = other.centre(settings_.robot_height) - from;
    const float distance = length(offset);
    offset.y = 0.0F;
    if (distance > weapon.lock_range || length(offset) < 1e-3F) {
      continue;
    }
    const float angle = std::acos(std::clamp(dot(normalize(offset), facing), -1.0F, 1.0F));
    if (angle < best_angle) {
      best_angle = angle;
      best = static_cast<int>(index);
    }
  }
  return best;
}

void Battle::set_lock_seconds(const int index, const float seconds) {
  robots_.at(static_cast<std::size_t>(index)).lock_seconds = std::max(seconds, 0.0F);
}

int Battle::player_lock(const WeaponSlot slot) const {
  return robots_.empty() ? -1 : robots_.front().locks[static_cast<std::size_t>(slot)];
}

int Battle::player_lock_candidate(const WeaponSlot slot) const {
  return robots_.empty() ? -1 : robots_.front().lock_candidates[static_cast<std::size_t>(slot)];
}

float Battle::player_lock_held(const WeaponSlot slot) const {
  return robots_.empty() ? 0.0F : robots_.front().lock_held[static_cast<std::size_t>(slot)];
}

void Battle::update_lock(Combatant &robot, const std::size_t slot, const float seconds) {
  const WeaponTuning *weapon = weapon_on(robot, slot);
  const int candidate = weapon != nullptr ? find_lock(robot, *weapon) : -1;
  if (candidate != robot.lock_candidates[slot]) {
    robot.lock_candidates[slot] = candidate;
    robot.lock_held[slot] = 0.0F;
  } else if (candidate >= 0) {
    robot.lock_held[slot] += seconds;
  }
  const bool held = weapon != nullptr &&
                    (!needs_lock_time(*weapon) || robot.lock_held[slot] >= robot.lock_seconds);
  const int before = robot.locks[slot];
  robot.locks[slot] = held ? candidate : -1;
  // 0x051045: a missile lock just made on a player's robot warns it (robot
  // 0 is the player's).
  if (robot.locks[slot] == 0 && before != 0 && needs_lock_time(*weapon)) {
    chatter(robots_.front(), chatter_event::locked_on);
  }
}

// The muzzle's flash and sound (0x098bb7, 0x099c02): once a trigger pull,
// and once more for each round of a missile volley.
void Battle::fired(const int index, const std::size_t slot, const WeaponTuning &weapon) {
  const Combatant &robot = robots_[static_cast<std::size_t>(index)];
  BattleEvent event{index, BattleEventKind::fired};
  event.slot = static_cast<int>(slot);
  event.weapon = &weapon;
  event.at = muzzle(robot, static_cast<WeaponSlot>(slot));
  events_.push_back(event);
}

void Battle::fire_weapons(const int index, const FireControl &fire, const float seconds) {
  Combatant &robot = robots_[static_cast<std::size_t>(index)];
  const std::vector<Target> bodies = targets();
  std::vector<WeaponHit> hits;
  for (std::size_t slot = 0; slot < weapon_slots; ++slot) {
    robot.cooldown[slot] = std::max(0.0F, robot.cooldown[slot] - seconds);
    Magazine &magazine = robot.magazines[slot];
    magazine.tick(seconds);
    // Optional, off by default: an empty weapon refills after a while
    // (port choice; the original refills only from supply pickups).
    if (settings_.ammo_refill_seconds > 0.0F && magazine.empty() && !magazine.recharging() &&
        magazine.dry_seconds >= settings_.ammo_refill_seconds) {
      magazine.refill(magazine.max);
    }
    update_lock(robot, slot, seconds);
    const WeaponTuning *weapon = weapon_on(robot, slot);
    const auto mount = static_cast<WeaponSlot>(slot);
    if (weapon == nullptr) {
      robot.queued_rounds[slot] = 0;
    } else if (robot.queued_rounds[slot] > 0) {
      robot.queue_left[slot] -= seconds;
      for (; robot.queued_rounds[slot] > 0 && robot.queue_left[slot] <= 0.0F;
           --robot.queued_rounds[slot]) {
        armory_.fire(*weapon, index, robot.team, muzzle(robot, mount), aim(robot),
                     robot.locks[slot], bodies);
        fired(index, slot, *weapon);
        robot.queue_left[slot] += missile_round_seconds;
      }
    }
    // The mask is the player's; drones fire what they carry.
    const bool allowed = index != 0 || (allowed_weapons_ & (1U << slot)) != 0;
    if (weapon == nullptr || !fire.fire[slot] || !allowed || robot.cooldown[slot] > 0.0F ||
        magazine.empty()) {
      continue;
    }
    robot.cooldown[slot] = weapon->cooldown;
    const int rounds = spend_rounds(*weapon, magazine);
    if (weapon->hud_kind == hud_kind::missile || weapon->hud_kind == hud_kind::grenade) {
      chatter(robot, chatter_event::weapon_fired); // 0x09b5db
    }
    fired(index, slot, *weapon);
    if (weapon->kind == WeaponKind::melee) {
      armory_.strike(*weapon, index, muzzle(robot, mount), robot.locks[slot], bodies, hits);
    } else if (fires_one_at_a_time(*weapon) && rounds > 1) {
      armory_.fire(*weapon, index, robot.team, muzzle(robot, mount), aim(robot), robot.locks[slot],
                   bodies);
      robot.queued_rounds[slot] = rounds - 1;
      robot.queue_left[slot] = missile_round_seconds;
    } else {
      armory_.fire(*weapon, index, robot.team, muzzle(robot, mount), aim(robot), robot.locks[slot],
                   bodies, rounds);
    }
  }
  apply_hits(hits);
}

} // namespace phantomcrash
