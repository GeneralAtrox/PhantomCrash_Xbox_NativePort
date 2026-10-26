#include "phantomcrash/chase_camera.hpp"

#include "phantomcrash/robot_mover.hpp"
#include "phantomcrash/skeleton.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace phantomcrash {
namespace {

// Tick counts summed from frame times reach a whole tick a little short.
constexpr float tick_slack = 1e-3F;

float number(const json::JsonValue &value, const char *key, const float fallback) {
  return static_cast<float>(value.number_or(key, fallback));
}

std::optional<float> optional_number(const json::JsonValue &value, const char *key) {
  if (value.find(key) == nullptr) {
    return std::nullopt;
  }
  return number(value, key, 0.0F);
}

} // namespace

ChaseCameraSettings parse_chase_camera(const json::JsonValue &value, ChaseCameraSettings base) {
  ChaseCameraSettings s = std::move(base);
  s.fov_degrees = number(value, "fov_degrees", s.fov_degrees);
  s.near_plane = number(value, "near_plane", s.near_plane);
  s.far_plane = number(value, "far_plane", s.far_plane);
  s.tick_rate = number(value, "tick_rate", s.tick_rate);
  s.follow = number(value, "follow", s.follow);
  s.pitch_limit_degrees = number(value, "pitch_limit_degrees", s.pitch_limit_degrees);
  s.behind = number(value, "behind", s.behind);
  s.look_ahead = number(value, "look_ahead", s.look_ahead);
  s.height = number(value, "height", s.height);
  s.pitch_lift = number(value, "pitch_lift", s.pitch_lift);
  if (value.find("eye_height") != nullptr) {
    s.eye_height = number(value, "eye_height", 0.0F);
  }
  s.eye_follow_ticks = number(value, "eye_follow_ticks", s.eye_follow_ticks);
  s.wall_margin = number(value, "wall_margin", s.wall_margin);
  s.robot_clearance = number(value, "robot_clearance", s.robot_clearance);
  s.clearance_lift = number(value, "clearance_lift", s.clearance_lift);
  s.eye_radius = number(value, "eye_radius", s.eye_radius);
  s.robot_top = number(value, "robot_top", s.robot_top);
  if (const json::JsonValue *air = value.find("air"); air != nullptr && air->is_object()) {
    s.air = SourceAirView{number(*air, "rise", 0.0F), number(*air, "look_scale", 1.0F),
                          number(*air, "ease", 0.0F), number(*air, "updates", 0.0F)};
  }
  if (const json::JsonValue *wall = value.find("wall"); wall != nullptr && wall->is_object()) {
    s.wall = SourceWallView{number(*wall, "margin", 0.0F),       number(*wall, "reach", 0.0F),
                            number(*wall, "normal_limit", 0.0F), number(*wall, "ease", 0.0F),
                            number(*wall, "rise", 0.0F),         number(*wall, "air_rise", 0.0F),
                            number(*wall, "air_climb", 0.0F)};
  }
  return s;
}

std::optional<SourceChaseEye> parse_source_chase_eye(const json::JsonValue *value) {
  if (value == nullptr || !value->is_object()) {
    return std::nullopt;
  }
  return SourceChaseEye{number(*value, "behind", 0.0F), number(*value, "eye_height", 0.0F),
                        optional_number(*value, "min_eye_height"),
                        optional_number(*value, "wall_rise")};
}

void ChaseCamera::tick(const CameraSubject &subject, const StageCollision &stage,
                       const float seconds, const bool snap) {
  const float ticks = seconds * settings_.tick_rate;
  const float pitch_limit = radians(settings_.pitch_limit_degrees);
  const float pitch = std::clamp(subject.pitch, -pitch_limit, pitch_limit);
  if (snap) {
    yaw_ = subject.yaw;
    pitch_ = pitch;
  } else {
    // 0.3 of the way a tick, at any presentation rate.
    const float ease = 1.0F - std::pow(1.0F - settings_.follow, ticks);
    yaw_ = wrap_angle(yaw_ + wrap_angle(subject.yaw - yaw_) * ease);
    pitch_ += (pitch - pitch_) * ease;
  }
  // 0x07edfa: pitch, then turn; -z is ahead.
  const Mat4 aim = rotation_x(pitch_) * rotation_y(yaw_);
  // 0x260344: in the air, rising or a few ticks up, the S.L.A.I. view
  // lifts its eye and draws its look point in.
  bool air_view = false;
  if (settings_.air) {
    const SourceAirView &air = *settings_.air;
    air_ticks_ = subject.grounded ? 0.0F : air_ticks_ + ticks;
    air_view =
        !subject.grounded && (subject.climb >= 0.0F || air_ticks_ + tick_slack >= air.updates);
    const float step = snap ? 1.0F : air.ease * ticks;
    look_scale_ = air_view ? std::max(look_scale_ - step, air.look_scale)
                           : std::min(look_scale_ + step, 1.0F);
  }
  const float lift = settings_.height + settings_.pitch_lift * std::fabs(pitch_);
  target_ =
      subject.position + transform_point({0.0F, lift, -settings_.look_ahead}, aim) * look_scale_;
  const float pitch_rise = settings_.pitch_lift * std::fabs(pitch_);
  Vec3 wanted{};
  if (settings_.eye_height) {
    // 0x260070: the legs' row, else the stage's.
    const SourceChaseEye source =
        subject.source_eye.value_or(SourceChaseEye{settings_.behind, *settings_.eye_height});
    wanted = subject.position +
             transform_point({0.0F, source.eye_height + pitch_rise, source.behind}, aim);
  } else {
    wanted = subject.torso + transform_point({0.0F, lift, settings_.behind}, aim);
  }
  const float r = subject.height;
  const float top = settings_.robot_top;
  // In the air the eye rises to look down on the robot, and the sight
  // line starts one leg radius lower.
  if (settings_.air) {
    wanted.y += air_view ? settings_.air->rise : 0.0F;
  } else if (!subject.grounded) {
    wanted.y += top - 0.5F * r;
  }
  // 0x2605a0: legs row 33 keeps the eye above the feet.
  if (subject.source_eye && subject.source_eye->min_eye_height) {
    wanted.y = std::max(wanted.y, subject.position.y + *subject.source_eye->min_eye_height);
  }
  anchor_ = subject.position + Vec3{0.0F, subject.grounded ? top : top - r, 0.0F};
  if (settings_.wall) {
    follow_source(subject, stage, wanted, ticks, snap);
    return;
  }
  // 0x053db4: a wall on the sight line brings the eye off it.
  if (const auto hit = stage.segment_contact(anchor_, wanted)) {
    wanted = hit->position + hit->normal * settings_.wall_margin;
  }
  const Vec3 from = eye_;
  if (snap) {
    eye_ = wanted;
  } else {
    // A third of the gap a tick (0x0805a0(3.0)).
    const float keep = 1.0F - 1.0F / std::max(settings_.eye_follow_ticks, 1.0F);
    eye_ = eye_ + (wanted - eye_) * (1.0F - std::pow(keep, ticks));
  }
  // Too near the robot's lower sphere, the eye rises.
  const Vec3 centre = subject.position + Vec3{0.0F, r, 0.0F};
  if (length(eye_ - centre) < r + settings_.robot_clearance) {
    eye_.y += settings_.clearance_lift * ticks;
  }
  // 0x052c1f: the eye's sphere stops at the stage on its way.
  if (!snap) {
    if (const auto hit = stage.segment_contact(from, eye_)) {
      eye_ = hit->position + hit->normal * settings_.eye_radius;
    }
  }
  static_cast<void>(stage.push_sphere(eye_, settings_.eye_radius));
}

void ChaseCamera::follow_source(const CameraSubject &subject, const StageCollision &stage,
                                Vec3 wanted, const float ticks, const bool snap) {
  const SourceWallView &wall = *settings_.wall;
  const auto hit = stage.segment_contact(anchor_, wanted);
  if (hit) {
    wanted = hit->position + hit->normal * wall.margin;
  }
  if (snap) {
    eye_ = wanted;
    return;
  }
  // 0x2607a8: a share of the gap a tick.
  const float share = 1.0F - std::pow(1.0F - wall.ease, ticks);
  const Vec3 gap = wanted - eye_;
  Vec3 eye = eye_ + gap * share;
  const Vec3 across{eye.x - subject.position.x, 0.0F, eye.z - subject.position.z};
  if (!hit || length(across) >= wall.reach) {
    eye_ = eye;
    return;
  }
  // 0x260810: off a surface not facing down, the eye rises from it. The
  // original's further rise with the camera's pitch is not carried over.
  if (-hit->normal.y < wall.normal_limit) {
    float rise = subject.source_eye ? subject.source_eye->wall_rise.value_or(wall.rise) : wall.rise;
    if (!subject.grounded) {
      rise += wall.air_rise + wall.air_climb * subject.climb / settings_.tick_rate;
    }
    eye.y = eye_.y + gap.y * (1.0F - std::pow(1.0F - wall.ease * wall.ease, ticks)) +
            rise * wall.ease * ticks;
  }
  // 0x2609c4: the second sight line.
  if (const auto again = stage.segment_contact(anchor_, eye)) {
    eye = eye_ + (again->position + again->normal * wall.margin - eye_) * share;
  }
  eye_ = eye;
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
