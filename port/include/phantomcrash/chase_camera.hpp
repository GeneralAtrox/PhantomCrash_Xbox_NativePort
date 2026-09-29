#pragma once

#include "phantomcrash/json.hpp"
#include "phantomcrash/math3d.hpp"
#include "phantomcrash/stage_collision.hpp"

#include <optional>

namespace phantomcrash {

// The battle camera: behind and above the player's upper body, looking
// past it along the aim. The original's battle camera is not recovered
// yet; these are port values (data/world/training.json "chase_camera").
struct ChaseCameraSettings {
  float fov_degrees{55.0F};
  float near_plane{0.3F};
  float far_plane{6000.0F};
  float distance{10.0F};         // behind the robot
  float height{5.0F};            // above its ground point
  float look_height{3.4F};       // the point looked at, above the ground point ...
  float look_ahead{8.0F};        // ... and this far along the aim
  float follow_per_second{8.0F}; // how fast the camera swings after the aim
  float pitch_follow{6.0F};      // look height per radian of aim pitch
  float wall_margin{1.5F};       // clearance from a wall toward the robot
};

[[nodiscard]] ChaseCameraSettings parse_chase_camera(const json::JsonValue &value);

// Where a camera stands and looks, its vertical field of view and its roll
// (radians about the line of sight).
struct CameraShot {
  Vec3 eye;
  Vec3 target;
  float fov_degrees{55.0F};
  float roll{};
};

class ChaseCamera final {
public:
  explicit ChaseCamera(const ChaseCameraSettings &settings) : settings_(settings) {}

  // Follows a robot at `position` aiming along `heading` and `pitch`;
  // The active stage brings the eye in front of walls. `snap` jumps there.
  void tick(Vec3 position, float heading, float pitch, const StageCollision &stage, float seconds,
            bool snap = false);

  // The point above the robot used to keep walls and doors out of the view.
  [[nodiscard]] Vec3 anchor() const noexcept {
    return anchor_;
  }

  // Something solid at `hit` between `from` and the eye: the eye comes
  // in front of it, wall_margin nearer `from`.
  void block_eye(Vec3 from, Vec3 hit);
  // A scripted shot (battle_scene.hpp) in place of the chase view; nullopt
  // gives the view back. The chase view keeps following underneath.
  void set_shot(std::optional<CameraShot> shot) {
    shot_ = shot;
  }
  // The chase view, whatever shot is shown.
  [[nodiscard]] CameraShot chase() const noexcept {
    return {eye_, target_, settings_.fov_degrees, 0.0F};
  }
  [[nodiscard]] Vec3 eye() const noexcept {
    return shot_ ? shot_->eye : eye_;
  }
  [[nodiscard]] Vec3 target() const noexcept {
    return shot_ ? shot_->target : target_;
  }
  [[nodiscard]] Mat4 view() const;
  [[nodiscard]] Mat4 projection(float aspect) const;

private:
  ChaseCameraSettings settings_;
  float heading_{};
  float pitch_{};
  Vec3 eye_{0.0F, 5.0F, 10.0F};
  Vec3 target_{};
  Vec3 anchor_{};
  std::optional<CameraShot> shot_;
};

} // namespace phantomcrash
