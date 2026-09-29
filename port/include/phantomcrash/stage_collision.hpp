#pragma once

#include "phantomcrash/json.hpp"
#include "phantomcrash/math3d.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace phantomcrash {

// A collision triangle in game space; the normal follows the winding,
// cross(b - a, c - a).
struct CollisionTriangle {
  std::array<Vec3, 3> points;
  Vec3 normal;
  int surface{};
};

// Result flags of the stage tests, as the original's (0x052c1f, 0x054c45).
namespace stage_contact {
inline constexpr unsigned touched = 0x1;
inline constexpr unsigned ground = 0x8;   // pushed up by a surface steep enough to stand on
inline constexpr unsigned wall = 0x10;    // pushed by anything else
inline constexpr unsigned fence = 0x2000; // surface 8: the arena's fence (0x07027b sparks)
} // namespace stage_contact

// The flags a surface adds on contact: surface % 1000 indexes the table at
// 0x2057d8.
[[nodiscard]] unsigned surface_flags(int surface);

// Normals with y above this can be stood on (0x054c45's 0.75).
inline constexpr float ground_normal_y = 0.75F;
// No ground under a point (0x053337 answers -1000).
inline constexpr float no_ground = -1000.0F;

// The walls: the square |x|, |z| <= half_size, less the corner towers and
// other blocks given as {x0, z0, x1, z1} boxes. Used when a stage has no
// collision file.
struct ArenaBounds {
  float half_size{100.0F};
  std::vector<std::array<float, 4>> blocks;
};

// A stage's collision: wall triangles pushed against as spheres and floor
// triangles under the robots (the converted `collision/*.json`, see
// src/phantomcrash_research/collision.py), searched through a grid over x
// and z.
class StageCollision final {
public:
  StageCollision() = default;
  StageCollision(std::vector<CollisionTriangle> walls, std::vector<CollisionTriangle> floor);

  // Four walls, the blocks as boxes and a floor at y = 0.
  [[nodiscard]] static StageCollision box(const ArenaBounds &arena, float wall_height);

  // The highest ground under `at` that is no more than `step` above it:
  // floor triangles and walls flat enough to stand on. no_ground if none.
  [[nodiscard]] float ground_height(Vec3 at, float step) const;
  // Pushes a sphere out of the walls; returns stage_contact flags. Faces
  // push only from their front; edges and corners from either side.
  unsigned push_sphere(Vec3 &centre, float radius) const;
  // Where the segment first meets a wall or the floor.
  [[nodiscard]] std::optional<Vec3> segment_hit(Vec3 from, Vec3 to) const;

  [[nodiscard]] const std::vector<CollisionTriangle> &walls() const noexcept {
    return triangles_walls_;
  }
  [[nodiscard]] const std::vector<CollisionTriangle> &floor() const noexcept {
    return triangles_floor_;
  }

private:
  struct Cell {
    std::vector<std::uint32_t> walls;
    std::vector<std::uint32_t> floor;
  };
  struct CellRange {
    int first_column, last_column, first_row, last_row;
  };
  void build_grid();
  // The cells overlapping the box x0..x1, z0..z1 (clamped to the grid).
  [[nodiscard]] CellRange cell_range(float x0, float z0, float x1, float z1) const;
  [[nodiscard]] std::vector<std::uint32_t> candidates(float x0, float z0, float x1, float z1,
                                                      bool floor) const;

  std::vector<CollisionTriangle> triangles_walls_;
  std::vector<CollisionTriangle> triangles_floor_;
  std::vector<Cell> grid_;
  float origin_x_{};
  float origin_z_{};
  int columns_{};
  int rows_{};
};

[[nodiscard]] StageCollision parse_stage_collision(const json::JsonValue &value);

} // namespace phantomcrash
