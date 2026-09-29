#include "phantomcrash/battle.hpp"
#include "phantomcrash/battle_chatter.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace phantomcrash {
namespace {

float number(const json::JsonValue &value, const char *key, const float fallback) {
  return static_cast<float>(value.number_or(key, fallback));
}

// Height of the muzzles above the robot's ground point, per slot.
constexpr std::array<float, weapon_slots> muzzle_height{2.6F, 2.6F, 4.2F, 4.2F};
constexpr std::array<float, weapon_slots> muzzle_side{1.6F, -1.6F, 1.2F, -1.2F};

} // namespace

BattleSettings parse_battle_settings(const json::JsonValue &value) {
  BattleSettings s;
  if (const json::JsonValue *arena = value.find("arena")) {
    s.arena.half_size = number(*arena, "half_size", s.arena.half_size);
    if (const json::JsonValue *blocks = arena->find("blocks"); blocks && blocks->is_array()) {
      for (const json::JsonValue &block : blocks->array()) {
        const auto &b = block.array();
        if (b.size() != 4) {
          throw std::runtime_error("arena block needs {x0, z0, x1, z1}");
        }
        s.arena.blocks.push_back(
            {static_cast<float>(b[0].number()), static_cast<float>(b[1].number()),
             static_cast<float>(b[2].number()), static_cast<float>(b[3].number())});
      }
    }
  }
  if (const json::JsonValue *movement = value.find("movement")) {
    s.movement = parse_movement_tuning(*movement);
  }
  if (const json::JsonValue *weapons = value.find("weapons"); weapons && weapons->is_object()) {
    s.weapons = parse_weapon_tunings(*weapons);
  }
  s.robot_radius = number(value, "robot_radius", s.robot_radius);
  s.robot_height = number(value, "robot_height", s.robot_height);
  s.arena_wall_height = number(value, "arena_wall_height", s.arena_wall_height);
  s.drone_respawn_seconds = number(value, "drone_respawn_seconds", s.drone_respawn_seconds);
  s.drone_walk = number(value, "drone_walk", s.drone_walk);
  s.drone_reach = number(value, "drone_reach", s.drone_reach);
  s.camouflage_seconds = number(value, "camouflage_seconds", s.camouflage_seconds);
  s.camouflage_recharge_seconds =
      number(value, "camouflage_recharge_seconds", s.camouflage_recharge_seconds);
  s.camouflage_fade_seconds = number(value, "camouflage_fade_seconds", s.camouflage_fade_seconds);
  s.ammo_refill_seconds = number(value, "ammo_refill_seconds", s.ammo_refill_seconds);
  s.idle_chatter_seconds = number(value, "idle_chatter_seconds", s.idle_chatter_seconds);
  if (const json::JsonValue *shield = value.find("entry_shield")) {
    s.entry_shield = parse_entry_shield(*shield);
  }
  if (const json::JsonValue *ai = value.find("ai")) {
    s.ai = parse_drone_ai(*ai);
  }
  return s;
}

Battle::Battle(BattleSettings settings, EnemyPath path, const std::uint32_t seed)
    : settings_(std::move(settings)),
      stage_(StageCollision::box(settings_.arena, settings_.arena_wall_height)),
      path_(std::move(path)), random_(seed) {}

void Battle::set_stage(StageCollision stage) {
  stage_ = std::move(stage);
}

void Battle::set_doors(StageDoors doors) {
  doors_ = std::move(doors);
}

void Battle::set_radius(const int index, const float radius) {
  robots_.at(static_cast<std::size_t>(index)).radius = radius;
}

void Battle::set_tag(const int index, std::string tag) {
  robots_.at(static_cast<std::size_t>(index)).tag = std::move(tag);
}

int Battle::add_robot(const std::string &name, const int team, const Vec3 position,
                      const float heading, const std::array<std::string, weapon_slots> &weapons,
                      const float hp) {
  Combatant robot(settings_.movement);
  robot.name = name;
  robot.team = team;
  robot.piloted = robots_.empty(); // the player's robot comes first (player())
  robot.hp = robot.max_hp = hp;
  robot.radius = settings_.robot_radius;
  robot.mover.set_floor(stage_.ground_height(position, ground_snap));
  robot.mover.place(position, heading);
  robots_.push_back(std::move(robot));
  const int index = static_cast<int>(robots_.size()) - 1;
  arm(index, weapons);
  return index;
}

void Battle::set_movement(const int index, const MovementTuning &tuning) {
  if (index >= 0 && index < static_cast<int>(robots_.size())) {
    robots_[static_cast<std::size_t>(index)].mover.set_tuning(tuning);
  }
}

void Battle::set_drones_active(const bool active) {
  drones_active_ = active;
  for (Combatant &robot : robots_) {
    if (robot.drone) {
      robot.frozen = !active;
    }
  }
}

void Battle::place(const int index, const Vec3 position, const float heading) {
  RobotMover &mover = robots_.at(static_cast<std::size_t>(index)).mover;
  mover.set_floor(stage_.ground_height(position, ground_snap));
  mover.place(position, heading);
}

void Battle::script(const int index, const ScriptedStep &step) {
  robots_.at(static_cast<std::size_t>(index)).mover.script(step);
}

void Battle::release(const int index) {
  robots_.at(static_cast<std::size_t>(index)).mover.release();
}

void Battle::tell(const int index, const int event) {
  chatter(robots_.at(static_cast<std::size_t>(index)), event);
}

void Battle::place_facing_player(const int index, const float distance) {
  if (index <= 0 || index >= static_cast<int>(robots_.size())) {
    return;
  }
  const RobotMover &player = robots_.front().mover;
  Combatant &robot = robots_[static_cast<std::size_t>(index)];
  robot.alive = true;
  robot.hp = robot.max_hp;
  robot.damage_from.clear();
  robot.mover.place(player.position() + heading_forward(player.heading()) * distance,
                    player.heading() + pi);
}

Vec3 Battle::aim(const Combatant &robot) const {
  const float pitch = robot.mover.pitch();
  return heading_forward(robot.mover.aim_heading()) * std::cos(pitch) +
         Vec3{0.0F, std::sin(pitch), 0.0F};
}

Vec3 Battle::muzzle(const Combatant &robot, const WeaponSlot slot) const {
  const auto index = static_cast<std::size_t>(slot);
  const float heading = robot.mover.aim_heading();
  return robot.mover.position() + Vec3{0.0F, muzzle_height[index], 0.0F} +
         heading_right(heading) * muzzle_side[index] + heading_forward(heading) * 1.5F;
}

RobotControl Battle::drone_control(Combatant &drone) {
  RobotControl control;
  if (drone.frozen) {
    return control;
  }
  if (settings_.ai.fight) {
    if (const int target = ai_target(drone); target >= 0) {
      return ai_control(drone, target);
    }
  }
  if (!drone.path.active()) {
    return control;
  }
  const Vec3 at = drone.mover.position();
  Vec3 offset = drone.path.update(path_, at, settings_.drone_reach, random_) - at;
  offset.y = 0.0F;
  if (length(offset) < 1e-3F) {
    return control;
  }
  // Turn toward the next point, walking once roughly facing it.
  const float error = wrap_angle(heading_toward(offset) - drone.mover.heading());
  control.move[0] = std::clamp(error * 2.0F, -1.0F, 1.0F);
  control.move[1] = std::abs(error) < 0.7F ? settings_.drone_walk : 0.0F;
  return control;
}

// Two spheres against the stage (0x07c7c0): one resting on the ground,
// which can stand the robot on what it touches, and one under the top of
// the robot, which only pushes it aside or stops a jump.
void Battle::collide(Combatant &robot) {
  // The doors stop robots walking in the arena; a scripted walk goes
  // through them (0x08928c sets its position outright).
  StageDoors *doors = robot.mover.mode() != MoveMode::scripted ? this->doors() : nullptr;
  const auto push = [&](Vec3 &centre, const float radius) {
    unsigned contact = stage_.push_sphere(centre, radius);
    if (doors != nullptr) {
      contact |= doors->push_sphere(centre, radius);
    }
    return contact;
  };
  const float r = robot.radius;
  const float head_height = std::max(settings_.robot_height - r, r);
  Vec3 at = robot.mover.position();
  Vec3 foot = at + Vec3{0.0F, r, 0.0F};
  const unsigned foot_contact = push(foot, r);
  at = foot - Vec3{0.0F, r, 0.0F};
  Vec3 head = at + Vec3{0.0F, head_height, 0.0F};
  const unsigned head_contact = push(head, r);
  const float drop = at.y + head_height - head.y;
  at = {head.x, at.y - std::max(drop, 0.0F), head.z};
  robot.mover.set_position(at);
  if ((foot_contact & stage_contact::ground) != 0) {
    robot.mover.land(at.y);
  }
  if (drop > 0.0F) {
    robot.mover.bump_head();
  }
  robot.stage_contact = foot_contact | head_contact;
}

void Battle::separate() {
  for (std::size_t a = 0; a < robots_.size(); ++a) {
    for (std::size_t b = a + 1; b < robots_.size(); ++b) {
      if (!robots_[a].alive || !robots_[b].alive) {
        continue;
      }
      const float gap = robots_[a].radius + robots_[b].radius;
      Vec3 pa = robots_[a].mover.position();
      Vec3 pb = robots_[b].mover.position();
      Vec3 offset{pb.x - pa.x, 0.0F, pb.z - pa.z};
      const float distance = length(offset);
      if (distance >= gap || std::abs(pa.y - pb.y) > settings_.robot_height) {
        continue;
      }
      const Vec3 push = (distance > 1e-4F ? offset * (1.0F / distance) : Vec3{1.0F, 0.0F, 0.0F}) *
                        ((gap - distance) * 0.5F);
      robots_[a].mover.set_position(pa - push);
      robots_[b].mover.set_position(pb + push);
    }
  }
}

void Battle::tick(const RobotControl &player, const FireControl &fire, const float seconds) {
  events_.clear();
  if (doors_) {
    doors_->tick(seconds);
  }
  for (std::size_t index = 0; index < robots_.size(); ++index) {
    Combatant &robot = robots_[index];
    robot.hit_flash = std::max(0.0F, robot.hit_flash - seconds);
    if (!robot.alive) {
      if (robot.drone && robot.auto_respawn && (robot.respawn_left -= seconds) <= 0.0F) {
        respawn(robot);
      }
      continue;
    }
    robot.mover.set_floor(stage_.ground_height(robot.mover.position(), ground_snap));
    const RobotControl control = index == 0 ? player : drone_control(robot);
    robot.mover.tick(control, seconds);
    recover(robot, seconds);
  }
  separate();
  for (Combatant &robot : robots_) {
    if (robot.alive) {
      collide(robot);
    }
  }

  if (!robots_.empty() && robots_.front().alive) {
    Combatant &self = robots_.front();
    if (fire.camouflage) {
      self.camouflaged = !self.camouflaged && self.camouflage > 0.1F;
      if (self.camouflaged) {
        chatter(self, chatter_event::camouflage); // 0x074fa4
      }
    }
    const float drain = self.camouflaged ? -seconds / settings_.camouflage_seconds
                                         : seconds / settings_.camouflage_recharge_seconds;
    const float before = self.camouflage;
    self.camouflage = std::clamp(self.camouflage + drain, 0.0F, 1.0F);
    // 0x0751be: once, as a running camouflage's gauge falls through a quarter.
    if (self.camouflaged && before > camouflage_low_share &&
        self.camouflage <= camouflage_low_share) {
      chatter(self, chatter_event::camouflage_low);
    }
    self.camouflaged = self.camouflaged && self.camouflage > 0.0F;
    fire_weapons(0, fire, seconds);
    watch_idle(self, player, fire, seconds);
  }
  if (settings_.ai.fight) {
    fire_drones(seconds);
  }
  for (Combatant &robot : robots_) {
    if (robot.alive) {
      watch_ammo(robot);
    }
  }
  const float sweep = seconds / std::max(settings_.camouflage_fade_seconds, 1e-3F);
  for (Combatant &robot : robots_) {
    const float target = robot.camouflaged ? 1.0F : 0.0F;
    robot.camouflage_sweep += std::clamp(target - robot.camouflage_sweep, -sweep, sweep);
  }
  tick_supply(seconds);
  std::vector<WeaponHit> hits;
  armory_.tick(seconds, targets(), stage_, hits, doors());
  apply_hits(hits);
  for (const Landing &landing : armory_.take_landings()) {
    BattleEvent impact{landing.owner, BattleEventKind::impact};
    impact.weapon = landing.weapon;
    impact.at = landing.at;
    impact.on_robot = landing.on_robot;
    events_.push_back(impact);
  }
}

} // namespace phantomcrash
