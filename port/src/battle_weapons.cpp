// The Armory: projectiles in flight, their hits and their blasts.
#include "phantomcrash/battle_weapons.hpp"

#include <algorithm>
#include <cmath>

namespace phantomcrash {
namespace {

// Port values: blast weapons without a record's blast damage arm after
// this long and then go off when an enemy's body comes within this share
// of the blast radius, so a missile fired point-blank does not circle its
// target.
constexpr float fuse_arming_seconds = 0.2F;
constexpr float proximity_fuse_share = 0.4F;
// Missiles home only after this long in flight (port value).
constexpr float homing_delay_seconds = 0.2F;
// Missiles leave upward and forward (the lesson at 0x26); port value.
constexpr float missile_climb = 0.6F;
// The steepest a lobbed round leaves.
constexpr float max_launch_degrees = 85.0F;
// Impact flashes (port values).
constexpr float impact_radius = 0.8F;
constexpr float impact_seconds = 0.2F;
constexpr float blast_flash_seconds = 0.6F;
constexpr float strike_radius = 1.5F;
constexpr float strike_seconds = 0.25F;

// The distance from `point` to the segment a..b.
float segment_distance(const Vec3 a, const Vec3 b, const Vec3 point) {
  const Vec3 ab = b - a;
  const float span = dot(ab, ab);
  const float t = span > 0.0F ? std::clamp(dot(point - a, ab) / span, 0.0F, 1.0F) : 0.0F;
  return length(a + ab * t - point);
}

// Turns unit vector `from` toward `to` by at most `angle` radians.
Vec3 turn_toward(const Vec3 from, const Vec3 to, const float angle) {
  const float cosine = std::clamp(dot(from, to), -1.0F, 1.0F);
  const float between = std::acos(cosine);
  if (between <= angle || between < 1e-4F) {
    return to;
  }
  const float t = angle / between;
  return normalize(from * (1.0F - t) + to * t);
}

// Unit vector `direction` raised by `degrees` above its own elevation.
Vec3 pitched_up(const Vec3 direction, const float degrees) {
  const Vec3 flat{direction.x, 0.0F, direction.z};
  const float across = length(flat);
  if (across < 1e-4F) {
    return direction;
  }
  const float elevation =
      std::min(std::atan2(direction.y, across) + radians(degrees), radians(max_launch_degrees));
  return flat * (std::cos(elevation) / across) + Vec3{0.0F, std::sin(elevation), 0.0F};
}

// Record blasts hurt every body they reach each tick; the others (port
// tuning) go off at once, hurting less with distance.
bool growing_blast(const WeaponTuning &weapon) {
  return weapon.blast_damage > 0.0F;
}

float blast_radius_at(const WeaponTuning &weapon, const float age) {
  if (weapon.blast_seconds <= 0.0F) {
    return weapon.blast_radius;
  }
  const float grown = std::min(age / weapon.blast_seconds, 1.0F);
  return weapon.blast_start_radius + (weapon.blast_radius - weapon.blast_start_radius) * grown;
}

// The way a hit at `from` pushes a body at `to`: level, away from it.
Vec3 push_direction(const Vec3 from, const Vec3 to) {
  const Vec3 away{to.x - from.x, 0.0F, to.z - from.z};
  return length(away) > 1e-4F ? normalize(away) : Vec3{};
}

// A projectile's level heading.
Vec3 flight_direction(const Vec3 velocity) {
  return push_direction(Vec3{}, velocity);
}

// Hurts every body within `blast`'s radius as it grows from its start
// radius to its full one (0x098298): an explosion hurts each body it
// touches by the blast damage every battle tick (projectile state 3).
void reach(const Blast &blast, const std::vector<Target> &targets, std::vector<WeaponHit> &hits) {
  const WeaponTuning &weapon = *blast.weapon;
  const float radius = blast_radius_at(weapon, blast.age);
  for (std::size_t index = 0; index < targets.size(); ++index) {
    const Target &body = targets[index];
    if (!body.alive || body.team == blast.team) {
      continue;
    }
    if (length(body.centre - blast.position) - body.radius < radius) {
      hits.push_back({static_cast<int>(index), blast.owner, weapon.blast_damage,
                      weapon.stagger_seconds, body.centre, &weapon,
                      push_direction(blast.position, body.centre), false});
    }
  }
}

// Hurts with `blast` for every battle tick due by its age, while it grows.
void pulse(Blast &blast, const std::vector<Target> &targets, std::vector<WeaponHit> &hits) {
  constexpr float tick = 1.0F / battle_ticks_per_second;
  constexpr float slack = 1e-4F; // float drift in the ages
  while (blast.next_pulse <= blast.age + slack &&
         blast.next_pulse < blast.weapon->blast_seconds - slack) {
    reach(blast, targets, hits);
    blast.next_pulse += tick;
  }
}

} // namespace

Vec3 lob_velocity(const Vec3 from, const Vec3 to, const float speed, const float gravity) {
  Vec3 flat = to - from;
  const float height = flat.y;
  flat.y = 0.0F;
  const float distance = length(flat);
  if (distance < 1e-3F || gravity <= 0.0F) {
    return normalize(to - from) * speed;
  }
  const Vec3 across = flat * (1.0F / distance);
  const float v2 = speed * speed;
  const float root = v2 * v2 - gravity * (gravity * distance * distance + 2.0F * height * v2);
  const float angle = root >= 0.0F ? std::atan((v2 - std::sqrt(root)) / (gravity * distance))
                                   : pi * 0.25F; // out of reach: throw as far as it goes
  return across * (speed * std::cos(angle)) + Vec3{0.0F, speed * std::sin(angle), 0.0F};
}

Vec3 Armory::scatter(const Vec3 direction, const float degrees) {
  if (degrees <= 0.0F) {
    return direction;
  }
  std::uniform_real_distribution<float> unit(0.0F, 1.0F);
  const float off = radians(degrees) * unit(random_);
  const float roll = 2.0F * pi * unit(random_);
  const Vec3 side = std::abs(direction.y) < 0.99F
                        ? normalize(cross(direction, Vec3{0.0F, 1.0F, 0.0F}))
                        : Vec3{1.0F, 0.0F, 0.0F};
  const Vec3 lift = cross(side, direction);
  return normalize(direction * std::cos(off) +
                   (side * std::cos(roll) + lift * std::sin(roll)) * std::sin(off));
}

Vec3 Armory::launch_velocity(const WeaponTuning &weapon, const Vec3 muzzle, const Vec3 direction,
                             const Target *locked) const {
  switch (weapon.kind) {
  case WeaponKind::missile: {
    const float start = weapon.start_speed > 0.0F ? weapon.start_speed : weapon.speed * 0.5F;
    return normalize(direction + Vec3{0.0F, missile_climb, 0.0F}) * start;
  }
  case WeaponKind::grenade: {
    // A record's lobbed round leaves at a fixed angle above the aim
    // (0x097ac9); without one, the port lobs it onto the target.
    if (weapon.launch_degrees > 0.0F) {
      return pitched_up(direction, weapon.launch_degrees) * weapon.speed;
    }
    const Vec3 landing =
        locked != nullptr ? locked->centre
                          : muzzle + Vec3{direction.x, 0.0F, direction.z} * (weapon.range * 0.5F);
    return lob_velocity(muzzle, landing, weapon.speed, weapon.gravity);
  }
  case WeaponKind::gun:
  case WeaponKind::melee:
    break;
  }
  return direction * weapon.speed;
}

void Armory::fire(const WeaponTuning &weapon, const int owner, const int team, const Vec3 muzzle,
                  const Vec3 aim, const int target, const std::vector<Target> &targets,
                  const int rounds) {
  if (weapon.kind == WeaponKind::melee) {
    return;
  }
  const bool valid = target >= 0 && target < static_cast<int>(targets.size());
  const Target *locked = valid ? &targets[static_cast<std::size_t>(target)] : nullptr;
  // Guns and grenades turn toward the lock; missiles leave along the aim.
  Vec3 direction = aim;
  if (locked != nullptr && weapon.kind != WeaponKind::missile) {
    direction = normalize(locked->centre - muzzle);
  }
  for (int round = 0; round < std::max(rounds, 1); ++round) {
    Projectile shot;
    shot.weapon = &weapon;
    shot.owner = owner;
    shot.team = team;
    shot.position = muzzle;
    shot.target = weapon.kind == WeaponKind::missile && weapon.homing && valid ? target : -1;
    const Vec3 heading = round == 0 ? direction : scatter(direction, weapon.spread_degrees);
    shot.velocity = launch_velocity(weapon, muzzle, heading, locked);
    projectiles_.push_back(shot);
  }
}

bool starts_impact(const WeaponTuning &weapon, const bool on_robot) noexcept {
  const int hit = on_robot ? weapon.robot_hit : weapon.surface_hit;
  return hit == shot_hit::burst || hit == shot_hit::explode || hit == shot_hit::strike ||
         hit == shot_hit::pierce;
}

bool Armory::strike(const WeaponTuning &weapon, const int owner, const Vec3 origin,
                    const int target, const std::vector<Target> &targets,
                    std::vector<WeaponHit> &hits) {
  if (target < 0 || target >= static_cast<int>(targets.size())) {
    return false;
  }
  const Target &victim = targets[static_cast<std::size_t>(target)];
  if (!victim.alive || length(victim.centre - origin) > weapon.range + victim.radius) {
    return false;
  }
  hits.push_back({target, owner, weapon.damage, weapon.stagger_seconds, victim.centre, &weapon,
                  push_direction(origin, victim.centre), true});
  explosions_.push_back({victim.centre, strike_radius, 0.0F, strike_seconds});
  landings_.push_back({&weapon, owner, victim.centre, true});
  return true;
}

void Armory::detonate(const Projectile &shot, const Vec3 at, const std::vector<Target> &targets,
                      std::vector<WeaponHit> &hits, const bool on_robot) {
  const WeaponTuning &weapon = *shot.weapon;
  landings_.push_back({&weapon, shot.owner, at, on_robot});
  if (weapon.blast_radius <= 0.0F) {
    explosions_.push_back({at, impact_radius, 0.0F, impact_seconds});
    return;
  }
  explosions_.push_back(
      {at, weapon.blast_radius, 0.0F, std::max(weapon.blast_seconds, blast_flash_seconds)});
  if (growing_blast(weapon)) {
    Blast blast{&weapon, shot.owner, shot.team, at};
    if (weapon.blast_seconds > 0.0F) {
      pulse(blast, targets, hits);
      blasts_.push_back(blast);
    } else {
      reach(blast, targets, hits); // a blast that does not grow hurts once
    }
    return;
  }
  for (std::size_t index = 0; index < targets.size(); ++index) {
    const Target &body = targets[index];
    if (!body.alive || body.team == shot.team) {
      continue;
    }
    const float distance = length(body.centre - at) - body.radius;
    if (distance < weapon.blast_radius) {
      const float falloff = 1.0F - std::max(distance, 0.0F) / weapon.blast_radius * 0.5F;
      hits.push_back({static_cast<int>(index), shot.owner, weapon.damage * falloff,
                      weapon.stagger_seconds, body.centre, &weapon, push_direction(at, body.centre),
                      false});
    }
  }
}

void Armory::spread_blasts(const float seconds, const std::vector<Target> &targets,
                           std::vector<WeaponHit> &hits) {
  for (Blast &blast : blasts_) {
    blast.age += seconds;
    pulse(blast, targets, hits);
  }
  std::erase_if(blasts_,
                [](const Blast &blast) { return blast.age >= blast.weapon->blast_seconds; });
}

void Armory::tick(const float seconds, const std::vector<Target> &targets,
                  const StageCollision &stage, std::vector<WeaponHit> &hits,
                  const StageDoors *doors) {
  for (Explosion &flash : explosions_) {
    flash.age += seconds;
  }
  std::erase_if(explosions_, [](const Explosion &flash) { return flash.age >= flash.life; });
  spread_blasts(seconds, targets, hits);

  std::vector<Projectile> flying;
  flying.reserve(projectiles_.size());
  for (Projectile shot : projectiles_) {
    const WeaponTuning &weapon = *shot.weapon;
    shot.age += seconds;
    if (weapon.kind == WeaponKind::missile) {
      const float acceleration = weapon.acceleration > 0.0F ? weapon.acceleration : weapon.speed;
      const float speed = std::min(weapon.speed, length(shot.velocity) + acceleration * seconds);
      Vec3 heading = normalize(shot.velocity);
      if (shot.target >= 0 && targets[static_cast<std::size_t>(shot.target)].alive &&
          shot.age > homing_delay_seconds) {
        const Vec3 wanted =
            normalize(targets[static_cast<std::size_t>(shot.target)].centre - shot.position);
        heading = turn_toward(heading, wanted, radians(weapon.turn_degrees_per_second) * seconds);
      }
      shot.velocity = heading * speed;
    }
    shot.velocity.y -= weapon.gravity * seconds;
    const Vec3 from = shot.position;
    shot.position = shot.position + shot.velocity * seconds;
    shot.travelled += length(shot.velocity) * seconds;

    bool spent = false;
    for (std::size_t index = 0; index < targets.size() && !spent; ++index) {
      const Target &body = targets[index];
      if (!body.alive || body.team == shot.team) {
        continue;
      }
      const float gap = segment_distance(from, shot.position, body.centre) - body.radius;
      const bool fused = weapon.blast_radius > 0.0F && !growing_blast(weapon) &&
                         shot.age > fuse_arming_seconds &&
                         gap <= weapon.blast_radius * proximity_fuse_share;
      if (gap <= 0.0F || fused) {
        // Port-tuned blast weapons hurt only through their blast.
        const bool direct = weapon.blast_radius <= 0.0F || growing_blast(weapon);
        if (direct) {
          hits.push_back({static_cast<int>(index), shot.owner, weapon.damage,
                          weapon.stagger_seconds, body.centre, &weapon,
                          flight_direction(shot.velocity), true});
        }
        detonate(shot, shot.position, targets, hits, true);
        spent = true;
      }
    }
    if (!spent) {
      std::optional<Vec3> wall = stage.segment_hit(from, shot.position);
      // A shut door leaf is a wall too (0x06f82e's triangles).
      if (const std::optional<Vec3> leaf =
              doors != nullptr ? doors->segment_hit(from, wall.value_or(shot.position))
                               : std::nullopt) {
        wall = leaf;
      }
      if (wall) {
        detonate(shot, *wall, targets, hits); // a wall, a door or the floor
        spent = true;
      }
    }
    const bool expired = weapon.life_seconds > 0.0F ? shot.age >= weapon.life_seconds
                                                    : shot.travelled > weapon.range * 1.5F;
    if (!spent && expired) {
      if (weapon.blast_radius > 0.0F) {
        detonate(shot, shot.position, targets, hits); // blast weapons go off at the end
      }
      spent = true;
    }
    if (!spent) {
      flying.push_back(shot);
    }
  }
  projectiles_ = std::move(flying);
}

void Armory::clear() {
  projectiles_.clear();
  blasts_.clear();
  explosions_.clear();
  landings_.clear();
}

} // namespace phantomcrash
