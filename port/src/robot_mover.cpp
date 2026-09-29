#include "phantomcrash/robot_mover.hpp"

#include <algorithm>
#include <cmath>

namespace phantomcrash {
namespace {

float number(const json::JsonValue &value, const char *key, const float fallback) {
  return static_cast<float>(value.number_or(key, fallback));
}

// Moves `value` toward `target` by at most `step`.
float approach(const float value, const float target, const float step) {
  if (value < target) {
    return std::min(value + step, target);
  }
  return std::max(value - step, target);
}

} // namespace

MovementTuning parse_movement_tuning(const json::JsonValue &value) {
  MovementTuning t;
  t.walk_speed = number(value, "walk_speed", t.walk_speed);
  t.back_speed = number(value, "back_speed", t.back_speed);
  t.walk_accel = number(value, "walk_accel", t.walk_accel);
  t.dash_speed = number(value, "dash_speed", t.dash_speed);
  t.dash_back_speed = number(value, "dash_back_speed", t.dash_back_speed);
  t.turn_degrees_per_second = number(value, "turn_degrees_per_second", t.turn_degrees_per_second);
  t.dash_turn_degrees_per_second =
      number(value, "dash_turn_degrees_per_second", t.dash_turn_degrees_per_second);
  t.stick_threshold = number(value, "stick_threshold", t.stick_threshold);
  t.dash_stick = number(value, "dash_stick", t.dash_stick);
  t.dash_hold_seconds = number(value, "dash_hold_seconds", t.dash_hold_seconds);
  t.hit_seconds = number(value, "hit_seconds", t.hit_seconds);
  t.jump_speed = number(value, "jump_speed", t.jump_speed);
  t.jump_boost = number(value, "jump_boost", t.jump_boost);
  t.jump_boost_seconds = number(value, "jump_boost_seconds", t.jump_boost_seconds);
  t.gravity = number(value, "gravity", t.gravity);
  t.air_control = number(value, "air_control", t.air_control);
  t.dodge_speed = number(value, "dodge_speed", t.dodge_speed);
  t.dodge_seconds = number(value, "dodge_seconds", t.dodge_seconds);
  t.twist_degrees_per_second =
      number(value, "twist_degrees_per_second", t.twist_degrees_per_second);
  t.twist_limit_degrees = number(value, "twist_limit_degrees", t.twist_limit_degrees);
  t.twist_stretch_degrees = number(value, "twist_stretch_degrees", t.twist_stretch_degrees);
  t.twist_stick_keep = number(value, "twist_stick_keep", t.twist_stick_keep);
  t.twist_reset_seconds = number(value, "twist_reset_seconds", t.twist_reset_seconds);
  t.pitch_limit = number(value, "pitch_limit", t.pitch_limit);
  t.pitch_per_second = number(value, "pitch_per_second", t.pitch_per_second);
  return t;
}

Vec3 heading_forward(const float heading) {
  return {-std::sin(heading), 0.0F, -std::cos(heading)};
}

float heading_toward(const Vec3 direction) {
  return std::atan2(-direction.x, -direction.z);
}

Vec3 heading_right(const float heading) {
  return {-std::cos(heading), 0.0F, std::sin(heading)};
}

float wrap_angle(float angle) {
  angle = std::fmod(angle + pi, 2.0F * pi);
  if (angle < 0.0F) {
    angle += 2.0F * pi;
  }
  return angle - pi;
}

void RobotMover::place(const Vec3 position, const float heading) {
  position_ = position;
  heading_ = wrap_angle(heading);
  twist_ = 0.0F;
  twist_stick_ = 0.0F;
  twist_reset_left_ = 0.0F;
  pitch_ = 0.0F;
  grounded_ = position.y <= floor_ + ground_snap;
  if (grounded_) {
    position_.y = std::max(position_.y, floor_);
  }
  local_velocity_ = {};
  vertical_speed_ = 0.0F;
  set_mode(MoveMode::stand, robot_motion::stand, true);
}

Vec3 RobotMover::velocity() const {
  return heading_forward(heading_) * local_velocity_.z +
         heading_right(heading_) * local_velocity_.x + Vec3{0.0F, vertical_speed_, 0.0F};
}

void RobotMover::set_mode(const MoveMode mode, const int motion, const bool loops) {
  if (mode != mode_ || motion != motion_) {
    mode_seconds_ = 0.0F;
  }
  mode_ = mode;
  motion_ = motion;
  loops_ = loops;
  motion_speed_ = 1.0F;
}

void RobotMover::stagger(const float seconds, const Vec3 push) {
  stagger_left_ = std::max({stagger_left_, seconds, tuning_.hit_seconds});
  // The push in the robot's own frame picks the reaction: along or across,
  // whichever is larger, and its sign.
  const float along = dot(push, heading_forward(heading_));
  const float across = dot(push, heading_right(heading_));
  int motion = robot_motion::hit_left;
  if (std::abs(along) > std::abs(across)) {
    motion = along <= 0.0F ? robot_motion::hit_back : robot_motion::hit_forward;
  } else if (across > 0.0F) {
    motion = robot_motion::hit_right;
  }
  if (grounded_) {
    local_velocity_ = {};
  } else {
    motion += robot_motion::air_hit_offset;
  }
  set_mode(MoveMode::hit, motion, false);
}

void RobotMover::script(const ScriptedStep &step) {
  if (mode_ != MoveMode::scripted || motion_ != step.motion) {
    set_mode(MoveMode::scripted, step.motion, step.loops);
  }
  heading_ = wrap_angle(step.heading);
  local_velocity_ = {dot(step.velocity, heading_right(heading_)), 0.0F,
                     dot(step.velocity, heading_forward(heading_))};
  motion_speed_ = step.motion_speed;
}

void RobotMover::release() {
  local_velocity_ = {};
  set_mode(MoveMode::stand, robot_motion::stand, true);
}

void RobotMover::turn(const float stick, const float degrees_per_second, const float seconds) {
  heading_ = wrap_angle(heading_ + radians(degrees_per_second) * stick * seconds);
}

void RobotMover::tick(const RobotControl &control, const float seconds) {
  mode_seconds_ += seconds;
  dodged_ = 0;
  if (mode_ == MoveMode::scripted) {
    if (!grounded_) {
      vertical_speed_ -= tuning_.gravity * seconds;
    }
  } else {
    tick_twist(control, seconds);
    if (grounded_) {
      tick_ground(control, seconds);
    } else {
      tick_air(control, seconds);
    }
  }
  position_ = position_ + velocity() * seconds;
  if (grounded_ && position_.y > floor_ + ground_snap) {
    grounded_ = false; // walked off an edge
    if (mode_ != MoveMode::scripted) {
      set_mode(MoveMode::jump, robot_motion::jump, true);
    }
  }
  if (position_.y <= floor_ && vertical_speed_ <= 0.0F) {
    land(floor_);
  } else if (grounded_) {
    position_.y = floor_;
  }
}

void RobotMover::land(const float height) {
  position_.y = height;
  if (grounded_) {
    return;
  }
  grounded_ = true;
  vertical_speed_ = 0.0F;
  if (mode_ == MoveMode::hit) {
    local_velocity_ = {}; // the rest of the reaction on the spot
  }
  if (mode_ == MoveMode::jump) {
    loops_ = false; // the jump motion plays its landing
    mode_seconds_ = 0.0F;
  }
}

void RobotMover::bump_head() {
  vertical_speed_ = std::min(vertical_speed_, 0.0F);
  boost_left_ = 0.0F;
}

void RobotMover::tick_twist(const RobotControl &control, const float seconds) {
  heading_ = wrap_angle(heading_ + control.mouse_aim[0]);
  if (control.reset_twist && twist_reset_left_ <= 0.0F) {
    twist_reset_from_ = twist_;
    twist_reset_left_ = tuning_.twist_reset_seconds;
  }
  if (twist_reset_left_ > 0.0F) {
    twist_reset_left_ = std::max(twist_reset_left_ - seconds, 0.0F);
    twist_ = twist_reset_from_ * twist_reset_left_ / tuning_.twist_reset_seconds;
  } else {
    const float keep = std::pow(tuning_.twist_stick_keep, seconds * battle_tick_rate);
    twist_stick_ = twist_stick_ * keep + std::abs(control.twist) * (1.0F - keep);
    const float limit =
        radians(tuning_.twist_limit_degrees + tuning_.twist_stretch_degrees * twist_stick_);
    twist_ =
        std::clamp(twist_ + radians(tuning_.twist_degrees_per_second) * control.twist * seconds,
                   -limit, limit);
  }
  pitch_ =
      std::clamp(pitch_ + control.mouse_aim[1] + tuning_.pitch_per_second * control.pitch * seconds,
                 -tuning_.pitch_limit, tuning_.pitch_limit);
}

void RobotMover::tick_ground(const RobotControl &control, const float seconds) {
  const float forward = control.move[1];
  const float side = control.move[0];
  const bool pushed = std::abs(forward) > tuning_.stick_threshold;
  const bool steering = std::abs(side) > tuning_.stick_threshold;

  if (mode_ == MoveMode::hit) {
    stagger_left_ -= seconds;
    if (stagger_left_ > 0.0F) {
      return;
    }
    set_mode(MoveMode::stand, robot_motion::stand, true);
  }
  if (mode_ == MoveMode::dodge) {
    if (mode_seconds_ < tuning_.dodge_seconds) {
      return;
    }
    local_velocity_.x = 0.0F;
    set_mode(MoveMode::stand, robot_motion::stand, true);
  }
  if (mode_ == MoveMode::jump && !loops_ && mode_seconds_ < 0.25F) {
    local_velocity_ = local_velocity_ * std::max(0.0F, 1.0F - 8.0F * seconds);
    return; // landing
  }
  if (control.dodge != 0) {
    dodged_ = control.dodge;
    local_velocity_ = {tuning_.dodge_speed * static_cast<float>(control.dodge), 0.0F, 0.0F};
    set_mode(MoveMode::dodge,
             control.dodge < 0 ? robot_motion::dodge_left : robot_motion::dodge_right, false);
    return;
  }
  if (control.jump) {
    grounded_ = false;
    vertical_speed_ = tuning_.jump_speed;
    boost_left_ = tuning_.jump_boost_seconds;
    set_mode(MoveMode::jump, robot_motion::jump, true);
    return;
  }

  // Dashing: a click of the stick, or holding it far over for 30 frames
  // (0x08ff1e); it lasts while the stick stays pushed.
  if (mode_ == MoveMode::dash) {
    turn(side, tuning_.dash_turn_degrees_per_second, seconds);
    if (std::abs(forward) < tuning_.dash_stick * 0.5F || (forward < 0.0F) != backward_) {
      set_mode(MoveMode::brake, robot_motion::brake, false);
    } else {
      const float speed = backward_ ? -tuning_.dash_back_speed : tuning_.dash_speed;
      local_velocity_.z = approach(local_velocity_.z, speed, tuning_.walk_accel * 2.0F * seconds);
      local_velocity_.x = 0.0F;
      motion_speed_ = tuning_.dash_motion_rate;
      return;
    }
  }
  if (mode_ == MoveMode::brake) {
    local_velocity_.z = approach(local_velocity_.z, 0.0F, tuning_.walk_accel * 1.5F * seconds);
    if (mode_seconds_ < 0.35F) {
      return;
    }
  }
  if (pushed && (control.dash || dash_hold_ >= tuning_.dash_hold_seconds)) {
    dash_hold_ = 0.0F;
    backward_ = forward < 0.0F;
    set_mode(MoveMode::dash, backward_ ? robot_motion::dash_back : robot_motion::dash, true);
    motion_speed_ = tuning_.dash_motion_rate;
    return;
  }
  dash_hold_ = std::abs(forward) > tuning_.dash_stick ? dash_hold_ + seconds : 0.0F;

  if (pushed) {
    backward_ = forward < 0.0F;
    turn(side, tuning_.turn_degrees_per_second, seconds);
    const float full = backward_ ? tuning_.back_speed : tuning_.walk_speed;
    // The walk runs at half to full speed with the stick (0x08e6b6).
    const float amount = std::clamp(std::abs(forward), 0.5F, 1.0F);
    const float target = (backward_ ? -full : full) * amount;
    local_velocity_.z = approach(local_velocity_.z, target, tuning_.walk_accel * seconds);
    local_velocity_.x = 0.0F;
    set_mode(MoveMode::walk, backward_ ? robot_motion::walk_back : robot_motion::walk, true);
    motion_speed_ = amount * tuning_.walk_motion_rate;
    return;
  }
  local_velocity_.z = approach(local_velocity_.z, 0.0F, tuning_.walk_accel * seconds);
  local_velocity_.x = 0.0F;
  if (steering) {
    // Sideways on the spot turns in place (the lesson at state 0x1a).
    turn(side, tuning_.turn_degrees_per_second, seconds);
    set_mode(MoveMode::turn, side > 0.0F ? robot_motion::turn_right : robot_motion::turn_left,
             true);
    return;
  }
  if (mode_ != MoveMode::stand && std::abs(local_velocity_.z) < 0.5F) {
    set_mode(MoveMode::stand, robot_motion::stand, true);
  }
}

void RobotMover::tick_air(const RobotControl &control, const float seconds) {
  if (mode_ == MoveMode::hit) {
    stagger_left_ -= seconds;
    boost_left_ = 0.0F;
    vertical_speed_ -= tuning_.gravity * seconds;
    if (stagger_left_ <= 0.0F) {
      set_mode(MoveMode::jump, robot_motion::air, true);
    }
    return;
  }
  if (control.dodge != 0 && mode_ != MoveMode::dodge) {
    dodged_ = control.dodge;
    local_velocity_.x = tuning_.dodge_speed * static_cast<float>(control.dodge);
    set_mode(MoveMode::dodge,
             control.dodge < 0 ? robot_motion::air_dodge_left : robot_motion::air_dodge_right,
             false);
  }
  if (mode_ == MoveMode::dodge && mode_seconds_ >= tuning_.dodge_seconds) {
    local_velocity_.x = 0.0F;
    set_mode(MoveMode::jump, robot_motion::air, true);
  }
  if (control.jump && boost_left_ > 0.0F) {
    vertical_speed_ += tuning_.jump_boost * seconds;
  }
  boost_left_ = control.jump ? boost_left_ - seconds : 0.0F;
  vertical_speed_ -= tuning_.gravity * seconds;
  turn(control.move[0], tuning_.turn_degrees_per_second * 0.5F, seconds);
  const float steer = control.move[1] * tuning_.walk_speed * tuning_.air_control;
  local_velocity_.z =
      std::clamp(local_velocity_.z + steer * seconds, -tuning_.dash_speed, tuning_.dash_speed);
}

} // namespace phantomcrash
