#pragma once

#include "phantomcrash/robot.hpp"
#include "phantomcrash/stage_collision.hpp"

namespace phantomcrash {

struct RobotWreckSettings {
  float burn_seconds{6.45F};
  int reward_ny{5000};
  int break_effect{15};
  std::string break_sound{"sound/sfx/battle/16.wav"};
};

[[nodiscard]] RobotWreckSettings parse_robot_wreck_settings(const json::JsonValue &value);

// Independent of the living roster: replacing a pilot never removes their wreck.
struct RobotWreck {
  RobotAssembly assembly;
  Pose pose;
  std::array<Mat4, 2> shoulders;
  std::array<bool, 4> lost{};
  Mat4 root;
  Vec3 centre;
  float radius{};
  float burn_left{};
  bool removed{};
  std::optional<StageCollision> collision;

  [[nodiscard]] bool solid() const {
    return !removed && burn_left <= 0 && collision.has_value();
  }
};

class RobotWrecks final {
public:
  void add(RobotWreck wreck);
  void tick(float seconds);
  // Installed from the same posed geometry used to draw this wreck.
  void set_collision(std::size_t index, StageCollision collision);
  [[nodiscard]] bool destroy(std::size_t index);
  [[nodiscard]] const std::vector<RobotWreck> &items() const {
    return items_;
  }
  [[nodiscard]] float ground_height(Vec3 at, float step, float radius) const;
  unsigned push_sphere(Vec3 &at, float radius) const;
  [[nodiscard]] std::optional<Vec3> segment_hit(Vec3 from, Vec3 to) const;

private:
  // Tombstones retain target indices held by in-flight projectiles.
  std::vector<RobotWreck> items_;
};
} // namespace phantomcrash
