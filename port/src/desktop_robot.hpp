#pragma once

#include "desktop_world_renderer.hpp"
#include "phantomcrash/robot.hpp"
#include "phantomcrash/skeleton.hpp"

#include <array>
#include <optional>
#include <vector>

namespace phantomcrash {

// A robot's part models on the GPU, drawn from a pose: the body and legs
// share pose part 0 (the legs' bone slots follow the body's), each arm has
// its own part, and the shoulder weapons ride the upper body's bone (the
// node flagged 0x4; 0x0acb36 hands it to all four weapon slots).
class RobotVisual final {
public:
  RobotVisual(WorldRenderer &world, const RobotAssembly &assembly);

  [[nodiscard]] bool ready() const noexcept {
    return body_ >= 0 && legs_ >= 0;
  }

  // Appends the robot's parts, posed by `pose`, to `items`; `root` places
  // parts whose bones the pose did not reach.
  void add_items(const Pose &pose, const Mat4 &root, std::vector<WorldItem> &items);
  // Where `pose` puts the upper body's bone, if it reached it.
  [[nodiscard]] std::optional<Vec3> torso(const Pose &pose) const;

private:
  struct Part {
    int model{-1};
    int pose_part{};
    int first_slot{};
    std::vector<Mat4> node_worlds;
  };

  void add(Part &part, const Pose &pose, const Mat4 &root, std::vector<WorldItem> &items);

  WorldRenderer &world_;
  int body_{-1};
  int legs_{-1};
  int paint_{-1};
  int torso_slot_{-1};
  std::vector<Part> parts_;
  std::vector<Part> shoulders_;
};

} // namespace phantomcrash
