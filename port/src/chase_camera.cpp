#include "phantomcrash/chase_camera.hpp"

#include "phantomcrash/robot_mover.hpp"

#include <algorithm>
#include <cmath>

namespace phantomcrash {
namespace {

float number(const json::JsonValue &value, const char *key, const float fallback) {
  return static_cast<float>(value.number_or(key, fallback));
}

} // namespace

ChaseCameraSettings parse_chase_camera(const json::JsonValue &value) {
  ChaseCameraSettings s;
  s.fov_degrees = number(value, "fov_degrees", s.fov_degrees);
  s.near_plane = number(value, "near_plane", s.near_plane);
  s.far_plane = number(value, "far_plane", s.far_plane);
  s.distance = number(value, "distance", s.distance);
  s.height = number(value, "height", s.height);
  s.look_height = number(value, "look_height", s.look_height);
  s.look_ahead = number(value, "look_ahead", s.look_ahead);
  s.follow_per_second = number(value, "follow_per_second", s.follow_per_second);
  s.pitch_follow = number(value, "pitch_follow", s.pitch_follow);
  s.wall_margin = number(value, "wall_margin", s.wall_margin);
  return s;
}

void ChaseCamera::tick(const Vec3 position, const float heading, const float pitch,
                       const float half_size, const float seconds, const bool snap) {
  if (snap) {
    heading_ = heading;
    pitch_ = pitch;
  } else {
    const float blend = 1.0F - std::exp(-settings_.follow_per_second * seconds);
    heading_ = wrap_angle(heading_ + wrap_angle(heading - heading_) * blend);
    pitch_ += (pitch - pitch_) * blend;
  }
  const Vec3 forward = heading_forward(heading_);
  eye_ = position + Vec3{0.0F, settings_.height, 0.0F} - forward * settings_.distance;
  const float limit = half_size - settings_.wall_margin;
  eye_.x = std::clamp(eye_.x, -limit, limit);
  eye_.z = std::clamp(eye_.z, -limit, limit);
  target_ = position + forward * settings_.look_ahead +
            Vec3{0.0F, settings_.look_height + pitch_ * settings_.pitch_follow, 0.0F};
}

void ChaseCamera::block_eye(const Vec3 from, const Vec3 hit) {
  const Vec3 back = from - hit;
  eye_ = hit + normalize(back) * std::min(settings_.wall_margin, length(back));
}

Mat4 ChaseCamera::view() const {
  if (!shot_) {
    return look_at(eye_, target_, {0.0F, 1.0F, 0.0F});
  }
  // The roll turns the up vector about the line of sight.
  const Vec3 look = normalize(shot_->target - shot_->eye);
  const Vec3 side = normalize(cross(look, {0.0F, 1.0F, 0.0F}));
  const Vec3 up = cross(side, look);
  return look_at(shot_->eye, shot_->target,
                 up * std::cos(shot_->roll) + side * std::sin(shot_->roll));
}

Mat4 ChaseCamera::projection(const float aspect) const {
  const float fov = shot_ ? shot_->fov_degrees : settings_.fov_degrees;
  return perspective(radians(fov), aspect, settings_.near_plane, settings_.far_plane);
}

} // namespace phantomcrash
