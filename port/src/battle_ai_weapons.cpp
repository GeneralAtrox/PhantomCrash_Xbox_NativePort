// Original weapon request/timer policy (0x05b139 / 0x05b360).
#include "phantomcrash/battle.hpp"
#include "phantomcrash/battle_chatter.hpp"
#include "target_view.hpp"
#include <algorithm>

namespace phantomcrash {
void Battle::ai_weapons(Combatant &drone, const float seconds) {
  auto &s = drone.ai;
  const auto &data = *settings_.ai.original;
  const auto &level = data.levels[static_cast<std::size_t>(s.ability)];
  const Combatant *target = s.target >= 0 && s.target < static_cast<int>(robots_.size())
                                ? &robots_[static_cast<std::size_t>(s.target)]
                                : nullptr;
  // 0x05b360 serves a remembered target with nothing between them
  // (0x05b992, 0x04f152's blocked byte) and, above ability 0, in the
  // robot's view (the record's frustum byte); the sight angles of 0x05d11e
  // do not apply.
  bool can_aim = s.pickup < 0 && target && target->alive && target->shield <= 0;
  if (can_aim) {
    const Vec3 centre = target->centre(settings_.robot_height);
    const std::optional<TargetView> view = sight_view(drone);
    can_aim = clear_target_sight(drone.aim_origin(), centre) &&
              (s.ability < 1 || (view && in_target_view(*view, centre)));
  }
  const float distance =
      target ? length(target->centre(settings_.robot_height) - drone.aim_origin()) : 0;
  // The original gates new requests using the previous number of active
  // triggers; it does not permanently favour the first two weapon mounts.
  const auto active = std::ranges::count(s.fire, true);
  for (std::size_t slot = 0; slot < weapon_slots; ++slot) {
    s.fire_wait[slot] -= seconds;
    const auto *weapon = drone.weapons[slot];
    // 0x09d42f: the weapon's reach (+0x74, +0x78); a melee weapon's from
    // the target's surface. The lock it waits for below bounds it too.
    const float reach =
        weapon ? weapon->range + (weapon->kind == WeaponKind::melee && target ? target->radius : 0)
               : 0;
    const bool in_reach = weapon && distance <= reach && distance >= weapon->min_range;
    s.request[slot] = can_aim && weapon && !drone.lost[slot] && !drone.magazines[slot].empty() &&
                      in_reach && (active < level.max_weapon_requests || s.request[slot]);
    if (!s.request[slot]) {
      s.fire[slot] = false;
      s.fire_delay[slot] = level.fire_delay;
      continue;
    }
    // The native pose must finish raising the weapon before firing. Retain
    // the original trigger request until Battle::fired acknowledges it.
    if (s.fire[slot]) {
      continue;
    }
    s.fire_delay[slot] -= seconds;
    const unsigned flag = weapon->hud_kind == hud_kind::beam          ? 1U
                          : weapon->hud_kind == hud_kind::missile     ? 2U
                          : weapon->hud_kind == hud_kind::cannon      ? 4U
                          : weapon->hud_kind == hud_kind::machine_gun ? 8U
                                                                      : 0U;
    const bool forced = (s.forced_weapons & flag) != 0;
    if (s.fire_delay[slot] > 0 || (!forced && s.fire_wait[slot] > 0) ||
        drone.locks[slot] != s.target) {
      continue;
    }
    float probability = forced ? data.value("fire_probability_scale") : level.fire_probability;
    if (!forced && s.mode == AiMode::withdraw) {
      probability *= data.value("withdraw_fire_share");
    }
    if (ai_chance(random_, probability, data.value("fire_probability_scale"))) {
      s.fire[slot] = true;
      ++s.shots;
    }
    s.fire_wait[slot] = level.fire_interval * ai_random(random_);
  }
  s.camouflage_wait -= seconds;
  s.camouflage_left -= seconds;
  if (drone.camouflaged) {
    if (s.camouflage_left <= 0) {
      s.camouflage_toggle = true;
    }
  } else if (s.camouflage_wait <= 0 || s.threat) {
    if (drone.camouflage > data.value("cloak_minimum") &&
        (s.threat || (s.entrant != static_cast<int>(data.value("ranker_no_random_cloak")) &&
                      ai_chance(random_, level.camouflage_probability,
                                data.value("fire_probability_scale"))))) {
      s.camouflage_toggle = true;
      s.camouflage_left = (drone.combat.camouflage_seconds >= 0 ? drone.combat.camouflage_seconds
                                                                : settings_.camouflage_seconds) *
                          level.camouflage_duration * ai_random(random_);
    }
    s.camouflage_wait = level.camouflage_interval * ai_random(random_);
  }
}

FireControl Battle::ai_fire(const Combatant &drone) const {
  FireControl fire;
  if (!settings_.ai.original || drone.ai.target < 0) {
    return fire;
  }
  for (std::size_t slot = 0; slot < weapon_slots; ++slot) {
    const int lock = drone.locks[slot];
    if (!drone.ai.fire[slot] || lock != drone.ai.target || lock < 0 ||
        lock >= static_cast<int>(robots_.size())) {
      continue;
    }
    const auto &target = robots_[static_cast<std::size_t>(lock)];
    fire.fire[slot] = target.shield <= 0 && target_visible(drone, target);
  }
  return fire;
}

void Battle::fire_drones(const float seconds) {
  for (std::size_t index = 1; index < robots_.size(); ++index) {
    auto &robot = robots_[index];
    if (!robot.drone || !robot.alive || robot.frozen || robot.mover.mode() == MoveMode::scripted) {
      continue;
    }
    if (settings_.ai.original) {
      update_camouflage(robot, robot.ai.camouflage_toggle, seconds);
      robot.ai.camouflage_toggle = false;
    }
    fire_weapons(static_cast<int>(index), ai_fire(robot), seconds);
  }
}
} // namespace phantomcrash
