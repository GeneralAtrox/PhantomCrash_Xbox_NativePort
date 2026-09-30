#pragma once

#include "phantomcrash/optical_camouflage.hpp"
#include "phantomcrash/stage_collision.hpp"

#include <string>
#include <utility>
#include <vector>

namespace phantomcrash {

// 0x07027b / 0x071869: a pool of surface-aligned, refracting mesh flipbooks.
struct FenceLook {
  std::string model;
  std::vector<int> meshes;
  int capacity{20};
  float frame_rate{30.0F};
  float scale{0.1F};
  float growth{0.05F};
  float depth{0.2F};
  float offset{-0.5F};
  OpticalSphere refraction{{}, 0.0F, 1.0F, 4.0F, 1.0F, 0.25F};
};

struct FenceFrame {
  int mesh{};
  Mat4 world;
  OpticalSphere refraction;
};

[[nodiscard]] FenceLook parse_fence_look(const json::JsonValue &value);

class FenceImpacts {
public:
  explicit FenceImpacts(FenceLook look) : look_(std::move(look)) {}
  void start(const StageHit &hit);
  void tick(float seconds);
  [[nodiscard]] std::vector<FenceFrame> frames() const;

private:
  struct Impact {
    Vec3 at;
    Vec3 normal;
    double age{};
  };
  FenceLook look_;
  std::vector<Impact> impacts_;
};

} // namespace phantomcrash
