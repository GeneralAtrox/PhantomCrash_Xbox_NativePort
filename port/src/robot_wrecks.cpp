#include "phantomcrash/robot_wrecks.hpp"
#include "phantomcrash/money.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace phantomcrash {
RobotWreckSettings parse_robot_wreck_settings(const json::JsonValue &value) {
  RobotWreckSettings out;
  out.burn_seconds = static_cast<float>(value.number_or("burn_seconds", out.burn_seconds));
  out.reward_ny = static_cast<int>(value.number_or("reward_ny", out.reward_ny));
  out.break_effect = static_cast<int>(value.number_or("break_effect", out.break_effect));
  out.break_sound = value.string_or("break_sound", out.break_sound);
  if (!std::isfinite(out.burn_seconds) || out.burn_seconds < 0 || out.reward_ny < 0 ||
      out.reward_ny % ny_per_stored_unit != 0) {
    throw std::runtime_error(
        "robot_wrecks needs nonnegative burn_seconds and reward_ny in multiples of 10 NY");
  }
  return out;
}

void RobotWrecks::add(RobotWreck wreck) {
  items_.push_back(std::move(wreck));
}

void RobotWrecks::tick(const float seconds) {
  for (auto &wreck : items_) {
    wreck.burn_left = std::max(0.0F, wreck.burn_left - seconds);
  }
}

void RobotWrecks::set_collision(const std::size_t index, StageCollision collision) {
  auto &wreck = items_.at(index);
  if (wreck.removed || collision.walls().empty()) {
    return;
  }
  Vec3 low = collision.walls().front().points.front(), high = low;
  for (const auto &triangle : collision.walls()) {
    for (const Vec3 p : triangle.points) {
      low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
      high = {std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z)};
    }
  }
  wreck.centre = (low + high) * 0.5F;
  wreck.radius = length(high - low) * 0.5F;
  wreck.collision = std::move(collision);
}

bool RobotWrecks::destroy(const std::size_t index) {
  auto &wreck = items_.at(index);
  if (!wreck.solid()) {
    return false;
  }
  wreck.removed = true;
  wreck.collision.reset();
  return true;
}

float RobotWrecks::ground_height(const Vec3 at, const float step, const float radius) const {
  float height = no_ground;
  for (const auto &wreck : items_) {
    if (wreck.solid()) {
      height = std::max(height, wreck.collision->ground_height(at, step, radius));
    }
  }
  return height;
}

unsigned RobotWrecks::push_sphere(Vec3 &at, const float radius) const {
  unsigned contact = 0;
  for (const auto &wreck : items_) {
    if (wreck.solid()) {
      contact |= wreck.collision->push_sphere(at, radius);
    }
  }
  return contact;
}

std::optional<Vec3> RobotWrecks::segment_hit(const Vec3 from, const Vec3 to) const {
  std::optional<Vec3> nearest;
  for (const auto &wreck : items_) {
    if (wreck.solid()) {
      if (const auto at = wreck.collision->segment_hit(from, to);
          at && (!nearest || length(*at - from) < length(*nearest - from))) {
        nearest = at;
      }
    }
  }
  return nearest;
}
} // namespace phantomcrash
