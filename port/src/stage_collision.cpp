#include "phantomcrash/stage_collision.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace phantomcrash {
namespace {

// The contact flags per surface % 1000 (0x2057d8).
constexpr std::array<unsigned, 11> surface_table{0x1,   0x40,   0x80,   0x100,  0x200, 0x400,
                                                 0x800, 0x1000, 0x2000, 0x4000, 0x8000};
constexpr int surface_modulus = 1000;
constexpr float grid_cell = 8.0F;   // port grid, world units
constexpr int push_passes = 3;      // corners need a second push
constexpr int box_wall_surface = 9; // a plain wall
constexpr float parallel = 1e-7F;

struct Closest {
  Vec3 point;
  bool face{}; // inside the triangle, not on an edge or corner
};

// The point of the triangle nearest `p` (Ericson, Real-Time Collision
// Detection 5.1.5).
Closest closest_point(const Vec3 p, const std::array<Vec3, 3> &t) {
  const Vec3 a = t[0], b = t[1], c = t[2];
  const Vec3 ab = b - a, ac = c - a, ap = p - a;
  const float d1 = dot(ab, ap), d2 = dot(ac, ap);
  if (d1 <= 0.0F && d2 <= 0.0F) {
    return {a};
  }
  const Vec3 bp = p - b;
  const float d3 = dot(ab, bp), d4 = dot(ac, bp);
  if (d3 >= 0.0F && d4 <= d3) {
    return {b};
  }
  const float vc = d1 * d4 - d3 * d2;
  if (vc <= 0.0F && d1 >= 0.0F && d3 <= 0.0F) {
    return {a + ab * (d1 / (d1 - d3))};
  }
  const Vec3 cp = p - c;
  const float d5 = dot(ab, cp), d6 = dot(ac, cp);
  if (d6 >= 0.0F && d5 <= d6) {
    return {c};
  }
  const float vb = d5 * d2 - d1 * d6;
  if (vb <= 0.0F && d2 >= 0.0F && d6 <= 0.0F) {
    return {a + ac * (d2 / (d2 - d6))};
  }
  const float va = d3 * d6 - d5 * d4;
  if (va <= 0.0F && d4 - d3 >= 0.0F && d5 - d6 >= 0.0F) {
    return {b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)))};
  }
  const float scale = 1.0F / (va + vb + vc);
  return {a + ab * (vb * scale) + ac * (vc * scale), true};
}

// The triangle's height at (x, z), if the point lies over it.
std::optional<float> height_at(const CollisionTriangle &t, const float x, const float z) {
  if (std::abs(t.normal.y) < parallel) {
    return std::nullopt;
  }
  const Vec3 a = t.points[0], b = t.points[1], c = t.points[2];
  const float det = (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
  if (std::abs(det) < parallel) {
    return std::nullopt;
  }
  const float u = ((b.z - c.z) * (x - c.x) + (c.x - b.x) * (z - c.z)) / det;
  const float v = ((c.z - a.z) * (x - c.x) + (a.x - c.x) * (z - c.z)) / det;
  if (u < 0.0F || v < 0.0F || u + v > 1.0F) {
    return std::nullopt;
  }
  return a.y * u + b.y * v + c.y * (1.0F - u - v);
}

// Möller-Trumbore: the segment parameter where it crosses the triangle.
std::optional<float> segment_cross(const Vec3 from, const Vec3 delta, const CollisionTriangle &t) {
  const Vec3 e1 = t.points[1] - t.points[0];
  const Vec3 e2 = t.points[2] - t.points[0];
  const Vec3 p = cross(delta, e2);
  const float det = dot(e1, p);
  if (std::abs(det) < parallel) {
    return std::nullopt;
  }
  const Vec3 s = from - t.points[0];
  const float u = dot(s, p) / det;
  const Vec3 q = cross(s, e1);
  const float v = dot(delta, q) / det;
  const float along = dot(e2, q) / det;
  if (u < 0.0F || v < 0.0F || u + v > 1.0F || along < 0.0F || along > 1.0F) {
    return std::nullopt;
  }
  return along;
}

CollisionTriangle triangle(const Vec3 a, const Vec3 b, const Vec3 c, const int surface) {
  return {{a, b, c}, normalize(cross(b - a, c - a)), surface};
}

// Two triangles facing `inward` for the quad a b c d (in order round it).
void quad(std::vector<CollisionTriangle> &out, const Vec3 a, const Vec3 b, const Vec3 c,
          const Vec3 d, const Vec3 inward) {
  CollisionTriangle first = triangle(a, b, c, box_wall_surface);
  CollisionTriangle second = triangle(a, c, d, box_wall_surface);
  if (dot(first.normal, inward) < 0.0F) {
    first = triangle(a, c, b, box_wall_surface);
    second = triangle(a, d, c, box_wall_surface);
  }
  out.push_back(first);
  out.push_back(second);
}

Vec3 point(const json::JsonValue &value) {
  const auto &xyz = value.array();
  if (xyz.size() != 3) {
    throw std::runtime_error("collision point needs [x, y, z]");
  }
  return {static_cast<float>(xyz[0].number()), static_cast<float>(xyz[1].number()),
          static_cast<float>(xyz[2].number())};
}

std::vector<CollisionTriangle> triangles(const json::JsonValue &value, const char *key) {
  std::vector<CollisionTriangle> out;
  const json::JsonValue *list = value.find(key);
  if (list == nullptr) {
    return out;
  }
  for (const json::JsonValue &entry : list->array()) {
    const json::JsonValue *corners = entry.find("points");
    if (corners == nullptr || corners->array().size() != 3) {
      throw std::runtime_error("collision triangle needs three points");
    }
    const auto &points = corners->array();
    out.push_back(triangle(point(points[0]), point(points[1]), point(points[2]),
                           static_cast<int>(entry.number_or("surface", box_wall_surface))));
  }
  return out;
}

} // namespace

unsigned surface_flags(const int surface) {
  const auto index = static_cast<std::size_t>(std::abs(surface) % surface_modulus);
  return index < surface_table.size() ? surface_table[index] : 0U;
}

StageCollision::StageCollision(std::vector<CollisionTriangle> walls,
                               std::vector<CollisionTriangle> floor)
    : triangles_walls_(std::move(walls)), triangles_floor_(std::move(floor)) {
  build_grid();
}

StageCollision StageCollision::box(const ArenaBounds &arena, const float wall_height) {
  std::vector<CollisionTriangle> walls;
  const float h = arena.half_size;
  const float top = wall_height;
  // The outer walls face the middle.
  const std::array<Vec3, 4> corners{Vec3{-h, 0, -h}, Vec3{h, 0, -h}, Vec3{h, 0, h}, Vec3{-h, 0, h}};
  for (std::size_t i = 0; i < corners.size(); ++i) {
    const Vec3 a = corners[i];
    const Vec3 b = corners[(i + 1) % corners.size()];
    quad(walls, a, b, b + Vec3{0, top, 0}, a + Vec3{0, top, 0}, (a + b) * -1.0F);
  }
  // The blocks face out.
  for (const auto &block : arena.blocks) {
    const std::array<Vec3, 4> box{Vec3{block[0], 0, block[1]}, Vec3{block[2], 0, block[1]},
                                  Vec3{block[2], 0, block[3]}, Vec3{block[0], 0, block[3]}};
    const Vec3 middle{(block[0] + block[2]) * 0.5F, 0.0F, (block[1] + block[3]) * 0.5F};
    for (std::size_t i = 0; i < box.size(); ++i) {
      const Vec3 a = box[i];
      const Vec3 b = box[(i + 1) % box.size()];
      quad(walls, a, b, b + Vec3{0, top, 0}, a + Vec3{0, top, 0}, (a + b) * 0.5F - middle);
    }
  }
  std::vector<CollisionTriangle> floor;
  quad(floor, corners[0], corners[1], corners[2], corners[3], {0.0F, 1.0F, 0.0F});
  return {std::move(walls), std::move(floor)};
}

void StageCollision::build_grid() {
  float x0 = 0.0F, z0 = 0.0F, x1 = 0.0F, z1 = 0.0F;
  bool first = true;
  for (const auto *set : {&triangles_walls_, &triangles_floor_}) {
    for (const CollisionTriangle &t : *set) {
      for (const Vec3 &p : t.points) {
        x0 = first ? p.x : std::min(x0, p.x);
        z0 = first ? p.z : std::min(z0, p.z);
        x1 = first ? p.x : std::max(x1, p.x);
        z1 = first ? p.z : std::max(z1, p.z);
        first = false;
      }
    }
  }
  origin_x_ = x0;
  origin_z_ = z0;
  columns_ = std::max(1, static_cast<int>(std::ceil((x1 - x0) / grid_cell)) + 1);
  rows_ = std::max(1, static_cast<int>(std::ceil((z1 - z0) / grid_cell)) + 1);
  grid_.assign(static_cast<std::size_t>(columns_ * rows_), {});
  const auto add = [&](const std::vector<CollisionTriangle> &set, const bool floor) {
    for (std::uint32_t index = 0; index < set.size(); ++index) {
      const auto &p = set[index].points;
      const float lx = std::min({p[0].x, p[1].x, p[2].x});
      const float lz = std::min({p[0].z, p[1].z, p[2].z});
      const float hx = std::max({p[0].x, p[1].x, p[2].x});
      const float hz = std::max({p[0].z, p[1].z, p[2].z});
      const CellRange range = cell_range(lx, lz, hx, hz);
      for (int c = range.first_column; c <= range.last_column; ++c) {
        for (int r = range.first_row; r <= range.last_row; ++r) {
          Cell &cell = grid_[static_cast<std::size_t>(c * rows_ + r)];
          (floor ? cell.floor : cell.walls).push_back(index);
        }
      }
    }
  };
  add(triangles_walls_, false);
  add(triangles_floor_, true);
}

StageCollision::CellRange StageCollision::cell_range(const float x0, const float z0, const float x1,
                                                     const float z1) const {
  const auto column = [&](const float x) {
    return std::clamp(static_cast<int>(std::floor((x - origin_x_) / grid_cell)), 0, columns_ - 1);
  };
  const auto row = [&](const float z) {
    return std::clamp(static_cast<int>(std::floor((z - origin_z_) / grid_cell)), 0, rows_ - 1);
  };
  return {column(x0), column(x1), row(z0), row(z1)};
}

std::vector<std::uint32_t> StageCollision::candidates(const float x0, const float z0,
                                                      const float x1, const float z1,
                                                      const bool floor) const {
  std::vector<std::uint32_t> found;
  if (grid_.empty()) {
    return found;
  }
  const CellRange range = cell_range(x0, z0, x1, z1);
  for (int c = range.first_column; c <= range.last_column; ++c) {
    for (int r = range.first_row; r <= range.last_row; ++r) {
      const Cell &cell = grid_[static_cast<std::size_t>(c * rows_ + r)];
      const auto &list = floor ? cell.floor : cell.walls;
      found.insert(found.end(), list.begin(), list.end());
    }
  }
  std::ranges::sort(found);
  const auto repeated = std::ranges::unique(found);
  found.erase(repeated.begin(), repeated.end());
  return found;
}

float StageCollision::ground_height(const Vec3 at, const float step) const {
  float best = no_ground;
  const auto consider = [&](const CollisionTriangle &t) {
    if (t.normal.y <= ground_normal_y) {
      return;
    }
    if (const auto h = height_at(t, at.x, at.z); h && *h <= at.y + step && *h > best) {
      best = *h;
    }
  };
  for (const std::uint32_t index : candidates(at.x, at.z, at.x, at.z, true)) {
    consider(triangles_floor_[index]);
  }
  for (const std::uint32_t index : candidates(at.x, at.z, at.x, at.z, false)) {
    consider(triangles_walls_[index]);
  }
  return best;
}

unsigned StageCollision::push_sphere(Vec3 &centre, const float radius) const {
  unsigned flags = 0;
  const std::vector<std::uint32_t> near =
      candidates(centre.x - radius, centre.z - radius, centre.x + radius, centre.z + radius, false);
  for (int pass = 0; pass < push_passes; ++pass) {
    bool pushed = false;
    for (const std::uint32_t index : near) {
      const CollisionTriangle &t = triangles_walls_[index];
      const Closest nearest = closest_point(centre, t.points);
      const Vec3 away = centre - nearest.point;
      const float distance = length(away);
      if (distance >= radius) {
        continue;
      }
      if (nearest.face && dot(away, t.normal) < 0.0F) {
        continue; // behind a face
      }
      const Vec3 direction = distance > parallel ? away * (1.0F / distance) : t.normal;
      centre = nearest.point + direction * radius;
      flags |= stage_contact::touched | surface_flags(t.surface) |
               (direction.y > ground_normal_y ? stage_contact::ground : stage_contact::wall);
      pushed = true;
    }
    if (!pushed) {
      break;
    }
  }
  return flags;
}

std::optional<Vec3> StageCollision::segment_hit(const Vec3 from, const Vec3 to) const {
  const Vec3 delta = to - from;
  std::optional<float> nearest;
  const auto test = [&](const CollisionTriangle &t) {
    if (const auto along = segment_cross(from, delta, t);
        along && (!nearest || *along < *nearest)) {
      nearest = along;
    }
  };
  const float x0 = std::min(from.x, to.x), x1 = std::max(from.x, to.x);
  const float z0 = std::min(from.z, to.z), z1 = std::max(from.z, to.z);
  for (const std::uint32_t index : candidates(x0, z0, x1, z1, false)) {
    test(triangles_walls_[index]);
  }
  for (const std::uint32_t index : candidates(x0, z0, x1, z1, true)) {
    test(triangles_floor_[index]);
  }
  if (!nearest) {
    return std::nullopt;
  }
  return from + delta * *nearest;
}

StageCollision parse_stage_collision(const json::JsonValue &value) {
  return {triangles(value, "walls"), triangles(value, "floor")};
}

} // namespace phantomcrash
